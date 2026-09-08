"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {ProfileDeviceService} = require("../../core/session/profile-device-service");
const {fingerprint} = require("../../core/model/portable-profile");
const {document} = require("../fixtures/portable-profile");

test("macro saves pass the reviewed identity and recovery writer to the verified restore path", async () => {
    const service = new ProfileDeviceService();
    service.portable = {document: document(), fingerprint: fingerprint(document())};
    const saveRecovery = async () => "/recovery.json";
    let calls = 0;
    service.restorePortableProfile = async (target, options) => {
        calls++;
        assert.equal(options.expectedFingerprint, service.portable.fingerprint);
        assert.equal(options.saveRecovery, saveRecovery);
        assert.equal(Buffer.from(target.macros[0], "base64").toString(), "hello");
        throw Object.assign(new Error("readback mismatch"), {code: "RESTORE_VERIFY_FAILED"});
    };
    const message = {keycode: "VIA_MACRO_0", payload: "hello", expectedFingerprint: service.portable.fingerprint};
    await assert.rejects(service.saveMacroEdit({...message, payload: "{+KC_A}"}, {saveRecovery}), /Release/);
    assert.equal(calls, 0);
    await assert.rejects(service.saveMacroEdit(message, {saveRecovery}), error => error.code === "RESTORE_VERIFY_FAILED");
    assert.equal(calls, 1);
});

test("macro picker names assign the correct native keys and unknown firmware vocabularies are refused", async () => {
    const service = new ProfileDeviceService(), stored = new Map(), writes = [];
    service.connection = {connected: true, request: async request => {
        const reply = Buffer.from(request), position = [...request.subarray(1, 4)].join(":");
        if (request[0] === 5) {stored.set(position, request.readUInt16BE(4)); writes.push(request.readUInt16BE(4));}
        else reply.writeUInt16BE(stored.get(position), 4);
        return reply;
    }};
    service.capabilities = {actionAbiDigest: 0xeb80829c};
    service.layout = {state: "read", layers: [{layer: 0, keys: []}]};
    const groups = [{layer: "Layer 0", changes: [{layoutIndex: 0, keycode: "VIA_MACRO_63"}, {layoutIndex: 1, keycode: "MACRO_15"}]}];
    const result = await service.writeLayoutKeys(groups);
    assert.equal(result.written, 2); assert.deepEqual(writes, [0x773f, 0x7e4f]);
    service.capabilities = {actionAbiDigest: 123};
    const refused = await service.writeLayoutKeys(groups);
    assert.equal(refused.written, 0); assert.equal(refused.rejected.length, 2); assert.equal(writes.length, 2);
});
