"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {document} = require("../fixtures/portable-profile");
const {fingerprint, validateSnapshot} = require("../../core/model/portable-profile");
const {settingsEditorView, editSettings} = require("../../core/model/settings-editor");
const {buildDeviceModel} = require("../../core/session/device-model");
const snapshot = () => {const value = document(); return {document: value, fingerprint: fingerprint(value), limits: {brightnessMax: 200}};};
function message(current, sectionId, updates = {}) {
    const section = settingsEditorView(current).sections.find(section => section.id === sectionId);
    return {sectionId, expectedFingerprint: current.fingerprint, fields: section.fields.map(field => field.kind === "toggle" ? {macro: field.macro, enabled: updates[field.macro] ?? field.enabled} : {macro: field.macro, value: updates[field.macro] ?? field.value})};
}

test("Defaults and inherited behaviour timing come entirely from the complete keyboard snapshot", () => {
    const current = snapshot(), view = settingsEditorView(current);
    const model = buildDeviceModel({settingsView: view, capabilities: {compiledLayerCount: 8}});
    assert.equal(model.configDefaults.length, 10);
    const fields = model.configDefaults.flatMap(section => section.fields);
    assert.equal(fields.find(field => field.macro === "volumeDpi").value, "0");
    assert.equal(fields.find(field => field.macro === "mouseLayer").value, "Layer 4");
    assert.equal(fields.find(field => field.macro === "brightness").value, "200");
    assert.equal(model.behaviorTimingDefaults.tapHoldTerm, "150");
    assert.equal(model.settingsEditing.writable, true);
    assert.equal(buildDeviceModel({settingsView: view, capabilities: {compiledLayerCount: 5}}).settingsEditing.writable, false);
    assert.equal(buildDeviceModel({settingsView: view, capabilities: {compiledLayerCount: 8}, busy: true}).settingsEditing.writable, false);
    assert.equal(settingsEditorView({incomplete: true}), null);
    assert.deepEqual(buildDeviceModel().configDefaults, []);
});

test("every unchanged section is a byte-exact round trip and edits preserve unrelated domains and banks", () => {
    const current = snapshot();
    for (const section of settingsEditorView(current).sections) assert.deepEqual(editSettings(current, message(current, section.id)), current.document);
    const next = editSettings(current, message(current, "autoMouse", {mouseTimeout: "900", mouseFadeDelay: "300", mouseLayer: "Layer 7", autoMouse: false}));
    const before = validateSnapshot(current.document), after = validateSnapshot(next);
    assert.deepEqual({...next, profile: current.document.profile}, current.document);
    assert.deepEqual(after.rgb, before.rgb); assert.deepEqual(after.behaviors, before.behaviors); assert.deepEqual(after.combos, before.combos);
    assert.deepEqual(after.settings.names, before.settings.names); assert.deepEqual(after.settings.macros, before.settings.macros);
    assert.deepEqual(after.settings.values.map((v, id) => [4,5,6,16].includes(id) ? before.settings.values[id] : v), before.settings.values);
    assert.equal(after.settings.values[4], 0); assert.equal(after.settings.values[5], 7); assert.equal(after.settings.values[6], 900); assert.equal(after.settings.values[16], 300);
    assert.deepEqual(current.document, document(), "source snapshot is not mutated");
});

test("base lighting edits preserve the effect and flags while independently updating packed channels", () => {
    const current = snapshot(), before = validateSnapshot(current.document).settings.values;
    const next = editSettings(current, message(current, "rgbAppearance", {hue: "23", saturation: "0", brightness: "0", effectSpeed: "255", lightingEnabled: false, lightingTimeout: "0"}));
    const values = validateSnapshot(next).settings.values;
    assert.equal(values[21] & 0xff00ff00, before[21] & 0xff00ff00);
    assert.equal(values[21] & 255, 0); assert.equal((values[21] >>> 16) & 255, 255);
    assert.equal(values[22], 23); assert.equal(values[17], 0);
});

test("invalid settings, incomplete sections and stale drafts fail before any device write", () => {
    const current = snapshot(), edit = (section, fields) => editSettings(current, message(current, section, fields));
    assert.throws(() => editSettings(current, {...message(current, "autoMouse"), expectedFingerprint: "old"}), /changed/);
    assert.throws(() => edit("autoMouse", {mouseTimeout: "400", mouseFadeDelay: "400"}), /timeout.*longer/);
    assert.throws(() => edit("autoMouse", {mouseDebounce: "256"}), /debounce.*range/);
    assert.throws(() => edit("autoMouse", {mouseLayer: "Layer 8"}), /layer/);
    assert.throws(() => edit("keyTiming", {combosEnabled: "false"}), /enabled or disabled/);
    for (const value of ["", "1.5", "Infinity", "-1", "65536", " 150 "]) assert.throws(() => edit("keyTiming", {tapHoldTerm: value}));
    assert.throws(() => edit("normalPointerSpeed", {normalDpi: "500"}), /range/);
    assert.throws(() => edit("lightingFeedback", {feedbackFlash: "0"}), /range/);
    assert.throws(() => edit("rgbAppearance", {lightingTimeout: "86400001"}), /range/);
    const invalid = message(current, "keyTiming"); invalid.fields[0] = invalid.fields[1];
    assert.throws(() => editSettings(current, invalid), /repeated/);
    assert.throws(() => editSettings(current, {...invalid, fields: []}), /complete/);
});


