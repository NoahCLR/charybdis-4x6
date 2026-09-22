"use strict";

const assert = require("node:assert/strict");
const test = require("node:test");

const {RAW_HID_REPORT_SIZE} = require("../../core/transport/device-adapter");
const {PROFILE_WIRE_STATUS, PROFILE_WIRE_V1} = require("../../core/protocol/profile-wire-v1");
const {
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
} = require("../../core/protocol/runtime-cadence-v1");

// The encoder below is an independent reference implementation of the firmware
// page layout in users/noah/lib/state/diagnostics/runtime_diag.c. Keeping it
// separate from the decoder is what makes these tests a contract check rather
// than a restatement of the production code.
function encodeMetadataPage(state) {
    const page = Buffer.alloc(PROFILE_WIRE_V1.PAYLOAD_SIZE);
    page[0] = state.layoutVersion ?? RUNTIME_CADENCE_V1.LAYOUT_VERSION;
    page[1] = state.pageCount ?? RUNTIME_CADENCE_V1.PAGE_COUNT;
    page[2] = state.windows.length;
    page.writeUInt32LE(state.sequence, 3);
    page.writeUInt32LE(state.windowUs ?? 1000000, 7);
    const bounds = state.histogramUpperBoundsUs ?? RUNTIME_CADENCE_V1.HISTOGRAM_UPPER_BOUNDS_US;
    bounds.forEach((bound, index) => page.writeUInt16LE(bound, 11 + index * 2));
    page[21] = (state.started ?? true) ? 1 : 0;
    return page;
}

function encodeWindowPage(state, pageIndex) {
    const page = Buffer.alloc(PROFILE_WIRE_V1.PAYLOAD_SIZE);
    page.writeUInt32LE(state.sequence, 0);
    const window = state.windows[pageIndex - 1];
    if (!window) {
        page[4] = RUNTIME_CADENCE_V1.UNUSED_LOGICAL_INDEX;
        return page;
    }
    page[4] = window.index ?? pageIndex - 1;
    page.writeUInt32LE(window.maxGapUs, 5);
    page.writeUInt16LE(window.matrixScans, 9);
    page.writeUInt16LE(window.pointingPolls, 11);
    window.histogram.forEach((count, bucket) => page.writeUInt16LE(count, 13 + bucket * 2));
    return page;
}

function encodePages(state) {
    const pages = [encodeMetadataPage(state)];
    for (let page = 1; page < RUNTIME_CADENCE_V1.PAGE_COUNT; page += 1) {
        pages.push(encodeWindowPage(state, page));
    }
    return pages;
}

function okResponse(request, payload) {
    const report = Buffer.alloc(RAW_HID_REPORT_SIZE);
    request.copy(report, 0, 0, 5);
    report[5] = PROFILE_WIRE_STATUS.OK;
    report[6] = PROFILE_WIRE_V1.PAYLOAD_SIZE;
    payload.copy(report, PROFILE_WIRE_V1.PAYLOAD_OFFSET);
    return report;
}

// A fake device that serves pages from `states`. Each entry is the snapshot
// visible for one page read, so a test can make the recorder roll mid-read.
function fakeDevice(stateForPage) {
    const requests = [];
    return {
        requests,
        request(report) {
            const request = Buffer.from(report);
            assert.equal(request[0], PROFILE_WIRE_V1.COMMAND_GET);
            assert.equal(request[1], PROFILE_WIRE_V1.CHANNEL);
            assert.equal(request[2], RUNTIME_CADENCE_V1.VALUE);
            assert.notEqual(request[3], 0);
            const page = request[4];
            assert.ok(page < RUNTIME_CADENCE_V1.PAGE_COUNT);
            requests.push({page, requestId: request[3]});
            const state = stateForPage(page, requests.length - 1);
            const payload = page === 0 ? encodeMetadataPage(state) : encodeWindowPage(state, page);
            return Promise.resolve(okResponse(request, payload));
        },
    };
}

function window(index, {matrixScans = 1000, pointingPolls = 1000, maxGapUs = 1100, histogram} = {}) {
    return {
        index,
        matrixScans,
        pointingPolls,
        maxGapUs,
        histogram: histogram ?? [pointingPolls, 0, 0, 0, 0, 0],
    };
}

function state({sequence = 7, windowCount = RUNTIME_CADENCE_V1.WINDOW_COUNT, ...rest} = {}) {
    return {
        sequence,
        windows: Array.from({length: windowCount}, (unused, index) => window(index)),
        ...rest,
    };
}

