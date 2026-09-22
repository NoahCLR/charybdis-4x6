import assert from "node:assert/strict";
import test from "node:test";
import {entriesForPickerSection, pickerSections} from "../webview/view/picker-sections.mjs";

const catalogue = [
    {value: "KC_A", group: "basic"},
    {value: "KC_SLASH", group: "basic"},
    {value: "KC_HOME", group: "basic"},
    {value: "KC_KP_1", group: "basic"},
    {value: "KC_F12", group: "basic"},
    {value: "KC_MUTE", group: "media"},
    {value: "RGB_TOG", group: "rgb"},
    {value: "PD_SLOT_0", group: "Pointing modes"},
    {value: "QK_USER_0", group: "user"},
    {value: "QK_BOOT", group: "quantum"},
];

test("the picker uses task-shaped sections instead of raw QMK group names", () => {
    assert.deepEqual(pickerSections().map(({label}) => label), [
        "Keyboard", "Symbols", "Navigation", "Numpad", "Layers", "Pointing modes", "Macros",
        "Mouse", "Media", "Lighting", "Magic", "Custom", "More keys", "Other QMK", "All keycodes",
    ]);
});

test("curated picker sections route representative keycodes", () => {
    const section = (id) => pickerSections().find((candidate) => candidate.id === id);
    assert.deepEqual(entriesForPickerSection(catalogue, section("symbols")).map((entry) => entry.value), ["KC_SLASH"]);
    assert.deepEqual(entriesForPickerSection(catalogue, section("navigation")).map((entry) => entry.value), ["KC_HOME"]);
    assert.deepEqual(entriesForPickerSection(catalogue, section("numpad")).map((entry) => entry.value), ["KC_KP_1"]);
    assert.deepEqual(entriesForPickerSection(catalogue, section("lighting")).map((entry) => entry.value), ["RGB_TOG"]);
    assert.deepEqual(entriesForPickerSection(catalogue, section("custom")).map((entry) => entry.value), ["QK_USER_0"]);
    assert.deepEqual(entriesForPickerSection(catalogue, section("other")).map((entry) => entry.value), ["QK_BOOT"]);
    assert.equal(entriesForPickerSection(catalogue, section("all")).length, catalogue.length);
});
