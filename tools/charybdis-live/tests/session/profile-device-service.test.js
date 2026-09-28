"use strict";

const assert = require("node:assert/strict");
const test = require("node:test");

const {FakeDeviceAdapter} = require("../../core/transport/fake-device-adapter");
const {crc32, fnv1a32} = require("../../core/schema/profile-blob-v1");
const {CANDIDATE_ERROR, CANDIDATE_OPERATION, CANDIDATE_STATE} = require("../../core/protocol/profile-candidate-v1");
const {
    ProfileRequestIdSequence,
    ProfileDeviceService,
    evaluateProfileCompatibility,
    evaluateLiveMutationCompatibility,
} = require("../../core/session/profile-device-service");
const {PROFILE_ACTIVE_KIND, PROFILE_WIRE_FEATURES, PROFILE_WIRE_STATUS, PROFILE_WIRE_V1, VIA_READS} = require("../../core/protocol/profile-wire-v1");
const {document} = require("../fixtures/pd-profile");

function response(request, payload) {
    const report = Buffer.from(request);
    report.fill(0, 5);
    report[5] = 0;
    report[6] = payload.length;
    Buffer.from(payload).copy(report, 7);
    return report;
}

function readPages(options = {}) {
    const capabilityIdentity = Buffer.alloc(25);
    const featureFlags = PROFILE_WIRE_FEATURES.READ_SURFACE
        | PROFILE_WIRE_FEATURES.STORAGE_LAYOUT
        | PROFILE_WIRE_FEATURES.RGB_SCHEMA
        | PROFILE_WIRE_FEATURES.KEY_BEHAVIOR_SCHEMA
        | PROFILE_WIRE_FEATURES.SPLIT_KEYBOARD
        | PROFILE_WIRE_FEATURES.ACTION_ABI_DIGEST
        | PROFILE_WIRE_FEATURES.COMPILED_PROFILE_HASH
        | (options.mutation
            ? PROFILE_WIRE_FEATURES.CANDIDATE_WRITE
                | PROFILE_WIRE_FEATURES.PERSISTENT_COMMIT
                | PROFILE_WIRE_FEATURES.RUNTIME_ACTIVATION
                | PROFILE_WIRE_FEATURES.PEER_RECONCILIATION
                | PROFILE_WIRE_FEATURES.ATOMIC_LOGICAL_APPLY
            : 0);
    capabilityIdentity.set([1, 2, 1, 0, options.schemaMajor || 1, 0, 32, options.mutation ? 20 : 0, 2], 0);
    capabilityIdentity.writeUInt32LE(featureFlags, 9);
    capabilityIdentity.writeUInt32LE(options.actionAbiDigest ?? 0x12345678, 13);
    const capacity = Buffer.from([
        5, options.maxLayers || 8, 64, 5, 128, 32, 4, 16, 32, 58, 8, 16, 64,
        0xe0, 0x0f, 0xe0, 0x0f, 0x00, 0x10, 0x7f, 0x1d, 3, 0, 0, 0,
    ]);
    const statusIdentity = Buffer.alloc(25);
    statusIdentity.set([1, 2], 0);
    const stateFlags = options.stateFlags ?? (options.mutation ? 0x31 : 0x81);
    statusIdentity.writeUInt16LE(stateFlags, 2);
    statusIdentity.writeUInt32LE(options.activeDigest ?? 0, 12);
    statusIdentity.writeUInt32LE(options.committedDigest ?? 0, 20);
    statusIdentity[24] = options.activeKind ?? PROFILE_ACTIVE_KIND.COMPILED_ONLY;
    return {
        [`${PROFILE_WIRE_V1.VALUE_CAPABILITIES}:0`]: capabilityIdentity,
        [`${PROFILE_WIRE_V1.VALUE_CAPABILITIES}:1`]: capacity,
        [`${PROFILE_WIRE_V1.VALUE_STATUS}:0`]: statusIdentity,
        [`${PROFILE_WIRE_V1.VALUE_STATUS}:1`]: Buffer.alloc(25),
    };
}

