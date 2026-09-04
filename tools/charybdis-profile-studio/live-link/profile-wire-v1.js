"use strict";

const {RAW_HID_REPORT_SIZE, normalizeRawHidReport} = require("./device-adapter");

const PROFILE_WIRE_V1 = Object.freeze({
    CAPABILITY_PAGE_COUNT: 2,
    CHANNEL: 0x00,
    COMMAND_GET: 0x08,
    PAYLOAD_OFFSET: 7,
    PAYLOAD_SIZE: 25,
    STATUS_PAGE_COUNT: 2,
    VALUE_CAPABILITIES: 0x01,
    VALUE_STATUS: 0x02,
});

const PROFILE_WIRE_STATUS = Object.freeze({
    OK: 0,
    MALFORMED: 1,
    UNKNOWN_PAGE: 2,
    UNAVAILABLE: 3,
});

const PROFILE_WIRE_KNOWN_MASKS = Object.freeze({
    FEATURE_FLAGS: 0x00000fff,
    REQUIRED_READ_FEATURES: 0x0000000f,
    STATE_FLAGS: 0x00ff,
    SUPPORTED_DOMAINS: 0x03,
});

const PROFILE_WIRE_FEATURES = Object.freeze({
    READ_SURFACE: 1 << 0,
    STORAGE_LAYOUT: 1 << 1,
    RGB_SCHEMA: 1 << 2,
    KEY_BEHAVIOR_SCHEMA: 1 << 3,
    SPLIT_KEYBOARD: 1 << 4,
    CANDIDATE_WRITE: 1 << 5,
    PERSISTENT_COMMIT: 1 << 6,
    RGB_PREVIEW: 1 << 7,
    RUNTIME_ACTIVATION: 1 << 8,
    PEER_RECONCILIATION: 1 << 9,
    ACTION_ABI_DIGEST: 1 << 10,
    COMPILED_PROFILE_HASH: 1 << 11,
});

const PROFILE_WIRE_DOMAINS = Object.freeze({
    RGB: 1 << 0,
    KEY_BEHAVIORS: 1 << 1,
});

const PROFILE_ACTIVE_KIND = Object.freeze({
    COMPILED_ONLY: 0,
    COMMITTED: 1,
    PREVIEW: 2,
    PENDING: 3,
});

const PROFILE_STATE_FLAGS = Object.freeze({
    ACTIVE_IS_COMPILED_DEFAULT: 1 << 0,
    COMMITTED_VALID: 1 << 1,
    CANDIDATE_PENDING: 1 << 2,
    PREVIEW_ACTIVE: 1 << 3,
    PEER_KNOWN: 1 << 4,
    PEER_CONVERGED: 1 << 5,
    WAITING_SAFE_BOUNDARY: 1 << 6,
    DIGESTS_UNAVAILABLE: 1 << 7,
});

const VIA_READS = Object.freeze({
    COMMAND_GET_PROTOCOL_VERSION: 0x01,
    COMMAND_GET_KEYBOARD_VALUE: 0x02,
    VALUE_FIRMWARE_VERSION: 0x04,
    UNHANDLED: 0xff,
    EXPECTED_PROTOCOL_VERSION: 0x000c,
});

class ProfileWireProtocolError extends Error {
    constructor(code, message, details = {}) {
        super(message);
        this.name = "ProfileWireProtocolError";
        this.code = code;
        Object.assign(this, details);
    }
}

function assertByte(value, label, {nonzero = false} = {}) {
    if (!Number.isInteger(value) || value < (nonzero ? 1 : 0) || value > 0xff) {
        throw new RangeError(`${label} must be ${nonzero ? "a nonzero " : "an "}8-bit integer.`);
    }
    return value;
}

function buildProfileGetRequest(valueId, page, requestId) {
    const report = Buffer.alloc(RAW_HID_REPORT_SIZE);
    report[0] = PROFILE_WIRE_V1.COMMAND_GET;
    report[1] = PROFILE_WIRE_V1.CHANNEL;
    report[2] = assertByte(valueId, "valueId");
    report[3] = assertByte(requestId, "requestId", {nonzero: true});
    report[4] = assertByte(page, "page");
    return report;
}

function profileResponseMatcher(response, request) {
    const actual = normalizeRawHidReport(response, "Profile Wire response");
    const expected = normalizeRawHidReport(request, "Profile Wire request");
    return actual[0] === expected[0]
        && actual[1] === expected[1]
        && actual[2] === expected[2]
        && actual[3] === expected[3]
        && actual[4] === expected[4];
}

