import assert from "node:assert/strict";
import test from "node:test";
import {isOff} from "../webview/lib/colour.mjs";
import {baseColour, feedbackColours, keyLight, pdColourRow, stageEnabled} from "../webview/view/lighting.mjs";

const colour = (h, s, v) => ({h: String(h), s: String(s), v: String(v)});
const position = (layoutIndex, keycode) => ({layoutIndex, keycode});

function model(overrides = {}) {
    return {
        rgb: {
            baseEffect: {state: "read", enabled: true, effectId: 1, previewColor: colour(140, 210, 180)},
            stages: [
                {label: "Layer colours", enabled: true},
                {label: "Auto-mouse fade", enabled: true},
                {label: "Pointing modes", enabled: true},
                {label: "Combo feedback", enabled: false},
                {label: "Key behaviour feedback", enabled: true},
            ],
            layerColors: [
                {layer: "Layer 0", layerId: 0, color: colour(0, 0, 0), mode: "ALL_KEYS"},
                {layer: "Layer 3", layerId: 3, color: colour(180, 255, 200), mode: "KEYS_MAPPED_ON_THIS_LAYER_ONLY"},
                {layer: "Layer 4", layerId: 4, color: colour(0, 0, 158), mode: "ALL_KEYS"},
            ],
            pdModeColors: [{pointingMode: "PD_MODE_DRAGSCROLL", color: colour(21, 255, 200), locality: "RGB_RIGHT_HALF"}],
            comboFeedback: {color: colour(0, 0, 0), locality: "RGB_KEYS_ONLY"},
            keyBehaviorFeedback: {
                tapCommittedColor: colour(0, 0, 158), holdActiveColor: colour(18, 255, 200),
                longHoldActiveColor: colour(148, 255, 200), tapBranchColors: [colour(169, 255, 200)],
            },
            ...overrides.rgb,
        },
        ...overrides,
    };
}

test("stages report what the device said, and the base effect is its own read", () => {
    const m = model();
    assert.equal(stageEnabled(m, "layers"), true);
    assert.equal(stageEnabled(m, "combo"), false);
    assert.equal(stageEnabled(m, "base"), true);
    assert.deepEqual(baseColour(m), colour(140, 210, 180));
    const unread = model({rgb: {baseEffect: {state: "unread"}}});
    assert.equal(baseColour(unread), null, "an unread base effect has no colour to draw");
    const animated = model({rgb: {baseEffect: {state: "read", enabled: true, effectId: 3}}});
    assert.equal(baseColour(animated), null, "an animated effect has no single colour");
});

test("a mapped-keys-only layer paints its own keys and leaves the rest on the base", () => {
    const m = model();
    const layer = {index: 3};
    const mapped = keyLight(m, layer, position(13, "KC_UP"));
    assert.deepEqual(mapped.colour, colour(180, 255, 200));
    assert.equal(mapped.source, "layer");
    const through = keyLight(m, layer, position(14, "KC_TRANSPARENT"));
    assert.deepEqual(through.colour, colour(140, 210, 180), "a transparent key shows the base effect");
    assert.equal(through.source, "base");
});

test("an all-keys layer paints transparent positions too", () => {
    const light = keyLight(model(), {index: 4}, position(14, "KC_TRANSPARENT"));
    assert.deepEqual(light.colour, colour(0, 0, 158));
    assert.equal(light.source, "layer");
});

test("a layer stored as black paints nothing, and a disabled stage paints nothing", () => {
    const m = model();
    assert.equal(keyLight(m, {index: 0}, position(13, "KC_A")).source, "base", "HSV value zero is not a colour");
    const off = model({rgb: {...model().rgb, stages: [{label: "Layer colours", enabled: false}]}});
    const light = keyLight(off, {index: 3}, position(13, "KC_UP"));
    assert.equal(light.source, "base", "with the stage off the layer colour is not painted");
});

test("with no base effect and nothing else painting, a key is simply unlit", () => {
    const dark = model({rgb: {...model().rgb, baseEffect: {state: "read", enabled: false}}});
    const light = keyLight(dark, {index: 0}, position(13, "KC_A"));
    assert.equal(isOff(light.colour), true);
    assert.equal(light.source, "off");
});

test("a pointing mode is an overlay on its locality, never on the key that binds it", () => {
    const m = model();
    const row = pdColourRow(m, 0);
    assert.deepEqual(row.color, colour(21, 255, 200));
    const preview = {color: row.color, locality: row.locality, triggerIndex: 0};

    const bindingKey = keyLight(m, {index: 3}, position(0, "DRAGSCROLL"));
    assert.notDeepEqual(bindingKey.colour, row.color, "binding a mode does not colour its key");

    const right = keyLight(m, {index: 3}, position(7, "KC_TRANSPARENT"), {pdActive: preview});
    assert.deepEqual(right.colour, row.color, "the right half takes the overlay");
    assert.equal(right.source, "pointing");

    const left = keyLight(m, {index: 3}, position(0, "KC_TRANSPARENT"), {pdActive: preview});
    assert.equal(left.source, "base", "the left half is outside this locality");
    const leftMapped = keyLight(m, {index: 3}, position(0, "DRAGSCROLL"), {pdActive: preview});
    assert.equal(leftMapped.source, "layer", "and there the layer still owns its own keys");

    const stageOff = model({rgb: {...model().rgb, stages: [{label: "Pointing modes", enabled: false}]}});
    assert.equal(keyLight(stageOff, {index: 3}, position(7, "KC_NO"), {pdActive: preview}).source, "base");
});

test("feedback colours come back per semantic, missing ones as black", () => {
    const colours = feedbackColours(model());
    assert.deepEqual(colours.hold, colour(18, 255, 200));
    assert.deepEqual(colours.branches[0], colour(169, 255, 200));
    assert.equal(isOff(feedbackColours({}).tap), true);
});