function candidateStatus(overrides = {}) {
    return {
        state: CANDIDATE_STATE.IDLE,
        lastOperation: CANDIDATE_OPERATION.NONE,
        flags: 0,
        mailboxPending: false,
        poisoned: false,
        transactionId: 0,
        nextOffset: 0,
        payloadLength: 0,
        digest: 0,
        error: {id: CANDIDATE_ERROR.NONE, name: "NONE"},
        operationSequence: 0,
        ...overrides,
    };
}

function serviceHarness(options = {}) {
    const pages = readPages(options);
    const adapter = new FakeDeviceAdapter({
        devices: [{
            id: "/native/raw/hid/path",
            path: "/native/raw/hid/path",
            product: "Charybdis 4x6",
            serialNumber: "SERIAL-1",
        }],
        onWrite({connection, report}) {
            if (report[0] === VIA_READS.COMMAND_GET_PROTOCOL_VERSION) {
                const viaResponse = Buffer.from(report);
                viaResponse[1] = 0;
                viaResponse[2] = VIA_READS.EXPECTED_PROTOCOL_VERSION;
                queueMicrotask(() => connection.emitReport(viaResponse));
                return;
            }
            if (report[0] === VIA_READS.COMMAND_GET_KEYBOARD_VALUE && report[1] === VIA_READS.VALUE_FIRMWARE_VERSION) {
                queueMicrotask(() => connection.emitReport(Buffer.from(report)));
                return;
            }
            assert.equal(report[0], PROFILE_WIRE_V1.COMMAND_GET, "Stage 01 service must only send read requests");
            const payload = pages[`${report[2]}:${report[4]}`];
            assert.ok(payload, `unexpected Profile Wire read ${report[2]} page ${report[4]}`);
            queueMicrotask(() => connection.emitReport(response(report, payload)));
        },
    });
    const changes = [];
    const service = new ProfileDeviceService({
        adapter,
        defaultTimeoutMs: 100,
        onChange(snapshot) {
            changes.push(snapshot);
        },
        profileSummary: {
            layerCount: 5,
            behaviorRows: 5,
            maxTapStepsPerBehavior: 3,
            populatedBehaviorSteps: 8,
            comboCount: 4,
            maxKeysPerCombo: 2,
            reusableRgbGroups: 5,
            rgbStageGroupRows: 12,
            highestLedIndex: 57,
        },
        readCandidateStatus: options.readCandidateStatus || (async () => candidateStatus(options.candidateStatus)),
    });
    return {adapter, changes, service};
}

function profileReadHarness({committed, rejectChunk = false}) {
    const bytes = Buffer.from(document().profile, "base64");
    const seen = [];
    const identity = Buffer.alloc(25), generation = Buffer.alloc(25);
    identity[0] = 1; identity[1] = 2;
    identity.writeUInt16LE(committed ? 0x32 : 0x31, 2);
    identity[24] = committed ? PROFILE_ACTIVE_KIND.COMMITTED : PROFILE_ACTIVE_KIND.COMPILED_ONLY;
    if (committed) {
        generation.writeUInt32LE(4, 0);
        generation.writeUInt32LE(4, 5);
        generation.writeUInt32LE(4, 10);
    }
    const metadata = Buffer.alloc(25);
    metadata[0] = 1; metadata[1] = 25;
    metadata.writeUInt16LE(bytes.length, 2);
    metadata.writeUInt32LE(committed ? 4 : 0, 4);
    metadata.writeUInt32LE(fnv1a32(bytes), 8);
    metadata.writeUInt32LE(crc32(bytes), 12);
    metadata[16] = 2; metadata[18] = 31;
    const connection = {connected: true, async request(request) {
        seen.push([request[2], request[4]]);
        if (request[2] === PROFILE_WIRE_V1.VALUE_STATUS) return response(request, request[4] ? generation : identity);
        if (rejectChunk && request[4] === 1) {
            const rejected = response(request, Buffer.alloc(0));
            rejected[5] = PROFILE_WIRE_STATUS.UNAVAILABLE;
            return rejected;
        }
        return response(request, request[4] ? bytes.subarray((request[4] - 1) * 25, request[4] * 25) : metadata);
    }};
    const service = new ProfileDeviceService({adapter: new FakeDeviceAdapter()});
    service.connection = connection;
    service.requestIds = new ProfileRequestIdSequence();
    return {service, seen};
}