function decodeProfileResponse(response, request) {
    const report = Buffer.from(normalizeRawHidReport(response, "Profile Wire response"));
    if (!profileResponseMatcher(report, request)) {
        throw new ProfileWireProtocolError(
            "CORRELATION_MISMATCH",
            "Profile Wire response does not match the request correlation header."
        );
    }
    const status = report[5];
    const payloadLength = report[6];
    if (payloadLength > PROFILE_WIRE_V1.PAYLOAD_SIZE) {
        throw new ProfileWireProtocolError("MALFORMED_RESPONSE", "Profile Wire payload length exceeds one report.");
    }
    for (let index = PROFILE_WIRE_V1.PAYLOAD_OFFSET + payloadLength; index < RAW_HID_REPORT_SIZE; index += 1) {
        if (report[index] !== 0) {
            throw new ProfileWireProtocolError("NONCANONICAL_RESPONSE", "Profile Wire response has nonzero trailing bytes.");
        }
    }
    if (status !== PROFILE_WIRE_STATUS.OK) {
        throw new ProfileWireProtocolError("DEVICE_REJECTED", `Profile Wire request failed with status ${status}.`, {status});
    }
    if (payloadLength !== PROFILE_WIRE_V1.PAYLOAD_SIZE) {
        throw new ProfileWireProtocolError("MALFORMED_RESPONSE", "Successful Profile Wire read pages must contain 25 bytes.");
    }
    return report.subarray(PROFILE_WIRE_V1.PAYLOAD_OFFSET, PROFILE_WIRE_V1.PAYLOAD_OFFSET + payloadLength);
}

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

