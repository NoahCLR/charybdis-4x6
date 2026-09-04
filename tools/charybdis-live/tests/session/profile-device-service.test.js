"use strict";

const assert = require("node:assert/strict");
const test = require("node:test");

const {FakeDeviceAdapter} = require("../../core/transport/fake-device-adapter");
const {
    CANDIDATE_ERROR,
    CANDIDATE_OPERATION,
    CANDIDATE_STATE,
    candidateMetadataForBlob,
} = require("../../core/protocol/profile-candidate-v1");
const {encodeProfileBlob, PROFILE_DOMAIN_IDS} = require("../../core/schema/profile-blob-v1");
const {
    ProfileRequestIdSequence,
    ProfileDeviceService,
    evaluateProfileCompatibility,
    evaluateLiveMutationCompatibility,
} = require("../../core/session/profile-device-service");
const {PROFILE_ACTIVE_KIND, PROFILE_WIRE_FEATURES, PROFILE_WIRE_V1, VIA_READS} = require("../../core/protocol/profile-wire-v1");

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

function liveProfileBlob() {
    return encodeProfileBlob({domains: [
        {id: PROFILE_DOMAIN_IDS.RGB, version: 1, payload: Buffer.alloc(0)},
        {id: PROFILE_DOMAIN_IDS.KEY_BEHAVIORS, version: 1, payload: Buffer.alloc(0)},
    ]});
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
        createCandidateUploadCoordinator: options.createCandidateUploadCoordinator,
        readCandidateStatus: options.readCandidateStatus || (async () => candidateStatus(options.candidateStatus)),
        synchronizeViaLayout: options.synchronizeViaLayout,
    });
    return {adapter, changes, service};
}