test("profile read chooses compiled defaults from a fresh status", async () => {
    const {service, seen} = profileReadHarness({committed: false});
    service.status = {activeKind: PROFILE_ACTIVE_KIND.COMMITTED, committedGeneration: 9};
    const state = await service.readCommittedProfile();
    assert.equal(state.error, null);
    assert.equal(state.committed.source, "compiled");
    assert.equal(state.committed.state, "read");
    assert.equal(state.status.activeKind, PROFILE_ACTIVE_KIND.COMPILED_ONLY);
    assert.ok(seen.some(([value]) => value === 5));
    assert.ok(!seen.some(([value]) => value === 4));
});

test("a rejected committed payload chunk stays a read failure", async () => {
    const {service, seen} = profileReadHarness({committed: true, rejectChunk: true});
    service.status = {activeKind: PROFILE_ACTIVE_KIND.COMPILED_ONLY, committedGeneration: 0};
    const state = await service.readCommittedProfile();
    assert.equal(state.error.code, "DEVICE_REJECTED");
    assert.equal(state.committed.state, "reading");
    assert.equal(state.status.activeKind, PROFILE_ACTIVE_KIND.COMMITTED, "a failed payload read still reports the fresh device status");
    assert.ok(seen.some(([value]) => value === 4));
    assert.ok(!seen.some(([value]) => value === 5), "a failed committed chunk is not treated as missing storage");
});

test("service exposes opaque descriptors and performs only capability/status reads", async () => {
    const {adapter, service} = serviceHarness();
    const scanned = await service.enumerate();
    assert.equal(scanned.devices.length, 1);
    assert.equal(scanned.devices[0].id, "charybdis-1");
    assert.equal(JSON.stringify(scanned).includes("/native/raw/hid/path"), false);

    const connected = await service.connect(scanned.devices[0].id);
    assert.equal(connected.connected, true);
    assert.equal(connected.connectionToken, 1);
    assert.equal(connected.capabilities.schema.major, 1);
    assert.equal(connected.status.activeKind, 0);
    assert.equal(connected.compatibility.compatible, true);
    assert.equal(adapter.lastConnection().writes.length, 6);
    assert.deepEqual(adapter.lastConnection().writes.map((report) => report[0]), [1, 2, 8, 8, 8, 8]);
    assert.deepEqual(adapter.lastConnection().writes.slice(2).map((report) => report[2]), [1, 1, 2, 2]);
    assert.deepEqual(adapter.lastConnection().writes.slice(2).map((report) => report[3]), [1, 2, 3, 4]);

    const disconnected = await service.disconnect();
    assert.equal(disconnected.connected, false);
    assert.equal(disconnected.connectionToken, null);
    assert.equal(disconnected.capabilities, null);
    const reconnected = await service.connect(scanned.devices[0].id);
    assert.equal(reconnected.connectionToken, 2, "the same HID path is still a new connection for draft safety");
});