function decodeCapabilityPages(pages) {
    assertPageSet(pages, PROFILE_WIRE_V1.CAPABILITY_PAGE_COUNT, "capability");
    const identity = pages[0];
    const capacity = pages[1];
    if (identity[0] !== 1 || identity[1] !== PROFILE_WIRE_V1.CAPABILITY_PAGE_COUNT) {
        throw new ProfileWireProtocolError("INCOMPATIBLE_RESPONSE", "Unsupported Profile Wire capability layout.");
    }
    assertZeroRange(capacity, 22, 25, "capability page 1");
    const decoded = {
        responseVersion: identity[0],
        protocol: {major: identity[2], minor: identity[3]},
        schema: {major: identity[4], minor: identity[5]},
        reportSize: identity[6],
        candidateChunkMax: identity[7],
        statusPageCount: identity[8],
        featureFlags: readU32(identity, 9),
        actionAbiDigest: readU32(identity, 13),
        firmwareVersion: readU32(identity, 17),
        compiledDefaultDigest: readU32(identity, 21),
        compiledLayerCount: capacity[0],
        maxLogicalLayers: capacity[1],
        maxBehaviorRows: capacity[2],
        maxTapStepsPerBehavior: capacity[3],
        maxPopulatedBehaviorSteps: capacity[4],
        maxCombos: capacity[5],
        maxKeysPerCombo: capacity[6],
        maxReusableRgbGroups: capacity[7],
        maxRgbStageGroupRows: capacity[8],
        physicalLedCount: capacity[9],
        ledBitmapSize: capacity[10],
        hardcodedMacroSlots: capacity[11],
        viaMacroSlots: capacity[12],
        maxProfilePayload: readU16(capacity, 13),
        profileSlotPayload: readU16(capacity, 15),
        profileSlotSize: readU16(capacity, 17),
        viaMacroBytes: readU16(capacity, 19),
        supportedDomainMask: capacity[21],
    };
    assertKnownMask(decoded.featureFlags, PROFILE_WIRE_KNOWN_MASKS.FEATURE_FLAGS, "capability feature flags");
    assertKnownMask(decoded.supportedDomainMask, PROFILE_WIRE_KNOWN_MASKS.SUPPORTED_DOMAINS, "supported domain mask");
    if (decoded.reportSize !== RAW_HID_REPORT_SIZE) {
        throw new ProfileWireProtocolError("INCOMPATIBLE_RESPONSE", `Firmware advertises ${decoded.reportSize}-byte reports; ${RAW_HID_REPORT_SIZE} are required.`);
    }
    if (decoded.statusPageCount !== PROFILE_WIRE_V1.STATUS_PAGE_COUNT) {
        throw new ProfileWireProtocolError("INCOMPATIBLE_RESPONSE", `Firmware advertises ${decoded.statusPageCount} status pages; ${PROFILE_WIRE_V1.STATUS_PAGE_COUNT} are required.`);
    }
    if (decoded.candidateChunkMax > 20) {
        throw new ProfileWireProtocolError("MALFORMED_RESPONSE", "Candidate chunk capacity exceeds the 32-byte Profile Wire frame.");
    }
    const hasFeature = (feature) => (decoded.featureFlags & feature) !== 0;
    const hasDomain = (domain) => (decoded.supportedDomainMask & domain) !== 0;
    if (hasFeature(PROFILE_WIRE_FEATURES.CANDIDATE_WRITE) !== (decoded.candidateChunkMax > 0)) {
        throw new ProfileWireProtocolError("MALFORMED_RESPONSE", "Candidate-write support and candidate chunk capacity disagree.");
    }
    if (hasDomain(PROFILE_WIRE_DOMAINS.RGB) !== hasFeature(PROFILE_WIRE_FEATURES.RGB_SCHEMA)
        || hasDomain(PROFILE_WIRE_DOMAINS.KEY_BEHAVIORS) !== hasFeature(PROFILE_WIRE_FEATURES.KEY_BEHAVIOR_SCHEMA)) {
        throw new ProfileWireProtocolError("MALFORMED_RESPONSE", "Supported profile domains and schema feature flags disagree.");
    }
    if (hasFeature(PROFILE_WIRE_FEATURES.PERSISTENT_COMMIT) && !hasFeature(PROFILE_WIRE_FEATURES.CANDIDATE_WRITE)) {
        throw new ProfileWireProtocolError("MALFORMED_RESPONSE", "Persistent commit requires candidate-write support.");
    }
    if (hasFeature(PROFILE_WIRE_FEATURES.RGB_PREVIEW)
        && (!hasFeature(PROFILE_WIRE_FEATURES.CANDIDATE_WRITE) || !hasDomain(PROFILE_WIRE_DOMAINS.RGB))) {
        throw new ProfileWireProtocolError("MALFORMED_RESPONSE", "RGB preview requires candidate-write support and the RGB domain.");
    }
    if (hasFeature(PROFILE_WIRE_FEATURES.RUNTIME_ACTIVATION) && !hasFeature(PROFILE_WIRE_FEATURES.CANDIDATE_WRITE)) {
        throw new ProfileWireProtocolError("MALFORMED_RESPONSE", "Runtime activation requires candidate-write support.");
    }
    if (hasFeature(PROFILE_WIRE_FEATURES.PEER_RECONCILIATION)
        && (!hasFeature(PROFILE_WIRE_FEATURES.SPLIT_KEYBOARD) || !hasFeature(PROFILE_WIRE_FEATURES.PERSISTENT_COMMIT))) {
        throw new ProfileWireProtocolError("MALFORMED_RESPONSE", "Peer reconciliation requires split-keyboard and persistent-commit support.");
    }
    if (decoded.compiledLayerCount > decoded.maxLogicalLayers) {
        throw new ProfileWireProtocolError("MALFORMED_RESPONSE", "Compiled layer count exceeds the advertised logical-layer capacity.");
    }
    if (decoded.physicalLedCount > decoded.ledBitmapSize * 8) {
        throw new ProfileWireProtocolError("MALFORMED_RESPONSE", "LED bitmap capacity is smaller than the advertised physical LED count.");
    }
    if (decoded.maxProfilePayload > decoded.profileSlotPayload || decoded.profileSlotPayload >= decoded.profileSlotSize) {
        throw new ProfileWireProtocolError("MALFORMED_RESPONSE", "Profile payload and slot capacities are internally inconsistent.");
    }
    return decoded;
}

