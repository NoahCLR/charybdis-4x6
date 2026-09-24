"use strict";
const test = require("node:test"), assert = require("node:assert/strict");
const {actionName, nativeCode, keycodeAction, pdSlotOfCode, actionLimitsFor, layerRef, layerOfRef, knownActionAbi} = require("../../core/schema/actions");
const {PROFILE_ACTION_KINDS: ACTION} = require("../../core/schema/profile-blob-v1");

test("an action has one name and one native keycode", () => {
    assert.equal(actionName({kind: ACTION.QMK_KEYCODE, operand: 0x04}), "KC_A");
    assert.equal(actionName({kind: ACTION.LAYER_MOMENTARY, operand: 2}), "MO(2)");
    assert.equal(actionName({kind: ACTION.PD_MODE_MOMENTARY, operand: 0}), "DRAGSCROLL");
    assert.equal(actionName({kind: ACTION.PD_MODE_LOCK, operand: 7}), "PD_SLOT_7_LOCK");
    assert.equal(nativeCode({kind: ACTION.QMK_KEYCODE, operand: 0x04}), 0x04);
    assert.equal(nativeCode({kind: ACTION.PD_MODE_LOCK, operand: 0}), 0x7e56);
    assert.deepEqual(keycodeAction(0x29), {kind: ACTION.QMK_KEYCODE, operand: 0x29});
    assert.equal(pdSlotOfCode(0x7ef2), 7);
    assert.equal(pdSlotOfCode(0x0004), undefined);
    assert.throws(() => actionName({kind: ACTION.PD_MODE_MOMENTARY, operand: 9}), /pointing slot/);
});

test("decode limits follow the schema version, and a layer reference reads back", () => {
    assert.deepEqual(actionLimitsFor(2), {actionLimits: {maxPdModes: 8}});
    assert.deepEqual(actionLimitsFor(1), {actionLimits: {maxPdModes: 6}});
    assert.equal(layerRef(3), "Layer 3");
    assert.equal(layerOfRef("Layer 3"), 3);
    assert.equal(layerOfRef("Numbers"), undefined);
    assert.equal(knownActionAbi(0x61072732), true);
    assert.equal(knownActionAbi(1), false);
});

test("placement follows the keyboard's rule: only layer keycodes are restricted", () => {
    const {PLACEMENT, placementProblem} = require("../../core/schema/actions");
    const order = [PLACEMENT.KEY, PLACEMENT.TAP, PLACEMENT.HOLD_PRESS, PLACEMENT.HOLD_OTHER, PLACEMENT.COMBO_OUTPUT];
    const places = (action) => order.map(placement => placementProblem(action, placement, {layerCount: 8}) ? "-" : "ok").join(" ");
    const code = operand => ({kind: 1, operand});
    assert.equal(places(code(0x0004)), "ok ok ok ok ok", "KC_A goes anywhere");
    assert.equal(places({kind: 3, operand: 2}), "ok ok ok ok ok", "LOCK_LAYER goes anywhere");
    assert.equal(places(code(0x5262)), "ok ok ok ok ok", "TG goes anywhere");
    assert.equal(places(code(0x5200)), "ok ok ok ok ok", "TO goes anywhere");
    assert.equal(places({kind: 2, operand: 2}), "ok - ok - -", "MO needs a held key");
    assert.equal(places(code(0x52c2)), "ok - ok - -", "TT needs a held key");
    assert.equal(places(code(0x5282)), "ok ok - - -", "OSL is a key or a tap");
    assert.equal(places(code(0x4104)), "ok - - - -", "LT is a key of its own");
    for (const unowned of [0x5241, 0x52e1, 0x5022, 0x5268]) assert.equal(places(code(unowned)), "- - - - -", `0x${unowned.toString(16)} goes nowhere`);
});
