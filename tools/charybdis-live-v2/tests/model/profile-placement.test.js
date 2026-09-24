"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {behaviorPlacementProblem, comboPlacementProblem, profilePlacementProblem} = require("../../core/model/profile-placement");
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
        payload: encodeComboDomainV1([{inputs: [code(0x04), code(0x05)], output: code(0x5022), termMs: 0, holdTermMs: 0, mustHold: false, mustTap: false, ordered: false}], {actionLimits: {maxPdModes: 8}})}]});
    assert.match(profilePlacementProblem(blob, options), /^Combo 1: /);
});
