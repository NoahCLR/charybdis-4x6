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

test("base is always on, so it is never toggled or listed", () => {
    assert.deepEqual(toggleLayer(3, [1], 0), {top: 3, on: [1]});
    assert.deepEqual(toggleLayer(0, [], 2), {top: 2, on: []}, "from base, a ⌘-click is the same as picking that layer");
    assert.deepEqual(layersOn(3, [0, 1, 2], 8), [1, 2]);
});

test("a remembered set keeps only layers under the top that still exist", () => {
    assert.deepEqual(layersOn(3, [1, 4, 2], 8), [1, 2], "nothing above the top");
    assert.deepEqual(layersOn(7, [2, 6], 5), [2], "nothing past the end of a shorter stack");
    assert.deepEqual(layersOn(2, undefined, 8), []);
});
