import assert from "node:assert/strict";
import test from "node:test";
import {behaviourFor, behaviourGridSteps, behaviourGroups, behaviourTiers, bindingKeycode, bindingsForSlot, comboKeysOnLayer, combosForKey, keyFace, keyMeaning, macroKeycodes, pointingSlotFor, slotKeycodes} from "../webview/view/keyface.mjs";

test("a key face uses the model's own resolution, and names the layer a dual-role key reaches", () => {
    assert.deepEqual(keyFace({keycode: "KC_TRANSPARENT", display: "▽"}), {main: "▽", sub: "", kind: "transparent"});
    assert.deepEqual(keyFace({keycode: "KC_NO", display: ""}), {main: "", sub: "", kind: "disabled"});
    assert.deepEqual(keyFace({keycode: "KC_A", display: "A"}), {main: "A", sub: "", kind: "key"});
    assert.deepEqual(keyFace({keycode: "LT(LAYER_NAV,KC_F)", display: "F"}), {main: "F", sub: "nav", kind: "layer"});
    assert.deepEqual(keyFace({keycode: "MO(LAYER_SYM)", display: "L2"}), {main: "L2", sub: "momentary", kind: "layer"});
    assert.equal(keyFace(undefined).kind, "none");
});

test("tiers count the branches that use them, so a key shows what it can do", () => {
    const model = {keyBehaviors: [{
        keycode: "LEFT_THUMB",
        steps: [
            {tapCount: 0, tap: {action: "LOCK_LAYER(LAYER_SYM)"}, hold: {action: "MO(LAYER_SYM)"}},
            {tapCount: 1, tap: {action: "KC_MPLY"}, hold: {action: "KC_ESC"}, longHold: {action: "LOCK_LAYER(LAYER_NUM)"}},
            {tapCount: 2, tap: {action: "KC_MNXT"}, longHold: {action: "KC_MNXT"}},
        ],
    }]};
    const behaviour = behaviourFor(model, "LEFT_THUMB");
    assert.deepEqual(behaviourTiers(behaviour), [{kind: "tap", count: 3}, {kind: "hold", count: 2}, {kind: "long", count: 2}]);
    assert.deepEqual(behaviourTiers(undefined), []);
    assert.equal(behaviourFor(model, "KC_A"), undefined);
});

test("the behaviour editor keeps every supported tap column visible", () => {
    const behaviour = {steps: [{tapCount: 0, tap: {action: "KC_A"}}, {tapCount: 3, hold: {action: "KC_B"}}]};
    const steps = behaviourGridSteps(behaviour, 5);
    assert.deepEqual(steps.map((step) => step.tapCount), [0, 1, 2, 3, 4]);
    assert.equal(steps[0], behaviour.steps[0], "populated cells keep the device-backed row");
    assert.deepEqual(steps[1], {tapCount: 1}, "missing cells are empty editor slots");
    assert.equal(steps[3], behaviour.steps[1]);
    assert.equal(behaviourGridSteps(behaviour, 3).length, 3, "a lower advertised device limit is respected");
});

test("combos follow the device's own input positions before falling back to keycodes", () => {
    const model = {combos: [
        {badge: "C1", inputs: ["KC_D", "KC_F"], inputPositions: [27, 28], output: "KC_ESC"},
        {badge: "C2", inputs: ["KC_J", "KC_K"], output: "KC_TAB"},
    ]};
    assert.deepEqual(combosForKey(model, {layoutIndex: 27, keycode: "KC_D"}).map((c) => c.badge), ["C1"]);
    assert.deepEqual(combosForKey(model, {layoutIndex: 99, keycode: "KC_J"}).map((c) => c.badge), ["C2"]);
    assert.deepEqual(combosForKey(model, {layoutIndex: 99, keycode: "KC_Z"}), []);
});

test("macro and pointing-mode keycodes resolve to the slots the keyboard reported", () => {
    assert.deepEqual(macroKeycodes("VIA_MACRO_11"), ["VIA_MACRO_11"]);
    assert.deepEqual(macroKeycodes("KC_A"), []);
    const model = {pdModes: [{id: 0, name: "Dragscroll", kind: 2}, {id: 6, name: "", kind: 0}]};
    assert.equal(pointingSlotFor(model, "DRAGSCROLL").name, "Dragscroll");
    assert.equal(pointingSlotFor(model, "DRAGSCROLL_LOCK").name, "Dragscroll", "the lock keycode reaches the same slot");
    assert.equal(pointingSlotFor(model, "PD_SLOT_6").id, 6);
    assert.equal(pointingSlotFor(model, "KC_A"), undefined);
});

