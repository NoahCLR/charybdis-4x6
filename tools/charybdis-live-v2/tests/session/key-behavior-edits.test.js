"use strict";
const assert = require("node:assert/strict");
const test = require("node:test");
const {bytes, capabilities} = require("../fixtures/device-profile");
const {decodeProfileBlob} = require("../../core/schema/profile-blob-v1");
const {decodeKeyBehaviorDomain} = require("../../core/schema/key-behavior-domain-v1");
const {behaviorRowsForView, actionName} = require("../../core/session/device-profile-view");
const {editDeviceProfile} = require("../../core/session/device-profile-edits");
const {editKeyBehaviors} = require("../../core/session/key-behavior-edits");
const {ProfileDeviceService} = require("../../core/session/profile-device-service");

const payload = decodeProfileBlob(bytes).domains[1].payload;
const original = decodeKeyBehaviorDomain(payload);
const views = behaviorRowsForView(original);
const disabled = () => ({helper: "", action: "", repeatHz: ""});
const form = () => ({keycode: "KC_A", tapHoldTerm: "", longerHoldTerm: "0", multiTapTerm: "250", keepsAutoMouseAnchored: true,
    steps: [{tapCount: 0, tap: {helper: "TAP_SENDS", action: "G(KC_N)"}, hold: disabled(), longHold: disabled()},
        {tapCount: 4, tap: disabled(), hold: {helper: "REPEAT_WHILE_HELD", action: "KC_RIGHT", repeatHz: "25"}, longHold: disabled()}]});
const edit = (message, caps = capabilities, input = payload) => editKeyBehaviors(input, message, caps);
const rowFor = (input, name) => decodeKeyBehaviorDomain(input).rows.find(row => actionName(row.target) === name);

test("a stale UI draft is refused before profile encoding or any upload", async () => {
    const service = new ProfileDeviceService();
    service.profileBytes = bytes;
    service.committed = {state: "read", failures: [], source: "committed", generation: 12, digest: 42, originHalf: 1};
    let uploads = 0;
    service.applyLiveProfile = async () => {uploads++;};
    const expectedBase = {...service.committed, generation: 11};
    await assert.rejects(service.saveProfileEdit({type: "saveBehavior", behavior: form(), expectedBase}), error => error.code === "PROFILE_EDIT_CONFLICT");
    assert.equal(uploads, 0);
});

test("every device behaviour survives the editor's sparse and disabled branches byte-for-byte", () => {
    for (const row of views) {
        const behavior = {...row, steps: Array.from({length: 5}, (_, tapCount) => ({tapCount, tap: disabled(), hold: disabled(), longHold: disabled(), ...row.steps.find(step => step.tapCount === tapCount)}))};
        assert.deepEqual(edit({type: "saveBehavior", behavior}), payload, row.keycode);
    }
});

test("behaviour creation, edit and deletion preserve unrelated rows and profile domains", () => {
    const added = edit({type: "saveBehavior", behavior: form()});
    const row = rowFor(added, "KC_A");
    assert.equal(decodeKeyBehaviorDomain(added).rowCount, original.rowCount + 1);
    assert.deepEqual(row.steps.map(step => step.tapIndex), [0, 4]);
    assert.deepEqual(row.steps[0].tap, {kind: 1, flags: 0, operand: 0x0811});
    assert.deepEqual(row.steps[1].hold, {mode: 3, repeatHz: 25, action: {kind: 1, flags: 0, operand: 0x004f}});
    assert.equal(row.tapHoldTerm, 0); assert.equal(row.multiTapTerm, 250); assert.equal(row.keepsAutoMouseAnchored, true);
    const changed = edit({type: "saveBehavior", behavior: {...form(), tapHoldTerm: "180", steps: []}}, capabilities, added);
    assert.equal(rowFor(changed, "KC_A").tapHoldTerm, 180);
    assert.deepEqual(rowFor(changed, "KC_A").steps, [], "timing-only rows are valid");
    const removed = edit({type: "deleteBehavior", keycode: "KC_A"}, capabilities, changed);
    assert.deepEqual(removed, payload);
    const next = editDeviceProfile(bytes, {type: "saveBehavior", behavior: form()}, {capabilities});
    assert.deepEqual(decodeProfileBlob(next).domains[0].payload, decodeProfileBlob(bytes).domains[0].payload);
    assert.deepEqual(decodeKeyBehaviorDomain(added).rows.filter(row => actionName(row.target) !== "KC_A"), original.rows);
});

