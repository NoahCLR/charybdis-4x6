"use strict";

const assert = require("node:assert/strict");
const test = require("node:test");

const {buildDeviceModel} = require("../../core/session/device-model");
const {decodedDeviceProfile} = require("../fixtures/device-profile");
const {resolve} = require("../../core/data/keycode-catalog");

test("behaviour editing requires supported firmware and a complete idle read", () => {
    const state = {capabilities: {supportedDomainMask: 3, maxTapStepsPerBehavior: 5}, committed: decodedDeviceProfile()};
    assert.equal(buildDeviceModel(state).behaviorEditing.writable, true);
    assert.equal(buildDeviceModel(state).behaviorEditing.maxTapStepsPerBehavior, 5);
    for (const update of [{capabilities: {supportedDomainMask: 1}}, {committed: null}, {busy: true}, {committed: {...state.committed, failures: [{domainId: 0x10}]}}]) {
        assert.equal(buildDeviceModel({...state, ...update}).behaviorEditing.writable, false);
    }
});

test("the picker offers TT, OSL and TO only when the keyboard owns its layer keys", () => {
    assert.equal(buildDeviceModel({}).ownsLayerKeys, false);
    assert.equal(buildDeviceModel({capabilities: {featureFlags: ~(1 << 14) & 0xffff}}).ownsLayerKeys, false);
    assert.equal(buildDeviceModel({capabilities: {featureFlags: 1 << 14}}).ownsLayerKeys, true);
});

// The ported Studio UI renders whatever shape it is given, so these assertions
// pin the contract between the device and that UI. Getting a field name wrong
// here shows up as a silently empty tab, which is exactly the failure the port
// is most exposed to.

const MODEL_FIELDS = [
    "layers", "keyBehaviors", "combos", "viaMacros", "macroNameSpace", "behaviorTimingDefaults",
    "configDefaults", "rgb", "qmkKeycodes", "qmkKeyLabels", "qmkKeycodeAliases",
    "qmkKeycodeSource", "macroPayloadKeycodes", "diagnostics", "ownsLayerKeys",
];

function layoutWith(keys) {
    return {state: "read", layers: [{layer: 0, keys}]};
}

test("device shortcut labels stay complete and semantic names require the advertised ABI", () => {
    const values = [0x0806, 0x0a1d, 0x7e50, 0x7e61, 0x7e62];
    const state = {
        layout: layoutWith(values.map((keycode, layoutIndex) => ({keycode, layoutIndex, resolved: resolve(keycode)}))),
        committed: decodedDeviceProfile(), capabilities: {actionAbiDigest: 0xdcb00959},
    };
    const model = buildDeviceModel(state);
    assert.deepEqual(model.layers[0].positions.map(key => key.display), ["Cmd+C", "Shift+Cmd+Z", "Dragscroll", "Lock Layer 5", "Lock Layer 6"],
        "a layer lock is a user slot under the native ABI, named by what it does to which layer");
    assert.equal(model.layers[0].positions[2].keycode, "QK_USER_16", "the editable identity still encodes to the original numeric value");
    assert.equal(buildDeviceModel({...state, capabilities: {}}).layers[0].positions[2].display, "User 16");
});

test("a position carries what its value means, not only what the keyboard calls it", () => {
    // A pointing mode, a macro and a behaviour target are all stored as plain
    // user keycodes, while every other domain names them semantically. The
    // position publishes both, so a lookup from a key to what it reaches has
    // something to match on and does not re-derive the mapping.
    const values = [0x7e50, 0x7700, 0x0004];
    const model = buildDeviceModel({
        layout: layoutWith(values.map((keycode, layoutIndex) => ({keycode, layoutIndex, resolved: resolve(keycode)}))),
        committed: decodedDeviceProfile(),
        capabilities: {actionAbiDigest: 0xdcb00959},
        macroView: {viaMacros: [{keycode: "VIA_MACRO_0", kind: "via", name: "Sign-off"}]},
    });
    assert.deepEqual(model.layers[0].positions.map((key) => [key.keycode, key.semantic]), [
        ["QK_USER_16", "DRAGSCROLL"], ["QK_MACRO_0", "VIA_MACRO_0"], ["KC_A", "KC_A"]]);
    assert.equal(model.qmkKeyLabels.QK_MACRO_0, "Sign-off", "a named macro is labelled by its name");
});

