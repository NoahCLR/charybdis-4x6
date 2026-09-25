"use strict";
const test = require("node:test"), assert = require("node:assert/strict");
const {IDENTITY, compose, inverse, isIdentity, layerUnit, rearranged} = require("../../core/model/layer-order");
const {fingerprint, summary} = require("../../core/model/portable-profile");
const {document} = require("../fixtures/portable-profile");

const SWAP = [0, 1, 3, 2, 4, 5, 6, 7];

test("orders compose step by step, and an order and its inverse cancel", () => {
    assert.deepEqual(compose(IDENTITY, SWAP), SWAP);
    assert.ok(isIdentity(compose(SWAP, SWAP)), "swapping back is the keyboard's order");
    const moved = compose(SWAP, [0, 2, 1, 3, 4, 5, 6, 7]);
    assert.deepEqual(moved, [0, 3, 1, 2, 4, 5, 6, 7], "slot 1 now holds the keyboard's layer 3");
    assert.ok(isIdentity(compose(moved, inverse(moved))));
});

test("a rearranged document moves each layer with its name, and moving it back restores it", () => {
    const value = document(), moved = rearranged(value, SWAP);
    assert.deepEqual(summary(moved).names.slice(2, 4), ["Navigation", "Symbols"]);
    assert.equal(fingerprint(rearranged(moved, inverse(SWAP))), fingerprint(value));
    const renumbered = rearranged(value, [0, 2, 1, 3, 4, 5, 6, 7]);
    assert.notEqual(renumbered.layers[0][0], value.layers[0][0], "a key reaching a moved layer follows it");
});

test("units that belong to a layer are named by the layer, not the slot", () => {
    assert.equal(layerUnit("layout:2:5", SWAP), "layout@3:5");
    assert.equal(layerUnit("layerName:3", SWAP), "layerName@2");
    assert.equal(layerUnit("rgb:layer:2", SWAP), "rgb:layer@3");
    assert.equal(layerUnit("macro:2", SWAP), "macro:2", "other units have no layer");
    assert.equal(layerUnit("rgb:layerGroups", SWAP), "rgb:layerGroups");
});
