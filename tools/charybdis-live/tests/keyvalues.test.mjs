import assert from "node:assert/strict";
import test from "node:test";
import {MODIFIER_BITS, keyName, modifierNames} from "../webview/view/keyvalues.mjs";

const model = {qmkKeycodes: [
    {keycode: 0x0004, value: "KC_A"},
    {keycode: 0x0006, value: "KC_C"},
    {keycode: 0x00a9, value: "KC_VOLU"},
    {keycode: 0x7e50, value: "PD_SLOT_0"},
]};

test("a stored number is named from the keyboard's own catalogue", () => {
    assert.equal(keyName(model, 0x00a9), "KC_VOLU");
    assert.equal(keyName(model, 0x7e50), "PD_SLOT_0");
    assert.equal(keyName(model, 0), "", "zero means no shortcut, not a keycode");
});

test("modifier-wrapped values are unwrapped the way the keyboard encodes them", () => {
    assert.equal(keyName(model, 0x0806), "G(KC_C)", "left GUI + C");
    assert.equal(keyName(model, 0x0a04), "S(G(KC_A))", "left GUI + shift + A");
    assert.equal(keyName(model, 0x1806), "RGUI(KC_C)", "the right-hand bit switches the wrappers");
});

test("an unknown value keeps its number rather than gaining an invented name", () => {
    assert.equal(keyName(model, 0x7e61), "0x7E61");
    assert.equal(keyName({}, 0x0004), "0x4", "with no catalogue, nothing is claimed");
});

test("modifier masks read back as the names the keyboard holds", () => {
    assert.deepEqual(modifierNames(0), []);
    assert.deepEqual(modifierNames(1 | 8), ["Left Ctrl", "Left GUI"]);
    assert.deepEqual(modifierNames(255), MODIFIER_BITS.map(([, name]) => name));
});