function capture(overrides = {}) {
    const source = state(overrides);
    return normalizeRuntimeCadenceCapture({
        format: RUNTIME_CADENCE_V1.FORMAT,
        layoutVersion: RUNTIME_CADENCE_V1.LAYOUT_VERSION,
        pageCount: RUNTIME_CADENCE_V1.PAGE_COUNT,
        completedCount: source.windows.length,
        sequence: source.sequence,
        windowUs: source.windowUs ?? 1000000,
        histogramUpperBoundsUs: [...RUNTIME_CADENCE_V1.HISTOGRAM_UPPER_BOUNDS_US],
        started: true,
        windows: source.windows,
    });
}

test("reads every cadence page and decodes one stable snapshot", async () => {
    const device = fakeDevice(() => state({sequence: 42}));
    const decoded = await readRuntimeCadenceCapture(device);

    assert.deepEqual(
        device.requests.map(({page}) => page),
        Array.from({length: RUNTIME_CADENCE_V1.PAGE_COUNT}, (unused, index) => index)
    );
    assert.equal(decoded.format, RUNTIME_CADENCE_V1.FORMAT);
    assert.equal(decoded.sequence, 42);
    assert.equal(decoded.completedCount, RUNTIME_CADENCE_V1.WINDOW_COUNT);
    assert.equal(decoded.windows.length, RUNTIME_CADENCE_V1.WINDOW_COUNT);
    assert.equal(decoded.windowUs, 1000000);
    assert.equal(decoded.started, true);
});

test("each page read carries a distinct nonzero request id", async () => {
    const device = fakeDevice(() => state());
    await readRuntimeCadenceCapture(device, {requestId: 250});

    const ids = device.requests.map(({requestId}) => requestId);
    assert.equal(new Set(ids).size, ids.length);
    assert.ok(ids.every((id) => id >= 1 && id <= 0xff));
    // The id source must wrap past 0xff without ever emitting the reserved 0.
    assert.deepEqual(ids.slice(0, 8), [250, 251, 252, 253, 254, 255, 1, 2]);
});

test("a partial recording decodes only its completed windows", async () => {
    const device = fakeDevice(() => state({windowCount: 4}));
    const decoded = await readRuntimeCadenceCapture(device);

    assert.equal(decoded.completedCount, 4);
    assert.deepEqual(decoded.windows.map(({index}) => index), [0, 1, 2, 3]);
});

test("a recorder that has not started yet decodes as an empty capture", async () => {
    const device = fakeDevice(() => state({windowCount: 0, started: false, sequence: 0}));
    const decoded = await readRuntimeCadenceCapture(device);

    assert.equal(decoded.started, false);
    assert.equal(decoded.completedCount, 0);
    assert.deepEqual(decoded.windows, []);
});

test("a snapshot that rolls mid-read is retried until it is stable", async () => {
    let served = 0;
    const device = fakeDevice((page) => {
        served += 1;
        // Roll the sequence once, part-way through the first attempt.
        return state({sequence: served <= 5 ? 3 : 4});
    });

    const decoded = await readRuntimeCadenceCapture(device);
    assert.equal(decoded.sequence, 4);
    // The first attempt aborted at the rolled page instead of reading all 31.
    assert.ok(device.requests.length < RUNTIME_CADENCE_V1.PAGE_COUNT * 2);
});

test("a permanently rolling snapshot fails with a bounded retry error", async () => {
    let served = 0;
    const device = fakeDevice(() => {
        served += 1;
        return state({sequence: served});
    });

    await assert.rejects(
        readRuntimeCadenceCapture(device, {sequenceRetries: 2}),
        (error) => {
            assert.equal(error.code, "SEQUENCE_UNSTABLE");
            assert.equal(error.attempts, 3);
            return true;
        }
    );
});

test("metadata reads reject firmware that advertises another layout", () => {
    for (const [overrides, code] of [
        [{layoutVersion: 2}, "INCOMPATIBLE_RESPONSE"],
        [{pageCount: 16}, "INCOMPATIBLE_RESPONSE"],
        [{histogramUpperBoundsUs: [900, 1250, 1500, 2000, 5000]}, "INCOMPATIBLE_RESPONSE"],
        [{windowUs: 0}, "MALFORMED_RESPONSE"],
    ]) {
        assert.throws(
            () => decodeRuntimeCadenceMetadata(encodeMetadataPage(state(overrides))),
            (error) => {
                assert.equal(error.code, code);
                return true;
            }
        );
    }
});

test("metadata reads reject a completed count beyond capacity", () => {
    const page = encodeMetadataPage(state({windowCount: 0}));
    page[2] = RUNTIME_CADENCE_V1.WINDOW_COUNT + 1;
    assert.throws(() => decodeRuntimeCadenceMetadata(page), (error) => {
        assert.equal(error.code, "MALFORMED_RESPONSE");
        return true;
    });
});

