"use strict";
const {test} = require("node:test");
const assert = require("node:assert/strict");
const {captureProfile, restoreProfile, fingerprint, summary, validateSnapshot} = require("../../core/session/portable-profile-session");
const {document} = require("../fixtures/portable-profile");
const capabilities = {compiledLayerCount: 8, supportedDomainMask: 15, actionAbiDigest: 0xeb80829c, candidateChunkMax: 20, viaMacroBytes: 7191};
function fixture() {
    const source = document(), target = validateSnapshot(source), events = [];
    const capture = async () => ({document: source, fingerprint: fingerprint(source), summary: summary(source)});
    const operations = {capture, readCandidate: async () => ({state: 0}),
        createCoordinator: () => ({upload: async (bytes, options) => {events.push("stage"); await options.verifyBase(); return {transactionId: 1, metadata: {digest: 42}};}, commit: async () => {events.push("commit");}}),
        writeMacros: async () => events.push("macros"), writeLayout: async () => events.push("layout"), waitStorage: async () => events.push("both halves"),
        readStored: async (connection, command) => command === 0x12 ? target.layout : target.macros};
    const options = {operations, expectedFingerprint: fingerprint(source), saveRecovery: async backup => {assert.equal(fingerprint(backup), fingerprint(source)); events.push("backup"); return "/recovery.json";}};
    return {source, target, events, operations, options};
}
test("restore saves recovery first, restores both stores, then verifies complete readback", async () => {
    const f = fixture(), result = await restoreProfile({}, {}, capabilities, f.source, f.options);
    assert.equal(result.fingerprint, fingerprint(f.source));
    assert.deepEqual(f.events, ["backup", "stage", "commit", "macros", "layout", "both halves"]);
});
test("stale review and failed recovery save perform no device mutations", async () => {
    const f = fixture(); f.options.expectedFingerprint = "stale";
    await assert.rejects(restoreProfile({}, {}, capabilities, f.source, f.options), /changed/); assert.deepEqual(f.events, []);
    f.options.expectedFingerprint = fingerprint(f.source); f.options.saveRecovery = async () => {throw Error("disk full");};
    await assert.rejects(restoreProfile({}, {}, capabilities, f.source, f.options), /disk full/); assert.deepEqual(f.events, []);
});
test("changes after acquiring the profile lease prevent commit", async () => {
    const f = fixture(); let reads = 0;
    f.operations.capture = async () => ({document: f.source, fingerprint: ++reads === 1 ? fingerprint(f.source) : "changed", summary: summary(f.source)});
    await assert.rejects(restoreProfile({}, {}, capabilities, f.source, f.options), /changed/);
    assert.deepEqual(f.events, ["backup", "stage"]);
});
test("an interrupted write reports recovery and cannot report a successful restore", async () => {
    const f = fixture(); f.operations.writeMacros = async () => {throw Error("disconnected");};
    await assert.rejects(restoreProfile({}, {}, capabilities, f.source, f.options), error => error.code === "RESTORE_INCOMPLETE" && error.message.includes("/recovery.json"));
    assert.deepEqual(f.events, ["backup", "stage", "commit"]);
});
test("mismatching final hardware readback fails verification", async () => {
    const f = fixture(); f.operations.readStored = async () => Buffer.alloc(1);
    await assert.rejects(restoreProfile({}, {}, capabilities, f.source, f.options), /readback/);
});
test("recovery retries allow an incomplete base but require a complete final snapshot", async () => {
    const f = fixture(); let reads = 0;
    f.operations.capture = async (connection, ids, caps, progress, candidate, allowIncomplete) => {
        if (++reads < 3) {
            assert.equal(allowIncomplete, true);
            return {document: {format: "charybdis-recovery-capture"}, incomplete: true, fingerprint: "interrupted", summary: null};
        }
        assert.equal(allowIncomplete, undefined);
        return {document: f.source, fingerprint: fingerprint(f.source), summary: summary(f.source)};
    };
    f.options.expectedFingerprint = "interrupted";
    f.options.saveRecovery = async value => {assert.equal(value.format, "charybdis-recovery-capture"); return "/interrupted.diagnostic.json";};
    const restored = await restoreProfile({}, {}, capabilities, f.source, f.options);
    assert.equal(restored.fingerprint, fingerprint(f.source)); assert.equal(reads, 3);
});
test("older firmware is rejected before any device read or write", async () => {
    const connection = {request: () => {throw Error("must not access device");}};
    await assert.rejects(captureProfile(connection, {}, {compiledLayerCount: 5}), error => error.code === "FIRMWARE_UPDATE_REQUIRED");
});
