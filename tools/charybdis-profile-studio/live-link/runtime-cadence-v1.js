"use strict";

const {
    PROFILE_WIRE_V1,
    ProfileWireProtocolError,
    buildProfileGetRequest,
    decodeProfileResponse,
    profileResponseMatcher,
} = require("./profile-wire-v1");

const RUNTIME_CADENCE_V1 = Object.freeze({
    FORMAT: "charybdis-runtime-cadence-v1",
    VALUE: 0x03,
    LAYOUT_VERSION: 1,
    PAGE_COUNT: 31,
    WINDOW_COUNT: 30,
    HISTOGRAM_UPPER_BOUNDS_US: Object.freeze([1000, 1250, 1500, 2000, 5000]),
    HISTOGRAM_BUCKET_COUNT: 6,
    UNUSED_LOGICAL_INDEX: 0xff,
    DEFAULT_SEQUENCE_RETRIES: 4,
});

const RUNTIME_CADENCE_THRESHOLDS = Object.freeze({
    MEDIAN_RATE_RATIO: 0.95,
    FIFTH_PERCENTILE_RATE_RATIO: 0.90,
    POINTING_TO_SCAN_RATIO: 0.99,
    P999_GAP_RATIO: 1.10,
    P999_GAP_ALLOWANCE_US: 250,
    WORST_GAP_ALLOWANCE_US: 2000,
});

function readU16(buffer, offset) {
    return buffer[offset] | (buffer[offset + 1] << 8);
}

function readU32(buffer, offset) {
    return (
        buffer[offset]
        | (buffer[offset + 1] << 8)
        | (buffer[offset + 2] << 16)
        | (buffer[offset + 3] << 24)
    ) >>> 0;
}

function decodeRuntimeCadenceMetadata(payload) {
    const page = normalizePayload(payload, "runtime cadence metadata page");
    const histogramUpperBoundsUs = RUNTIME_CADENCE_V1.HISTOGRAM_UPPER_BOUNDS_US.map((unused, index) =>
        readU16(page, 11 + index * 2)
    );
    const decoded = {
        layoutVersion: page[0],
        pageCount: page[1],
        completedCount: page[2],
        sequence: readU32(page, 3),
        windowUs: readU32(page, 7),
        histogramUpperBoundsUs,
        started: decodeBoolean(page[21], "runtime cadence started flag"),
    };

    assertZeroRange(page, 22, PROFILE_WIRE_V1.PAYLOAD_SIZE, "runtime cadence metadata page");
    if (decoded.layoutVersion !== RUNTIME_CADENCE_V1.LAYOUT_VERSION) {
        throw protocolError("INCOMPATIBLE_RESPONSE", `Unsupported runtime cadence layout ${decoded.layoutVersion}.`);
    }
    if (decoded.pageCount !== RUNTIME_CADENCE_V1.PAGE_COUNT) {
        throw protocolError("INCOMPATIBLE_RESPONSE", `Runtime cadence layout requires ${RUNTIME_CADENCE_V1.PAGE_COUNT} pages; firmware advertised ${decoded.pageCount}.`);
    }
    if (decoded.completedCount > RUNTIME_CADENCE_V1.WINDOW_COUNT) {
        throw protocolError("MALFORMED_RESPONSE", "Runtime cadence completed-window count exceeds capacity.");
    }
    if (decoded.windowUs === 0) {
        throw protocolError("MALFORMED_RESPONSE", "Runtime cadence window duration must be nonzero.");
    }
    if (!arraysEqual(decoded.histogramUpperBoundsUs, RUNTIME_CADENCE_V1.HISTOGRAM_UPPER_BOUNDS_US)) {
        throw protocolError("INCOMPATIBLE_RESPONSE", "Runtime cadence histogram boundaries do not match layout v1.");
    }
    return decoded;
}

function decodeRuntimeCadenceWindow(payload) {
    const page = normalizePayload(payload, "runtime cadence window page");
    const histogram = [];
    for (let bucket = 0; bucket < RUNTIME_CADENCE_V1.HISTOGRAM_BUCKET_COUNT; bucket += 1) {
        histogram.push(readU16(page, 13 + bucket * 2));
    }
    const decoded = {
        sequence: readU32(page, 0),
        logicalIndex: page[4],
        maxGapUs: readU32(page, 5),
        matrixScans: readU16(page, 9),
        pointingPolls: readU16(page, 11),
        histogram,
    };
    if (decoded.logicalIndex !== RUNTIME_CADENCE_V1.UNUSED_LOGICAL_INDEX && decoded.logicalIndex >= RUNTIME_CADENCE_V1.WINDOW_COUNT) {
        throw protocolError("MALFORMED_RESPONSE", `Runtime cadence logical window index ${decoded.logicalIndex} is out of range.`);
    }
    const histogramCount = sum(histogram);
    if (histogramCount > decoded.pointingPolls) {
        throw protocolError("MALFORMED_RESPONSE", "Runtime cadence histogram contains more gaps than pointing polls.");
    }
    return decoded;
}