test("rescans keep each interface ID and never give a replacement the old draft ID", async () => {
    const {adapter, service} = serviceHarness();
    const first = (await service.enumerate()).devices[0].id;
    adapter.devices = [{id: "/native/second/path", product: "Charybdis 4x6", serialNumber: "SERIAL-2"}];
    const second = (await service.enumerate()).devices[0].id;
    assert.notEqual(second, first);
    assert.equal((await service.connect(first)).connected, false, "a removed interface cannot be selected from the latest scan");
    adapter.devices = [
        {id: "/native/second/path", product: "Charybdis 4x6", serialNumber: "SERIAL-2"},
        {id: "/native/raw/hid/path", product: "Charybdis 4x6", serialNumber: "SERIAL-1"},
    ];
    const rescanned = (await service.enumerate()).devices;
    assert.deepEqual(rescanned.map((entry) => entry.id), [second, first], "enumeration order does not change identities");
});

test("compatibility reports schema and source-capacity blockers", async () => {
    const {service} = serviceHarness({schemaMajor: 2, maxLayers: 8});
    const scanned = await service.enumerate();
    await service.connect(scanned.devices[0].id);
    const changed = service.setProfileSummary({layerCount: 9});
    assert.equal(changed.compatibility.compatible, false);
    assert.match(changed.compatibility.reasons.join("\n"), /Schema major/);
    assert.match(changed.compatibility.reasons.join("\n"), /Logical layers needs 9/);
});

test("compatibility is recomputed when the active source profile changes", async () => {
    const {service} = serviceHarness();
    const scanned = await service.enumerate();
    await service.connect(scanned.devices[0].id);
    const changed = service.setProfileSummary({layerCount: 9});
    assert.equal(changed.compatibility.compatible, false);
    assert.match(changed.compatibility.reasons[0], /Logical layers/);
});

test("compatibility checks Milestone A domains and fixed report framing", () => {
    const compatibility = evaluateProfileCompatibility({
        protocol: {major: 1},
        schema: {major: 1},
        reportSize: 31,
        statusPageCount: 2,
        supportedDomainMask: 1,
        maxLogicalLayers: 8,
        maxBehaviorRows: 64,
        maxTapStepsPerBehavior: 5,
        maxPopulatedBehaviorSteps: 128,
        maxCombos: 32,
        maxKeysPerCombo: 4,
        maxReusableRgbGroups: 16,
        maxRgbStageGroupRows: 32,
        physicalLedCount: 58,
    }, {}, {protocolVersion: 12, firmwareVersion: 0});
    assert.equal(compatibility.compatible, false);
    assert.match(compatibility.reasons.join("\n"), /Raw HID report size/);
    assert.match(compatibility.reasons.join("\n"), /Milestone A domains/);
});

test("persistent live apply requires every mutation capability", () => {
    const compatible = {compatible: true, reasons: []};
    const completeFlags = PROFILE_WIRE_FEATURES.CANDIDATE_WRITE
        | PROFILE_WIRE_FEATURES.PERSISTENT_COMMIT
        | PROFILE_WIRE_FEATURES.RUNTIME_ACTIVATION
        | PROFILE_WIRE_FEATURES.PEER_RECONCILIATION
        | PROFILE_WIRE_FEATURES.ATOMIC_LOGICAL_APPLY;
    const readyStatus = {peerKnown: true, peerConverged: true, candidatePending: false};
    assert.equal(evaluateLiveMutationCompatibility({featureFlags: completeFlags, candidateChunkMax: 20}, compatible, true, readyStatus).available, true);

    const blocked = evaluateLiveMutationCompatibility({
        featureFlags: completeFlags & ~PROFILE_WIRE_FEATURES.PEER_RECONCILIATION,
        candidateChunkMax: 20,
    }, compatible, true, readyStatus);
    assert.equal(blocked.available, false);
    assert.match(blocked.reasons.join("\n"), /persistent split apply mask/);
});