test("a key for an empty pointing slot says so on its second line", () => {
    // The catalogue label carries "(empty)" so the picker can tell the two
    // apart; on the cap that belongs under the name, not inside it.
    assert.deepEqual(keyFace({keycode: "0x7EF0", display: "Slot 7 · hold (empty)"}),
        {main: "Slot 7 · hold", sub: "empty", kind: "key"});
    assert.equal(keyFace({keycode: "QK_USER_16", display: "Dragscroll · hold"}).sub, "");
});

test("what a key reaches is looked up by what its value means", () => {
    // The keyboard stores a behaviour target, a macro and a pointing mode as
    // plain user keycodes; every domain that refers back to them uses the
    // semantic name. A lookup that matched the stored name would find nothing.
    const model = {
        keyBehaviors: [{keycode: "DRAGSCROLL", steps: [{tapCount: 1, hold: {action: "KC_ESC"}}]}],
        combos: [{badge: "C1", inputs: ["VIA_MACRO_0", "KC_F"], output: "KC_ESC"}],
        pdModes: [{id: 0, name: "Dragscroll", kind: 2}],
    };
    const pointingKey = {layoutIndex: 3, keycode: "QK_USER_16", semantic: "DRAGSCROLL"};
    const macroKey = {layoutIndex: 4, keycode: "QK_MACRO_0", semantic: "VIA_MACRO_0"};

    assert.equal(keyMeaning(pointingKey), "DRAGSCROLL");
    assert.equal(keyMeaning({keycode: "KC_A"}), "KC_A", "an ordinary key means itself");
    assert.equal(keyMeaning(undefined), "");
    assert.equal(behaviourFor(model, keyMeaning(pointingKey)).keycode, "DRAGSCROLL");
    assert.deepEqual(macroKeycodes(keyMeaning(macroKey)), ["VIA_MACRO_0"]);
    assert.equal(pointingSlotFor(model, keyMeaning(pointingKey)).name, "Dragscroll");
    assert.deepEqual(combosForKey(model, macroKey).map((combo) => combo.badge), ["C1"],
        "a combo input named semantically still finds its key");
});

test("a pointing key is recognised as the keyboard names it, not only as the app labels it", () => {
    // Layout positions carry the device's own name for the value: a bare user
    // keycode for the six named modes, and plain hex for a slot the shipped
    // vocabulary never named. Both must reach the slot, or a bound key would
    // read as an ordinary key on the board and in the hover card.
    const model = {
        pdModes: [{id: 0, name: "Dragscroll", kind: 2}, {id: 6, name: "", kind: 0}],
        qmkKeycodeAliases: {QK_USER_16: "DRAGSCROLL", QK_USER_22: "DRAGSCROLL_LOCK", "0x7EF0": "PD_SLOT_6"},
    };
    assert.equal(pointingSlotFor(model, "QK_USER_16").name, "Dragscroll");
    assert.equal(pointingSlotFor(model, "QK_USER_22").name, "Dragscroll");
    assert.equal(pointingSlotFor(model, "0x7EF0").id, 6);
    assert.equal(pointingSlotFor(model, "0x7EF1").id, 6, "the toggle keycode of an unnamed slot too");
    assert.equal(pointingSlotFor(model, 0x7ef0).id, 6);
    assert.equal(pointingSlotFor(model, "0x0041"), undefined, "an ordinary value is not a pointing key");
});

test("what still reaches a pointing slot is listed, so an inert key can be named", () => {
    const slot = {id: 0, kind: 0, name: ""};
    const model = {
        layers: [
            {name: "Layer 0", displayName: "Base", positions: [
                {layoutIndex: 3, keycode: "DRAGSCROLL"}, {layoutIndex: 4, keycode: "KC_A"}]},
            {name: "Layer 3", displayName: "Navigation", positions: [
                {layoutIndex: 3, keycode: "DRAGSCROLL_LOCK"}]},
        ],
        keyBehaviors: [
            {keycode: "LEFT_THUMB", steps: [{tapCount: 1, hold: {action: "DRAGSCROLL"}}]},
            {keycode: "RIGHT_THUMB", steps: [{tapCount: 0, tap: {action: "KC_ESC"}}]},
        ],
    };
    const found = bindingsForSlot(model, slot);
    assert.equal(found.keys.length, 2, "both the hold and the lock keycode count");
    assert.deepEqual(found.layers, ["Base", "Navigation"]);
    assert.deepEqual(found.behaviours.map((row) => row.keycode), ["LEFT_THUMB"]);
    assert.equal(bindingKeycode({id: 6}), "PD_SLOT_6", "slots past the named six use their number");
    assert.deepEqual(bindingsForSlot(model, undefined).keys, []);
});