test("a dual-role key shows its tap the way that keycode is named elsewhere", () => {
    // `LT(3,KC_SLASH)` used to read SLASH on the cap while a plain slash read /,
    // so the same key looked like two different keys across layers.
    const model = buildDeviceModel({
        layout: layoutWith([0x4338, 0x4109].map((keycode, layoutIndex) => ({keycode, layoutIndex, resolved: resolve(keycode)}))),
    });
    assert.deepEqual(model.layers[0].positions.map((key) => [key.keycode, key.display]),
        [["LT(3,KC_SLASH)", "/"], ["LT(1,KC_F)", "F"]]);
});

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
        layoutIndex: 0, keycode: "KC_A", semantic: "KC_A", display: "A", editLabel: "A", row: 0, column: 0, value: 0x0004,
    });
    // Transparent gets a glyph rather than the word, to fit a key cap.
    assert.equal(model.layers[0].positions[1].display, "▽");
});

test("layer and tap-hold keys name their layer without losing the expression", () => {
    const model = buildDeviceModel({
        committed: {state: "read", domains: {settings: {names: ["Base", "Numbers", "Symbols"]}}},
        layout: layoutWith([
            {layoutIndex: 0, row: 0, column: 0, keycode: 0x5222, resolved: {name: "MO(2)", label: "Layer hold 2", kind: "layer", layer: 2, known: true}},
            {layoutIndex: 1, row: 0, column: 1, keycode: 0x4105, resolved: {name: "LT(1,KC_B)", label: "B / layer 1", kind: "layer-tap", tap: "KC_B", layer: 1, known: true}},
        ]),
    });

    const [momentary, layerTap] = model.layers[0].positions;
    assert.equal(momentary.display, "Symbols", "the cap names the layer, not its number");
    assert.equal(momentary.editLabel, "Hold Symbols");
    assert.equal(momentary.keycode, "MO(2)", "the raw keycode keeps the number");
    assert.equal(layerTap.display, "B");
    assert.equal(layerTap.layerLabel, "Numbers");
    assert.equal(layerTap.editLabel, "B / Numbers");
    assert.equal(model.qmkKeyLabels["LT(1,KC_B)"], "B / Numbers", "any keycode on the board can be named by keycode");
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
    assert.equal(model.layers[0].positions[0].display, "0xFFFE");
});

test("domains awaiting the payload read stay empty rather than invented", () => {
    const model = buildDeviceModel({capabilities: {compiledLayerCount: 5}});
    assert.deepEqual(model.rgb, {baseEffect: {state: "unread", message: "Base RGB has not been read from the keyboard."}});
    assert.deepEqual(model.keyBehaviors, []);
    assert.deepEqual(model.combos, []);
    assert.deepEqual(model.viaMacros, []);
    assert.match(model.diagnostics.join(" "), /committed profile read/);
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
        status: {committedGeneration: 7, committedDigest: 0xabcd1234, activeGeneration: 7, peerGeneration: 7, peerKnown: true, peerConverged: true},
    }).device;
    assert.equal(connected.connected, true);
    assert.equal(connected.label, "bastardkb Charybdis 4x6");
    assert.match(connected.summary, /generation 7/);
    assert.match(connected.summary, /0xABCD1234/);
});

test("half divergence is stated plainly rather than shown as healthy", () => {
    const converged = buildDeviceModel({
        capabilities: {}, status: {committedGeneration: 4, activeGeneration: 4, peerGeneration: 4, committedDigest: 1, peerKnown: true, peerConverged: true},
    }).device;
    assert.match(converged.summary, /both halves agree/);

    const diverged = buildDeviceModel({
        capabilities: {}, status: {committedGeneration: 4, activeGeneration: 4, peerGeneration: 3, committedDigest: 1, peerKnown: true, peerConverged: false},
    }).device;
    assert.match(diverged.summary, /not converged/);

    for (const peer of [{peerKnown: false, peerConverged: false}, {peerKnown: true, peerConverged: false}, {peerKnown: true, peerConverged: true, conflictCount: 1}]) {
        const absentOrUnready = buildDeviceModel({capabilities: {}, status: {
            committedGeneration: 0, activeGeneration: 0, peerGeneration: 0, committedDigest: 0, ...peer,
        }}).device;
        assert.equal(absentOrUnready.health.converged, false);
        assert.match(absentOrUnready.summary, /not converged/);
    }
});

