import assert from "node:assert/strict";
import test from "node:test";
import {layersOn, toggleLayer} from "../webview/view/layer-set.mjs";

test("⌘-clicking a layer adds it, and the highest layer on is the top", () => {
    assert.deepEqual(toggleLayer(2, [], 1), {top: 2, on: [1]}, "a lower layer goes under the viewed one");
    assert.deepEqual(toggleLayer(2, [], 3), {top: 3, on: [2]}, "a higher layer takes the top, as it wins on the keyboard");
    assert.deepEqual(toggleLayer(3, [1], 5), {top: 5, on: [1, 3]});
});

test("⌘-clicking a layer that is on removes it, and the next highest takes the top", () => {
    assert.deepEqual(toggleLayer(3, [1, 2], 2), {top: 3, on: [1]});
    assert.deepEqual(toggleLayer(3, [1, 2], 3), {top: 2, on: [1]}, "removing the top hands it to the next layer down");
    assert.deepEqual(toggleLayer(2, [1], 2), {top: 1, on: []}, "one layer left is a plain single-layer view");
    assert.deepEqual(toggleLayer(2, [], 2), {top: 0, on: []}, "with nothing left, base is shown");
});

test("base is under every preview, so ⌘-clicking it alone previews the picked layer over base", () => {
    assert.deepEqual(toggleLayer(3, [], 0), {top: 3, on: [0]}, "a layer over base alone");
    assert.deepEqual(toggleLayer(3, [0], 0), {top: 3, on: []}, "and back to the plain single-layer view");
    assert.deepEqual(toggleLayer(3, [1], 0), {top: 3, on: [1]}, "with other layers on, base stays on");
    assert.deepEqual(toggleLayer(0, [], 0), {top: 0, on: []}, "base on its own has nothing to go under");
    assert.deepEqual(toggleLayer(0, [], 2), {top: 2, on: []}, "from base, a ⌘-click is the same as picking that layer");
});

test("a base listed on stays on as layers come and go above it", () => {
    assert.deepEqual(toggleLayer(3, [0], 1), {top: 3, on: [0, 1]});
    assert.deepEqual(toggleLayer(3, [0, 1], 1), {top: 3, on: [0]});
    assert.deepEqual(toggleLayer(3, [0, 1], 3), {top: 1, on: [0]}, "the next layer down takes the top, still over base");
    assert.deepEqual(toggleLayer(3, [0], 3), {top: 0, on: []}, "with nothing left, base is shown on its own");
    assert.deepEqual(layersOn(3, [0, 1, 2], 8), [0, 1, 2]);
});

test("a remembered set keeps only layers under the top that still exist", () => {
    assert.deepEqual(layersOn(3, [1, 4, 2], 8), [1, 2], "nothing above the top");
    assert.deepEqual(layersOn(7, [2, 6], 5), [2], "nothing past the end of a shorter stack");
    assert.deepEqual(layersOn(2, undefined, 8), []);
});
