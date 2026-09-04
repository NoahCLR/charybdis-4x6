"use strict";

const assert = require("node:assert/strict");
const test = require("node:test");

const {buildDeviceModel} = require("../../core/session/device-model");

// The ported Studio UI renders whatever shape it is given, so these assertions
// pin the contract between the device and that UI. Getting a field name wrong
// here shows up as a silently empty tab, which is exactly the failure the port
// is most exposed to.

const MODEL_FIELDS = [
    "root", "profiles", "activeProfile", "files", "layers", "customKeycodes",
    "keyBehaviors", "combos", "viaMacros", "hardcodedMacros", "behaviorTimingDefaults",
    "configDefaults", "rgb", "qmkKeycodes", "qmkKeyLabels", "qmkKeycodeAliases",
    "qmkKeycodeSource", "macroPayloadKeycodes", "diagnostics",
];

function layoutWith(keys) {
    return {state: "read", layers: [{layer: 0, keys}]};
}

test("the model always carries every field the UI reads", () => {
    const model = buildDeviceModel({});
    for (const field of MODEL_FIELDS) {
        assert.ok(field in model, `model is missing ${field}`);
    }
});

test("the keycode catalog reaches the UI in Studio's shape", () => {
    const model = buildDeviceModel({});
    assert.ok(model.qmkKeycodes.length > 500);

    // Studio's entries key on the keycode name, not the number.
    const a = model.qmkKeycodes.find((entry) => entry.value === "KC_A");
    assert.equal(a.label, "A");
    assert.equal(a.keycode, 0x0004);
    assert.ok(a.search.includes("kc_a"));

    assert.equal(model.qmkKeyLabels.KC_A, "A");
    assert.equal(model.qmkKeycodeAliases.KC_A, "KC_A");
    assert.equal(model.qmkKeycodeAliases._______, "_______");
});

test("no layers are reported until the device has actually been read", () => {
    assert.deepEqual(buildDeviceModel({}).layers, []);
    assert.deepEqual(buildDeviceModel({layout: {state: "reading", layers: []}}).layers, []);
    assert.match(buildDeviceModel({}).diagnostics.join(" "), /has not been read/);
});

test("device layers become the layer model the UI keys on", () => {
    const model = buildDeviceModel({
        layout: layoutWith([
            {layoutIndex: 0, row: 0, column: 0, keycode: 0x0004, resolved: {name: "KC_A", label: "A", kind: "basic", known: true}},
            {layoutIndex: 1, row: 0, column: 1, keycode: 0x0001, resolved: {name: "KC_TRANSPARENT", label: "Transparent", kind: "basic", known: true}},
        ]),
    });

    assert.equal(model.layers.length, 1);
    assert.equal(model.layers[0].name, "Layer 0");
    assert.equal(model.layers[0].index, 0);
    assert.deepEqual(model.layers[0].positions[0], {
        layoutIndex: 0, keycode: "KC_A", display: "A", editLabel: "A", row: 0, column: 0, value: 0x0004,
    });
    // Transparent gets a glyph rather than the word, to fit a key cap.
    assert.equal(model.layers[0].positions[1].display, "▽");
});

test("layer and tap-hold keys show their target without losing the expression", () => {
    const model = buildDeviceModel({
        layout: layoutWith([
            {layoutIndex: 0, row: 0, column: 0, keycode: 0x5222, resolved: {name: "MO(2)", label: "Layer hold 2", kind: "layer", layer: 2, known: true}},
            {layoutIndex: 1, row: 0, column: 1, keycode: 0x4105, resolved: {name: "LT(1,KC_B)", label: "B / layer 1", kind: "layer-tap", tap: "KC_B", layer: 1, known: true}},
        ]),
    });

    const [momentary, layerTap] = model.layers[0].positions;
    assert.equal(momentary.display, "L2");
    assert.equal(momentary.keycode, "MO(2)");
    assert.equal(layerTap.display, "B");
    assert.equal(layerTap.keycode, "LT(1,KC_B)");
});

test("an unresolved keycode is shown, not hidden", () => {
    const model = buildDeviceModel({
        layout: layoutWith([
            {layoutIndex: 0, row: 0, column: 0, keycode: 0xfffe, resolved: {name: "0xFFFE", label: "0xFFFE", kind: "unknown", known: false}},
        ]),
    });
    assert.equal(model.layers[0].positions[0].keycode, "0xFFFE");
    assert.equal(model.layers[0].positions[0].value, 0xfffe);
});

test("domains awaiting the payload read stay empty rather than invented", () => {
    const model = buildDeviceModel({capabilities: {compiledLayerCount: 5}});
    assert.deepEqual(model.rgb, {});
    assert.deepEqual(model.keyBehaviors, []);
    assert.deepEqual(model.combos, []);
    assert.deepEqual(model.viaMacros, []);
    assert.match(model.diagnostics.join(" "), /committed profile read/);
});

test("the connected device stands in for Studio's profile files", () => {
    const model = buildDeviceModel({
        capabilities: {compiledLayerCount: 5},
        device: {id: "dev0", manufacturer: "bastardkb", product: "Charybdis 4x6"},
    });
    assert.equal(model.activeProfile.name, "bastardkb Charybdis 4x6");
    assert.equal(model.activeProfile.id, "dev0");
    assert.deepEqual(model.profiles, []);
    assert.equal(model.root, "");
    // No file paths, because there are no files.
    assert.equal(model.activeProfile.keymapPath, "");
});
