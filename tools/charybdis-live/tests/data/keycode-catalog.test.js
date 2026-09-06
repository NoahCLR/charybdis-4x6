"use strict";

const assert = require("node:assert/strict");
const test = require("node:test");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");

const {buildCatalog, CATALOG_FORMAT, parseKeycodeEntries} = require("../../scripts/generate-keycode-catalog");
const catalog = require("../../core/data/keycode-catalog.json");
const {encode, lookup, metadata, resolve} = require("../../core/data/keycode-catalog");

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

test("catalog revisions reset only their fragment and remove deleted values", () => {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), "live-keycode-spec-"));
    try {
        const dir = path.join(root, "data/constants/keycodes");
        fs.mkdirSync(dir, {recursive: true});
        fs.writeFileSync(path.join(root, "version.txt"), "test");
        for (const [name, keycodes] of Object.entries({
            "0.0.1_basic": {"0x0004": {key: "KC_A"}},
            "0.0.1_midi": {"0x7190": {key: "VELOCITY"}},
            "0.0.2_midi": {"!reset!": 0, "0x7166": {key: "VELOCITY"}},
            "0.0.1_quantum": {"0x7C20": {key: "OLD_OUTPUT"}, "0x7C30": {key: "KEEP"}},
            "0.0.2_quantum": {"0x7C20": "!delete!"},
        })) fs.writeFileSync(path.join(dir, `keycodes_${name}.hjson`), JSON.stringify({keycodes}));
        assert.deepEqual(buildCatalog(root).entries.map(entry => entry.value), [0x0004, 0x7166, 0x7c30]);
    } finally {
        fs.rmSync(root, {recursive: true, force: true});
    }
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

// Resolution is what makes readback legible: the device sends a uint16 and the
// user has to see a key. The expected values below were cross-checked against
// the compiled expectations in the via-layout tests.

test("basic keycodes resolve to their catalog entry", () => {
    assert.deepEqual(resolve(0x0004), {value: 0x0004, name: "KC_A", label: "A", group: "basic", kind: "basic", known: true});
    assert.equal(resolve(0x001c).name, "KC_Y");
    assert.equal(resolve(0x0001).name, "KC_TRANSPARENT");
});

test("layer keycodes decode their layer argument", () => {
    assert.equal(resolve(0x5220).name, "MO(0)");
    assert.equal(resolve(0x5222).name, "MO(2)");
    assert.equal(resolve(0x5222).layer, 2);
    assert.equal(resolve(0x5220).kind, "layer");
});

test("layer-tap keycodes decode both the layer and the tapped key", () => {
    const decoded = resolve(0x4005);
    assert.equal(decoded.name, "LT(0,KC_B)");
    assert.equal(decoded.layer, 0);
    assert.equal(decoded.tap, "KC_B");
    assert.equal(decoded.kind, "layer-tap");
    assert.equal(resolve(0x4105).name, "LT(1,KC_B)");
});

test("custom firmware keycodes still resolve, since the device may report them", () => {
    // The authored profile's RIGHT_THUMB compiles to a QK_USER value.
    assert.equal(resolve(0x7e5d).known, true);
    assert.match(resolve(0x7e5d).name, /^QK_USER_/);
});

test("an unrecognised keycode renders as hex rather than a guess", () => {
    const decoded = resolve(0xfffe);
    assert.equal(decoded.known, false);
    assert.equal(decoded.name, "0xFFFE");
    assert.equal(decoded.kind, "unknown");
});

test("out-of-range and non-integer inputs never throw", () => {
    for (const value of [-1, 0x10000, 1.5, undefined, null, "KC_A"]) {
        const decoded = resolve(value);
        assert.equal(decoded.known, false);
        assert.equal(typeof decoded.name, "string");
    }
});

test("names and aliases resolve back to entries, and metadata is reported", () => {
    assert.equal(lookup("KC_A").value, 0x0004);
    assert.equal(lookup("_______").name, "KC_TRANSPARENT");
    assert.equal(lookup("nonsense"), undefined);
    assert.equal(metadata().keycodeCount, catalog.entries.length);
    assert.equal(metadata().qmkVersion, catalog.qmkVersion);
});

