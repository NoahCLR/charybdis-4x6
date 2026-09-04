"use strict";

const assert = require("node:assert/strict");
const test = require("node:test");

const {CATALOG_FORMAT, parseKeycodeEntries} = require("../../scripts/generate-keycode-catalog");
const catalog = require("../../live-link/keycode-catalog.json");

// These assertions run against the vendored file, not a QMK checkout, so they
// hold on a machine that has no firmware workspace at all. That is the point of
// vendoring it.

test("the shipped catalog declares its format and provenance", () => {
    assert.equal(catalog.format, CATALOG_FORMAT);
    assert.match(catalog.qmkVersion, /\S/);
    assert.notEqual(catalog.qmkVersion, "unknown");
    assert.ok(catalog.generatedFrom.length > 0);
    assert.ok(catalog.entries.length > 500, `expected a full catalog, got ${catalog.entries.length}`);
});

test("numeric values are unique, so a device keycode resolves to one entry", () => {
    const values = catalog.entries.map((entry) => entry.value);
    assert.equal(new Set(values).size, values.length);
});

test("every entry can be rendered and written", () => {
    for (const entry of catalog.entries) {
        assert.ok(Number.isInteger(entry.value) && entry.value >= 0 && entry.value <= 0xffff, entry.name);
        assert.match(entry.name, /^[A-Z0-9_]+$/);
        assert.ok(entry.label.length > 0, entry.name);
        assert.ok(Array.isArray(entry.aliases));
    }
});

test("known keycodes carry the values the firmware uses", () => {
    const byName = new Map(catalog.entries.map((entry) => [entry.name, entry]));
    assert.equal(byName.get("KC_NO").value, 0x0000);
    assert.equal(byName.get("KC_TRANSPARENT").value, 0x0001);
    assert.equal(byName.get("KC_A").value, 0x0004);
    // The authored base layer's home row, as a spot check against real data.
    assert.equal(byName.get("KC_Y").value, 0x001c);
});

test("the aliases the authored profile uses resolve", () => {
    const aliases = new Map();
    for (const entry of catalog.entries) {
        for (const alias of entry.aliases) {
            aliases.set(alias, entry);
        }
    }
    assert.equal(aliases.get("_______").name, "KC_TRANSPARENT");
    assert.equal(aliases.get("XXXXXXX").name, "KC_NO");
});

test("range markers are excluded, since they are boundaries not keycodes", () => {
    const excluded = catalog.entries.filter(
        (entry) => entry.name.endsWith("_MIN") || entry.name.endsWith("_MAX") || entry.name === "SAFE_RANGE"
    );
    assert.deepEqual(excluded, []);
});

test("the parser reads a numeric value, which is what readback needs", () => {
    const entries = parseKeycodeEntries(`{
        "keycodes": {
            "0x1234": {"group": "basic", "key": "KC_EXAMPLE", "label": "Example", "aliases": ["KC_EX"]},
            "0x1235": {"group": "basic", "key": "KC_NO_LABEL"}
        }
    }`);

    assert.deepEqual(entries, [
        {value: 0x1234, name: "KC_EXAMPLE", label: "Example", group: "basic", aliases: ["KC_EX"]},
        {value: 0x1235, name: "KC_NO_LABEL", label: "KC_NO_LABEL", group: "basic", aliases: []},
    ]);
});

test("the parser drops negated aliases and reads the aliases section too", () => {
    const entries = parseKeycodeEntries(`{
        "keycodes": {"0x0001": {"key": "KC_ONE", "aliases": ["OK", "!HIDDEN"]}},
        "aliases": {"0x0002": {"key": "KC_TWO"}}
    }`);

    assert.deepEqual(entries.map((entry) => entry.name), ["KC_ONE", "KC_TWO"]);
    assert.deepEqual(entries[0].aliases, ["OK"]);
});