test("simple additions and all hold modes produce canonical rows", () => {
    const next = edit({type: "addBehavior", behavior: {keycode: "KC_A", tapHoldTerm: "90", tap: {helper: "TAP_SENDS", action: "KC_TRNS"}}});
    assert.equal(rowFor(next, "KC_A").steps[0].tap.operand, 1);
    for (const [helper, mode] of [["PRESS_AND_HOLD_UNTIL_RELEASE", 1], ["TAP_AT_HOLD_THRESHOLD", 2], ["TAP_ON_RELEASE_AFTER_HOLD", 4]]) {
        const result = edit({type: "saveBehavior", behavior: {...form(), steps: [{tapCount: 2, hold: {helper, action: "MO(2)", repeatHz: "25"}, longHold: {helper, action: "LOCK_LAYER(3)"}}]}});
        const step = rowFor(result, "KC_A").steps[0];
        assert.deepEqual(step.hold, {mode, repeatHz: 0, action: {kind: 2, flags: 0, operand: 2}});
        assert.equal(step.longHold.action.kind, 3);
    }
});

test("native aliases update existing semantic targets without duplicating or renumbering them", () => {
    const row = views.find(row => row.keycode === "DRAGSCROLL");
    const result = edit({type: "saveBehavior", behavior: {...row, keycode: "QK_USER_16", tapHoldTerm: "125"}});
    assert.equal(decodeKeyBehaviorDomain(result).rowCount, original.rowCount);
    assert.equal(rowFor(result, "DRAGSCROLL").target.kind, 4);
    assert.equal(rowFor(result, "DRAGSCROLL").tapHoldTerm, 125);
    assert.throws(() => edit({type: "addBehavior", behavior: {...form(), keycode: "QK_USER_16"}}), /already/);
    const anchored = views.find(row => row.keepsAutoMouseAnchored);
    const {keepsAutoMouseAnchored, ...withoutFlag} = anchored;
    assert.deepEqual(edit({type: "saveBehavior", behavior: withoutFlag}), payload);
});

test("stable semantic actions and standard shortcuts do not require a known custom vocabulary", () => {
    const unknown = {...capabilities, actionAbiDigest: 0};
    for (const row of views) assert.deepEqual(edit({type: "saveBehavior", behavior: row}, unknown), payload);
    assert.equal(rowFor(edit({type: "saveBehavior", behavior: form()}, unknown), "KC_A").steps[0].tap.operand, 0x0811);
    assert.throws(() => edit({type: "saveBehavior", behavior: {...form(), steps: [{tapCount: 0, tap: {helper: "TAP_SENDS", action: "DPI_MOD"}}]}}, unknown), /vocabulary/);
});

test("invalid edits and references are rejected before upload", () => {
    const save = behavior => edit({type: "saveBehavior", behavior});
    for (const value of [-1, 65536, "abc", "1.5", true]) assert.throws(() => save({...form(), tapHoldTerm: value}));
    for (const steps of [undefined, [{tapCount: 5}], [{tapCount: 0}, {tapCount: 0}], [{tapCount: 0, tap: {helper: "" , action: "KC_A"}}], [{tapCount: 0, tap: {helper: "BOGUS", action: "KC_A"}}]]) {
        assert.throws(() => save({...form(), steps}));
    }
    for (const action of ["", "UNKNOWN", "MO(5)", "LOCK_LAYER(5)", "VIA_MACRO_64", "MACRO_16", "TG(2)", "G(MO(2))"]) {
        assert.throws(() => save({...form(), steps: [{tapCount: 0, tap: {helper: "TAP_SENDS", action}}]}), undefined, action);
    }
    for (const repeatHz of ["", "0", "101", "-1", "2.5"]) assert.throws(() => save({...form(), steps: [{tapCount: 0, hold: {helper: "REPEAT_WHILE_HELD", action: "KC_A", repeatHz}}]}));
    assert.throws(() => save({...form(), keepsAutoMouseAnchored: "false"}), /flag/);
    assert.throws(() => edit({type: "deleteBehavior", keycode: "KC_A"}), /no longer/);
    assert.throws(() => edit({type: "saveBehavior", behavior: form()}, {...capabilities, supportedDomainMask: 1}), /firmware/);
    assert.throws(() => edit({type: "saveBehavior", behavior: form()}, {...capabilities, maxBehaviorRows: original.rowCount}), /row count/);
    assert.throws(() => edit({type: "saveBehavior", behavior: form()}, {...capabilities, maxPopulatedBehaviorSteps: original.populatedStepCount}), /step count/);
    assert.throws(() => edit({type: "saveBehavior", behavior: form()}, {...capabilities, maxTapStepsPerBehavior: 4}), /Tap index/);
});
