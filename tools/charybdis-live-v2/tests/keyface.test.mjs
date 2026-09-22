import assert from "node:assert/strict";
import test from "node:test";
import {behaviourFor, behaviourTiers, bindingKeycode, bindingsForSlot, combosForKey, keyFace, macroKeycodes, pointingSlotFor, slotKeycodes} from "../webview/view/keyface.mjs";

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
