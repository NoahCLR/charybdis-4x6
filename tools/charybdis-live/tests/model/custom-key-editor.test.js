"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {document: pdDocument} = require("../fixtures/pd-profile");
const {fingerprint, validateSnapshot} = require("../../core/model/portable-profile");
const {customKeyEditorView, editCustomKey} = require("../../core/model/custom-key-editor");
const {macroEditorView, editMacro} = require("../../core/model/macro-editor");
const {profileReview} = require("../../core/model/profile-review");
const {revertUnits} = require("../../core/model/profile-revert");
const {buildDeviceModel} = require("../../core/session/device-model");
const {macroNamesOf} = require("../../core/schema/settings-domain-v1");
const capabilities = {compiledLayerCount: 8, supportedDomainMask: 31, actionAbiDigest: 0x1d3fcacc};
const snapshot = value => ({document: value, fingerprint: fingerprint(value)});

test("all 64 custom keys are listed, and a name takes the settings to v5 without touching anything else", () => {
    const before = snapshot(pdDocument());
    const view = customKeyEditorView(before, capabilities);
    assert.equal(view.keys.length, 64);
    assert.deepEqual(view.keys[2], {slot: 2, keycode: "CUSTOM_KEY_2", code: 0x7e42, name: "", hasBehavior: true}, "the compiled profile gives Click Spam a behaviour");
    assert.deepEqual(view.names, {perName: 20});
    const named = editCustomKey(before, {keycode: "CUSTOM_KEY_2", name: " Click Spam ", expectedFingerprint: before.fingerprint}, capabilities);
    const after = validateSnapshot(named, capabilities);
    assert.equal(after.settings.formatVersion, 5);
    assert.equal(after.settings.customKeyNames[2], "Click Spam", "trimmed");
    assert.deepEqual(macroNamesOf(after.settings), macroNamesOf(validateSnapshot(before.document).settings), "the macros keep their names");
    assert.deepEqual(named.layers, before.document.layers);
    const [item] = profileReview(before, snapshot(named)).filter(row => row.unit === "customKey:2");
    assert.equal(item.title, "Custom key 2 · Click Spam");
    assert.equal(item.fields.find(field => field.label === "Name").after, "Click Spam");
    assert.deepEqual(revertUnits(before, snapshot(named), new Set(["customKey:2"]), capabilities).layers, before.document.layers);
    assert.equal(validateSnapshot(revertUnits(before, snapshot(named), new Set(["customKey:2"]), capabilities)).settings.customKeyNames[2], "", "discarded alone");
    const model = buildDeviceModel({customKeyView: customKeyEditorView(snapshot(named), capabilities), capabilities});
    assert.equal(model.customKeys[2].name, "Click Spam");
    assert.equal(model.qmkKeyLabels.CUSTOM_KEY_2, "Click Spam", "it reads by its name wherever a key is labelled");
    assert.equal(model.customKeyEditing.writable, true);
});

test("a macro name edit keeps the v5 custom-key names", () => {
    let current = snapshot(pdDocument());
    current = snapshot(editCustomKey(current, {keycode: "CUSTOM_KEY_0", name: "Right Thumb", expectedFingerprint: current.fingerprint}, capabilities));
    current = snapshot(editMacro(current, {keycode: "VIA_MACRO_3", name: "OCR Copy", expectedFingerprint: current.fingerprint}, capabilities));
    const settings = validateSnapshot(current.document).settings;
    assert.equal(settings.formatVersion, 5);
    assert.equal(settings.customKeyNames[0], "Right Thumb");
    assert.equal(macroEditorView(current).viaMacros[3].name, "OCR Copy");
});

test("an imported profile without custom-key names can take one name back from the keyboard", () => {
    let keyboard = snapshot(pdDocument());
    keyboard = snapshot(editCustomKey(keyboard, {keycode: "CUSTOM_KEY_0", name: "Right Thumb", expectedFingerprint: keyboard.fingerprint}, capabilities));
    keyboard = snapshot(editCustomKey(keyboard, {keycode: "CUSTOM_KEY_1", name: "Left Thumb", expectedFingerprint: keyboard.fingerprint}, capabilities));
    const imported = snapshot(pdDocument());
    assert.equal(validateSnapshot(imported.document).settings.formatVersion, 2, "a backup from before the custom keys");
    assert.deepEqual(profileReview(keyboard, imported).filter(row => row.unit.startsWith("customKey:")).map(row => row.unit), ["customKey:0", "customKey:1"]);
    const kept = snapshot(revertUnits(keyboard, imported, new Set(["customKey:0"]), capabilities));
    const settings = validateSnapshot(kept.document, capabilities).settings;
    assert.equal(settings.formatVersion, 5);
    assert.deepEqual(settings.customKeyNames.slice(0, 2), ["Right Thumb", ""]);
    assert.deepEqual(macroNamesOf(settings), macroNamesOf(validateSnapshot(imported.document).settings), "the imported macro names stay");
    assert.deepEqual(profileReview(keyboard, kept).filter(row => row.unit.startsWith("customKey:")).map(row => row.unit), ["customKey:1"]);
});

test("wrong keys, stale drafts, long names and older firmware are refused", () => {
    const current = snapshot(pdDocument());
    const message = {keycode: "CUSTOM_KEY_0", name: "Thumb", expectedFingerprint: current.fingerprint};
    assert.throws(() => editCustomKey(current, {...message, expectedFingerprint: "stale"}, capabilities), /changed/);
    for (const keycode of ["CUSTOM_KEY_64", "CUSTOM_KEY_01", "VIA_MACRO_0", "KC_A"]) assert.throws(() => editCustomKey(current, {...message, keycode}, capabilities), /custom key/);
    assert.throws(() => editCustomKey(current, {...message, name: "x".repeat(21)}, capabilities), /20 plain characters/);
    assert.throws(() => editCustomKey(current, {...message, name: "Café"}, capabilities), /plain characters/);
    const older = {...capabilities, actionAbiDigest: 0x61072732};
    assert.equal(customKeyEditorView(current, older), null, "no custom keys on firmware without the keycode blocks");
    assert.throws(() => editCustomKey(current, message, older), /keycode blocks/);
    assert.equal(editCustomKey(current, {...message, name: ""}, capabilities), validateSnapshot(current.document, capabilities).document, "an unchanged name is no edit");
});

test("a name that meets the profile's ceiling says the profile is full", () => {
    const {encodeNamedProfile} = require("../../core/model/portable-profile");
    assert.throws(() => encodeNamedProfile({schema: {major: 2, minor: 0}, domains: [{id: 0x40, version: 5, payload: Buffer.alloc(5100)}]}),
        error => error.code === "PROFILE_FULL" && /profile is full/.test(error.message));
});
