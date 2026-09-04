"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const {
    PROFILE_WIRE_KNOWN_MASKS,
    PROFILE_WIRE_FEATURES,
    PROFILE_WIRE_V1,
    VIA_READS,
    buildViaFirmwareVersionRequest,
    buildViaProtocolVersionRequest,
    buildProfileGetRequest,
    decodeCapabilityPages,
    decodeProfileResponse,
    decodeStatusPages,
    decodeViaFirmwareVersion,
    decodeViaProtocolVersion,
    profileResponseMatcher,
    readProfileCapabilities,
    readViaIdentity,
} = require("../../live-link/profile-wire-v1");

function goldenFixtures() {
    const fixturePath = path.resolve(__dirname, "../../../../tests/fixtures/profile_wire_v1_reads.fixture");
    const entries = {};
    for (const line of fs.readFileSync(fixturePath, "utf8").split(/\r?\n/)) {
        if (!line || line.startsWith("#")) continue;
        const [name, hex] = line.split("=");
        entries[name] = Buffer.from(hex, "hex");
    }
    return entries;
}

function response(request, payload, status = 0) {
    const report = Buffer.from(request);
    report.fill(0, 5);
    report[5] = status;
    report[6] = payload.length;
    Buffer.from(payload).copy(report, 7);
    return report;
}

function compiledOnlyPages() {
    const identity = Buffer.alloc(PROFILE_WIRE_V1.PAYLOAD_SIZE);
    identity.set([1, 2, 1, 0, 1, 0, 32, 20, 2, 0x3f], 0);
    const capacity = Buffer.from([
        5, 8, 64, 5, 128, 32, 4, 16, 32, 58, 8, 16, 64,
        0xe0, 0x0f, 0xe0, 0x0f, 0x00, 0x10, 0x7f, 0x1d, 3, 0, 0, 0,
    ]);
    return [identity, capacity];
}

test("Profile Wire get requests are canonical 32-byte frames", () => {
    const request = buildProfileGetRequest(PROFILE_WIRE_V1.VALUE_CAPABILITIES, 1, 0x42);
    assert.equal(request.length, 32);
    assert.deepEqual(Array.from(request.subarray(0, 5)), [0x08, 0, 1, 0x42, 1]);
    assert.equal(request.subarray(5).every((byte) => byte === 0), true);
    assert.throws(() => buildProfileGetRequest(1, 0, 0), /nonzero/);
});

test("JavaScript consumes the same golden reports as the C firmware codec", () => {
    const fixtures = goldenFixtures();
    const pages = {};
    for (const prefix of ["capabilities-page-0", "capabilities-page-1", "status-page-0", "status-page-1"]) {
        const request = fixtures[`${prefix}-request`];
        const actualRequest = buildProfileGetRequest(request[2], request[4], request[3]);
        assert.deepEqual(actualRequest, request);
        pages[prefix] = decodeProfileResponse(fixtures[`${prefix}-response`], request);
    }
    assert.deepEqual(decodeCapabilityPages([pages["capabilities-page-0"], pages["capabilities-page-1"]]), {
        responseVersion: 1,
        protocol: {major: 1, minor: 0},
        schema: {major: 1, minor: 0},
        reportSize: 32,
        candidateChunkMax: 0,
        statusPageCount: 2,
        featureFlags: 0x00000c1f,
        actionAbiDigest: 0x12345678,
        firmwareVersion: 0x00010000,
        compiledDefaultDigest: 0x89abcdef,
        compiledLayerCount: 5,
        maxLogicalLayers: 8,
        maxBehaviorRows: 64,
        maxTapStepsPerBehavior: 5,
        maxPopulatedBehaviorSteps: 128,
        maxCombos: 32,
        maxKeysPerCombo: 4,
        maxReusableRgbGroups: 16,
        maxRgbStageGroupRows: 32,
        physicalLedCount: 58,
        ledBitmapSize: 8,
        hardcodedMacroSlots: 16,
        viaMacroSlots: 64,
        maxProfilePayload: 4064,
        profileSlotPayload: 4064,
        profileSlotSize: 4096,
        viaMacroBytes: 7551,
        supportedDomainMask: 3,
    });
    assert.deepEqual(decodeStatusPages([pages["status-page-0"], pages["status-page-1"]]), {
        responseVersion: 1,
        stateFlags: 0x00ff,
        sourceDigest: 0x01020304,
        compiledDefaultDigest: 0x11121314,
        activeDigest: 0x21222324,
        pendingDigest: 0x31323334,
        committedDigest: 0x41424344,
        activeKind: 2,
        activeGeneration: 0x51525354,
        activeOriginHalf: 1,
        committedGeneration: 0x61626364,
        committedOriginHalf: 0,
        peerGeneration: 0x71727374,
        peerOriginHalf: 1,
        candidateTransactionId: 0x8182,
        lastCommittedTransactionId: 0x9192,
        conflictCount: 0xa1a2,
        validationState: 0xb1,
        lastError: 0xc1,
        candidatePending: true,
        peerKnown: true,
        peerConverged: true,
        waitingSafeBoundary: true,
    });
});