function decodeRuntimeCadencePages(pages) {
    if (!Array.isArray(pages) || pages.length !== RUNTIME_CADENCE_V1.PAGE_COUNT) {
        throw protocolError("MALFORMED_RESPONSE", `Runtime cadence read requires exactly ${RUNTIME_CADENCE_V1.PAGE_COUNT} pages.`);
    }
    const metadata = decodeRuntimeCadenceMetadata(pages[0]);
    const decodedWindows = pages.slice(1).map(decodeRuntimeCadenceWindow);
    const mismatched = decodedWindows.find((window) => window.sequence !== metadata.sequence);
    if (mismatched) {
        throw protocolError(
            "SEQUENCE_MISMATCH",
            `Runtime cadence snapshot changed while pages were read (${metadata.sequence} to ${mismatched.sequence}).`,
            {expectedSequence: metadata.sequence, actualSequence: mismatched.sequence}
        );
    }

    const used = decodedWindows.filter((window) => window.logicalIndex !== RUNTIME_CADENCE_V1.UNUSED_LOGICAL_INDEX);
    const unused = decodedWindows.filter((window) => window.logicalIndex === RUNTIME_CADENCE_V1.UNUSED_LOGICAL_INDEX);
    if (used.length !== metadata.completedCount || unused.length !== RUNTIME_CADENCE_V1.WINDOW_COUNT - metadata.completedCount) {
        throw protocolError("MALFORMED_RESPONSE", "Runtime cadence page usage does not match completed-window count.");
    }
    used.sort((left, right) => left.logicalIndex - right.logicalIndex);
    used.forEach((window, index) => {
        if (window.logicalIndex !== index) {
            throw protocolError("MALFORMED_RESPONSE", "Runtime cadence logical window indexes must be unique and contiguous.");
        }
    });

    return normalizeRuntimeCadenceCapture({
        format: RUNTIME_CADENCE_V1.FORMAT,
        layoutVersion: metadata.layoutVersion,
        pageCount: metadata.pageCount,
        completedCount: metadata.completedCount,
        sequence: metadata.sequence,
        windowUs: metadata.windowUs,
        histogramUpperBoundsUs: metadata.histogramUpperBoundsUs,
        started: metadata.started,
        windows: used.map(({sequence, logicalIndex, ...window}) => ({index: logicalIndex, ...window})),
    });
}

async function readRuntimeCadenceMetadata(connection, options = {}) {
    const requestIds = createRequestIdSource(options);
    const payload = await requestCadencePage(connection, 0, requestIds, options);
    return decodeRuntimeCadenceMetadata(payload);
}

async function readRuntimeCadenceCapture(connection, options = {}) {
    assertConnection(connection);
    const retries = normalizeRetryCount(options.sequenceRetries);
    const requestIds = createRequestIdSource(options);
    let lastMismatch;

    for (let attempt = 0; attempt <= retries; attempt += 1) {
        const pages = [];
        const metadataPayload = await requestCadencePage(connection, 0, requestIds, options);
        const metadata = decodeRuntimeCadenceMetadata(metadataPayload);
        pages.push(metadataPayload);

        let rolled = false;
        for (let page = 1; page < metadata.pageCount; page += 1) {
            const payload = await requestCadencePage(connection, page, requestIds, options);
            const window = decodeRuntimeCadenceWindow(payload);
            pages.push(payload);
            if (window.sequence !== metadata.sequence) {
                lastMismatch = {expectedSequence: metadata.sequence, actualSequence: window.sequence, page};
                rolled = true;
                break;
            }
        }
        if (!rolled) {
            return decodeRuntimeCadencePages(pages);
        }
    }

    throw protocolError(
        "SEQUENCE_UNSTABLE",
        `Runtime cadence snapshot rolled during ${retries + 1} consecutive read attempts.`,
        {...lastMismatch, attempts: retries + 1}
    );
}

async function requestCadencePage(connection, page, requestIds, options) {
    assertConnection(connection);
    const request = buildProfileGetRequest(RUNTIME_CADENCE_V1.VALUE, page, requestIds.next());
    const response = await connection.request(request, {
        matchResponse: profileResponseMatcher,
        signal: options.signal,
        timeoutMs: options.timeoutMs,
    });
    return Buffer.from(decodeProfileResponse(response, request));
}

