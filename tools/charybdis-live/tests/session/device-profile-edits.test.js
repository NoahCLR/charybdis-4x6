"use strict";
const assert = require("node:assert/strict");
const test = require("node:test");
const {editDeviceProfile} = require("../../core/session/device-profile-edits");
const {decodeProfileBlob} = require("../../core/schema/profile-blob-v1");
const {decodeRgbDomainV1} = require("../../core/schema/rgb-domain-v1");
const {bytes} = require("../fixtures/device-profile");
const rgb = blob => decodeRgbDomainV1(decodeProfileBlob(blob).domains[0].payload);

test("RGB edits replace the selected fields and retain all behaviour bytes", () => {
    const edited = editDeviceProfile(bytes, {type: "updateLayerColor", layer: "Layer 0", hue: "0", sat: "255", val: "120", mode: "KEYS_MAPPED_ON_THIS_LAYER_ONLY"});
    assert.deepEqual(decodeProfileBlob(edited).domains[1].payload, decodeProfileBlob(bytes).domains[1].payload);
    const expected = rgb(bytes);
    expected.layerColors[0].color = {h: 0, s: 255, v: 120}; expected.layerColors[0].mode = 1;
    assert.deepEqual(rgb(edited), expected);
});

test("RGB policies preserve zero and reject missing or malformed values", () => {
    const edited = editDeviceProfile(bytes, {type: "updateRgbStages", stageEnableMask: 0});
    assert.equal(rgb(edited).stageEnableMask, 0);
    const fade = editDeviceProfile(bytes, {type: "updateAutomouseFade", mode: "END_COLOR_ON_ALL_KEYS", hue: 1, sat: 0, val: 0});
    assert.deepEqual(rgb(fade).automouseFade, {mode: 2, endColor: {h: 1, s: 0, v: 0}});
    for (const hue of ["", "bad", null, -1, 256]) assert.throws(() => editDeviceProfile(bytes, {type: "updateLayerColor", layer: "Layer 0", hue, sat: 0, val: 0, mode: "ALL_KEYS"}));
});

test("LED membership edits preserve references and refuse deleting an assigned group", () => {
    const next = editDeviceProfile(bytes, {type: "saveRgbReusableLedGroup", group: {originalName: "Group 0", ledIndices: [0, 1, 57]}});
    assert.deepEqual(rgb(next).groups[0].leds, [0, 1, 57]);
    assert.throws(() => editDeviceProfile(bytes, {type: "deleteRgbReusableLedGroup", name: "Group 0"}), /assignments/);
    assert.throws(() => editDeviceProfile(bytes, {type: "saveRgbReusableLedGroup", group: {originalName: "Group 0", ledIndices: [58]}}));
});