function decodeStatusPages(pages) {
    assertPageSet(pages, PROFILE_WIRE_V1.STATUS_PAGE_COUNT, "status");
    const identity = pages[0];
    const generation = pages[1];
    if (identity[0] !== 1 || identity[1] !== PROFILE_WIRE_V1.STATUS_PAGE_COUNT) {
        throw new ProfileWireProtocolError("INCOMPATIBLE_RESPONSE", "Unsupported Profile Wire status layout.");
    }
    assertZeroRange(generation, 23, 25, "status page 1");
    const decoded = {
        responseVersion: identity[0],
        stateFlags: readU16(identity, 2),
        sourceDigest: readU32(identity, 4),
        compiledDefaultDigest: readU32(identity, 8),
        activeDigest: readU32(identity, 12),
        pendingDigest: readU32(identity, 16),
        committedDigest: readU32(identity, 20),
        activeKind: identity[24],
        activeGeneration: readU32(generation, 0),
        activeOriginHalf: generation[4],
        committedGeneration: readU32(generation, 5),
        committedOriginHalf: generation[9],
        peerGeneration: readU32(generation, 10),
        peerOriginHalf: generation[14],
        candidateTransactionId: readU16(generation, 15),
        lastCommittedTransactionId: readU16(generation, 17),
        conflictCount: readU16(generation, 19),
        validationState: generation[21],
        lastError: generation[22],
    };
    decoded.candidatePending = (decoded.stateFlags & PROFILE_STATE_FLAGS.CANDIDATE_PENDING) !== 0;
    decoded.peerKnown = (decoded.stateFlags & PROFILE_STATE_FLAGS.PEER_KNOWN) !== 0;
    decoded.peerConverged = (decoded.stateFlags & PROFILE_STATE_FLAGS.PEER_CONVERGED) !== 0;
    decoded.waitingSafeBoundary = (decoded.stateFlags & PROFILE_STATE_FLAGS.WAITING_SAFE_BOUNDARY) !== 0;
    assertKnownMask(decoded.stateFlags, PROFILE_WIRE_KNOWN_MASKS.STATE_FLAGS, "status state flags");
    if (!Object.values(PROFILE_ACTIVE_KIND).includes(decoded.activeKind)) {
        throw new ProfileWireProtocolError("INCOMPATIBLE_RESPONSE", `Unknown active profile kind ${decoded.activeKind}.`);
    }
    for (const [label, value] of [
        ["active origin half", decoded.activeOriginHalf],
        ["committed origin half", decoded.committedOriginHalf],
        ["peer origin half", decoded.peerOriginHalf],
    ]) {
        if (value > 1) {
            throw new ProfileWireProtocolError("MALFORMED_RESPONSE", `${label} must be 0 or 1.`);
        }
    }
    return decoded;
}

async function readProfilePages(connection, valueId, pageCount, options = {}) {
    if (!connection || typeof connection.request !== "function") {
        throw new TypeError("connection must provide request(report, options).");
    }
    let requestId = options.requestId === undefined ? 1 : assertByte(options.requestId, "requestId", {nonzero: true});
    const nextRequestId = options.nextRequestId;
    if (nextRequestId !== undefined && typeof nextRequestId !== "function") {
        throw new TypeError("nextRequestId must be a function.");
    }
    const pages = [];
    for (let page = 0; page < pageCount; page += 1) {
        const currentRequestId = nextRequestId
            ? assertByte(nextRequestId(), "requestId", {nonzero: true})
            : requestId;
        const request = buildProfileGetRequest(valueId, page, currentRequestId);
        const response = await connection.request(request, {
            matchResponse: profileResponseMatcher,
            signal: options.signal,
            timeoutMs: options.timeoutMs,
        });
        pages.push(Buffer.from(decodeProfileResponse(response, request)));
        requestId = currentRequestId === 0xff ? 1 : currentRequestId + 1;
    }
    return pages;
}

function buildViaProtocolVersionRequest() {
    const report = Buffer.alloc(RAW_HID_REPORT_SIZE);
    report[0] = VIA_READS.COMMAND_GET_PROTOCOL_VERSION;
    return report;
}

function buildViaFirmwareVersionRequest() {
    const report = Buffer.alloc(RAW_HID_REPORT_SIZE);
    report[0] = VIA_READS.COMMAND_GET_KEYBOARD_VALUE;
    report[1] = VIA_READS.VALUE_FIRMWARE_VERSION;
    return report;
}

function viaReadResponseMatcher(response, request) {
    const actual = normalizeRawHidReport(response, "VIA read response");
    const expected = normalizeRawHidReport(request, "VIA read request");
    return actual[0] === expected[0] || actual[0] === VIA_READS.UNHANDLED;
}

function decodeViaProtocolVersion(response) {
    const report = Buffer.from(normalizeRawHidReport(response, "VIA protocol-version response"));
    assertViaHandled(report, VIA_READS.COMMAND_GET_PROTOCOL_VERSION);
    assertZeroRange(report, 3, RAW_HID_REPORT_SIZE, "VIA protocol-version response");
    return (report[1] << 8) | report[2];
}