function summarizeRuntimeCadence(capture) {
    const normalized = normalizeRuntimeCadenceCapture(capture);
    const matrixRates = normalized.windows.map((window) => ratePerSecond(window.matrixScans, normalized.windowUs));
    const pointingRates = normalized.windows.map((window) => ratePerSecond(window.pointingPolls, normalized.windowUs));
    const matrixScans = sum(normalized.windows.map((window) => window.matrixScans));
    const pointingPolls = sum(normalized.windows.map((window) => window.pointingPolls));
    const histogram = Array(RUNTIME_CADENCE_V1.HISTOGRAM_BUCKET_COUNT).fill(0);
    let worstGapUs = 0;
    normalized.windows.forEach((window) => {
        worstGapUs = Math.max(worstGapUs, window.maxGapUs);
        window.histogram.forEach((count, bucket) => {
            histogram[bucket] += count;
        });
    });
    const p999 = histogramUpperQuantile(histogram, normalized.histogramUpperBoundsUs, 0.999, worstGapUs);
    return {
        windowCount: normalized.windows.length,
        durationUs: normalized.windows.length * normalized.windowUs,
        matrixScans,
        pointingPolls,
        medianMatrixScansPerSecond: quantile(matrixRates, 0.5),
        medianPointingPollsPerSecond: quantile(pointingRates, 0.5),
        fifthPercentileMatrixScansPerSecond: quantile(matrixRates, 0.05),
        fifthPercentilePointingPollsPerSecond: quantile(pointingRates, 0.05),
        pointingToScanRatio: matrixScans === 0 ? null : pointingPolls / matrixScans,
        gapHistogram: histogram,
        gapCount: sum(histogram),
        p999GapUpperBoundUs: p999?.upperBoundUs ?? null,
        p999GapBucket: p999?.bucket ?? null,
        worstGapUs,
    };
}

function compareRuntimeCadenceCaptures(baselineCapture, liveCapture) {
    const baseline = normalizeRuntimeCadenceCapture(baselineCapture, {requireComplete: true});
    const live = normalizeRuntimeCadenceCapture(liveCapture, {requireComplete: true});
    if (!arraysEqual(baseline.histogramUpperBoundsUs, live.histogramUpperBoundsUs)) {
        throw new TypeError("Baseline and live captures use different histogram boundaries.");
    }
    const baselineSummary = summarizeRuntimeCadence(baseline);
    const liveSummary = summarizeRuntimeCadence(live);
    const checks = [
        minimumRatioCheck("medianMatrixScansPerSecond", liveSummary.medianMatrixScansPerSecond, baselineSummary.medianMatrixScansPerSecond, RUNTIME_CADENCE_THRESHOLDS.MEDIAN_RATE_RATIO),
        minimumRatioCheck("medianPointingPollsPerSecond", liveSummary.medianPointingPollsPerSecond, baselineSummary.medianPointingPollsPerSecond, RUNTIME_CADENCE_THRESHOLDS.MEDIAN_RATE_RATIO),
        minimumRatioCheck("fifthPercentileMatrixScansPerSecond", liveSummary.fifthPercentileMatrixScansPerSecond, baselineSummary.fifthPercentileMatrixScansPerSecond, RUNTIME_CADENCE_THRESHOLDS.FIFTH_PERCENTILE_RATE_RATIO),
        minimumRatioCheck("fifthPercentilePointingPollsPerSecond", liveSummary.fifthPercentilePointingPollsPerSecond, baselineSummary.fifthPercentilePointingPollsPerSecond, RUNTIME_CADENCE_THRESHOLDS.FIFTH_PERCENTILE_RATE_RATIO),
        minimumCheck("pointingToScanRatio", liveSummary.pointingToScanRatio, RUNTIME_CADENCE_THRESHOLDS.POINTING_TO_SCAN_RATIO),
        maximumCheck(
            "p999GapUpperBoundUs",
            liveSummary.p999GapUpperBoundUs,
            baselineSummary.p999GapUpperBoundUs === null
                ? null
                : Math.max(
                    baselineSummary.p999GapUpperBoundUs * RUNTIME_CADENCE_THRESHOLDS.P999_GAP_RATIO,
                    baselineSummary.p999GapUpperBoundUs + RUNTIME_CADENCE_THRESHOLDS.P999_GAP_ALLOWANCE_US
                )
        ),
        maximumCheck(
            "worstGapUs",
            liveSummary.worstGapUs,
            baselineSummary.worstGapUs + RUNTIME_CADENCE_THRESHOLDS.WORST_GAP_ALLOWANCE_US
        ),
    ];
    return {
        passed: checks.every((check) => check.passed),
        thresholds: {...RUNTIME_CADENCE_THRESHOLDS},
        baseline: baselineSummary,
        live: liveSummary,
        checks,
    };
}

