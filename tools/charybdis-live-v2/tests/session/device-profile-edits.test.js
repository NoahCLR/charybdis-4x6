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

const {decodeComboDomainV1} = require("../../core/schema/combo-domain-v1");
const {decodeComboPages} = require("../../core/protocol/combo-readback-v1");
const {fixturePages} = require("../fixtures/device-combos");
const {assertEffectiveCombos} = require("../../core/session/device-profile-edits");
const pages = fixturePages();
const context = {capabilities: {supportedDomainMask: 7, actionAbiDigest: 0xdcb00959}, combos: {state: "read", ...decodeComboPages(pages[0], pages.slice(1))}};
const comboRows = bytes => decodeComboDomainV1(decodeProfileBlob(bytes).domains.find(row => row.id === 0x30).payload);
test("adding, editing and deleting combos preserves every untouched profile domain and combo", () => {
    const added = editDeviceProfile(bytes, {type: "addCombo", inputs: ["KC_A", "KC_B"], output: "LGUI(KC_C)", termMs: "25", ordered: true}, context);
    const rows = comboRows(added);
    assert.equal(rows.length, 3);
    assert.deepEqual(rows[0].inputs.map(action => action.operand), context.combos.rows[0].inputs);
    assert.equal(rows[2].output.operand, 0x806);
    for (let id = 0; id < 2; id++) assert.deepEqual(decodeProfileBlob(added).domains[id].payload, decodeProfileBlob(bytes).domains[id].payload);
    const edited = editDeviceProfile(added, {type: "saveCombo", id: 2, inputs: ["KC_C", "KC_D"], output: "MO(2)", termMs: "0", mustHold: true}, context);
    assert.deepEqual(comboRows(edited)[2].output, {kind: 2, flags: 0, operand: 2});
    assert.equal(comboRows(edited)[2].mustHold, true);
    const timed = editDeviceProfile(edited, {type: "updateComboHoldTerm", holdTermMs: 400}, context);
    assert.deepEqual(comboRows(timed).map(row => row.holdTermMs), [400, 400, 400]);
    const removed = editDeviceProfile(timed, {type: "deleteCombo", id: 1}, context);
    assert.equal(comboRows(removed).length, 2);
    assert.equal(comboRows(removed)[1].output.kind, 2);
    const rgbChanged = editDeviceProfile(removed, {type: "updateRgbStages", stageEnableMask: 0});
    assert.deepEqual(comboRows(rgbChanged), comboRows(removed));
});
test("Cmd+N from the combo picker saves and verifies as a standard modified key", () => {
    const message = {type: "saveCombo", id: 0, inputs: ["G(KC_C)", "G(KC_V)"], output: "G(KC_N)", termMs: "50"};
    // Standard shortcuts do not depend on the keyboard's custom action ABI.
    const edited = editDeviceProfile(bytes, message, {...context, capabilities: {supportedDomainMask: 7, actionAbiDigest: 0}});
    const row = comboRows(edited)[0];
    assert.deepEqual(row.output, {kind: 1, flags: 0, operand: 0x0811});
    assert.deepEqual(row.inputs.map(action => action.operand), [0x0806, 0x0819]);
    const read = {...context.combos, rows: context.combos.rows.map((row, id) => id ? row : {...row, inputs: [0x0806, 0x0819], output: 0x0811, termMs: 50, mustHold: false, mustTap: false, ordered: false})};
    assert.doesNotThrow(() => assertEffectiveCombos(edited, read));
    assert.deepEqual(comboRows(edited)[1], comboRows(editDeviceProfile(bytes, {type: "updateComboHoldTerm", holdTermMs: row.holdTermMs}, context))[1]);
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
    assert.doesNotThrow(() => assertEffectiveCombos(next, {...context.combos, rows: context.combos.rows.map(row => ({...row, holdTermMs: 300}))}));
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