test("metadata reads reject noncanonical reserved bytes and started flags", () => {
    const reserved = encodeMetadataPage(state());
    reserved[24] = 1;
    assert.throws(() => decodeRuntimeCadenceMetadata(reserved), (error) => {
        assert.equal(error.code, "NONCANONICAL_RESPONSE");
        return true;
    });

    const started = encodeMetadataPage(state());
    started[21] = 2;
    assert.throws(() => decodeRuntimeCadenceMetadata(started), (error) => {
        assert.equal(error.code, "MALFORMED_RESPONSE");
        return true;
    });
});

test("window reads reject an out-of-range index and an impossible histogram", () => {
    const index = encodeWindowPage(state(), 1);
    index[4] = RUNTIME_CADENCE_V1.WINDOW_COUNT;
    assert.throws(() => decodeRuntimeCadenceWindow(index), (error) => {
        assert.equal(error.code, "MALFORMED_RESPONSE");
        return true;
    });

    const histogram = encodeWindowPage(
        {sequence: 1, windows: [window(0, {pointingPolls: 10, histogram: [11, 0, 0, 0, 0, 0]})]},
        1
    );
    assert.throws(() => decodeRuntimeCadenceWindow(histogram), (error) => {
        assert.equal(error.code, "MALFORMED_RESPONSE");
        return true;
    });
});

test("page sets whose usage disagrees with the completed count are refused", () => {
    const pages = encodePages(state({windowCount: 3}));
    // Claim a fourth completed window that no page actually carries.
    pages[0][2] = 4;
    assert.throws(() => decodeRuntimeCadencePages(pages), (error) => {
        assert.equal(error.code, "MALFORMED_RESPONSE");
        return true;
    });
});

test("page sets with duplicated logical windows are refused", () => {
    const pages = encodePages(state({windowCount: 3}));
    pages[2][4] = 0;
    assert.throws(() => decodeRuntimeCadencePages(pages), (error) => {
        assert.equal(error.code, "MALFORMED_RESPONSE");
        return true;
    });
});

test("page sets that mix two snapshots are refused", () => {
    const pages = encodePages(state({sequence: 9, windowCount: 3}));
    pages[2].writeUInt32LE(10, 0);
    assert.throws(() => decodeRuntimeCadencePages(pages), (error) => {
        assert.equal(error.code, "SEQUENCE_MISMATCH");
        assert.equal(error.expectedSequence, 9);
        assert.equal(error.actualSequence, 10);
        return true;
    });
});

test("an incomplete page set is refused before decoding", () => {
    assert.throws(() => decodeRuntimeCadencePages(encodePages(state()).slice(0, 5)), (error) => {
        assert.equal(error.code, "MALFORMED_RESPONSE");
        return true;
    });
});

test("metadata-only reads issue exactly one page request", async () => {
    const device = fakeDevice(() => state({sequence: 11}));
    const metadata = await readRuntimeCadenceMetadata(device);

    assert.deepEqual(device.requests.map(({page}) => page), [0]);
    assert.equal(metadata.sequence, 11);
    assert.equal(metadata.completedCount, RUNTIME_CADENCE_V1.WINDOW_COUNT);
});

test("reads require a connection that can issue requests", async () => {
    await assert.rejects(() => readRuntimeCadenceCapture({}), TypeError);
});

test("summaries report rates, ratios, and gap quantiles", () => {
    const summary = summarizeRuntimeCadence(capture({windowCount: 4}));

    assert.equal(summary.windowCount, 4);
    assert.equal(summary.durationUs, 4000000);
    assert.equal(summary.matrixScans, 4000);
    assert.equal(summary.pointingPolls, 4000);
    assert.equal(summary.medianMatrixScansPerSecond, 1000);
    assert.equal(summary.medianPointingPollsPerSecond, 1000);
    assert.equal(summary.fifthPercentilePointingPollsPerSecond, 1000);
    assert.equal(summary.pointingToScanRatio, 1);
    assert.equal(summary.gapCount, 4000);
    assert.equal(summary.p999GapUpperBoundUs, 1000);
    assert.equal(summary.worstGapUs, 1100);
});

test("summaries convert window counts using the firmware window duration", () => {
    const halfSecond = capture({windowCount: 2, windowUs: 500000});
    const summary = summarizeRuntimeCadence(halfSecond);

    assert.equal(summary.durationUs, 1000000);
    assert.equal(summary.medianMatrixScansPerSecond, 2000);
});

test("a comparison against an identical baseline passes every check", () => {
    const comparison = compareRuntimeCadenceCaptures(capture(), capture());

    assert.equal(comparison.passed, true);
    assert.ok(comparison.checks.length >= 7);
    assert.deepEqual(comparison.thresholds, {...RUNTIME_CADENCE_THRESHOLDS});
    assert.ok(comparison.checks.every((check) => check.passed));
});