test("service exposes opaque descriptors and performs only capability/status reads", async () => {
    const {adapter, service} = serviceHarness();
    const scanned = await service.enumerate();
    assert.equal(scanned.devices.length, 1);
    assert.equal(scanned.devices[0].id, "charybdis-1");
    assert.equal(JSON.stringify(scanned).includes("/native/raw/hid/path"), false);

    const connected = await service.connect(scanned.devices[0].id);
    assert.equal(connected.connected, true);
    assert.equal(connected.capabilities.schema.major, 1);
    assert.equal(connected.status.activeKind, 0);
    assert.equal(connected.compatibility.compatible, true);
    assert.equal(adapter.lastConnection().writes.length, 6);
    assert.deepEqual(adapter.lastConnection().writes.map((report) => report[0]), [1, 2, 8, 8, 8, 8]);
    assert.deepEqual(adapter.lastConnection().writes.slice(2).map((report) => report[2]), [1, 1, 2, 2]);
    assert.deepEqual(adapter.lastConnection().writes.slice(2).map((report) => report[3]), [1, 2, 3, 4]);

    const disconnected = await service.disconnect();
    assert.equal(disconnected.connected, false);
    assert.equal(disconnected.capabilities, null);
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
        | PROFILE_WIRE_FEATURES.PEER_RECONCILIATION;
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
            | PROFILE_WIRE_FEATURES.PEER_RECONCILIATION,
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

test("service uploads, commits, and verifies a live profile before reporting success", async () => {
    const blob = liveProfileBlob();
    const metadata = candidateMetadataForBlob(blob, {actionAbiDigest: 0x12345678, requestedDomains: 3});
    const digest = metadata.digest;
    const calls = [];
    const {adapter, service} = serviceHarness({
        mutation: true,
        activeKind: PROFILE_ACTIVE_KIND.COMMITTED,
        activeDigest: digest,
        committedDigest: digest,
        createCandidateUploadCoordinator(connection, options) {
            assert.equal(connection.connected, true);
            return {
                async upload(blob, uploadOptions) {
                    calls.push({kind: "upload", blob: Buffer.from(blob), options: {...uploadOptions}});
                    options.onProgress({phase: "writing", bytesSent: blob.length, totalBytes: blob.length});
                    return {transactionId: 0x1234, metadata: uploadOptions.metadata, progress: {phase: "complete"}};
                },
                async commit(transactionId, commitOptions) {
                    calls.push({kind: "commit", transactionId, options: {...commitOptions}});
                    options.onProgress({phase: "committing", transactionId});
                    return {progress: {phase: "complete", transactionId}, status: candidateStatus({
                        state: CANDIDATE_STATE.IDLE,
                        lastOperation: CANDIDATE_OPERATION.COMMIT,
                        transactionId,
                        digest,
                    })};
                },
            };
        },
        async synchronizeViaLayout(connection, entries, options) {
            assert.equal(connection.connected, true);
            calls.push({kind: "layout", entries});
            options.onProgress({phase: "reading-layout", completed: entries.length, total: entries.length, changed: 1});
            options.onProgress({phase: "writing-layout", completed: 1, total: 1, changed: 1});
            return {checkedKeys: entries.length, changedKeys: 1, verifiedKeys: 1};
        },
    });
    const scanned = await service.enumerate();
    const connected = await service.connect(scanned.devices[0].id);
    assert.equal(connected.mutationCompatibility.available, true);

    const layoutEntries = [{layer: 0, row: 0, column: 0, keycode: 4}];
    const applied = await service.applyLiveProfile(blob, {layoutEntries});
    assert.equal(applied.error, null);
    assert.equal(applied.liveApply.state, "complete");
    assert.deepEqual(applied.liveApply.result, {
        transactionId: 0x1234,
        digest,
        byteLength: blob.length,
        layout: {checkedKeys: 1, changedKeys: 1, verifiedKeys: 1},
    });
    assert.deepEqual(calls, [
        {
            kind: "upload",
            blob,
            options: {metadata},
        },
        {kind: "commit", transactionId: 0x1234, options: {digest}},
        {kind: "layout", entries: layoutEntries},
    ]);
    assert.equal(adapter.lastConnection().writes.length, 10, "apply should recheck split readiness and verify both status pages after commit");
});

test("service resumes a matching candidate already preparing the peer", async () => {
    const blob = liveProfileBlob();
    const metadata = candidateMetadataForBlob(blob, {actionAbiDigest: 0x12345678, requestedDomains: 3});
    const calls = [];
    const {service} = serviceHarness({
        mutation: true,
        stateFlags: 0x35,
        activeKind: PROFILE_ACTIVE_KIND.COMMITTED,
        activeDigest: metadata.digest,
        committedDigest: metadata.digest,
        candidateStatus: candidateStatus({
            state: CANDIDATE_STATE.PREPARING_PEER,
            lastOperation: CANDIDATE_OPERATION.COMMIT,
            transactionId: 1,
            payloadLength: blob.length,
            digest: metadata.digest,
        }),
        createCandidateUploadCoordinator() {
            return {
                async upload() {
                    assert.fail("a matching prepared candidate must not be uploaded again");
                },
                async commit(transactionId, options) {
                    calls.push({transactionId, options});
                    return {progress: {phase: "complete"}, status: candidateStatus({
                        state: CANDIDATE_STATE.IDLE,
                        lastOperation: CANDIDATE_OPERATION.COMMIT,
                        transactionId,
                        digest: metadata.digest,
                    })};
                },
            };
        },
    });
    const scanned = await service.enumerate();
    const connected = await service.connect(scanned.devices[0].id);
    assert.equal(connected.candidateStatus.stateName, "PREPARING_PEER");
    const applied = await service.applyLiveProfile(blob);
    assert.equal(applied.error, null);
    assert.equal(applied.liveApply.state, "complete");
    assert.deepEqual(calls, [{transactionId: 1, options: {digest: metadata.digest}}]);
    assert.ok(applied.diagnostics.some((entry) => entry.includes("Resuming matching candidate transaction 1 from PREPARING_PEER")));
});

test("service refuses to replace a nonmatching active candidate", async () => {
    const blob = liveProfileBlob();
    const metadata = candidateMetadataForBlob(blob, {actionAbiDigest: 0x12345678, requestedDomains: 3});
    const {service} = serviceHarness({
        mutation: true,
        stateFlags: 0x35,
        candidateStatus: candidateStatus({
            state: CANDIDATE_STATE.PREPARING_PEER,
            lastOperation: CANDIDATE_OPERATION.COMMIT,
            transactionId: 7,
            digest: metadata.digest ^ 0xffffffff,
        }),
        createCandidateUploadCoordinator() {
            return {
                async upload() {
                    assert.fail("a nonmatching active candidate must not be overwritten");
                },
                async commit() {
                    assert.fail("a nonmatching active candidate must not be committed");
                },
            };
        },
    });
    const scanned = await service.enumerate();
    await service.connect(scanned.devices[0].id);
    const failed = await service.applyLiveProfile(blob);
    assert.equal(failed.error.code, "ACTIVE_CANDIDATE");
    assert.match(failed.error.message, /PREPARING_PEER/);
    assert.match(failed.error.message, /Power-cycle both halves together/);
});

test("service fails closed when post-commit status does not confirm the digest", async () => {
    const blob = liveProfileBlob();
    const digest = candidateMetadataForBlob(blob, {actionAbiDigest: 0x12345678, requestedDomains: 3}).digest;
    const {service} = serviceHarness({
        mutation: true,
        createCandidateUploadCoordinator() {
            return {
                async upload() {
                    return {transactionId: 0x1234, metadata: {digest}, progress: {phase: "complete"}};
                },
                async commit() {
                    return {progress: {phase: "complete"}};
                },
            };
        },
    });
    const scanned = await service.enumerate();
    await service.connect(scanned.devices[0].id);
    const failed = await service.applyLiveProfile(blob);
    assert.equal(failed.liveApply.state, "failed");
    assert.equal(failed.liveApply.error.code, "LIVE_APPLY_VERIFICATION_FAILED");
    assert.equal(failed.error.code, "LIVE_APPLY_VERIFICATION_FAILED");
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