test("response correlation includes value, request id, and page", () => {
    const request = buildProfileGetRequest(1, 0, 0x41);
    const matching = response(request, Buffer.alloc(25));
    assert.equal(profileResponseMatcher(matching, request), true);
    matching[3] += 1;
    assert.equal(profileResponseMatcher(matching, request), false);
});

test("capability pages decode frozen Stage 00 capacities", () => {
    const decoded = decodeCapabilityPages(compiledOnlyPages());
    assert.deepEqual(decoded.protocol, {major: 1, minor: 0});
    assert.deepEqual(decoded.schema, {major: 1, minor: 0});
    assert.equal(decoded.featureFlags, 0x3f);
    assert.equal(decoded.compiledLayerCount, 5);
    assert.equal(decoded.maxBehaviorRows, 64);
    assert.equal(decoded.maxPopulatedBehaviorSteps, 128);
    assert.equal(decoded.physicalLedCount, 58);
    assert.equal(decoded.maxProfilePayload, 4064);
    assert.equal(decoded.profileSlotSize, 4096);
    assert.equal(decoded.viaMacroBytes, 7551);
});

test("patterned capability vectors decode every semantic field exactly", () => {
    const identity = Buffer.alloc(25);
    identity.set([1, 2, 1, 7, 1, 9, 32, 20, 2], 0);
    identity.writeUInt32LE(0x00000fff, 9);
    identity.writeUInt32LE(0x12345678, 13);
    identity.writeUInt32LE(0x89abcdef, 17);
    identity.writeUInt32LE(0x0badf00d, 21);
    const capacity = Buffer.alloc(25);
    capacity.set([5, 8, 64, 5, 128, 32, 4, 16, 32, 58, 8, 16, 64], 0);
    capacity.writeUInt16LE(4000, 13);
    capacity.writeUInt16LE(4064, 15);
    capacity.writeUInt16LE(4096, 17);
    capacity.writeUInt16LE(7551, 19);
    capacity[21] = 3;
    assert.deepEqual(decodeCapabilityPages([identity, capacity]), {
        responseVersion: 1,
        protocol: {major: 1, minor: 7},
        schema: {major: 1, minor: 9},
        reportSize: 32,
        candidateChunkMax: 20,
        statusPageCount: 2,
        featureFlags: 0x00000fff,
        actionAbiDigest: 0x12345678,
        firmwareVersion: 0x89abcdef,
        compiledDefaultDigest: 0x0badf00d,
        compiledLayerCount: 5,
        maxLogicalLayers: 8,
        maxBehaviorRows: 64,
        maxTapStepsPerBehavior: 5,
        maxPopulatedBehaviorSteps: 128,
        maxCombos: 32,
        maxKeysPerCombo: 4,
        maxReusableRgbGroups: 16,
        maxRgbStageGroupRows: 32,
        physicalLedCount: 58,
        ledBitmapSize: 8,
        hardcodedMacroSlots: 16,
        viaMacroSlots: 64,
        maxProfilePayload: 4000,
        profileSlotPayload: 4064,
        profileSlotSize: 4096,
        viaMacroBytes: 7551,
        supportedDomainMask: 3,
    });
});

test("status pages expose explicit compiled-only state", () => {
    const identity = Buffer.alloc(25);
    identity.set([1, 2, 0x81, 0], 0);
    const decoded = decodeStatusPages([identity, Buffer.alloc(25)]);
    assert.equal(decoded.stateFlags, 0x81);
    assert.equal(decoded.activeKind, 0);
    assert.equal(decoded.activeGeneration, 0);
    assert.equal(decoded.candidatePending, false);
    assert.equal(decoded.peerKnown, false);
    assert.equal(decoded.peerConverged, false);
    assert.equal(decoded.waitingSafeBoundary, false);
});