function decodeViaFirmwareVersion(response) {
    const report = Buffer.from(normalizeRawHidReport(response, "VIA firmware-version response"));
    assertViaHandled(report, VIA_READS.COMMAND_GET_KEYBOARD_VALUE);
    if (report[1] !== VIA_READS.VALUE_FIRMWARE_VERSION) {
        throw new ProfileWireProtocolError("CORRELATION_MISMATCH", "VIA firmware-version response has the wrong keyboard-value id.");
    }
    assertZeroRange(report, 6, RAW_HID_REPORT_SIZE, "VIA firmware-version response");
    return ((report[2] << 24) | (report[3] << 16) | (report[4] << 8) | report[5]) >>> 0;
}

async function readViaIdentity(connection, options = {}) {
    if (!connection || typeof connection.request !== "function") {
        throw new TypeError("connection must provide request(report, options).");
    }
    const protocolRequest = buildViaProtocolVersionRequest();
    const protocolResponse = await connection.request(protocolRequest, {
        matchResponse: viaReadResponseMatcher,
        signal: options.signal,
        timeoutMs: options.timeoutMs,
    });
    const firmwareRequest = buildViaFirmwareVersionRequest();
    const firmwareResponse = await connection.request(firmwareRequest, {
        matchResponse: viaReadResponseMatcher,
        signal: options.signal,
        timeoutMs: options.timeoutMs,
    });
    return {
        protocolVersion: decodeViaProtocolVersion(protocolResponse),
        firmwareVersion: decodeViaFirmwareVersion(firmwareResponse),
    };
}

async function readProfileCapabilities(connection, options) {
    return decodeCapabilityPages(await readProfilePages(connection, PROFILE_WIRE_V1.VALUE_CAPABILITIES, PROFILE_WIRE_V1.CAPABILITY_PAGE_COUNT, options));
}

async function readProfileStatus(connection, options) {
    return decodeStatusPages(await readProfilePages(connection, PROFILE_WIRE_V1.VALUE_STATUS, PROFILE_WIRE_V1.STATUS_PAGE_COUNT, options));
}

function assertPageSet(pages, count, label) {
    if (!Array.isArray(pages) || pages.length !== count || pages.some((page) => !(page instanceof Uint8Array) || page.byteLength !== PROFILE_WIRE_V1.PAYLOAD_SIZE)) {
        throw new ProfileWireProtocolError("MALFORMED_RESPONSE", `Profile Wire ${label} response requires ${count} complete pages.`);
    }
}

function assertZeroRange(buffer, start, end, label) {
    for (let index = start; index < end; index += 1) {
        if (buffer[index] !== 0) {
            throw new ProfileWireProtocolError("NONCANONICAL_RESPONSE", `${label} has nonzero reserved bytes.`);
        }
    }
}

function assertKnownMask(value, knownMask, label) {
    const unknown = (Number(value) >>> 0) & (~knownMask >>> 0);
    if (unknown !== 0) {
        throw new ProfileWireProtocolError("INCOMPATIBLE_RESPONSE", `${label} contains unknown bits 0x${unknown.toString(16)}.`);
    }
}

function assertViaHandled(report, command) {
    if (report[0] === VIA_READS.UNHANDLED) {
        throw new ProfileWireProtocolError("DEVICE_REJECTED", "Firmware rejected a standard VIA identity read.");
    }
    if (report[0] !== command) {
        throw new ProfileWireProtocolError("CORRELATION_MISMATCH", "VIA identity response has the wrong command id.");
    }
}

module.exports = {
    PROFILE_WIRE_STATUS,
    PROFILE_ACTIVE_KIND,
    PROFILE_STATE_FLAGS,
    PROFILE_WIRE_KNOWN_MASKS,
    PROFILE_WIRE_FEATURES,
    PROFILE_WIRE_DOMAINS,
    PROFILE_WIRE_V1,
    ProfileWireProtocolError,
    VIA_READS,
    buildProfileGetRequest,
    buildViaFirmwareVersionRequest,
    buildViaProtocolVersionRequest,
    decodeCapabilityPages,
    decodeProfileResponse,
    decodeStatusPages,
    decodeViaFirmwareVersion,
    decodeViaProtocolVersion,
    profileResponseMatcher,
    readProfileCapabilities,
    readProfilePages,
    readProfileStatus,
    readViaIdentity,
    viaReadResponseMatcher,
};