test("a median pointing-rate regression fails its own named check", () => {
    const baseline = capture();
    const live = capture();
    live.windows.forEach((entry) => {
        entry.pointingPolls = 900;
        entry.histogram = [900, 0, 0, 0, 0, 0];
    });

    const comparison = compareRuntimeCadenceCaptures(baseline, live);
    assert.equal(comparison.passed, false);
    const failed = comparison.checks.filter((check) => !check.passed).map(({metric}) => metric);
    assert.ok(failed.includes("medianPointingPollsPerSecond"));
    assert.ok(failed.includes("pointingToScanRatio"));

    const median = comparison.checks.find(({metric}) => metric === "medianPointingPollsPerSecond");
    assert.equal(median.direction, "minimum");
    assert.equal(median.actual, 900);
    assert.equal(median.threshold, 1000 * RUNTIME_CADENCE_THRESHOLDS.MEDIAN_RATE_RATIO);
});

test("a rate drop confined to a few windows fails the fifth-percentile check", () => {
    const baseline = capture();
    const live = capture();
    // Median survives, but the worst windows stall well below the floor.
    live.windows.slice(0, 3).forEach((entry) => {
        entry.matrixScans = 600;
        entry.pointingPolls = 600;
        entry.histogram = [600, 0, 0, 0, 0, 0];
    });

    const comparison = compareRuntimeCadenceCaptures(baseline, live);
    assert.equal(comparison.passed, false);
    const failed = comparison.checks.filter((check) => !check.passed).map(({metric}) => metric);
    assert.ok(failed.includes("fifthPercentilePointingPollsPerSecond"));
    assert.ok(failed.includes("fifthPercentileMatrixScansPerSecond"));
    assert.ok(!failed.includes("medianPointingPollsPerSecond"));
});

test("a worst-case stall beyond the allowance fails the gap checks", () => {
    const baseline = capture();
    const live = capture();
    live.windows[0].maxGapUs = 1100 + RUNTIME_CADENCE_THRESHOLDS.WORST_GAP_ALLOWANCE_US + 1;
    live.windows.forEach((entry) => {
        entry.histogram = [entry.pointingPolls - 5, 0, 0, 0, 0, 5];
    });

    const comparison = compareRuntimeCadenceCaptures(baseline, live);
    assert.equal(comparison.passed, false);
    const failed = comparison.checks.filter((check) => !check.passed).map(({metric}) => metric);
    assert.ok(failed.includes("worstGapUs"));
    assert.ok(failed.includes("p999GapUpperBoundUs"));
});

test("comparisons refuse partial captures instead of reporting a verdict", () => {
    assert.throws(() => compareRuntimeCadenceCaptures(capture({windowCount: 29}), capture()), TypeError);
    assert.throws(() => compareRuntimeCadenceCaptures(capture(), capture({windowCount: 29})), TypeError);
});

test("comparisons refuse captures recorded with different histogram boundaries", () => {
    const live = capture();
    live.histogramUpperBoundsUs = [900, 1250, 1500, 2000, 5000];
    assert.throws(() => compareRuntimeCadenceCaptures(capture(), live), TypeError);
});

test("normalization accepts a saved report envelope and rejects malformed captures", () => {
    const saved = {format: "charybdis-runtime-cadence-report-v1", capture: capture({windowCount: 2})};
    assert.equal(normalizeRuntimeCadenceCapture(saved).completedCount, 2);

    assert.throws(() => normalizeRuntimeCadenceCapture(null), TypeError);
    assert.throws(() => normalizeRuntimeCadenceCapture({format: "other"}), TypeError);

    const gap = capture({windowCount: 2});
    gap.windows.pop();
    assert.throws(() => normalizeRuntimeCadenceCapture(gap), TypeError);

    const reordered = capture({windowCount: 2});
    reordered.windows.reverse();
    assert.throws(() => normalizeRuntimeCadenceCapture(reordered), TypeError);
});

test("gap quantiles fall back to the observed worst gap in the overflow bucket", () => {
    const bounds = RUNTIME_CADENCE_V1.HISTOGRAM_UPPER_BOUNDS_US;
    assert.equal(histogramUpperQuantile([0, 0, 0, 0, 0, 0], bounds, 0.999, 0), null);
    assert.deepEqual(histogramUpperQuantile([1000, 0, 0, 0, 0, 0], bounds, 0.999, 9000), {
        bucket: 0,
        upperBoundUs: 1000,
    });
    assert.deepEqual(histogramUpperQuantile([998, 0, 0, 0, 0, 2], bounds, 0.999, 9000), {
        bucket: 5,
        upperBoundUs: 9000,
    });
});

test("quantiles interpolate and tolerate empty inputs", () => {
    assert.equal(quantile([], 0.5), null);
    assert.equal(quantile([5], 0.5), 5);
    assert.equal(quantile([10, 20, 30, 40], 0.5), 25);
    assert.equal(quantile([10, 20, 30, 40], 0.05), 11.5);
});
