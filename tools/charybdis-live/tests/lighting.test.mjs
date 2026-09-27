import assert from "node:assert/strict";
import test from "node:test";
import {isOff} from "../webview/lib/colour.mjs";
import {baseColour, feedbackColours, keyLight, mappedKeyCount, ownLight, pdColourRow, stageEnabled, stageIdle, stageInEffect, trackballLight} from "../webview/view/lighting.mjs";

const colour = (h, s, v) => ({h: String(h), s: String(s), v: String(v)});
const position = (layoutIndex, keycode) => ({layoutIndex, keycode});

test("mapped-key count follows the selected layer, excluding transparent and disabled keys", () => {
    const layer = {positions: [position(0, "KC_A"), position(1, "KC_B"), position(2, "KC_TRANSPARENT"),
        position(3, "KC_TRNS"), position(4, "_______"), position(5, "KC_NO"), position(6, "XXXXXXX"), position(7, "")]};
    assert.equal(mappedKeyCount(layer), 2);
    assert.equal(mappedKeyCount({positions: [position(0, "KC_TRANSPARENT")]}), 0);
    assert.equal(mappedKeyCount(null), 0);
});

function model(overrides = {}) {
    return {
        rgb: {
            baseEffect: {state: "read", enabled: true, effectId: 1, previewColor: colour(140, 210, 180)},
            stages: [
                {id: "layers", label: "Layer colours", enabled: true},
                {id: "auto", label: "Auto-mouse fade", enabled: true},
                {id: "pd", label: "Pointing modes", enabled: true},
                {id: "combo", label: "Combo feedback", enabled: false},
                {id: "key", label: "Key feedback", enabled: true},
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

// The fade runs only while auto-mouse moves the layer, so with auto-mouse off
// in the draft the stage paints nothing, whatever its stored switch says.
test("the auto-mouse fade is in effect only while auto-mouse is on", () => {
    const withAutoMouse = (enabled) => ({...model(), configDefaults: [
        {id: "autoMouse", area: "Mouse", fields: [{macro: "autoMouse", kind: "toggle", value: enabled ? "1" : "0", enabled}]}]});
    assert.equal(stageInEffect(withAutoMouse(true), "auto"), true);
    assert.equal(stageIdle(withAutoMouse(true), "auto"), null);
    const off = withAutoMouse(false);
    assert.equal(stageEnabled(off, "auto"), true, "the stored switch is left as it is");
    assert.equal(stageInEffect(off, "auto"), false);
    assert.match(stageIdle(off, "auto"), /auto-mouse is off/i);
    assert.equal(stageInEffect(off, "layers"), true, "other stages do not depend on auto-mouse");
    const stored = model();
    stored.rgb.stages = stored.rgb.stages.map((row) => row.id === "auto" ? {...row, enabled: false} : row);
    assert.equal(stageIdle({...stored, configDefaults: off.configDefaults}, "auto"), null, "a stage switched off is off, not idle");
    assert.equal(stageInEffect(model(), "auto"), true, "without settings readback nothing is claimed");
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

test("layers previewed on paint in the same ascending pass, under the viewed layer", () => {
    const m = model();
    m.rgb.layerColors = [...m.rgb.layerColors, {layer: "Layer 1", layerId: 1, color: colour(85, 255, 200), mode: "ALL_KEYS"}];
    const layer = {index: 3};
    assert.equal(keyLight(m, layer, position(14, "KC_TRANSPARENT")).source, "base", "alone, layer 3 leaves an unmapped key on the base");
    assert.deepEqual(keyLight(m, layer, position(14, "KC_TRANSPARENT"), {held: [1]}).colour, colour(85, 255, 200),
        "with layer 1 on, its all-keys wash shows under layer 3's unmapped key");
    assert.deepEqual(keyLight(m, layer, position(13, "KC_UP"), {held: [1]}).colour, colour(180, 255, 200),
        "layer 3 still paints its own keys over layer 1");
    assert.deepEqual(trackballLight(m, layer, {held: [1]}).colour, colour(85, 255, 200), "an all-keys wash reaches the trackball LED");
});

test("the trackball LED says which layer painted it, so the board can tell its own from what shows through", () => {
    const m = model();
    assert.deepEqual(trackballLight(m, {index: 4}), {colour: colour(0, 0, 158), source: "layer", layer: 4},
        "an all-keys layer paints the trackball itself");
    assert.deepEqual(trackballLight(m, {index: 3}), {colour: colour(140, 210, 180), source: "base", layer: null},
        "a mapped-keys-only layer never reaches it, so the base effect shows");
    assert.equal(trackballLight(m, {index: 3}, {held: [4]}).layer, 4, "a lower layer on in a preview paints it from underneath");
    const grouped = model({rgb: {...model().rgb, layerLedGroups: [{owner: "Layer 3", ledIndices: [56], color: colour(0, 0, 0)}]}});
    const light = trackballLight(grouped, {index: 3});
    assert.equal(light.source, "layer");
    assert.equal(light.layer, 3, "a layer's LED group containing LED 56 paints it as that layer");
});

test("the trackball's light is the viewed layer's own unless it is seen through from beneath", () => {
    const m = model();
    assert.equal(ownLight(trackballLight(m, {index: 0}), 0), true, "on base the base effect is base's own floor, not something below");
    assert.equal(ownLight(trackballLight(m, {index: 4}), 4), true, "an all-keys layer paints it itself");
    assert.equal(ownLight(trackballLight(m, {index: 3}), 3), false, "above base, the base effect is seen through a mapped-keys-only layer");
    assert.equal(ownLight(trackballLight(m, {index: 3}, {held: [4]}), 3), false, "a lower layer's paint is seen through too");
    assert.equal(ownLight({source: "pointing", layer: null}, 3), true, "a previewed pointing mode is what is being shown");
});

test("an all-keys layer paints transparent positions too", () => {
    const light = keyLight(model(), {index: 4}, position(14, "KC_TRANSPARENT"));
    assert.deepEqual(light.colour, colour(0, 0, 158));
    assert.equal(light.source, "layer");
});

test("a layer stored as black paints nothing, and a disabled stage paints nothing", () => {
    const m = model();
    assert.equal(keyLight(m, {index: 0}, position(13, "KC_A")).source, "base", "HSV value zero is not a colour");
    const off = model({rgb: {...model().rgb, stages: [{id: "layers", label: "Layer colours", enabled: false}]}});
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

    const bindingKey = keyLight(m, {index: 3}, position(0, "PD_SLOT_0"));
    assert.notDeepEqual(bindingKey.colour, row.color, "binding a mode does not colour its key");

    const right = keyLight(m, {index: 3}, position(7, "KC_TRANSPARENT"), {pdActive: preview});
    assert.deepEqual(right.colour, row.color, "the right half takes the overlay");
    assert.equal(right.source, "pointing");

    const left = keyLight(m, {index: 3}, position(0, "KC_TRANSPARENT"), {pdActive: preview});
    assert.equal(left.source, "base", "the left half is outside this locality");
    const leftMapped = keyLight(m, {index: 3}, position(0, "PD_SLOT_0"), {pdActive: preview});
    assert.equal(leftMapped.source, "layer", "and there the layer still owns its own keys");

    const stageOff = model({rgb: {...model().rgb, stages: [{id: "pd", label: "Pointing modes", enabled: false}]}});
    assert.equal(keyLight(stageOff, {index: 3}, position(7, "KC_NO"), {pdActive: preview}).source, "base");
});

test("feedback colours come back per semantic, missing ones as black", () => {
    const colours = feedbackColours(model());
    assert.deepEqual(colours.hold, colour(18, 255, 200));
    assert.deepEqual(colours.branches[0], colour(169, 255, 200));
    assert.equal(isOff(feedbackColours({}).tap), true);
});

test("the trackball LED is lit like any other: base effect, and an all-keys wash", () => {
    const m = model();
    const base = trackballLight(m, {index: 3});
    assert.deepEqual(base.colour, colour(140, 210, 180), "a mapped-keys-only layer never reaches it");
    assert.equal(base.source, "base");

    const wash = trackballLight(m, {index: 4});
    assert.deepEqual(wash.colour, colour(0, 0, 158), "an all-keys layer paints every LED, this one too");
    assert.equal(wash.source, "layer");

    const dark = model({rgb: {...model().rgb, baseEffect: {state: "read", enabled: false}}});
    assert.equal(isOff(trackballLight(dark, {index: 3}).colour), true);

    const row = pdColourRow(m, 0);
    const held = trackballLight(m, {index: 3}, {pdActive: {color: row.color, locality: row.locality}});
    assert.deepEqual(held.colour, row.color, "a right-half overlay covers the trackball LED");
    assert.equal(held.source, "pointing");
});

// The fixture model with some rgb fields replaced and the rest (stages, base
// effect) kept, which model({rgb}) does not do.
const withRgb = (rgb) => { const m = model(); Object.assign(m.rgb, rgb); return m; };

test("with the matrix off the keyboard is dark, whatever the layers hold", () => {
    const m = withRgb({baseEffect: {state: "read", enabled: false, effectId: 0}});
    assert.equal(keyLight(m, {index: 4}, position(13, "KC_UP")).source, "off", "an all-keys layer does not paint");
    assert.equal(isOff(trackballLight(m, {index: 4}).colour), true);
});

test("the default layer's colour shows under a mapped-keys-only layer", () => {
    const m = withRgb({layerColors: [
        {layer: "Layer 0", layerId: 0, color: colour(0, 0, 80), mode: "ALL_KEYS"},
        {layer: "Layer 1", layerId: 1, color: colour(85, 255, 200), mode: "KEYS_MAPPED_ON_THIS_LAYER_ONLY"},
    ]});
    const layer = {index: 1};
    assert.deepEqual(keyLight(m, layer, position(13, "KC_1")).colour, colour(85, 255, 200), "the viewed layer wins on its own keys");
    assert.deepEqual(keyLight(m, layer, position(14, "KC_TRANSPARENT")).colour, colour(0, 0, 80), "layer 0 paints the rest");
    assert.deepEqual(trackballLight(m, layer).colour, colour(0, 0, 80), "and the trackball, since it is all keys");
    // HSV(85, 255, 0) is solid to the firmware — it paints black, not through.
    const black = withRgb({layerColors: [{layer: "Layer 2", layerId: 2, color: colour(85, 255, 0), mode: "ALL_KEYS"}]});
    assert.deepEqual(keyLight(black, {index: 2}, position(13, "KC_1")).colour, colour(85, 255, 0));
});

test("LED group rows paint their LEDs, inheriting their stage colour when stored as HSV(0, 0, 0)", () => {
    const m = withRgb({
        layerColors: [{layer: "Layer 3", layerId: 3, color: colour(180, 255, 200), mode: "KEYS_MAPPED_ON_THIS_LAYER_ONLY"}],
        // LED 6 is key 13; LED 9 is key 14; LED 56 is the trackball
        layerLedGroups: [
            {owner: "Layer 3", color: colour(0, 0, 0), ledIndices: [9]},
            {owner: "RGB_LAYER_GROUP_ALL", color: colour(40, 255, 120), ledIndices: [56]},
            {owner: "Layer 5", color: colour(10, 255, 255), ledIndices: [6]},
        ],
        pdModeLedGroups: [{owner: "RGB_PD_MODE_GROUP_ALL", color: colour(0, 0, 0), ledIndices: [6]}],
    });
    const layer = {index: 3};
    assert.deepEqual(keyLight(m, layer, position(14, "KC_TRANSPARENT")).colour, colour(180, 255, 200), "inherits the layer colour on an unmapped key");
    assert.deepEqual(trackballLight(m, layer).colour, colour(40, 255, 120), "a group can reach the trackball");
    assert.deepEqual(keyLight(m, layer, position(13, "KC_UP")).colour, colour(180, 255, 200), "a row for an inactive layer does not paint");
    const pdActive = {mode: "PD_MODE_DRAGSCROLL", color: colour(21, 255, 200), locality: "RGB_RIGHT_HALF"};
    assert.deepEqual(keyLight(m, layer, position(13, "KC_UP"), {pdActive}).colour, colour(21, 255, 200),
        "a pointing group row paints the mode colour on the left half too");
});
