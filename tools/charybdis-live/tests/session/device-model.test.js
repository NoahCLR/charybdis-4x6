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

// The header is the first thing a user sees, and getting it wrong was the
// original sin of the port: it showed profile files and firmware compile
// buttons in an app that edits a keyboard.

test("the header reports the keyboard, not a profile directory", () => {
    const disconnected = buildDeviceModel({}).device;
    assert.equal(disconnected.connected, false);
    assert.match(disconnected.label + disconnected.summary, /^$/);

    const connected = buildDeviceModel({
        capabilities: {compiledLayerCount: 5},
        device: {manufacturer: "bastardkb", product: "Charybdis 4x6"},
        status: {committedGeneration: 7, committedDigest: 0xabcd1234, activeGeneration: 7, peerGeneration: 7},
    }).device;
    assert.equal(connected.connected, true);
    assert.equal(connected.label, "bastardkb Charybdis 4x6");
    assert.match(connected.summary, /generation 7/);
    assert.match(connected.summary, /0xABCD1234/);
});

test("half divergence is stated plainly rather than shown as healthy", () => {
    const converged = buildDeviceModel({
        capabilities: {}, status: {committedGeneration: 4, activeGeneration: 4, peerGeneration: 4, committedDigest: 1},
    }).device;
    assert.match(converged.summary, /both halves agree/);

    const diverged = buildDeviceModel({
        capabilities: {}, status: {committedGeneration: 4, activeGeneration: 4, peerGeneration: 3, committedDigest: 1},
    }).device;
    assert.match(diverged.summary, /not converged/);
});

test("the subtitle says what to do next", () => {
    const idle = buildDeviceModel({capabilities: {}}).device;
    assert.match(idle.subtitle, /Read from keyboard/);

    const read = buildDeviceModel({
        capabilities: {},
        layout: {state: "read", layers: [{layer: 0, keys: []}, {layer: 1, keys: []}]},
    }).device;
    assert.match(read.subtitle, /2 layers read/);
});

// The committed profile is what the keyboard is actually running. Showing a
// half-read or failed decode as if it were device state would be the worst
// failure this app can have, so those paths are pinned here.

function committedRead(overrides = {}) {
    return {
        state: "read",
        generation: 12,
        digest: 0xdeadbeef,
        byteLength: 320,
        domainIds: [0x10, 0x20],
        domains: {},
        failures: [],
        ...overrides,
    };
}

test("decoded domains reach the UI only after a verified read", () => {
    const rgb = {stageEnableMask: 3, layerColors: [{layerId: 0}]};
    const behaviors = {rows: [{keycode: 4}]};

    const reading = buildDeviceModel({committed: {state: "reading", progress: {done: 10, total: 320}}});
    assert.deepEqual(reading.rgb, {}, "a partial read must not render as device state");
    assert.deepEqual(reading.keyBehaviors, []);

    const done = buildDeviceModel({committed: committedRead({domains: {rgb, keyBehaviors: behaviors}})});
    assert.deepEqual(done.rgb, rgb);
    assert.deepEqual(done.keyBehaviors, behaviors.rows);
});

test("key behaviours decode whether the domain is a list or a wrapper", () => {
    const asRows = buildDeviceModel({committed: committedRead({domains: {keyBehaviors: {rows: [{keycode: 1}]}}})});
    assert.deepEqual(asRows.keyBehaviors, [{keycode: 1}]);

    const asArray = buildDeviceModel({committed: committedRead({domains: {keyBehaviors: [{keycode: 2}]}})});
    assert.deepEqual(asArray.keyBehaviors, [{keycode: 2}]);
});

test("a keyboard with no committed profile says so instead of looking broken", () => {
    const model = buildDeviceModel({capabilities: {}, committed: {state: "none", reason: "no committed profile"}});
    assert.deepEqual(model.rgb, {});
    assert.deepEqual(model.keyBehaviors, []);
    assert.match(model.diagnostics.join(" "), /need the committed profile read/);
});

test("a domain that fails to decode is named, and the others still render", () => {
    const model = buildDeviceModel({
        committed: committedRead({
            domains: {rgb: {stageEnableMask: 1}},
            failures: [{domainId: 0x20, message: "row count exceeds the declared limit"}],
        }),
    });

    assert.deepEqual(model.rgb, {stageEnableMask: 1}, "one bad domain must not discard the whole profile");
    assert.deepEqual(model.keyBehaviors, []);
    assert.match(model.diagnostics.join(" "), /0x20 did not decode/);
    assert.match(model.diagnostics.join(" "), /row count exceeds/);
});

test("the header reports the generation once the profile is read", () => {
    const model = buildDeviceModel({
        capabilities: {},
        layout: {state: "read", layers: [{layer: 0, keys: []}]},
        committed: committedRead({generation: 12}),
    });
    assert.match(model.device.subtitle, /1 layers and generation 12 read/);
    assert.match(model.diagnostics.join(" "), /generation 12 read from the keyboard \(320 bytes\)/);
});

// A keyboard with nothing committed is still running something. Showing its
// compiled defaults is the honest answer; showing an empty editor is not. But
// the two must never be confused for each other.

test("compiled defaults are shown, and labelled as compiled", () => {
    const model = buildDeviceModel({
        capabilities: {},
        status: {committedGeneration: 0, committedDigest: 0, activeGeneration: 0, peerGeneration: 0},
        committed: committedRead({source: "compiled", generation: 0, byteLength: 210, domains: {rgb: {stageEnableMask: 7}}}),
    });

    assert.deepEqual(model.rgb, {stageEnableMask: 7}, "the tabs must populate from compiled defaults");
    assert.match(model.diagnostics.join(" "), /compiled defaults/);
    assert.match(model.diagnostics.join(" "), /Nothing is committed/);
    assert.match(model.device.subtitle, /compiled defaults/);
});

test("an uncommitted keyboard does not claim generation 0 as a generation", () => {
    const model = buildDeviceModel({
        capabilities: {},
        status: {committedGeneration: 0, committedDigest: 0, activeGeneration: 0, peerGeneration: 0},
    });
    assert.match(model.device.summary, /no committed profile/);
    assert.doesNotMatch(model.device.summary, /generation 0/);
});

test("a committed profile is still reported as committed", () => {
    const model = buildDeviceModel({
        capabilities: {},
        status: {committedGeneration: 7, committedDigest: 0xabcd1234, activeGeneration: 7, peerGeneration: 7},
        committed: committedRead({source: "committed", generation: 7}),
    });
    assert.match(model.device.summary, /generation 7/);
    assert.doesNotMatch(model.device.summary, /compiled defaults/);
    assert.match(model.diagnostics.join(" "), /Committed profile generation 7/);
});
