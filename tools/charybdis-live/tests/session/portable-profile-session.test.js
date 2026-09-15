"use strict";
const {test} = require("node:test");
const assert = require("node:assert/strict");
const {captureProfile, restoreProfile, fingerprint, summary, validateSnapshot} = require("../../core/session/portable-profile-session");
const {document} = require("../fixtures/portable-profile");
const {viaStorageDigest} = require("../../core/protocol/via-storage-v1");
const capabilities = {compiledLayerCount: 8, supportedDomainMask: 15, featureFlags: 1 << 12, actionAbiDigest: 0xeb80829c, candidateChunkMax: 20, viaMacroBytes: 7191};
function fixture() {
    const source = document(), targetDocument = structuredClone(source), events = [];
    targetDocument.layers[0][0] ^= 1; targetDocument.macros[0] = Buffer.from("hello").toString("base64");
    const target = validateSnapshot(targetDocument), identity = {profile: "base", storageGeneration: 4, storageDigest: 5, settingsCrc: 6, settingsDigest: 7};
    const capture = async () => ({document: source, fingerprint: fingerprint(source), summary: summary(source), identity});
    const storageDigest = viaStorageDigest(target);
    const operations = {capture, readIdentity: async () => identity, readCandidate: async () => ({state: 0}),
        createCoordinator: () => ({upload: async (bytes, options) => {events.push("stage"); await options.verifyBase(); return {transactionId: 1, metadata: {digest: 42}};}, commit: async (id, options) => {events.push("commit"); await options.afterDecision();}}),
        createViaCoordinator: () => ({stage: async options => {events.push("via stage"); assert.equal(options.generation, 5); assert.equal(options.digest, storageDigest);}, waitUntilAccepted: async () => {events.push("via accepted");}, abort: async () => {events.push("via abort");}}),
        rollForwardLocal: async (connection, actualTarget, base, layoutRanges, macroRanges) => {events.push("local roll-forward"); assert.equal(actualTarget.profile.equals(target.profile), true); assert.equal(layoutRanges.length, 1); assert.equal(macroRanges.length, 1);},
        waitStorage: async () => {events.push("both halves"); return {ready: true, generation: 5, digest: storageDigest};},
        readStorage: async () => ({ready: true, generation: 5, digest: storageDigest}),
        readStored: async (connection, command, length, options = {}) => (command === 0x12 ? target.layout : target.macros).subarray(options.startOffset || 0, (options.startOffset || 0) + length),
        readProfile: async () => ({activeKind: 1, activeDigest: require("../../core/schema/profile-blob-v1").fnv1a32(target.profile), committedDigest: require("../../core/schema/profile-blob-v1").fnv1a32(target.profile), stateFlags: 32, conflictCount: 0})};
    const options = {operations, expectedFingerprint: fingerprint(source), saveRecovery: async backup => {assert.equal(fingerprint(backup), fingerprint(source)); events.push("backup"); return "/recovery.json";}};
    return {source, targetDocument, target, identity, events, operations, options};
}
test("restore saves recovery first, restores both stores, then verifies complete readback", async () => {
    const f = fixture(), result = await restoreProfile({}, {}, capabilities, f.targetDocument, f.options);
    assert.equal(result.fingerprint, fingerprint(f.targetDocument));
    assert.deepEqual({...result.performance, elapsedMs: 0}, {elapsedMs: 0, baseSource: "device-read", layoutBytes: 28, macroBytes: 28, viaConfigReports: 1, layoutReports: 3, macroReports: 3});
    assert.deepEqual(f.events, ["backup", "stage", "via stage", "commit", "via accepted", "local roll-forward", "both halves"]);
});
test("restore verifies and reuses the reviewed snapshot without rereading its complete payload", async () => {
    const f = fixture();
    let captures = 0, identityReads = 0;
    f.operations.capture = async () => {captures++; throw Error("must not capture");};
    f.operations.readIdentity = async () => {identityReads++; return f.identity;};
    f.options.baseSnapshot = {document: f.source, fingerprint: fingerprint(f.source), summary: summary(f.source), identity: f.identity};
    const result = await restoreProfile({}, {}, capabilities, f.targetDocument, f.options);
    assert.equal(captures, 0);
    assert.equal(identityReads, 2);
    assert.equal(result.performance.baseSource, "verified-cache");
    assert.deepEqual(f.events, ["backup", "stage", "via stage", "commit", "via accepted", "local roll-forward", "both halves"]);
});
test("a changed device identity rejects a cached recovery base before saving or staging", async () => {
    const f = fixture();
    f.options.baseSnapshot = {document: f.source, fingerprint: fingerprint(f.source), summary: summary(f.source), identity: f.identity};
    f.operations.readIdentity = async () => ({...f.identity, storageGeneration: f.identity.storageGeneration + 1});
    await assert.rejects(restoreProfile({}, {}, capabilities, f.targetDocument, f.options), /changed/);
    assert.deepEqual(f.events, []);
});
test("stale review and failed recovery save perform no device mutations", async () => {
    const f = fixture(); f.options.expectedFingerprint = "stale";
    await assert.rejects(restoreProfile({}, {}, capabilities, f.targetDocument, f.options), /changed/); assert.deepEqual(f.events, []);
    f.options.expectedFingerprint = fingerprint(f.source); f.options.saveRecovery = async () => {throw Error("disk full");};
    await assert.rejects(restoreProfile({}, {}, capabilities, f.targetDocument, f.options), /disk full/); assert.deepEqual(f.events, []);
});
test("changes after acquiring the profile lease prevent commit", async () => {
    const f = fixture();
    f.operations.readIdentity = async () => ({...f.identity, storageDigest: 99});
    await assert.rejects(restoreProfile({}, {}, capabilities, f.targetDocument, f.options), /changed/);
    assert.deepEqual(f.events, ["backup", "stage"]);
});
test("an interrupted write reports recovery and cannot report a successful restore", async () => {
    const f = fixture(); f.operations.createViaCoordinator = () => ({stage: async () => {throw Error("disconnected");}, abort: async () => {}});
    await assert.rejects(restoreProfile({}, {}, capabilities, f.targetDocument, f.options), error => error.code === "RESTORE_INCOMPLETE" && error.message.includes("/recovery.json"));
    assert.deepEqual(f.events, ["backup", "stage"]);
});
test("a post-decision local-write interruption preserves the peer recovery copy", async () => {
    const f = fixture();
    f.operations.rollForwardLocal = async () => {f.events.push("local roll-forward"); throw Error("disconnected");};
    await assert.rejects(restoreProfile({}, {}, capabilities, f.targetDocument, f.options), error => error.code === "RESTORE_INCOMPLETE" && error.message.includes("/recovery.json"));
    assert.equal(f.events.includes("via abort"), false);
    assert.deepEqual(f.events, ["backup", "stage", "via stage", "commit", "via accepted", "local roll-forward"]);
});
test("mismatching final hardware readback fails verification", async () => {
    const f = fixture(); f.operations.readStored = async () => Buffer.alloc(1);
    await assert.rejects(restoreProfile({}, {}, capabilities, f.targetDocument, f.options), /readback/);
});
test("a concurrent change outside the edited blocks fails whole-store verification", async () => {
    const f = fixture();
    f.operations.readStorage = async () => ({ready: true, generation: 8, digest: viaStorageDigest(f.target) + 1});
    await assert.rejects(restoreProfile({}, {}, capabilities, f.targetDocument, f.options), /confirm/);
});
test("recovery retries allow an incomplete base and prove the repaired target", async () => {
    const f = fixture();
    f.operations.capture = async (connection, ids, caps, progress, candidate, allowIncomplete) => {
        assert.equal(allowIncomplete, true);
        const base = validateSnapshot(f.source);
        return {document: {format: "charybdis-recovery-capture", version: 1, keyboard: "charybdis-4x6", actionAbiDigest: capabilities.actionAbiDigest,
            profile: base.profile.toString("base64"), layout: base.layout.toString("base64"), macros: base.macros.toString("base64")}, incomplete: true, fingerprint: "interrupted", summary: null, identity: f.identity};
    };
    f.options.expectedFingerprint = "interrupted";
    f.options.saveRecovery = async value => {assert.equal(value.format, "charybdis-recovery-capture"); return "/interrupted.diagnostic.json";};
    const restored = await restoreProfile({}, {}, capabilities, f.targetDocument, f.options);
    assert.equal(restored.fingerprint, fingerprint(f.targetDocument));
});
test("older firmware is rejected before any device read or write", async () => {
    const connection = {request: () => {throw Error("must not access device");}};
    await assert.rejects(captureProfile(connection, {}, {compiledLayerCount: 5}), error => error.code === "FIRMWARE_UPDATE_REQUIRED");
});
