"use strict";
const {test} = require("node:test");
const assert = require("node:assert/strict");
const {encodeSettings, decodeSettings, validateMacroIr} = require("../../core/schema/settings-domain-v1");
const defaults = () => ({values: [200,150,400,150,1,4,1200,25,1,3,100,0,0,400,400,200,400,900000,1200,200,1,257,0xc8ff00,1,0,200,10,0x76543210], names: Array.from({length: 8}, (_, id) => `Layer ${id}`), macros: Array.from({length: 16}, () => Buffer.alloc(0))});
test("portable settings round-trip Unicode names, zero settings and macro instructions", () => {
    const value = defaults(); value.names[1] = "Édition ⌘"; value.macros[0] = Buffer.from([3, 0xe3, 5, 1, 17, 4, 0xe3]);
    assert.deepEqual(decodeSettings(encodeSettings(value)), value);
});
test("settings reject invalid durations, names, padding and macro streams", () => {
    const value = defaults(); value.values[16] = 1200; assert.throws(() => encodeSettings(value), /settings/);
    const bytes = encodeSettings(defaults()); bytes[143] = 1; assert.throws(() => decodeSettings(bytes), /padding/);
    assert.throws(() => validateMacroIr(Buffer.from([3, 0xe3])), /held|pressed/);
    assert.throws(() => validateMacroIr(Buffer.from([5, 17])), /length/);
    assert.throws(() => validateMacroIr(Buffer.from([2, 1])), /Truncated/);
});
test("v5 adds the 64 custom-key names after the macros, and a name edit upgrades only as far as it needs", () => {
    const {SETTINGS, upgradeSettings, customKeyNamesOf} = require("../../core/schema/settings-domain-v1");
    const {macros, ...base} = defaults();
    const pd = {...base, values: base.values.map((v, id) => id >= 10 && id < 15 ? 0 : v)};
    const v4 = {...pd, formatVersion: 4, macroNames: Array.from({length: 64}, (_, i) => `${i}`.padEnd(20, "~"))};
    const full = {...v4, formatVersion: 5, customKeyNames: Array.from({length: 64}, (_, i) => `Key ${i}`.padEnd(20, "!"))};
    const bytes = encodeSettings(full);
    assert.equal(bytes.length, SETTINGS.V5_MAX_SIZE);
    assert.deepEqual([...bytes.subarray(0, 8)], [5, 8, 28, 64, 64, 0, 0, 0]);
    assert.deepEqual(decodeSettings(bytes), full);
    assert.throws(() => encodeSettings({...full, customKeyNames: full.customKeyNames.map((n, i) => i ? n : "x".repeat(21))}), /custom key name/);
    assert.throws(() => encodeSettings({...full, customKeyNames: full.customKeyNames.slice(1)}), /custom key names/);
    // A v4 header must not count custom keys, and a v5 one must.
    const relabelled = Buffer.from(encodeSettings(v4)); relabelled[4] = 64;
    assert.throws(() => decodeSettings(relabelled), /Unsupported settings format/);
    assert.deepEqual(customKeyNamesOf(v4), Array(64).fill(""));
    // A macro name stays v4; a custom key name takes it to v5, names kept.
    assert.equal(upgradeSettings(v4), v4);
    const upgraded = upgradeSettings(v4, 5);
    assert.equal(upgraded.formatVersion, 5);
    assert.deepEqual(upgraded.macroNames, v4.macroNames);
    assert.deepEqual(upgraded.customKeyNames, Array(64).fill(""));
});
test("v4 holds all 64 macro names at 20 plain characters, and v3 keeps its own ceiling", () => {
    const {SETTINGS, CURRENT_VERSION} = require("../../core/schema/settings-domain-v1");
    const {macros, ...base} = defaults();
    const pd = {...base, values: base.values.map((v, id) => id >= 10 && id < 15 ? 0 : v)};
    const full = {...pd, formatVersion: 4, macroNames: Array.from({length: 64}, (_, i) => `${i}`.padEnd(20, "~"))};
    assert.equal(CURRENT_VERSION, 5);
    const bytes = encodeSettings(full);
    assert.equal(bytes.length, SETTINGS.V4_MAX_SIZE);
    assert.equal(bytes.length, 1656);
    assert.deepEqual(decodeSettings(bytes), full);
    assert.throws(() => encodeSettings({...full, macroNames: full.macroNames.map((n, i) => i ? n : "é")}), /plain characters/);
    assert.throws(() => encodeSettings({...full, macroNames: full.macroNames.map((n, i) => i ? n : "x".repeat(21))}), /plain characters/);
    // A name byte v3 accepts is refused under a v4 header.
    const mixed = encodeSettings({...pd, formatVersion: 3, macroNames: Array.from({length: 64}, (_, i) => i ? "" : "é")});
    assert.equal(decodeSettings(mixed).macroNames[0], "é");
    const relabelled = Buffer.from(mixed); relabelled[0] = 4;
    assert.throws(() => decodeSettings(relabelled));
    // 64 full-length names do not fit v3's 1,368 bytes.
    assert.throws(() => encodeSettings({...full, formatVersion: 3}), /Shorten some names/);
    const tooBig = Buffer.from(bytes); tooBig[0] = 3;
    assert.throws(() => decodeSettings(tooBig), /Unsupported settings format/);
});
