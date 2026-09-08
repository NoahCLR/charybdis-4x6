"use strict";
const {test} = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const {createSnapshot, validateSnapshot, materializeProfile, reorderLayers, fingerprint} = require("../../core/model/portable-profile");
const {encodeSettings, decodeSettings} = require("../../core/schema/settings-domain-v1");
const {decodeProfileBlob, encodeProfileBlob} = require("../../core/schema/profile-blob-v1");
const {decodeKeyBehaviorDomain, encodeKeyBehaviorDomain} = require("../../core/schema/key-behavior-domain-v1");
const {settings, document} = require("../fixtures/portable-profile");
test("complete snapshots flatten effective domains and round-trip without destination defaults", () => {
    const source = document(), actual = validateSnapshot(JSON.stringify(source));
    assert.equal(actual.behaviors.rows.length, 37); assert.equal(actual.combos.length, 1);
    assert.equal(actual.settings.names[3], "Navigation"); assert.equal(actual.layout.length, 960);
    assert.equal(fingerprint(JSON.parse(JSON.stringify(source))), fingerprint(source));
});
test("empty domains are explicit and replace flashed behaviours and combos", () => {
    const source = document(), blob = decodeProfileBlob(Buffer.from(source.profile, "base64"));
    blob.domains[1].payload = encodeKeyBehaviorDomain({rows: []}); blob.domains[2].payload = Buffer.from([0, 0, 0, 0]);
    source.profile = encodeProfileBlob(blob).toString("base64");
    assert.equal(validateSnapshot(source).behaviors.rows.length, 0); assert.equal(validateSnapshot(source).combos.length, 0);
});
test("reorder moves matrix data, RGB, pointer policy and every layer action reference together", () => {
    const source = document(), reordered = reorderLayers(source, [0, 4, 2, 3, 1, 5, 6, 7]);
    const actual = validateSnapshot(reordered);
    assert.deepEqual(reordered.layers[0].slice(0, 5), [0x5224, 0x4431, 0x7e5d, 0x5082, 0x52c4]);
    assert.equal(actual.settings.names[1], "Pointer"); assert.equal(actual.settings.values[5], 1);
    assert.equal(actual.rgb.layerColors.find(row => row.layerId === 1).color.v, 150);
    assert.equal(fingerprint(reorderLayers(reordered, [0, 4, 2, 3, 1, 5, 6, 7])), fingerprint(source));
});
test("behaviour targets and tap/hold branches follow their layers", () => {
    const source = document(), blob = decodeProfileBlob(Buffer.from(source.profile, "base64"));
    const behaviors = decodeKeyBehaviorDomain(blob.domains[1].payload);
    behaviors.rows = [{target: {kind: 2, operand: 1}, steps: [{tapIndex: 0, tap: {kind: 3, operand: 4}, hold: {mode: 1, repeatHz: 0, action: {kind: 2, operand: 1}}}]}];
    blob.domains[1].payload = encodeKeyBehaviorDomain(behaviors); source.profile = encodeProfileBlob(blob).toString("base64");
    const row = validateSnapshot(reorderLayers(source, [0, 4, 2, 3, 1, 5, 6, 7])).behaviors.rows[0];
    assert.equal(row.target.operand, 4); assert.equal(row.steps[0].tap.operand, 1); assert.equal(row.steps[0].hold.action.operand, 4);
});
test("partial, incompatible, over-capacity and malformed profiles fail before restore", () => {
    const source = document();
    assert.throws(() => validateSnapshot({...source, layers: source.layers.slice(0, 4)}), /matrix/);
    assert.throws(() => validateSnapshot({...source, profile: source.profile + "!"}), /profile data/);
    assert.throws(() => validateSnapshot(source, {compiledLayerCount: 8, supportedDomainMask: 15, actionAbiDigest: 1}), /vocabulary/);
    assert.throws(() => reorderLayers(source, [1, 0, 2, 3, 4, 5, 6, 7]), /Base/);
});
module.exports = {settings, document};
test("the five-layer bridge migrates custom triggers and adds transparent space", () => {
    const fixture = fs.readFileSync(path.resolve(__dirname, "../../../../tests/fixtures/compiled_profile_v1.fixture"), "utf8");
    const bytes = Buffer.from(fixture.match(/^profile.full.hex=(.+)$/m)[1], "hex");
    const profile = materializeProfile(bytes, bytes, {rows: [{id: 0, inputs: [0x7e61, 4], output: 0x7e62, termMs: 50, holdTermMs: 200}]}, encodeSettings(settings()));
    const layout = Buffer.alloc(600); layout.writeUInt16BE(0x7e61, 0); layout.writeUInt16BE(0x7e60, 2);
    const source = createSnapshot({profile, actionAbiDigest: 0xdcb00959, via: {layers: 5, layout, macros: Buffer.alloc(7551), macroSlots: 64}});
    const result = validateSnapshot(source, {compiledLayerCount: 8, supportedDomainMask: 15, actionAbiDigest: 0xeb80829c});
    assert.deepEqual(result.document.layers[0].slice(0, 2), [0x7e64, 0x7e60]);
    assert.equal(result.document.layers.length, 8); assert.ok(result.document.layers.slice(5).flat().every(code => code === 1));
    assert.equal(result.combos[0].inputs[0].operand, 0x7e64); assert.equal(result.combos[0].output.operand, 0x7e65);
    assert.equal(result.rgb.layerColors.length, 8); assert.equal(result.behaviors.rows.length, 37);
    assert.equal(source.layers.length, 5);
    source.layers[0][0] = 0x7fff;
    assert.throws(() => validateSnapshot(source, {compiledLayerCount: 8, supportedDomainMask: 15, actionAbiDigest: 0xeb80829c}), /no corresponding slot/);
});