const {decodeComboDomain} = require("../../core/schema/combo-domain-v1");
const {decodeComboPages} = require("../../core/protocol/combo-readback-v1");
const {fixturePages} = require("../fixtures/device-combos");
const {assertEffectiveCombos} = require("../../core/session/device-profile-edits");
const readOf = pages => ({state: "read", ...decodeComboPages(pages[0], pages.slice(1))});
const pages = fixturePages();
const context = {capabilities: {supportedDomainMask: 7, actionAbiDigest: 0xdcb00959}, combos: readOf(pages)};
const comboTable = bytes => {const domain = decodeProfileBlob(bytes).domains.find(row => row.id === 0x30); return decodeComboDomain(domain.payload, domain.version);};
const comboRows = bytes => comboTable(bytes).rows;
test("adding, editing and deleting combos preserves every untouched profile domain and combo", () => {
    const added = editDeviceProfile(bytes, {type: "addCombo", inputs: ["KC_A", "KC_B"], output: "LGUI(KC_C)", termMs: "25", ordered: true}, context);
    const rows = comboRows(added);
    assert.equal(rows.length, 3);
    assert.deepEqual(rows[0].inputs.map(action => action.operand), context.combos.rows[0].inputs);
    // The keyboard's combos keep following its default; the new one has its own.
    assert.deepEqual(rows.map(row => row.termMs), [null, null, 25]);
    assert.equal(rows[2].output.operand, 0x806);
    for (let id = 0; id < 2; id++) assert.deepEqual(decodeProfileBlob(added).domains[id].payload, decodeProfileBlob(bytes).domains[id].payload);
    // A combo holds its output, so a layer hold works there; LT() does not.
    assert.throws(() => editDeviceProfile(added, {type: "saveCombo", id: 2, inputs: ["KC_C", "KC_D"], output: "LT(1,KC_A)", termMs: "30"}, context), /Combo 2: LT\(1,KC_A\) makes its own tap\/hold decision/);
    const edited = editDeviceProfile(added, {type: "saveCombo", id: 2, inputs: ["KC_C", "KC_D"], output: "MO(2)", termMs: "30", mustHold: true}, context);
    // QMK never fires a combo with a 0 ms window, so none is stored.
    assert.throws(() => editDeviceProfile(added, {type: "saveCombo", id: 2, inputs: ["KC_C", "KC_D"], output: "KC_E", termMs: "0"}, context), /at least 1 ms/);
    assert.deepEqual(comboRows(edited)[2].output, {kind: 2, flags: 0, operand: 2});
    assert.equal(comboRows(edited)[2].mustHold, true);
    const timed = editDeviceProfile(edited, {type: "updateComboHoldTerm", holdTermMs: 400}, context);
    assert.equal(comboTable(timed).holdTermMs, 400);
    const removed = editDeviceProfile(timed, {type: "deleteCombo", id: 1}, context);
    assert.equal(comboRows(removed).length, 2);
    assert.equal(comboRows(removed)[1].output.kind, 2);
    const rgbChanged = editDeviceProfile(removed, {type: "updateRgbStages", stageEnableMask: 0});
    assert.deepEqual(comboRows(rgbChanged), comboRows(removed));
});
test("a combo follows the default window until it has its own, and the default is one edit", () => {
    const added = editDeviceProfile(bytes, {type: "addCombo", inputs: ["KC_A", "KC_B"], output: "KC_C", termMs: ""}, context);
    assert.equal(comboRows(added)[2].termMs, null);
    const custom = editDeviceProfile(added, {type: "saveCombo", id: 2, inputs: ["KC_A", "KC_B"], output: "KC_C", termMs: "80"}, context);
    assert.equal(comboRows(custom)[2].termMs, 80);
    const back = editDeviceProfile(custom, {type: "saveCombo", id: 2, inputs: ["KC_A", "KC_B"], output: "KC_C", termMs: "80", followsDefault: true}, context);
    assert.equal(comboRows(back)[2].termMs, null);
    const retimed = editDeviceProfile(custom, {type: "updateComboDefaultTerm", defaultTermMs: "70"}, context);
    assert.equal(comboTable(retimed).defaultTermMs, 70);
    // Only the combos that follow the default move with it.
    assert.deepEqual(comboRows(retimed).map(row => row.termMs), [null, null, 80]);
    for (const defaultTermMs of ["0", "", "abc", 65536]) assert.throws(() => editDeviceProfile(custom, {type: "updateComboDefaultTerm", defaultTermMs}, context));
    // With every combo deleted, both combo-wide values stay stored.
    let empty = retimed;
    for (let id = 2; id >= 0; id--) empty = editDeviceProfile(empty, {type: "deleteCombo", id}, context);
    assert.deepEqual(comboTable(empty), {version: 2, defaultTermMs: 70, holdTermMs: 200, rows: []});
    assert.equal(comboTable(editDeviceProfile(empty, {type: "updateComboHoldTerm", holdTermMs: 250}, context)).holdTermMs, 250);
});
test("a keyboard without a stored default keeps every window explicit until a combo edit can upgrade it", () => {
    const old = {...context, combos: readOf(fixturePages(1))};
    const added = editDeviceProfile(bytes, {type: "addCombo", inputs: ["KC_A", "KC_B"], output: "KC_C", termMs: "30"}, old);
    assert.equal(comboTable(added).version, 1);
    assert.deepEqual(comboRows(added).map(row => row.termMs), [50, 50, 30]);
    assert.throws(() => editDeviceProfile(bytes, {type: "addCombo", inputs: ["KC_A", "KC_B"], output: "KC_C", termMs: ""}, old), /enter one/);
    assert.throws(() => editDeviceProfile(bytes, {type: "updateComboDefaultTerm", defaultTermMs: 70}, old), /no default combo window/);
    // A table from before the default, on a keyboard that stores one, takes
    // the keyboard's default and keeps its own windows.
    const upgraded = editDeviceProfile(added, {type: "deleteCombo", id: 0}, {...context, comboDefaults: {defaultTermMs: 60, holdTermMs: 150}});
    assert.deepEqual(comboTable(upgraded), {...comboTable(added), version: 2, defaultTermMs: 60, rows: comboRows(added).slice(1).map((row, id) => ({...row, id}))});
});
test("Cmd+N from the combo picker saves and verifies as a standard modified key", () => {
    const message = {type: "saveCombo", id: 0, inputs: ["G(KC_C)", "G(KC_V)"], output: "G(KC_N)", termMs: "50"};
    // Standard shortcuts do not depend on the keyboard's custom action ABI.
    const edited = editDeviceProfile(bytes, message, {...context, capabilities: {supportedDomainMask: 7, actionAbiDigest: 0}});
    const row = comboRows(edited)[0];
    assert.deepEqual(row.output, {kind: 1, flags: 0, operand: 0x0811});
    assert.deepEqual(row.inputs.map(action => action.operand), [0x0806, 0x0819]);
    const read = {...context.combos, rows: context.combos.rows.map((row, id) => id ? row : {...row, inputs: [0x0806, 0x0819], output: 0x0811, termMs: 50, followsDefault: false, mustHold: false, mustTap: false, ordered: false})};
    assert.doesNotThrow(() => assertEffectiveCombos(edited, read));
    // A window equal to the default is still its own: it does not follow a changed default.
    assert.throws(() => assertEffectiveCombos(edited, context.combos), /running combo/);
    assert.deepEqual(comboRows(edited)[1], comboRows(editDeviceProfile(bytes, {type: "updateComboHoldTerm", holdTermMs: 200}, context))[1]);
    for (const original of decodeProfileBlob(bytes).domains) {
        assert.deepEqual(decodeProfileBlob(edited).domains.find(domain => domain.id === original.id).payload, original.payload);
    }
});