test("patterned status vectors decode every semantic field exactly", () => {
    const identity = Buffer.alloc(25);
    identity.set([1, 2], 0);
    identity.writeUInt16LE(0x007f, 2);
    identity.writeUInt32LE(0x01020304, 4);
    identity.writeUInt32LE(0x11121314, 8);
    identity.writeUInt32LE(0x21222324, 12);
    identity.writeUInt32LE(0x31323334, 16);
    identity.writeUInt32LE(0x41424344, 20);
    identity[24] = 2;
    const generation = Buffer.alloc(25);
    generation.writeUInt32LE(0x11223344, 0);
    generation[4] = 1;
    generation.writeUInt32LE(0x55667788, 5);
    generation[9] = 0;
    generation.writeUInt32LE(0x99aabbcc, 10);
    generation[14] = 1;
    generation.writeUInt16LE(0x1234, 15);
    generation.writeUInt16LE(0x5678, 17);
    generation.writeUInt16LE(0x9abc, 19);
    generation[21] = 0xde;
    generation[22] = 0xef;
    assert.deepEqual(decodeStatusPages([identity, generation]), {
        responseVersion: 1,
        stateFlags: 0x007f,
        sourceDigest: 0x01020304,
        compiledDefaultDigest: 0x11121314,
        activeDigest: 0x21222324,
        pendingDigest: 0x31323334,
        committedDigest: 0x41424344,
        activeKind: 2,
        activeGeneration: 0x11223344,
        activeOriginHalf: 1,
        committedGeneration: 0x55667788,
        committedOriginHalf: 0,
        peerGeneration: 0x99aabbcc,
        peerOriginHalf: 1,
        candidateTransactionId: 0x1234,
        lastCommittedTransactionId: 0x5678,
        conflictCount: 0x9abc,
        validationState: 0xde,
        lastError: 0xef,
        candidatePending: true,
        peerKnown: true,
        peerConverged: true,
        waitingSafeBoundary: true,
    });
});

test("decoders reject unknown masks, active kinds, and invalid ranges", () => {
    const capabilities = compiledOnlyPages();
    const unknownFeature = capabilities.map((page) => Buffer.from(page));
    unknownFeature[0].writeUInt32LE(PROFILE_WIRE_KNOWN_MASKS.FEATURE_FLAGS + 1, 9);
    assert.throws(() => decodeCapabilityPages(unknownFeature), (error) => error.code === "INCOMPATIBLE_RESPONSE");
    const unknownDomain = capabilities.map((page) => Buffer.from(page));
    unknownDomain[1][21] = 0x80;
    assert.throws(() => decodeCapabilityPages(unknownDomain), (error) => error.code === "INCOMPATIBLE_RESPONSE");
    const invalidChunk = capabilities.map((page) => Buffer.from(page));
    invalidChunk[0][7] = 21;
    assert.throws(() => decodeCapabilityPages(invalidChunk), /chunk capacity/);

    const missingCandidateFeature = capabilities.map((page) => Buffer.from(page));
    missingCandidateFeature[0].writeUInt32LE(missingCandidateFeature[0].readUInt32LE(9) & ~PROFILE_WIRE_FEATURES.CANDIDATE_WRITE, 9);
    assert.throws(() => decodeCapabilityPages(missingCandidateFeature), /candidate chunk capacity disagree/i);

    const missingRgbSchema = capabilities.map((page) => Buffer.from(page));
    missingRgbSchema[0].writeUInt32LE(missingRgbSchema[0].readUInt32LE(9) & ~PROFILE_WIRE_FEATURES.RGB_SCHEMA, 9);
    assert.throws(() => decodeCapabilityPages(missingRgbSchema), /domains and schema feature flags disagree/i);

    const commitWithoutCandidate = capabilities.map((page) => Buffer.from(page));
    commitWithoutCandidate[0][7] = 0;
    commitWithoutCandidate[0].writeUInt32LE(
        (commitWithoutCandidate[0].readUInt32LE(9) | PROFILE_WIRE_FEATURES.PERSISTENT_COMMIT) & ~PROFILE_WIRE_FEATURES.CANDIDATE_WRITE,
        9
    );
    assert.throws(() => decodeCapabilityPages(commitWithoutCandidate), /Persistent commit requires candidate-write support/);

    const peerWithoutCommit = capabilities.map((page) => Buffer.from(page));
    peerWithoutCommit[0].writeUInt32LE(peerWithoutCommit[0].readUInt32LE(9) | PROFILE_WIRE_FEATURES.PEER_RECONCILIATION, 9);
    assert.throws(() => decodeCapabilityPages(peerWithoutCommit), /Peer reconciliation requires/);

    const statusIdentity = Buffer.alloc(25);
    statusIdentity.set([1, 2], 0);
    statusIdentity.writeUInt16LE(PROFILE_WIRE_KNOWN_MASKS.STATE_FLAGS + 1, 2);
    assert.throws(() => decodeStatusPages([statusIdentity, Buffer.alloc(25)]), (error) => error.code === "INCOMPATIBLE_RESPONSE");
    statusIdentity.writeUInt16LE(0, 2);
    statusIdentity[24] = 4;
    assert.throws(() => decodeStatusPages([statusIdentity, Buffer.alloc(25)]), /Unknown active profile kind/);
    statusIdentity[24] = 0;
    const invalidOrigin = Buffer.alloc(25);
    invalidOrigin[4] = 2;
    assert.throws(() => decodeStatusPages([statusIdentity, invalidOrigin]), /origin half/);
});