test("brightness uses a reported device limit and stays read-only on older firmware", () => {
    const current = snapshot();
    assert.throws(() => editSettings(current, message(current, "rgbAppearance", {brightness: "201"})), /reported limit/);
    delete current.limits;
    assert.equal(settingsEditorView(current).sections.flatMap(section => section.fields).find(field => field.macro === "brightness").readOnly, true);
    assert.throws(() => editSettings(current, message(current, "rgbAppearance", {brightness: "100"})), /brightness limit/);
    assert.deepEqual(editSettings(current, message(current, "rgbAppearance")), current.document);
});

function withSettings(change) {
    const current = snapshot();
    const {decodeProfileBlob, encodeProfileBlob} = require("../../core/schema/profile-blob-v1");
    const {encodeSettings} = require("../../core/schema/settings-domain-v1");
    const value = validateSnapshot(current.document); change(value.settings);
    current.document.profile = encodeProfileBlob({domains: decodeProfileBlob(value.profile).domains.map(domain => domain.id === 0x40 ? {...domain, payload: encodeSettings(value.settings)} : domain)}).toString("base64");
    current.fingerprint = fingerprint(current.document);
    current.options = require("../fixtures/keyboard-options").options();
    return current;
}
test("all native controls preserve a complete profile unchanged, including unknown reserved option bits", () => {
    const current = withSettings(settings => {settings.values[24] = 0x8004; settings.values[21] = 0xff1e0101;});
    for (const section of settingsEditorView(current).sections) assert.deepEqual(editSettings(current, message(current, section.id)), current.document);
    const fields = settingsEditorView(current).sections.flatMap(section => section.fields);
    assert.equal(fields.find(field => field.macro === "swapLeftAltGui").enabled, true);
    assert.equal(fields.find(field => field.macro === "autocorrect").readOnly, true);
    assert.equal(fields.find(field => field.macro === "effectMode").choices[1].label, "Breathing");
    const next = editSettings(current, message(current, "keyboardOptions", {swapLeftAltGui:false, swapControlCaps:true}));
    assert.equal(validateSnapshot(next).settings.values[24], 0x8001);
    assert.throws(() => editSettings(current, message(current, "keyboardOptions", {autocorrect:true})), /not enabled/);
});
test("startup layers validate the final mask, and combo references preserve untouched nibbles", () => {
    const current = withSettings(() => {});
    const next = editSettings(current, message(current, "startupLayers", {startupLayer0:false, startupLayer7:true}));
    assert.equal(validateSnapshot(next).settings.values[23], 128);
    assert.throws(() => editSettings(current, message(current, "startupLayers", {startupLayer0:false})), /at least one/);
    const combos = editSettings(current, message(current, "comboReferences", {comboReference7:"Layer 0", comboReference2:"Layer 1"}));
    assert.equal(validateSnapshot(combos).settings.values[27], 0x06543110);
    assert.throws(() => editSettings(current, message(current, "comboReferences", {comboReference0:"Layer 8"})), /layer/);
});
test("lighting choices only allow reported effects and LED classes, while preserving other bytes", () => {
    const current = withSettings(settings => {settings.values[21] = 0xff1e0101;});
    const next = editSettings(current, message(current, "rgbAppearance", {effectMode:"2", effectLeds:"4"}));
    assert.equal(validateSnapshot(next).settings.values[21], 0x041e0201);
    assert.throws(() => editSettings(current, message(current, "rgbAppearance", {effectMode:"4"})), /range/);
    assert.throws(() => editSettings(current, message(current, "rgbAppearance", {effectLeds:"2"})), /range/);
    delete current.options;
    assert.throws(() => editSettings(current, message(current, "rgbAppearance", {effectMode:"2"})));
});
test("key options use masks supplied by the keyboard rather than assuming bitfield layout", () => {
    const current = withSettings(settings => {settings.values[24] = 0x8000;});
    current.options.keymapMasks[0] = 0x8000;
    const field = settingsEditorView(current).sections.flatMap(section => section.fields).find(field => field.macro === "swapControlCaps");
    assert.equal(field.enabled, true);
    const next = editSettings(current, message(current, "keyboardOptions", {swapControlCaps:false}));
    assert.equal(validateSnapshot(next).settings.values[24], 0);
});
