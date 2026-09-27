"use strict";
const assert = require("node:assert/strict");
const test = require("node:test");
const {behaviorRowsForView, rgbForView} = require("../../core/session/device-profile-view");
const {actionName} = require("../../core/schema/actions");
const {encodeKeyBehaviorDomain, decodeKeyBehaviorDomain} = require("../../core/schema/key-behavior-domain-v1");
const {decodedDeviceProfile, capabilities} = require("../fixtures/device-profile");
const keycodes = require("../../core/data/keycode-catalog");

test("every v1 semantic action has an explicit display, including unknown numeric keycodes", () => {
    const cases = [[0, 0, "KC_NO"], [1, 4, "KC_A"], [1, 0xfffe, "0xFFFE"], [2, 0, "MO(0)"],
        [3, 4, "LOCK_LAYER(4)"], [4, 0, "PD_SLOT_0"], [4, 5, "PD_SLOT_5"],
        [5, 0, "PD_SLOT_0_LOCK"], [5, 2, "PD_SLOT_2_LOCK"], [6, 0, "VIA_MACRO_0"], [7, 15, "MACRO_15"]];
    for (const [kind, operand, name] of cases) assert.equal(actionName({kind, flags: 0, operand}), name);
    assert.throws(() => actionName({kind: 8, operand: 0}), /Unsupported/);
});

test("a behaviour branch carries the readable name of the keycode it stores, when one exists", () => {
    const branch = (operand) => ({mode: 1, repeatHz: 0, action: {kind: 1, operand}});
    const domain = decodeKeyBehaviorDomain(encodeKeyBehaviorDomain({rows: [{
        target: {kind: 1, operand: 4}, tapHoldTerm: 0, longerHoldTerm: 0, multiTapTerm: 0, keepsAutoMouseAnchored: false,
        steps: [
            {tapIndex: 0, tap: {kind: 1, operand: 0x0220}, hold: branch(0x0228), longHold: {mode: 1, repeatHz: 0, action: {kind: 2, operand: 3}}},
            {tapIndex: 1, tap: {kind: 1, operand: 0xfffe}},
        ],
    }]}));
    const [step, unknown] = behaviorRowsForView(domain)[0].steps;
    assert.deepEqual([step.tap.action, step.tap.label], ["KC_HASH", "#"]);
    assert.deepEqual([step.hold.action, step.hold.label], ["LSFT(KC_ENTER)", "Shift+Enter"]);
    // A layer action is named from the profile's layers by the interface, and
    // a keycode the catalogue knows only by itself has no second name.
    assert.equal("label" in step.longHold, false);
    assert.equal("label" in unknown.tap, false);
});

test("decoded behaviors preserve sparse branches, all hold modes, anchors and timing zeros", () => {
    const rows = [1, 2, 3, 4].map((mode) => ({
        target: {kind: 1, operand: mode + 3}, tapHoldTerm: 0, longerHoldTerm: 410, multiTapTerm: 190,
        keepsAutoMouseAnchored: mode === 3,
        steps: [{tapIndex: 4, tap: {kind: 6, operand: 0},
            hold: {mode, repeatHz: mode === 3 ? 37 : 0, action: {kind: 2, operand: 0}},
            longHold: {mode: 4, repeatHz: 0, action: {kind: 7, operand: 15}}}],
    }));
    const domain = decodeKeyBehaviorDomain(encodeKeyBehaviorDomain({rows}));
    const view = behaviorRowsForView(domain);
    assert.deepEqual(view.map(r => r.steps[0].hold.helper), ["PRESS_AND_HOLD_UNTIL_RELEASE", "TAP_AT_HOLD_THRESHOLD", "REPEAT_WHILE_HELD", "TAP_ON_RELEASE_AFTER_HOLD"]);
    assert.equal(view[0].tapHoldTerm, "0");
    assert.equal(view[0].longerHoldTerm, "410");
    assert.equal(view[0].multiTapTerm, "190");
    assert.equal(view[2].keepsAutoMouseAnchored, true);
    assert.equal(view[0].keepsAutoMouseAnchored, false);
    assert.equal(view[2].steps[0].hold.repeatHz, "37");
    assert.equal(view[0].steps[0].hold.repeatHz, "0");
    assert.equal(view[0].steps.length, 1);
    assert.equal(view[0].steps[0].tapCount, 4);
    assert.equal(view[0].steps[0].tapCountName, "Quintuple tap");
    assert.equal(view[0].steps[0].tap.action, "VIA_MACRO_0");
    assert.equal(view[0].steps[0].longHold.action, "MACRO_15");
});

test("real device RGB retains every value and reference through the presentation conversion", () => {
    const rgb = decodedDeviceProfile().domains.rgb;
    const original = JSON.stringify(rgb);
    const view = rgbForView(rgb);
    assert.deepEqual(view.layerColors[0], {layer: "Layer 0", layerId: 0, color: {h: "0", s: "0", v: "0"}, mode: "ALL_KEYS"});
    assert.equal(view.layerColors[1].mode, "KEYS_MAPPED_ON_THIS_LAYER_ONLY");
    assert.equal(view.pdModeColors[0].pointingMode, "PD_MODE_DRAGSCROLL");
    assert.equal(view.pdModeColors[0].locality, "RGB_RIGHT_HALF");
    assert.equal(view.automouseFade.mode, "FOLLOW_REAL_DESTINATION");
    assert.deepEqual(view.automouseFade.end_color, {h: "0", s: "255", v: "200"});
    assert.equal(view.keyBehaviorFeedback.tapCommitMode, "KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS");
    assert.equal(view.keyBehaviorFeedback.tapBranchColors.length, 4);
    assert.deepEqual(view.ledGroups[0].ledIndices, [24, 25, 26, 27, 28, 53, 54, 55]);
    assert.equal(view.ledGroups[0].usageCount, 3);
    assert.equal(view.pdModeLedGroups[0].owner, "RGB_PD_MODE_GROUP_ALL");
    assert.equal(view.keyBehaviorFeedbackLedGroups[0].owner, "KEY_FEEDBACK_GROUP_ALL");
    assert.equal(JSON.stringify(rgb), original, "converting a snapshot cannot mutate it");
});

test("RGB selectors and disabled stages are displayed without inventing enabled state", () => {
    const rgb = decodedDeviceProfile().domains.rgb;
    rgb.stageEnableMask = 0;
    rgb.layerGroupRows = [{selector: 0, groupId: 0, color: {h: 0, s: 0, v: 0}}, {selector: 255, groupId: 0, color: {h: 1, s: 2, v: 3}}];
    rgb.pdModeGroupRows[0].selector = 0;
    rgb.keyGroupRows[0].semantic = 0;
    const view = rgbForView(rgb);
    assert.ok(view.stages.every(stage => !stage.enabled));
    assert.deepEqual(view.layerLedGroups.map(r => r.owner), ["Layer 0", "RGB_LAYER_GROUP_ALL"]);
    assert.equal(view.pdModeLedGroups[0].owner, "PD_MODE_DRAGSCROLL");
    assert.equal(view.keyBehaviorFeedbackLedGroups[0].owner, "KEY_FEEDBACK_GROUP_TAP_BRANCH_PENDING");
});