test("shortcuts read from the keyboard include every modifier and basic key", () => {
    for (const [value, name, label] of [
        [0x0226, "LSFT(KC_9)", "Shift+9"],
        [0x0806, "LGUI(KC_C)", "Cmd+C"],
        [0x0a1d, "LSFT(LGUI(KC_Z))", "Shift+Cmd+Z"],
        [0x1104, "RCTL(KC_A)", "Right Ctrl+A"],
        [0x1f04, "RCTL(RSFT(RALT(RGUI(KC_A))))", "Right Ctrl+Right Shift+Right Alt+Right Cmd+A"],
    ]) {
        assert.equal(resolve(value).name, name);
        assert.equal(resolve(value).label, label);
        assert.equal(encode(name), value);
    }
});

test("picker shortcuts and QMK modifier aliases encode identically to device readback", () => {
    for (const [forms, value] of [
        [["C(KC_N)", "LCTL(KC_N)"], 0x0111],
        [["S(KC_N)", "LSFT(KC_N)"], 0x0211],
        [["A(KC_N)", "LALT(KC_N)", "LOPT(KC_N)"], 0x0411],
        [["G(KC_N)", "LGUI(KC_N)", "LCMD(KC_N)", "LWIN(KC_N)"], 0x0811],
        [["ALGR(KC_N)", "ROPT(KC_N)", "RALT(KC_N)"], 0x1411],
        [["RCMD(KC_N)", "RWIN(KC_N)", "RGUI(KC_N)"], 0x1811],
        [["S(G(KC_N))", "LSG(KC_N)"], 0x0a11],
        [["C(A(G(KC_N)))", "LCAG(KC_N)"], 0x0d11],
        [["C(S(A(KC_N)))", "MEH(KC_N)"], 0x0711],
        [["C(S(A(G(KC_N))))", "HYPR(KC_N)"], 0x0f11],
        [["RCTL(RSFT(RGUI(KC_N)))", "RCSG(KC_N)"], 0x1b11],
    ]) {
        for (const form of forms) {
            assert.equal(encode(form), value, form);
            assert.equal(encode(resolve(encode(form)).name), value, form);
        }
    }
    assert.equal(resolve(encode("G(KC_N)")).label, "Cmd+N");
    for (const form of ["G(MO(1))", "G(LT(1,KC_N))", "G(MT(MOD_LCTL,KC_N))", "G(UNKNOWN)", "G(KC_N,KC_C)", "UNKNOWN(KC_N)"]) {
        assert.equal(encode(form), undefined, form);
    }
});

test("tap-hold and layer forms use their distinct QMK ranges", () => {
    assert.equal(resolve(0x2104).name, "MT(MOD_LCTL,KC_A)");
    assert.equal(resolve(0x3804).name, "MT(MOD_RGUI,KC_A)");
    assert.notEqual(resolve(0x6104).kind, "mod-tap");
    assert.equal(resolve(0x52a3).name, "OSM(MOD_LCTL|MOD_LSFT)");
    for (const [base, form] of [[0x5200, "TO"], [0x5220, "MO"], [0x5240, "DF"], [0x5260, "TG"], [0x5280, "OSL"], [0x52c0, "TT"], [0x52e0, "PDF"]]) {
        assert.equal(resolve(base + 3).name, `${form}(3)`);
        assert.equal(encode(`${form}(31)`), base + 31);
        assert.equal(encode(`${form}(32)`), undefined);
    }
    assert.equal(encode("MT(MOD_UNKNOWN,KC_A)"), undefined);
    assert.equal(encode("OSM(MOD_UNKNOWN)"), undefined);
    assert.equal(encode("LGUI(MO(1))"), undefined);
});

test("every resolved uint16 can be written back without changing its value", () => {
    for (let value = 0; value <= 0xffff; value++) {
        const decoded = resolve(value);
        assert.equal(encode(decoded.name), value, `${value}: ${decoded.name}`);
    }
});