function normalizeRuntimeCadenceCapture(value, options = {}) {
    const capture = value?.capture && value.format !== RUNTIME_CADENCE_V1.FORMAT ? value.capture : value;
    if (!capture || typeof capture !== "object" || Array.isArray(capture)) {
        throw new TypeError("Runtime cadence capture must be an object.");
    }
    if (capture.format !== RUNTIME_CADENCE_V1.FORMAT || capture.layoutVersion !== RUNTIME_CADENCE_V1.LAYOUT_VERSION) {
        throw new TypeError("Runtime cadence capture has an unsupported format or layout version.");
    }
    const pageCount = integerInRange(capture.pageCount, 1, 0xff, "pageCount");
    if (pageCount !== RUNTIME_CADENCE_V1.PAGE_COUNT) {
        throw new TypeError(`Runtime cadence capture must declare ${RUNTIME_CADENCE_V1.PAGE_COUNT} pages.`);
    }
    const completedCount = integerInRange(capture.completedCount, 0, RUNTIME_CADENCE_V1.WINDOW_COUNT, "completedCount");
    const windows = Array.isArray(capture.windows) ? capture.windows.map(normalizeWindow) : undefined;
    if (!windows || windows.length !== completedCount) {
        throw new TypeError("Runtime cadence windows must match completedCount.");
    }
    windows.forEach((window, index) => {
        if (window.index !== index) {
            throw new TypeError("Runtime cadence window indexes must be unique and contiguous.");
        }
    });
    if (options.requireComplete && completedCount !== RUNTIME_CADENCE_V1.WINDOW_COUNT) {
        throw new TypeError(`Cadence comparison requires ${RUNTIME_CADENCE_V1.WINDOW_COUNT} completed windows.`);
    }
    const bounds = normalizeBounds(capture.histogramUpperBoundsUs);
    if (!arraysEqual(bounds, RUNTIME_CADENCE_V1.HISTOGRAM_UPPER_BOUNDS_US)) {
        throw new TypeError("Runtime cadence capture uses unsupported histogram boundaries.");
    }
    return {
        format: RUNTIME_CADENCE_V1.FORMAT,
        layoutVersion: RUNTIME_CADENCE_V1.LAYOUT_VERSION,
        pageCount,
        completedCount,
        sequence: integerInRange(capture.sequence, 0, 0xffffffff, "sequence"),
        windowUs: integerInRange(capture.windowUs, 1, 0xffffffff, "windowUs"),
        histogramUpperBoundsUs: bounds,
        started: strictBoolean(capture.started, "started"),
        windows,
    };
}

function normalizeWindow(window) {
    if (!window || typeof window !== "object" || Array.isArray(window)) {
        throw new TypeError("Runtime cadence window must be an object.");
    }
    const histogram = Array.isArray(window.histogram)
        ? window.histogram.map((count, index) => integerInRange(count, 0, 0xffff, `histogram[${index}]`))
        : undefined;
    if (!histogram || histogram.length !== RUNTIME_CADENCE_V1.HISTOGRAM_BUCKET_COUNT) {
        throw new TypeError(`Runtime cadence window requires ${RUNTIME_CADENCE_V1.HISTOGRAM_BUCKET_COUNT} histogram buckets.`);
    }
    const pointingPolls = integerInRange(window.pointingPolls, 0, 0xffff, "pointingPolls");
    if (sum(histogram) > pointingPolls) {
        throw new TypeError("Runtime cadence histogram contains more gaps than pointing polls.");
    }
    return {
        index: integerInRange(window.index, 0, RUNTIME_CADENCE_V1.WINDOW_COUNT - 1, "index"),
        maxGapUs: integerInRange(window.maxGapUs, 0, 0xffffffff, "maxGapUs"),
        matrixScans: integerInRange(window.matrixScans, 0, 0xffff, "matrixScans"),
        pointingPolls,
        histogram,
    };
}

function histogramUpperQuantile(histogram, upperBounds, percentile, worstGapUs) {
    const count = sum(histogram);
    if (count === 0) {
        return null;
    }
    const rank = Math.ceil(count * percentile);
    let cumulative = 0;
    for (let bucket = 0; bucket < histogram.length; bucket += 1) {
        cumulative += histogram[bucket];
        if (cumulative >= rank) {
            return {
                bucket,
                upperBoundUs: bucket < upperBounds.length ? upperBounds[bucket] : worstGapUs,
            };
        }
    }
    throw new Error("Histogram quantile could not be resolved.");
}

