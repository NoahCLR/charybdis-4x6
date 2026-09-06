"use strict";
const assert = require("node:assert/strict");
const test = require("node:test");
const {combosForLayerPreview} = require("../../webview/combo-preview");

test("combo badges follow inherited keys and device reference layers while preserving IDs", () => {
    const layer = (index, keys) => ({index, positions: keys.map((keycode, layoutIndex) => ({layoutIndex, keycode}))});
    const base = layer(0, ["KC_D", "KC_F", "KC_N"]);
    const upper = layer(1, ["KC_TRANSPARENT", "KC_F", "KC_NO"]);
    const model = {layers: [base, upper], comboReadback: {state: "read", enabled: true, layerReferences: [0, 1]}, combos: [
        {badge: "C3", inputs: ["KC_D", "KC_F"]}, {badge: "C7", inputs: ["KC_D", "KC_N"]},
    ]};
    let rows = combosForLayerPreview(model, upper);
    assert.deepEqual(rows.map(row => row.badge), ["C3"]);
    assert.deepEqual(rows[0].inputPositions, [0, 1]);
    model.comboReadback.fixedReference = true;
    assert.deepEqual(combosForLayerPreview(model, upper), [], "fixed-layer lookup does not inherit through transparency");
    model.comboReadback.fixedReference = false;
    model.comboReadback.layerReferences[1] = 0;
    assert.deepEqual(combosForLayerPreview(model, upper).map(row => row.badge), ["C3", "C7"]);
    model.comboReadback.enabled = false;
    assert.deepEqual(combosForLayerPreview(model, upper), []);
    assert.deepEqual(combosForLayerPreview({}, upper), []);
});