test("the header exposes one health model for connection, convergence and recovery", () => {
    const disconnected = buildDeviceModel({phase: "empty", error: {message: "Keyboard unavailable"}}).device;
    assert.deepEqual(disconnected.health, {
        profile: "unavailable",
        converged: false,
        recoveryPending: false,
        restartNeeded: false,
        busy: false,
        phase: "empty",
        error: "Keyboard unavailable",
    });

    const connected = buildDeviceModel({
        capabilities: {},
        busy: true,
        phase: "reading profile",
        status: {committedGeneration: 4, activeGeneration: 4, peerGeneration: 4, committedDigest: 1, candidatePending: true, peerKnown: true, peerConverged: true},
        mutationCompatibility: {recoveryPending: true},
    }).device;
    assert.equal(connected.health.profile, "synced");
    assert.equal(connected.health.converged, true);
    assert.equal(connected.health.recoveryPending, true);
    assert.equal(connected.health.busy, true);
    assert.equal(connected.health.phase, "reading profile");
    assert.equal(connected.health.restartNeeded, false);
    const wedged = buildDeviceModel({
        capabilities: {compiledLayerCount: 8},
        status: {committedGeneration: 4, activeGeneration: 4, peerGeneration: 4, committedDigest: 1, peerKnown: true, peerConverged: true, peerCleanupPending: true},
    }).device;
    assert.equal(wedged.health.restartNeeded, true, "the rail asks for a restart while the peer holds a cancelled save");
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
    const {rgb, keyBehaviors: behaviors} = decodedDeviceProfile().domains;

    const reading = buildDeviceModel({committed: {state: "reading", progress: {done: 10, total: 320}}});
    assert.deepEqual(reading.rgb, {baseEffect: {state: "unread", message: "Base RGB has not been read from the keyboard."}}, "a partial read must not render as device state");
    assert.deepEqual(reading.keyBehaviors, []);

    const done = buildDeviceModel({committed: committedRead({domains: {rgb, keyBehaviors: behaviors}})});
    assert.equal(done.rgb.layerColors[0].layer, "Layer 0");
    assert.equal(done.rgb.layerColors.length, 5);
    assert.equal(done.keyBehaviors.length, 37);
    assert.ok(done.keyBehaviors.every(row => row.keycode && row.steps.every(step => Number.isInteger(step.tapCount))));
});


test("a keyboard with no committed profile says so instead of looking broken", () => {
    const model = buildDeviceModel({capabilities: {}, committed: {state: "none", reason: "no committed profile"}});
    assert.deepEqual(model.rgb, {baseEffect: {state: "unread", message: "Base RGB has not been read from the keyboard."}});
    assert.deepEqual(model.keyBehaviors, []);
    assert.match(model.diagnostics.join(" "), /need the committed profile read/);
});

test("a domain that fails to decode is named, and the others still render", () => {
    const model = buildDeviceModel({
        committed: committedRead({
            domains: {rgb: decodedDeviceProfile().domains.rgb},
            failures: [{domainId: 0x20, message: "row count exceeds the declared limit"}],
        }),
    });

    assert.equal(model.rgb.layerColors.length, 5, "one bad domain must not discard the whole profile");
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
        committed: committedRead({source: "compiled", generation: 0, byteLength: 210, domains: decodedDeviceProfile().domains}),
    });

    assert.equal(model.rgb.layerColors.length, 5, "the tabs must populate from compiled defaults");
    assert.equal(model.keyBehaviors.length, 37);
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

test("a behaviour on a key never renames what the vocabulary already names", () => {
    const committed = decodedDeviceProfile();
    const row = (target) => ({target, tapHoldTerm: 0, longerHoldTerm: 0, multiTapTerm: 0, keepsAutoMouseAnchored: false, steps: []});
    committed.domains = {...committed.domains, keyBehaviors: {...committed.domains.keyBehaviors, rows: [
        row({kind: 2, flags: 0, operand: 1}),   // MO(1)
        row({kind: 3, flags: 0, operand: 2}),   // LOCK_LAYER(2)
        row({kind: 6, flags: 0, operand: 0}),   // VIA_MACRO_0
    ]}};
    const macroView = {viaMacros: [{kind: "via", keycode: "VIA_MACRO_0", payload: ""}], hardcodedMacros: []};
    const labels = buildDeviceModel({committed, macroView, capabilities: {actionAbiDigest: 0xdcb00959}}).qmkKeyLabels;
    const plain = buildDeviceModel({macroView, capabilities: {actionAbiDigest: 0xdcb00959}}).qmkKeyLabels;
    assert.equal(labels["MO(1)"], plain["MO(1)"], "MO(1) reads as it does with no behaviour on it, not \"Mo(1)\"");
    assert.equal(labels.QK_MACRO_0, "Macro 0", "the macro screen's name");
    assert.equal(labels.QK_USER_30, "Lock Layer 2", "a bare user slot reads by what it does to which layer");
    assert.equal(labels["LOCK_LAYER(2)"], "Lock Layer 2");
});
