"use strict";
const test = require("node:test"), assert = require("node:assert/strict");
const {VOCABULARY, word, layerName, slotName, branchName, modifierNames} = require("../../core/model/vocabulary");
const rgb = require("../../core/schema/rgb-domain-v1");
const pd = require("../../core/schema/pd-mode-domain-v1");
const {KEY_BEHAVIOR_HOLD_MODES} = require("../../core/schema/key-behavior-domain-v1");

// Every value the keyboard can store has a word, so a new one cannot reach
// the review or an editor unnamed — "Eight directions" once read `undefined`.
const covers = (list, values, label) => assert.deepEqual(list.map(([id]) => id).sort(), Object.keys(values).sort(), label);
const coversNumbers = (list, values, label) => assert.deepEqual(list.map(([id]) => id).sort(), Object.values(values).sort(), label);

test("every stored enum value has a word", () => {
    covers(VOCABULARY.holdHelpers, KEY_BEHAVIOR_HOLD_MODES, "hold helpers");
    covers(VOCABULARY.localities, rgb.RGB_LOCALITIES, "localities");
    covers(VOCABULARY.paintModes, rgb.RGB_LAYER_MODES, "layer paint modes");
    covers(VOCABULARY.fadeModes, rgb.RGB_AUTOMOUSE_MODES, "auto-mouse fades");
    covers(VOCABULARY.tapCommit, rgb.RGB_TAP_COMMIT_MODES, "tap commit");
    covers(VOCABULARY.feedbackOwners, rgb.RGB_KEY_SEMANTICS, "feedback owners");
    assert.deepEqual(VOCABULARY.stages.filter((stage) => stage.bit).map((stage) => stage.bit).sort(), Object.values(rgb.RGB_STAGE_BITS).sort(), "stages");
    coversNumbers(VOCABULARY.pointing.kinds, pd.PD_KIND, "pointing kinds");
    coversNumbers(VOCABULARY.pointing.axes, pd.PD_AXIS, "axes");
    coversNumbers(VOCABULARY.pointing.buttons, pd.PD_BUTTON, "button kinds");
    coversNumbers(VOCABULARY.pointing.emptyDirection, pd.PD_EMPTY_DIRECTION, "empty direction");
    coversNumbers(VOCABULARY.pointing.modifierPolicy, pd.PD_MODIFIERS, "modifier policy");
});

test("an unknown value reads as itself, never as nothing", () => {
    assert.equal(word(VOCABULARY.localities, "RGB_LEFT_HALF"), "Left half");
    assert.equal(word(VOCABULARY.localities, "RGB_SOMEWHERE"), "RGB_SOMEWHERE");
    assert.equal(word(VOCABULARY.pointing.axes, 3), "Eight directions");
});

test("one rule names an unnamed layer, slot, branch and modifier set", () => {
    assert.equal(layerName(["", "Numbers"], 0), "Base", "layer 0 is the base layer");
    assert.equal(layerName(["", ""], 3), "Layer 3");
    assert.equal(layerName(["", "Numbers"], 1), "Numbers");
    assert.equal(slotName({id: 4, kind: 0, name: "Arrow"}), "Slot 4", "an empty slot is not called by the name it kept");
    assert.equal(slotName({id: 4, kind: 1, name: "Arrow"}), "Arrow");
    assert.equal(branchName(2), "Double tap");
    assert.deepEqual(modifierNames(2 | 128), ["Left Shift", "Right GUI"]);
});
