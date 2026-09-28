"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {ProfileDeviceService} = require("../../core/session/profile-device-service");
const {settingsEditorView} = require("../../core/model/settings-editor");
const {fingerprint} = require("../../core/model/portable-profile");
const {legacyDocument: document} = require("../fixtures/portable-profile");
const {wire} = require("../fixtures/keyboard-options");
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

test("a restore that starts re-reads the keyboard's status, even when it fails", async () => {
    const service = new ProfileDeviceService();
    const base = {document: document(), fingerprint: fingerprint(document()), limits: {brightnessMax: 255}, options: require("../fixtures/keyboard-options").options()};
    service.portable = base;
    service.capabilities = {compiledLayerCount:8, supportedDomainMask:15, actionAbiDigest: document().actionAbiDigest};
    service.requestIds = {next: () => 1};
    service.connection = {connected: true, request: async () => {throw Object.assign(Error("link dropped mid-apply"), {code: "TIMEOUT"});}};
    let refreshed = 0;
    service.refreshStatus = async () => {refreshed++;};
    await assert.rejects(service.restorePortableProfile(document(), {expectedFingerprint: base.fingerprint, saveRecovery: () => ({})}));
    assert.equal(refreshed, 1, "the health strip is read again rather than left at its pre-apply value");
});

test("a profile with a misplaced action is refused, naming it, before recovery or any write", async () => {
    const {decodeProfileBlob, encodeProfileBlob, PROFILE_DOMAIN_IDS} = require("../../core/schema/profile-blob-v1");
    const {encodeComboDomain} = require("../../core/schema/combo-domain-v1");
    const service = new ProfileDeviceService();
    const base = {document: document(), fingerprint: fingerprint(document()), limits: {brightnessMax: 255}, options: require("../fixtures/keyboard-options").options()};
    const blob = decodeProfileBlob(Buffer.from(base.document.profile, "base64"));
    const code = operand => ({kind: 1, flags: 0, operand});
    const combos = {id: PROFILE_DOMAIN_IDS.COMBOS, version: 1, payload: encodeComboDomain({version: 1, defaultTermMs: null, holdTermMs: 0, rows: [{inputs: [code(0x04), code(0x05)], output: code(0x4104), termMs: 0, mustHold: false, mustTap: false, ordered: false}]}, {actionLimits: {maxPdModes: blob.schema.major === 2 ? 8 : 6}})};
    const target = {...base.document, profile: encodeProfileBlob({...blob, domains: [...blob.domains.filter(domain => domain.id !== PROFILE_DOMAIN_IDS.COMBOS), combos]}).toString("base64")};
    let writes = 0;
    service.portable = base;
    service.capabilities = {compiledLayerCount: 8, supportedDomainMask: 15, actionAbiDigest: document().actionAbiDigest};
    service.requestIds = {next: () => 1};
    service.connection = {connected: true, request: async () => {writes++; throw Error("must not reach the keyboard");}};
    await assert.rejects(service.restorePortableProfile(target, {expectedFingerprint: base.fingerprint, saveRecovery: () => {throw Error("must not start restore");}}), /Combo 0: LT\(1,KC_A\) makes its own tap\/hold decision/);
    assert.equal(writes, 0);
});
