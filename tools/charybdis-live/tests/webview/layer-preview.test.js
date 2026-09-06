"use strict";
const assert = require("node:assert/strict");
const test = require("node:test");
const vm = require("node:vm");
const {layoutKeyKind, layerPreviewPaint, composedLayerPreviewPaint, baseEffectPreviewNote} = require("../../webview/layer-preview");
const {getStudioHtml} = require("../../webview/studio-ui");
const {buildDeviceModel} = require("../../core/session/device-model");
const {decodedDeviceProfile} = require("../fixtures/device-profile");

const transparent = ["KC_TRANSPARENT", "KC_TRNS", "_______", "0x0001"];
const disabled = ["KC_NO", "XXXXXXX", "0x0000"];
const rgb = buildDeviceModel({committed: decodedDeviceProfile()}).rgb;
const mappedOnly = rgb.layerColors.find(row => row.layerId === 4);
const red = {h: "0", s: "255", v: "255"};
const baseEffect = {state: "read", enabled: true, effectName: "Solid colour", previewColor: red};
const base = {name: "Layer 0", index: 0, positions: Array.from({length: 56}, (_, layoutIndex) => ({layoutIndex, keycode: "KC_A"}))};
const upper = {name: "Layer 4", index: 4, positions: base.positions.map((key, index) => ({...key, keycode: index < 9 ? "KC_B" : "KC_TRANSPARENT"}))};

test("base pass-through is red, and Layer 4 paints nine keys over the inherited red", () => {
    const state = {...rgb, baseEffect};
    for (const key of base.positions) assert.deepEqual(composedLayerPreviewPaint(state, base, base, key, key.layoutIndex), red);
    const colors = upper.positions.map(key => composedLayerPreviewPaint(state, base, upper, key, key.layoutIndex));
    assert.equal(colors.filter(color => color === mappedOnly.color).length, 9);
    assert.equal(colors.filter(color => color === red).length, 47);
    assert.match(baseEffectPreviewNote(baseEffect), /last-read/);
});

test("composition respects base membership, selected edits, all-keys, and off state", () => {
    const blue = {h: "169", s: "255", v: "200"};
    const state = {...rgb, baseEffect, layerColors: [{layer: base.name, color: blue, mode: "KEYS_MAPPED_ON_THIS_LAYER_ONLY"}, mappedOnly]};
    const key = {layoutIndex: 20, keycode: "KC_TRANSPARENT", value: 4};
    assert.equal(composedLayerPreviewPaint(state, base, upper, key, 20), blue);
    const emptyBase = {...base, positions: [{layoutIndex: 20, keycode: "KC_NO"}]};
    assert.equal(composedLayerPreviewPaint(state, emptyBase, upper, key, 20), red);
    assert.equal(composedLayerPreviewPaint(state, base, upper, {...key, keycode: "KC_A", value: 1}, 20), mappedOnly.color);
    const allKeys = {...state, layerColors: [{...mappedOnly, mode: "ALL_KEYS"}]};
    assert.equal(composedLayerPreviewPaint(allKeys, base, upper, key, 20), mappedOnly.color);
    assert.equal(composedLayerPreviewPaint({...state, layerColorsEnabled: false}, base, upper, key, 20), red);
    const black = {h: "0", s: "0", v: "0"};
    assert.equal(composedLayerPreviewPaint({...state, baseEffect: {...baseEffect, enabled: false, previewColor: black}}, base, upper, key, 20), black);
});

test("LED groups follow firmware order and inherit their owning layer's paint only", () => {
    const blue = {h: "169", s: "255", v: "200"};
    const green = {h: "85", s: "255", v: "200"};
    const inherit = {h: "0", s: "0", v: "0"};
    const key = upper.positions[0];
    const group = (owner, color) => ({owner, color, ledIndices: [42]});
    const state = {...rgb, baseEffect, layerLedGroups: [group(base.name, blue)]};
    assert.equal(composedLayerPreviewPaint(state, base, upper, key, 42), blue, "base groups run after all layer colours");
    assert.equal(composedLayerPreviewPaint(state, base, upper, key, 41), mappedOnly.color, "LED identity, not layout index, controls membership");
    state.layerLedGroups.push(group("Layer 2", green));
    assert.equal(composedLayerPreviewPaint(state, base, upper, key, 42), blue, "inactive layers are excluded");
    state.layerLedGroups.push(group(base.name, inherit));
    assert.equal(composedLayerPreviewPaint(state, base, upper, key, 42), blue, "pass-through inheritance preserves the earlier paint");
    state.layerLedGroups.push(group("RGB_LAYER_GROUP_ALL", inherit));
    assert.equal(composedLayerPreviewPaint(state, base, upper, key, 42), mappedOnly.color);
    state.layerLedGroups.push(group(upper.name, green));
    assert.equal(composedLayerPreviewPaint(state, base, upper, key, 42), green, "later group rows win");
    const literalBlack = {h: "1", s: "0", v: "0"};
    state.layerLedGroups.push(group(upper.name, literalBlack));
    assert.equal(composedLayerPreviewPaint(state, base, upper, key, 42), literalBlack);
});

