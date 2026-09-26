"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {behaviorPlacementProblem, comboPlacementProblem, keyPlacementProblem, profilePlacementProblem} = require("../../core/model/profile-placement");
const {encodeProfileBlob, PROFILE_DOMAIN_IDS} = require("../../core/schema/profile-blob-v1");
const {encodeComboDomainV1} = require("../../core/schema/combo-domain-v1");
const {bytes} = require("../fixtures/device-profile");

const code = operand => ({kind: 1, flags: 0, operand});
const row = (target, step) => ({target, tapHoldTerm: 0, longerHoldTerm: 0, multiTapTerm: 0, keepsAutoMouseAnchored: false, steps: [{tapIndex: 0, ...step}]});
const options = {layerCount: 8};

test("a behaviour row is checked where each of its actions sits", () => {
    assert.equal(behaviorPlacementProblem([row(code(0x04), {tap: code(0x05)})], options), undefined);
    assert.equal(behaviorPlacementProblem([row(code(0x04), {hold: {mode: 1, repeatHz: 0, action: {kind: 2, flags: 0, operand: 2}}})], options), undefined);
    assert.match(behaviorPlacementProblem([row(code(0x04), {hold: {mode: 2, repeatHz: 0, action: {kind: 2, flags: 0, operand: 2}}})], options), /^The behaviour on KC_A: MO\(2\) holds a layer/);
    assert.match(behaviorPlacementProblem([row(code(0x04), {tap: {kind: 2, flags: 0, operand: 2}})], options), /MO\(2\) holds a layer/);
    assert.match(behaviorPlacementProblem([row(code(0x5241), {tap: code(0x05)})], options), /^The behaviour on DF\(1\)/);
});

test("a combo is checked by its output; layer holds and one-shots pass, LT does not", () => {
    const combo = output => ({inputs: [code(0x04), code(0x05)], output, termMs: 0, holdTermMs: 0, mustHold: false, mustTap: false, ordered: false});
    assert.equal(comboPlacementProblem([combo(code(0x08)), combo({kind: 2, flags: 0, operand: 2}), combo(code(0x5282))], options), undefined);
    assert.match(comboPlacementProblem([combo(code(0x08)), combo(code(0x4104))], options), /^Combo 2: LT\(1,KC_A\)/);
});

test("an encoded profile is checked across its behaviours and combos", () => {
    assert.equal(profilePlacementProblem(bytes, options), undefined, "the device fixture passes");
    const blob = encodeProfileBlob({schema: {major: 2, minor: 0}, domains: [{id: PROFILE_DOMAIN_IDS.COMBOS, version: 1,
        payload: encodeComboDomainV1([{inputs: [code(0x04), code(0x05)], output: code(0x5241), termMs: 0, holdTermMs: 0, mustHold: false, mustTap: false, ordered: false}], {actionLimits: {maxPdModes: 8}})}]});
    assert.match(profilePlacementProblem(blob, options), /^Combo 1: /);
});

test("a layout key takes every layer key the keyboard owns, and refuses DF, PDF and layers past the bank", () => {
    for (const owned of [0x04, 0x4104, 0x5022, 0x5202, 0x5222, 0x5262, 0x5282, 0x52c2, 0x7e5c]) assert.equal(keyPlacementProblem(owned, options), undefined, owned.toString(16));
    assert.match(keyPlacementProblem(0x5241, options), /^DF\(1\) is a layer keycode this keyboard does not run/);
    assert.match(keyPlacementProblem(0x52e1, options), /does not run through its layer tracking/);
    assert.match(keyPlacementProblem(0x5209, options), /does not run through its layer tracking/);
});

// The behaviour engine sends a keycode no other path claims through tap_code16,
// which keeps only modifier bits and the low byte: DRGSCRL (0x7E06) would reach
// the host as C. Such keys stay placeable on a key or a combo, which run QMK's
// full key handling.
test("a behaviour refuses QMK and keyboard functions on a keyboard that would send them as an unrelated key", () => {
    const hold = (operand, mode = 1) => ({hold: {mode, repeatHz: 0, action: code(operand)}});
    for (const sendable of [0x04, 0x021e, 0x1f04, 0x00cd, 0x2104, 0x52a1, 0x7700, 0x7e50, 0x7e5f])
        assert.equal(behaviorPlacementProblem([row(code(0x04), {tap: code(sendable)}), row(code(0x05), hold(sendable, 2))], options), undefined, sendable.toString(16));
    for (const [operand, name] of [[0x7e06, "DRGSCRL"], [0x7e00, "DPI_MOD"], [0x7c00, "QK_BOOTLOADER"], [0x7820, "QK_UNDERGLOW_TOGGLE"], [0x7e08, "QK_KB_8"]]) {
        assert.match(behaviorPlacementProblem([row(code(0x04), {tap: code(operand)})], options), new RegExp(`^The behaviour on KC_A: ${name} runs in QMK's own key handling`));
        assert.match(behaviorPlacementProblem([row(code(0x04), hold(operand))], options), new RegExp(name));
        assert.match(behaviorPlacementProblem([row(code(0x04), hold(operand, 2))], options), new RegExp(name));
        assert.match(behaviorPlacementProblem([row(code(operand), {tap: code(0x05)})], options), new RegExp(`^The behaviour on ${name}:`), "a tap with no branch sends the target");
        assert.equal(keyPlacementProblem(operand, options), undefined, `${name} on a key`);
        // A keyboard reporting feature bit 15 sends them as QMK records instead.
        const sending = {...options, behaviorQmkFunctions: true};
        assert.equal(behaviorPlacementProblem([row(code(operand), {tap: code(operand)}), row(code(0x05), hold(operand, 2))], sending), undefined, `${name} with bit 15`);
        const combo = {inputs: [code(0x04), code(0x05)], output: code(operand), termMs: 0, holdTermMs: 0, mustHold: false, mustTap: false, ordered: false};
        assert.equal(comboPlacementProblem([combo], options), undefined, `${name} as a combo output`);
    }
});