test("persistent live apply requires a detected and converged second half", () => {
    const capabilities = {
        featureFlags: PROFILE_WIRE_FEATURES.CANDIDATE_WRITE
            | PROFILE_WIRE_FEATURES.PERSISTENT_COMMIT
            | PROFILE_WIRE_FEATURES.RUNTIME_ACTIVATION
            | PROFILE_WIRE_FEATURES.PEER_RECONCILIATION
            | PROFILE_WIRE_FEATURES.ATOMIC_LOGICAL_APPLY,
        candidateChunkMax: 20,
    };
    const peerMissing = evaluateLiveMutationCompatibility(
        capabilities,
        {compatible: true, reasons: []},
        true,
        {peerKnown: false, peerConverged: false, candidatePending: false}
    );
    assert.equal(peerMissing.available, false);
    assert.match(peerMissing.reasons.join("\n"), /second keyboard half has not been detected/);

    const unconverged = evaluateLiveMutationCompatibility(
        capabilities,
        {compatible: true, reasons: []},
        true,
        {peerKnown: true, peerConverged: false, candidatePending: false}
    );
    assert.equal(unconverged.available, false);
    assert.match(unconverged.reasons.join("\n"), /have not established a converged profile state/);
});

test("request ids advance across refreshes and reject a delayed duplicate", async () => {
    const pages = readPages();
    let delayedResponse;
    let customWriteCount = 0;
    const adapter = new FakeDeviceAdapter({
        devices: [{id: "native-device", product: "Charybdis"}],
        onWrite({connection, report}) {
            if (report[0] === VIA_READS.COMMAND_GET_PROTOCOL_VERSION) {
                const viaResponse = Buffer.from(report);
                viaResponse[2] = VIA_READS.EXPECTED_PROTOCOL_VERSION;
                queueMicrotask(() => connection.emitReport(viaResponse));
                return;
            }
            if (report[0] === VIA_READS.COMMAND_GET_KEYBOARD_VALUE) {
                queueMicrotask(() => connection.emitReport(Buffer.from(report)));
                return;
            }
            const current = response(report, pages[`${report[2]}:${report[4]}`]);
            customWriteCount += 1;
            if (customWriteCount === 1) delayedResponse = Buffer.from(current);
            if (customWriteCount === 5) {
                queueMicrotask(() => connection.emitReport(delayedResponse));
            }
            queueMicrotask(() => connection.emitReport(current));
        },
    });
    const service = new ProfileDeviceService({adapter, defaultTimeoutMs: 100});
    const scanned = await service.enumerate();
    await service.connect(scanned.devices[0].id);
    const refreshed = await service.refresh();
    assert.equal(refreshed.connected, true);
    assert.equal(refreshed.error, null);
    const customWrites = adapter.lastConnection().writes.filter((report) => report[0] === PROFILE_WIRE_V1.COMMAND_GET);
    assert.deepEqual(customWrites.map((report) => report[3]), [1, 2, 3, 4, 5, 6, 7, 8]);
    assert.ok(refreshed.diagnostics.some((entry) => entry.includes("unexpected Raw HID report")));
});

test("request id allocation wraps from 255 to 1 without emitting zero", () => {
    const ids = new ProfileRequestIdSequence(0xfe);
    assert.deepEqual([ids.next(), ids.next(), ids.next(), ids.next()], [0xfe, 0xff, 1, 2]);
    assert.throws(() => new ProfileRequestIdSequence(0), /1 through 255/);
});

test("the layout read counts only keys the app cannot name", () => {
    const {unnamedKeyCount} = require("../../core/session/profile-device-service");
    const keycodes = require("../../core/data/keycode-catalog");
    const layers = [{layer: 0, keys: [0x0004, 0x7e42, 0x7e85, 0x7ea3, 0x7ec2, 0x7e90, 0x7ee0].map((keycode) => ({keycode, resolved: keycodes.resolve(keycode)}))}];
    assert.equal(unnamedKeyCount(layers, {actionAbiDigest: 0x1d3fcacc}), 2, "an unused pointing slot and a code past the blocks");
    assert.equal(unnamedKeyCount(layers, {actionAbiDigest: 0x61072732}), 5, "older firmware's user keys are not read as blocks");
});