test("standard VIA identity reads use canonical frames and big-endian values", async () => {
    const protocolRequest = buildViaProtocolVersionRequest();
    const firmwareRequest = buildViaFirmwareVersionRequest();
    assert.equal(protocolRequest.length, 32);
    assert.deepEqual(Array.from(protocolRequest.subarray(0, 3)), [1, 0, 0]);
    assert.deepEqual(Array.from(firmwareRequest.subarray(0, 3)), [2, 4, 0]);
    const writes = [];
    const connection = {
        async request(request, options) {
            writes.push(Buffer.from(request));
            const result = Buffer.from(request);
            if (request[0] === 1) {
                result[1] = 0;
                result[2] = 12;
            } else {
                result.set([0x89, 0xab, 0xcd, 0xef], 2);
            }
            assert.equal(options.matchResponse(result, request), true);
            return result;
        },
    };
    assert.equal(decodeViaProtocolVersion(Buffer.from([1, 0, 12, ...Buffer.alloc(29)])), 12);
    assert.equal(decodeViaFirmwareVersion(Buffer.from([2, 4, 0x89, 0xab, 0xcd, 0xef, ...Buffer.alloc(26)])), 0x89abcdef);
    assert.deepEqual(await readViaIdentity(connection), {protocolVersion: 12, firmwareVersion: 0x89abcdef});
    assert.deepEqual(writes.map((report) => report[0]), [1, 2]);
});

test("decoder rejects error, noncanonical, and mismatched responses", () => {
    const request = buildProfileGetRequest(1, 0, 0x41);
    assert.throws(() => decodeProfileResponse(response(request, Buffer.alloc(0), 1), request), (error) => error.code === "DEVICE_REJECTED" && error.status === 1);

    const noncanonical = response(request, Buffer.alloc(24));
    noncanonical[31] = 1;
    assert.throws(() => decodeProfileResponse(noncanonical, request), (error) => error.code === "NONCANONICAL_RESPONSE");

    const mismatched = response(request, Buffer.alloc(25));
    mismatched[4] = 1;
    assert.throws(() => decodeProfileResponse(mismatched, request), (error) => error.code === "CORRELATION_MISMATCH");
});

test("capability reads serialize through the transport connection", async () => {
    const pages = compiledOnlyPages();
    const requests = [];
    const connection = {
        async request(request, options) {
            requests.push(Buffer.from(request));
            assert.equal(options.matchResponse(response(request, pages[request[4]]), request), true);
            return response(request, pages[request[4]]);
        },
    };
    const decoded = await readProfileCapabilities(connection, {requestId: 0xfe});
    assert.deepEqual(requests.map((request) => request[3]), [0xfe, 0xff]);
    assert.deepEqual(requests.map((request) => request[4]), [0, 1]);
    assert.equal(decoded.maxProfilePayload, 4064);
});
