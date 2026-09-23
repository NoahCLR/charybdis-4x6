"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {document} = require("../fixtures/portable-profile");
const {document: pdDocument} = require("../fixtures/pd-profile");
const {fingerprint, validateSnapshot} = require("../../core/model/portable-profile");
const {macroEditorView, editMacro} = require("../../core/model/macro-editor");
const {decodeProfileBlob} = require("../../core/schema/profile-blob-v1");
const {buildDeviceModel} = require("../../core/session/device-model");
const snapshot = value => ({document: value, fingerprint: fingerprint(value)});
const settingsDomain = value => decodeProfileBlob(Buffer.from(value.profile, "base64")).domains.find(domain => domain.id === 0x40);

test("the 64 VIA macros populate the model, including empty slots and their names", () => {
    let current = snapshot(pdDocument());
    current = snapshot(editMacro(current, {keycode: "VIA_MACRO_63", payload: "{KC_LGUI,KC_N}", name: "New note", expectedFingerprint: current.fingerprint}));
    const view = macroEditorView(current);
    const model = buildDeviceModel({macroView: view, capabilities: {compiledLayerCount: 8, actionAbiDigest: 0x61072732}});
    assert.equal(model.viaMacros.length, 64);
    assert.equal(model.viaMacros[0].empty, true);
    assert.equal(model.viaMacros[63].name, "New note");
    assert.match(model.viaMacros[63].payload, /KC_N/);
    assert.equal(model.macroEditing.writable, true);
    assert.deepEqual(model.macroNameSpace, {used: 8, shared: 992, perName: 23});
    // A named macro reads by its name wherever a key is labelled; the
    // numeric alias still resolves for slots the shipped catalog lacks.
    assert.equal(model.qmkKeycodeAliases["0x773F"], "VIA_MACRO_63");
    assert.equal(model.qmkKeyLabels["0x773F"], "New note");
    assert.equal(model.qmkKeyLabels.VIA_MACRO_0, "VIA macro 0");
    assert.equal(model.hardcodedMacros, undefined, "user macros are retired");
    assert.equal(buildDeviceModel({macroView: view, capabilities: {compiledLayerCount: 8}, busy: true}).macroEditing.writable, false);
    assert.equal(macroEditorView({incomplete: true}), null);
});

test("naming a macro upgrades settings to v3 and changes nothing else", () => {
    const source = pdDocument(), current = snapshot(source);
    assert.equal(settingsDomain(source).version, 2, "the fixture is a keyboard's stored v2 profile");
    const named = editMacro(current, {keycode: "VIA_MACRO_3", name: "  Zoom mute ", expectedFingerprint: current.fingerprint});
    assert.deepEqual(named.macros, source.macros, "a rename leaves every macro's steps alone");
    const domain = settingsDomain(named);
    assert.equal(domain.version, 3);
    assert.equal(domain.payload[0], 3, "the payload repeats the envelope's version");
    const settings = validateSnapshot(named).settings, before = validateSnapshot(source).settings;
    assert.equal(settings.macroNames[3], "Zoom mute", "names are trimmed");
    assert.deepEqual(settings.values, before.values);
    assert.deepEqual(settings.names, before.names);
    assert.equal(settings.macros, undefined, "the retired user macros are not carried over");
    const before3 = decodeProfileBlob(Buffer.from(source.profile, "base64")).domains, after3 = decodeProfileBlob(Buffer.from(named.profile, "base64")).domains;
    assert.deepEqual(after3.filter(d => d.id !== 0x40), before3.filter(d => d.id !== 0x40), "other domains are byte-identical");
    // Steps and name together, then clearing the name.
    const both = editMacro(snapshot(named), {keycode: "VIA_MACRO_3", payload: "hello", name: "Greeting", expectedFingerprint: fingerprint(named)});
    assert.equal(validateSnapshot(both).settings.macroNames[3], "Greeting");
    assert.equal(Buffer.from(both.macros[3], "base64").toString(), "hello");
    const cleared = editMacro(snapshot(both), {keycode: "VIA_MACRO_3", name: "", expectedFingerprint: fingerprint(both)});
    assert.equal(validateSnapshot(cleared).settings.macroNames[3], "");
    // A steps-only edit keeps the version it read.
    const steps = editMacro(current, {keycode: "VIA_MACRO_4", payload: "hi", expectedFingerprint: current.fingerprint});
    assert.equal(settingsDomain(steps).version, 2);
    assert.deepEqual(source, pdDocument(), "input snapshot is never mutated");
});

test("names respect their per-name limit and the space they share", () => {
    let current = snapshot(pdDocument());
    const edit = (index, name) => editMacro(current, {keycode: `VIA_MACRO_${index}`, name, expectedFingerprint: current.fingerprint});
    assert.throws(() => edit(0, "x".repeat(24)), /23 UTF-8 bytes/);
    assert.throws(() => edit(0, "tab\there"), /control characters/);
    assert.equal(validateSnapshot(edit(0, "é".repeat(11))).settings.macroNames[0], "é".repeat(11), "22 bytes of two-byte characters fit");
    for (let index = 0; index < 43; index++) current = snapshot(edit(index, "x".repeat(23)));
    assert.throws(() => edit(43, "x".repeat(23)), /share 992/, "all 64 cannot be full length");
});

test("stale drafts, wrong slots and retired user macros are rejected before a write", () => {
    const current = snapshot(document());
    const message = {keycode: "VIA_MACRO_0", payload: "hello", expectedFingerprint: current.fingerprint};
    assert.throws(() => editMacro(current, {...message, expectedFingerprint: "stale"}), /changed/);
    for (const keycode of ["VIA_MACRO_64", "VIA_MACRO_01", "KC_A"]) assert.throws(() => editMacro(current, {...message, keycode}), /slot/);
    assert.throws(() => editMacro(current, {...message, keycode: "MACRO_2"}), /retired/);
    assert.throws(() => editMacro(current, {keycode: "VIA_MACRO_0", expectedFingerprint: current.fingerprint}), /steps, its name/);
    assert.throws(() => editMacro(current, {...message, payload: "x".repeat(7191)}), /bytes/);
    assert.throws(() => editMacro(current, {...message, name: "Too early"}), /schema 2/, "a schema-1 profile has no room for names");
});
