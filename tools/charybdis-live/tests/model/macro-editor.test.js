"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {document} = require("../fixtures/portable-profile");
const {fingerprint, validateSnapshot} = require("../../core/model/portable-profile");
const {macroEditorView, editMacro} = require("../../core/model/macro-editor");
const {decodeProfileBlob} = require("../../core/schema/profile-blob-v1");
const {buildDeviceModel} = require("../../core/session/device-model");
const snapshot = value => ({document: value, fingerprint: fingerprint(value)});

test("both device macro banks populate the existing Studio model, including empty slots", () => {
    let current = snapshot(document());
    for (const keycode of ["VIA_MACRO_63", "MACRO_15"]) current = snapshot(editMacro(current, {keycode, payload: "{KC_LGUI,KC_N}", expectedFingerprint: current.fingerprint}));
    const view = macroEditorView(current);
    const model = buildDeviceModel({macroView: view, capabilities: {compiledLayerCount: 8, actionAbiDigest: 0xeb80829c}});
    assert.equal(model.viaMacros.length, 64); assert.equal(model.hardcodedMacros.length, 16);
    assert.equal(model.viaMacros[0].empty, true); assert.equal(model.hardcodedMacros[15].empty, false);
    assert.match(model.hardcodedMacros[15].payload, /KC_N/);
    assert.equal(model.macroEditing.writable, true);
    // The shipped QMK catalog names only the first 32 macro keys. The board's
    // additional slots must also resolve through their actual numeric names.
    assert.equal(model.qmkKeycodeAliases["0x773F"], "VIA_MACRO_63");
    assert.equal(model.qmkKeyLabels["0x773F"], "VIA macro 63");
    assert.equal(model.qmkKeycodeAliases.QK_USER_15, "MACRO_15");
    assert.equal(model.qmkKeyLabels.QK_USER_15, "User macro 15");
    assert.equal(buildDeviceModel({macroView: view, capabilities: {compiledLayerCount: 8}, busy: true}).macroEditing.writable, false);
    assert.equal(buildDeviceModel({macroView: view, capabilities: {compiledLayerCount: 5}}).macroEditing.writable, false);
    assert.equal(macroEditorView({incomplete: true}), null);
});

test("editing or clearing one slot preserves all other profile data and macro bytes", () => {
    const source = document(), current = snapshot(source);
    const via = editMacro(current, {keycode: "VIA_MACRO_3", payload: "hello", expectedFingerprint: current.fingerprint});
    assert.deepEqual({...via, macros: source.macros}, source);
    assert.equal(via.macros.filter(Boolean).length, 1);
    const user = editMacro(snapshot(via), {keycode: "MACRO_2", payload: "{KC_LGUI,KC_N}", expectedFingerprint: fingerprint(via)});
    assert.deepEqual({...user, profile: via.profile}, via);
    const before = decodeProfileBlob(Buffer.from(via.profile, "base64")).domains;
    const after = decodeProfileBlob(Buffer.from(user.profile, "base64")).domains;
    assert.deepEqual(after.slice(0, 3), before.slice(0, 3));
    const settings = validateSnapshot(user).settings, oldSettings = validateSnapshot(via).settings;
    assert.deepEqual({...settings, macros: oldSettings.macros}, oldSettings);
    assert.equal(settings.macros.filter(bytes => bytes.length).length, 1);
    const cleared = editMacro(snapshot(user), {keycode: "MACRO_2", payload: "", expectedFingerprint: fingerprint(user)});
    assert.deepEqual(cleared, via);
    assert.deepEqual(source, document(), "input snapshot is never mutated");
});

test("stale drafts, wrong slots and bank overflow are rejected before a write", () => {
    const current = snapshot(document());
    const message = {keycode: "VIA_MACRO_0", payload: "hello", expectedFingerprint: current.fingerprint};
    assert.throws(() => editMacro(current, {...message, expectedFingerprint: "stale"}), /changed/);
    for (const keycode of ["VIA_MACRO_64", "MACRO_16", "VIA_MACRO_01", "KC_A"]) assert.throws(() => editMacro(current, {...message, keycode}), /slot/);
    assert.throws(() => editMacro(current, {...message, payload: "x".repeat(7191)}), /bytes/);
});