test("an empty slot's bindings are found by the keyboard's own values, not by a name it has none of", () => {
    // A cleared slot has no name for the model to resolve, so its keys arrive
    // as whatever the catalogue calls the raw value. The slot still answers to
    // the same two numbers, so that is what the lookup matches on.
    assert.deepEqual(slotKeycodes({id: 0}), [0x7e50, 0x7e56], "hold and toggle for a named slot");
    assert.deepEqual(slotKeycodes({id: 6}), [0x7ef0, 0x7ef1], "and for a numbered one");
    const model = {
        qmkKeycodes: [{keycode: 0x7ef0, value: "QK_USER_32"}],
        layers: [{name: "Layer 0", displayName: "Base", positions: [
            {layoutIndex: 5, keycode: "QK_USER_32", value: 0x7ef0},
            {layoutIndex: 6, keycode: "KC_A", value: 0x0004},
        ]}],
        keyBehaviors: [{keycode: "LEFT_THUMB", steps: [{tapCount: 0, hold: {action: "QK_USER_32"}}]}],
    };
    const found = bindingsForSlot(model, {id: 6, kind: 0});
    assert.equal(found.keys.length, 1, "the key bound to the empty slot is found");
    assert.deepEqual(found.behaviours.map((row) => row.keycode), ["LEFT_THUMB"]);
});

test("the combo table and the board's badges count the same keys", () => {
    // Real keyboards do not always report per-layer input references, and when
    // they do not, the board matched by keycode while the table looked at the
    // missing field and called every combo unreachable.
    const combo = {badge: "C1", inputs: ["KC_D", "LT(3,KC_F)"], output: "KC_TAB"};
    const layer = {name: "Layer 0", positions: [
        {layoutIndex: 27, keycode: "KC_D"}, {layoutIndex: 28, keycode: "LT(3,KC_F)"}, {layoutIndex: 29, keycode: "KC_G"}]};
    const model = {combos: [combo], layers: [layer]};

    assert.deepEqual(comboKeysOnLayer(layer, combo).map((key) => key.layoutIndex), [27, 28]);
    assert.deepEqual(combosForKey(model, layer.positions[0]).map((row) => row.badge), ["C1"],
        "the badge and the count come from one predicate");
    assert.deepEqual(combosForKey(model, layer.positions[2]), []);

    const half = {name: "Layer 3", positions: [{layoutIndex: 27, keycode: "KC_D"}]};
    assert.equal(comboKeysOnLayer(half, combo).length, 1, "a partly present combo is not reachable");
    assert.deepEqual(comboKeysOnLayer(undefined, combo), []);

    // A device that does report positions still wins, on those positions.
    const reported = {badge: "C2", inputs: ["KC_N", "KC_M"], inputPositions: [42]};
    assert.deepEqual(comboKeysOnLayer({positions: [{layoutIndex: 42, keycode: "KC_X"}, {layoutIndex: 43, keycode: "KC_M"}]}, reported)
        .map((key) => key.layoutIndex), [42]);
});

test("behaviours group by how this layer reaches them", () => {
    const model = {keyBehaviors: [
        {keycode: "KC_ESCAPE", steps: []},
        {keycode: "LEFT_THUMB", steps: []},
        {keycode: "KC_1", steps: []},
        {keycode: "KC_9", steps: []},
    ]};
    const at = (layoutIndex, keycode) => ({layoutIndex, keycode, display: keycode});
    const stack = [
        {index: 0, name: "Base", positions: [at(0, "KC_ESCAPE"), at(1, "KC_1"), at(2, "KC_9"), at(3, "KC_B")]},
        {index: 1, name: "Numbers", positions: [at(0, "LEFT_THUMB"), at(1, "KC_TRANSPARENT"), at(2, "KC_NO"), at(3, "KC_TRANSPARENT")]},
    ];

    const groups = behaviourGroups(model, stack, 1);
    assert.deepEqual(groups.here.map((row) => row.keycode), ["LEFT_THUMB"], "stored on this layer");
    assert.deepEqual(groups.through.map((entry) => entry.row.keycode), ["KC_1"],
        "a transparent key lets the layer underneath answer");
    assert.equal(groups.through[0].layer.name, "Base", "and the group names the layer that answers");
    assert.deepEqual(groups.elsewhere.map((row) => row.keycode), ["KC_ESCAPE", "KC_9"],
        "KC_ESCAPE is covered on this layer and KC_NO stops the fall-through to KC_9");

    const base = behaviourGroups(model, stack, 0);
    assert.deepEqual(base.here.map((row) => row.keycode), ["KC_ESCAPE", "KC_1", "KC_9"]);
    assert.deepEqual(base.through, [], "nothing lies under the base layer");
    assert.deepEqual(base.elsewhere.map((row) => row.keycode), ["LEFT_THUMB"]);
});
