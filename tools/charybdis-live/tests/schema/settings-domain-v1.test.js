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