function quantile(values, percentile) {
    if (!Array.isArray(values) || values.length === 0) {
        return null;
    }
    const sorted = values.slice().sort((left, right) => left - right);
    const position = (sorted.length - 1) * percentile;
    const lower = Math.floor(position);
    const upper = Math.ceil(position);
    if (lower === upper) {
        return sorted[lower];
    }
    return sorted[lower] + (sorted[upper] - sorted[lower]) * (position - lower);
}

function minimumRatioCheck(metric, actual, baseline, ratio) {
    return minimumCheck(metric, actual, baseline === null ? null : baseline * ratio, {baseline, ratio});
}

function minimumCheck(metric, actual, threshold, details = {}) {
    return {metric, direction: "minimum", actual, threshold, passed: actual !== null && threshold !== null && actual >= threshold, ...details};
}

function maximumCheck(metric, actual, threshold, details = {}) {
    return {metric, direction: "maximum", actual, threshold, passed: actual !== null && threshold !== null && actual <= threshold, ...details};
}

function createRequestIdSource(options) {
    if (options.nextRequestId !== undefined) {
        if (typeof options.nextRequestId !== "function") {
            throw new TypeError("nextRequestId must be a function.");
        }
        return {next: options.nextRequestId};
    }
    let requestId = options.requestId === undefined ? 1 : integerInRange(options.requestId, 1, 0xff, "requestId");
    return {
        next() {
            const current = requestId;
            requestId = requestId === 0xff ? 1 : requestId + 1;
            return current;
        },
    };
}

function normalizeRetryCount(value) {
    return value === undefined
        ? RUNTIME_CADENCE_V1.DEFAULT_SEQUENCE_RETRIES
        : integerInRange(value, 0, 20, "sequenceRetries");
}

function normalizePayload(value, label) {
    if (!(value instanceof Uint8Array) || value.byteLength !== PROFILE_WIRE_V1.PAYLOAD_SIZE) {
        throw protocolError("MALFORMED_RESPONSE", `${label} must contain exactly ${PROFILE_WIRE_V1.PAYLOAD_SIZE} bytes.`);
    }
    return Buffer.from(value.buffer, value.byteOffset, value.byteLength);
}

function normalizeBounds(bounds) {
    if (!Array.isArray(bounds) || bounds.length !== RUNTIME_CADENCE_V1.HISTOGRAM_UPPER_BOUNDS_US.length) {
        throw new TypeError("Runtime cadence capture has an invalid histogram boundary list.");
    }
    return bounds.map((bound, index) => integerInRange(bound, 1, 0xffff, `histogramUpperBoundsUs[${index}]`));
}

function integerInRange(value, minimum, maximum, label) {
    if (!Number.isSafeInteger(value) || value < minimum || value > maximum) {
        throw new TypeError(`${label} must be an integer from ${minimum} through ${maximum}.`);
    }
    return value;
}

function strictBoolean(value, label) {
    if (typeof value !== "boolean") {
        throw new TypeError(`${label} must be a boolean.`);
    }
    return value;
}

function decodeBoolean(value, label) {
    if (value !== 0 && value !== 1) {
        throw protocolError("MALFORMED_RESPONSE", `${label} must be encoded as 0 or 1.`);
    }
    return value === 1;
}

function assertZeroRange(buffer, start, end, label) {
    for (let index = start; index < end; index += 1) {
        if (buffer[index] !== 0) {
            throw protocolError("NONCANONICAL_RESPONSE", `${label} has nonzero reserved bytes.`);
        }
    }
}

function assertConnection(connection) {
    if (!connection || typeof connection.request !== "function") {
        throw new TypeError("connection must provide request(report, options).");
    }
}

function protocolError(code, message, details = {}) {
    return new ProfileWireProtocolError(code, message, details);
}

function arraysEqual(left, right) {
    return left.length === right.length && left.every((value, index) => value === right[index]);
}

function ratePerSecond(count, windowUs) {
    return count * 1000000 / windowUs;
}

function sum(values) {
    return values.reduce((total, value) => total + value, 0);
}

module.exports = {
    RUNTIME_CADENCE_THRESHOLDS,
    RUNTIME_CADENCE_V1,
    compareRuntimeCadenceCaptures,
    decodeRuntimeCadenceMetadata,
    decodeRuntimeCadencePages,
    decodeRuntimeCadenceWindow,
    histogramUpperQuantile,
    normalizeRuntimeCadenceCapture,
    quantile,
    readRuntimeCadenceCapture,
    readRuntimeCadenceMetadata,
    summarizeRuntimeCadence,
};
