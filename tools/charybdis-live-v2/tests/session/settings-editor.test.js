"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {ProfileDeviceService} = require("../../core/session/profile-device-service");
const {settingsEditorView} = require("../../core/model/settings-editor");
const {fingerprint, validateSnapshot} = require("../../core/model/portable-profile");
const {document} = require("../fixtures/portable-profile");
const {wire} = require("../fixtures/keyboard-options");
test("Defaults saves use complete-profile recovery and propagate restore verification failures", async () => {
    const service = new ProfileDeviceService();
    service.portable = {document: document(), fingerprint: fingerprint(document())};
    const section = settingsEditorView(service.portable).sections.find(section => section.id === "normalPointerSpeed");
    const message = {sectionId: section.id, expectedFingerprint: service.portable.fingerprint, fields: section.fields.map(field => ({macro: field.macro, value: field.macro === "normalDpi" ? "1600" : field.value}))};
    const saveRecovery = () => {}, calls = [];
    service.restorePortableProfile = async (target, options) => {calls.push({target, options}); throw Error("readback mismatch");};
    await assert.rejects(service.saveSettingsEdit(message, {saveRecovery}), /readback mismatch/);
    assert.equal(calls.length, 1); assert.equal(calls[0].options.saveRecovery, saveRecovery);
    assert.equal(calls[0].options.expectedFingerprint, message.expectedFingerprint);
    assert.equal(validateSnapshot(calls[0].target).settings.values[18], 1600);
    await assert.rejects(service.saveSettingsEdit({...message, expectedFingerprint: "stale"}), /changed/);
    assert.equal(calls.length, 1);
});


test("a profile exceeding the connected keyboard's brightness limit is refused before any writes", async () => {
    const service = new ProfileDeviceService(), requests = [];
    service.connection = {connected: true, request: async request => {
        requests.push(request); assert.equal(request[0], 8); assert.equal(request[2], 8); assert.equal(request[4], 1);
        const reply = Buffer.alloc(32); request.copy(reply, 0, 0, 5); reply[6] = 2; reply.set([1,100],7); return reply;
    }};
    service.requestIds = {next: () => 1};
    service.capabilities = {compiledLayerCount:8, supportedDomainMask:15, actionAbiDigest: document().actionAbiDigest};
    await assert.rejects(service.restorePortableProfile(document(), {saveRecovery: () => {throw Error("must not start restore");}}), /brightness.*limit of 100/);
    assert.equal(requests.length, 1);
});

test("an unavailable lighting effect is refused before recovery or profile staging", async () => {
    const service = new ProfileDeviceService(), requests = [], options = wire();
    // The source uses effect 2; the destination only supports effect 1.
    const {editSettings} = require("../../core/model/settings-editor");
    const source = {document: document(), fingerprint: fingerprint(document()), options: require("../fixtures/keyboard-options").options()};
    const section = settingsEditorView(source).sections.find(item => item.id === "rgbAppearance");
    const target = editSettings(source, {sectionId: section.id, expectedFingerprint: source.fingerprint,
        fields: section.fields.map(field => ({macro: field.macro, value: field.macro === "effectMode" ? "2" : field.value, enabled: field.enabled}))});
    options.bytes = options.bytes.subarray(0, 26 + 64); options.metadata[4] = 1; options.metadata.writeUInt16LE(options.bytes.length, 2);
    service.connection = {connected: true, request: async request => {
        requests.push(request); assert.equal(request[0], 8); assert.equal(request[2], 8);
        const page = request[4], offset = (page - 3) * 25;
        const payload = page === 1 ? Buffer.from([1,255]) : page === 2 ? options.metadata : options.bytes.subarray(offset, offset + 25);
        const reply = Buffer.alloc(32); request.copy(reply, 0, 0, 5); reply[6] = payload.length; payload.copy(reply, 7); return reply;
    }};
    service.requestIds = {next: () => 1};
    service.capabilities = {compiledLayerCount:8, supportedDomainMask:15, actionAbiDigest: document().actionAbiDigest};
    await assert.rejects(service.restorePortableProfile(target, {saveRecovery: () => {throw Error("must not start restore");}}), /lighting effect unavailable/);
    assert.equal(requests.length, 7);
});