test("unavailable and animated base effects do not become a fabricated solid colour", () => {
    for (const effect of [{state: "unavailable"}, {state: "read", enabled: true, effectName: "Effect 9"}]) {
        const state = {...rgb, baseEffect: effect};
        assert.equal(composedLayerPreviewPaint(state, base, upper, upper.positions[20], 20), undefined);
        assert.equal(composedLayerPreviewPaint(state, base, upper, upper.positions[0], 0), mappedOnly.color);
        assert.match(baseEffectPreviewNote(effect), /neutral placeholder/);
    }
});

test("device mapped-only RGB leaves every transparent and disabled alias unpainted", () => {
    for (const key of [...transparent, ...disabled]) {
        assert.equal(layerPreviewPaint(rgb, mappedOnly, key), undefined, key);
    }
    assert.deepEqual(layerPreviewPaint(rgb, mappedOnly, "KC_A"), {h: "0", s: "0", v: "150"});
    assert.equal(layoutKeyKind("0x7E62"), "mapped");
});

test("all-keys RGB also paints transparent keys, while stage disable and pass-through paint none", () => {
    const allKeys = {...mappedOnly, mode: "ALL_KEYS"};
    for (const key of [...transparent, ...disabled, "KC_A"]) {
        assert.equal(layerPreviewPaint(rgb, allKeys, key), allKeys.color);
        assert.equal(layerPreviewPaint({...rgb, layerColorsEnabled: false}, allKeys, key), undefined);
        assert.equal(layerPreviewPaint(rgb, rgb.layerColors[0], key), undefined);
    }
});

test("the delivered key renderer applies the policy before choosing neutral key styles", () => {
    const script = getStudioHtml().match(/<script nonce="[^"]+">([\s\S]*?)<\/script>/)[1];
    const functions = script.slice(script.indexOf("    function keyStyle("), script.indexOf("    function behaviorDotsForKey("));
    const context = vm.createContext({
        model: {rgb, layers: [base, upper]}, activeLayer: "Layer 4", row: mappedOnly,
        layoutKeyKind, layerPreviewPaint, composedLayerPreviewPaint,
        currentLayer: () => upper, layerWithPendingLayoutEdits: layer => layer, layoutToLedIndex: {},
        hsvToHex: color => color.v === "150" ? "#969696" : color.s === "255" ? "#ff0000" : "#000000",
        shadeColor: fill => fill, idealText: () => "#ffffff",
    });
    vm.runInContext("function colorForLayer() { return row; }\n" + functions, context);
    const style = (keycode, value) => context.keyStyle({keycode, value});
    assert.equal(style("KC_TRANSPARENT", 1).fill, "#5f686d");
    assert.equal(style("KC_NO", 0).fill, "#aeb4b7");
    assert.equal(style("KC_A", 4).fill, "#969696");
    // Staged edits change the expression before the original device value.
    assert.equal(style("KC_TRANSPARENT", 4).fill, "#5f686d");
    assert.equal(style("KC_A", 1).fill, "#969696");
    context.row = {...mappedOnly, mode: "ALL_KEYS"};
    assert.equal(style("KC_TRANSPARENT", 1).fill, "#969696");
    assert.equal(style("KC_NO", 0).fill, "#969696");
    context.row = mappedOnly;
    context.model = {rgb: {...rgb, baseEffect}, layers: [base, upper]};
    assert.equal(style("KC_TRANSPARENT", 1).fill, "#ff0000");
    assert.equal(style("KC_NO", 0).fill, "#ff0000");
    assert.equal(style("KC_A", 4).fill, "#969696");
});