test("combo edits refuse unsupported firmware, incomplete reads, invalid inputs and opaque callbacks", () => {
    const message = {type: "addCombo", inputs: ["KC_A", "KC_A"], output: "KC_TAB", termMs: 50};
    assert.throws(() => editDeviceProfile(bytes, message, {capabilities: {supportedDomainMask: 3}}), /firmware/);
    assert.throws(() => editDeviceProfile(bytes, message, {...context, combos: null}), /Read/);
    assert.throws(() => editDeviceProfile(bytes, message, context), /distinct/);
    assert.throws(() => editDeviceProfile(bytes, {...message, inputs: ["KC_A", "KC_B"], mustHold: true, mustTap: true}, context), /both/);
    assert.throws(() => editDeviceProfile(bytes, {...message, inputs: ["KC_A", "KC_B"], output: "KC_NO"}, context), /callback/);
    assert.throws(() => editDeviceProfile(bytes, {type: "deleteCombo", id: 99}, context), /index/);
});
test("save verification requires the running combo table to match the saved domain", () => {
    const next = editDeviceProfile(bytes, {type: "updateComboHoldTerm", holdTermMs: 300}, context);
    assert.throws(() => assertEffectiveCombos(next, context.combos), /running combo/);
    assert.throws(() => assertEffectiveCombos(next, null), /could not be verified/);
    assert.doesNotThrow(() => assertEffectiveCombos(next, {...context.combos, holdTermMs: 300}));
    const retimed = editDeviceProfile(bytes, {type: "updateComboDefaultTerm", defaultTermMs: 70}, context);
    assert.throws(() => assertEffectiveCombos(retimed, context.combos), /running combo/);
    assert.doesNotThrow(() => assertEffectiveCombos(retimed, {...context.combos, defaultTermMs: 70, rows: context.combos.rows.map(row => ({...row, termMs: 70}))}));
});

test("auto-mouse save encodes all three policies exactly and keeps the chosen end colour", () => {
    const names = ["FOLLOW_REAL_DESTINATION", "END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW", "END_COLOR_ON_ALL_KEYS"];
    for (const [mode, name] of names.entries()) {
        const edited = editDeviceProfile(bytes, {type: "updateAutomouseFade", mode: name, hue: 179, sat: 255, val: 199});
        assert.deepEqual(rgb(edited).automouseFade, {mode, endColor: {h: 179, s: 255, v: 199}});
        assert.deepEqual(rgb(edited).layerColors, rgb(bytes).layerColors);
        assert.deepEqual(rgb(edited).pdModeColors, rgb(bytes).pdModeColors);
    }
});
