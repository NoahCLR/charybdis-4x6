"use strict";
const {test} = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const {createSnapshot, validateSnapshot, materializeProfile, reorderLayers, fingerprint} = require("../../core/model/portable-profile");
const {encodeSettings, decodeSettings} = require("../../core/schema/settings-domain-v1");
const {decodeProfileBlob, encodeProfileBlob} = require("../../core/schema/profile-blob-v1");
const {decodeKeyBehaviorDomain, encodeKeyBehaviorDomain} = require("../../core/schema/key-behavior-domain-v1");
const {CHARYBDIS_4X6_LAYOUT_MATRIX} = require("../../core/data/charybdis-layout");
const {settings, document} = require("../fixtures/portable-profile");
test("complete snapshots flatten effective domains and round-trip without destination defaults", () => {
    const source = document(), actual = validateSnapshot(JSON.stringify(source));
    assert.equal(actual.behaviors.rows.length, 37); assert.equal(actual.combos.rows.length, 1);
    // The keyboard stores a default window, and its combo follows it.
    assert.deepEqual([actual.combos.version, actual.combos.defaultTermMs, actual.combos.holdTermMs, actual.combos.rows[0].termMs], [2, 50, 200, null]);
    assert.equal(actual.settings.names[3], "Navigation"); assert.equal(actual.layout.length, 960);
    assert.equal(fingerprint(JSON.parse(JSON.stringify(source))), fingerprint(source));
});
test("empty domains are explicit and replace flashed behaviours and combos", () => {
    const source = document(), blob = decodeProfileBlob(Buffer.from(source.profile, "base64"));
    blob.domains[1].payload = encodeKeyBehaviorDomain({rows: []}); blob.domains[2].payload = Buffer.from([0, 0, 0, 0, 50, 0, 200, 0]);
    source.profile = encodeProfileBlob(blob).toString("base64");
    assert.equal(validateSnapshot(source).behaviors.rows.length, 0); assert.equal(validateSnapshot(source).combos.rows.length, 0);
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
test("owned layer keys follow their layers on a key, in a behaviour and on a combo", () => {
    const {decodeComboDomain, encodeComboDomain} = require("../../core/schema/combo-domain-v1");
    const source = document(), blob = decodeProfileBlob(Buffer.from(source.profile, "base64"));
    // Layer 1 moves to 4 and 4 to 1; TG, TO, OSL, TT and DF all carry the layer in the low bits.
    source.layers[0].splice(10, 5, 0x5261, 0x5204, 0x5281, 0x52c4, 0x5241);
    const behaviors = decodeKeyBehaviorDomain(blob.domains[1].payload);
    behaviors.rows = [{target: {kind: 1, operand: 0x52c1}, steps: [{tapIndex: 0, tap: {kind: 1, operand: 0x5261}}]}];
    blob.domains[1].payload = encodeKeyBehaviorDomain(behaviors);
    const combos = decodeComboDomain(blob.domains[2].payload, blob.domains[2].version);
    combos.rows[0].output = {kind: 1, operand: 0x5284};
    blob.domains[2].payload = encodeComboDomain(combos);
    source.profile = encodeProfileBlob(blob).toString("base64");

    const reordered = reorderLayers(source, [0, 4, 2, 3, 1, 5, 6, 7]), actual = validateSnapshot(reordered);
    assert.deepEqual(reordered.layers[0].slice(10, 15), [0x5264, 0x5201, 0x5284, 0x52c1, 0x5244]);
    assert.equal(actual.behaviors.rows[0].target.operand, 0x52c4, "TT(1) as a behaviour's key becomes TT(4)");
    assert.equal(actual.behaviors.rows[0].steps[0].tap.operand, 0x5264, "TG(1) as a tap becomes TG(4)");
    assert.equal(actual.combos.rows[0].output.operand, 0x5281, "OSL(4) as a combo output becomes OSL(1)");
});

test("with keys not following, layers move but every layer key keeps its number", () => {
    const {decodeComboDomain, encodeComboDomain} = require("../../core/schema/combo-domain-v1");
    const source = document(), blob = decodeProfileBlob(Buffer.from(source.profile, "base64"));
    source.layers[0].splice(10, 3, 0x5261, 0x5281, 0x4104);
    const behaviors = decodeKeyBehaviorDomain(blob.domains[1].payload);
    behaviors.rows = [{target: {kind: 2, operand: 1}, steps: [{tapIndex: 0, tap: {kind: 3, operand: 4}, hold: {mode: 1, repeatHz: 0, action: {kind: 2, operand: 1}}}]}];
    blob.domains[1].payload = encodeKeyBehaviorDomain(behaviors);
    const combos = decodeComboDomain(blob.domains[2].payload, blob.domains[2].version);
    combos.rows[0].output = {kind: 1, operand: 0x5284};
    blob.domains[2].payload = encodeComboDomain(combos);
    source.profile = encodeProfileBlob(blob).toString("base64");
    const before = validateSnapshot(source);

    const order = [0, 4, 2, 3, 1, 5, 6, 7];
    const moved = reorderLayers(source, order, undefined, {keysFollow: false}), actual = validateSnapshot(moved);
    // Each layer's own keys move with it, untouched...
    order.forEach((old, next) => assert.deepEqual(moved.layers[next], source.layers[old], `layer ${old} lands at ${next} as it was`));
    // ...and the layer keys on them, in behaviours and on combos keep their numbers.
    assert.deepEqual(moved.layers[0].slice(10, 13), [0x5261, 0x5281, 0x4104]);
    assert.deepEqual(actual.behaviors.rows[0].target, before.behaviors.rows[0].target);
    assert.equal(actual.behaviors.rows[0].steps[0].tap.operand, 4);
    assert.equal(actual.behaviors.rows[0].steps[0].hold.action.operand, 1);
    assert.equal(actual.combos.rows[0].output.operand, 0x5284);
    // Names, colours and the pointer setting still follow their layer.
    assert.equal(actual.settings.names[1], before.settings.names[4]);
    assert.equal(actual.settings.values[5], order.indexOf(before.settings.values[5]));
    assert.deepEqual(actual.rgb.layerColors.find(row => row.layerId === 1).color, before.rgb.layerColors.find(row => row.layerId === 4).color);
    // The default still renumbers.
    assert.equal(validateSnapshot(reorderLayers(source, order)).combos.rows[0].output.operand, 0x5281);
});

test("a new base trades roles with the old one: keys to either keep their numbers, the rest follow their layers", () => {
    const {decodeComboDomain, encodeComboDomain} = require("../../core/schema/combo-domain-v1");
    const source = document(), blob = decodeProfileBlob(Buffer.from(source.profile, "base64"));
    // TO(0), TO(3), TG(2) and MO(3) on the old base; LOCK_LAYER(0) and TT(3) as behaviour branches; TO(3) on a combo.
    source.layers[0].splice(10, 4, 0x5200, 0x5203, 0x5262, 0x5223);
    const behaviors = decodeKeyBehaviorDomain(blob.domains[1].payload);
    behaviors.rows = [{target: {kind: 1, operand: 0x04}, steps: [{tapIndex: 0, tap: {kind: 3, operand: 0}, hold: {mode: 1, repeatHz: 0, action: {kind: 2, operand: 3}}}]}];
    blob.domains[1].payload = encodeKeyBehaviorDomain(behaviors);
    const combos = decodeComboDomain(blob.domains[2].payload, blob.domains[2].version);
    combos.rows[0].output = {kind: 1, operand: 0x5203};
    blob.domains[2].payload = encodeComboDomain(combos);
    source.profile = encodeProfileBlob(blob).toString("base64");
    const before = validateSnapshot(source);

    // Navigation (3) becomes the base; the old base takes slot 3.
    const order = [3, 1, 2, 0, 4, 5, 6, 7];
    const moved = reorderLayers(source, order), actual = validateSnapshot(moved);
    assert.deepEqual(moved.layers[3].slice(10, 14), [0x5200, 0x5203, 0x5262, 0x5223], "TO(0) stays home, TO(3) and MO(3) now reach the old base, TG(2) follows");
    assert.equal(actual.behaviors.rows[0].steps[0].tap.operand, 0, "LOCK_LAYER(0) still names the base");
    assert.equal(actual.behaviors.rows[0].steps[0].hold.action.operand, 3, "MO(3) held the new base, so it holds the old one");
    assert.equal(actual.combos.rows[0].output.operand, 0x5203);
    // Moved on past the swap, the old base is still what those keys reach.
    const further = reorderLayers(source, [3, 1, 2, 4, 5, 6, 7, 0]);
    assert.equal(further.layers[7][11], 0x5207, "TO(3) reaches the old base in slot 7");
    assert.equal(further.layers[7][12], 0x5262, "TG(2) stays with Symbols");
    // Keys not following leaves the other layers' keys alone, never the swap.
    const kept = reorderLayers(source, [3, 1, 4, 5, 6, 7, 2, 0], undefined, {keysFollow: false});
    assert.deepEqual(kept.layers[7].slice(10, 14), [0x5200, 0x5207, 0x5262, 0x5227], "TO(3) and MO(3) still reach the old base; TG(2) keeps its number");
    assert.deepEqual(validateSnapshot(kept).behaviors.rows[0].steps[0].hold.action.operand, 7);
    assert.equal(actual.settings.names[0], before.settings.names[3]);
    assert.equal(actual.settings.names[3], before.settings.names[0]);
    assert.deepEqual(actual.rgb.layerColors.find(row => row.layerId === 0).color, before.rgb.layerColors.find(row => row.layerId === 3).color, "the new base keeps its colour");
    assert.equal(actual.settings.values[23], 1, "the bottom slot is still the one that starts on");
    assert.equal(actual.settings.values[5], order.indexOf(before.settings.values[5]), "the pointer layer follows what it holds");
    assert.equal(actual.settings.values[27] >>> 0, 0x76543210, "each layer's combos still read from the layer itself");
});

test("Make base gives the old uncoloured base its saved HSV and normalizes empty keys", () => {
    const source = document(), blob = decodeProfileBlob(Buffer.from(source.profile, "base64"));
    const saved = decodeSettings(blob.domains[3].payload);
    saved.values[21] = (saved.values[21] & ~0xff00) | (2 << 8); // An animated effect still has saved HSV.
    saved.values[22] = 27 | (180 << 8) | (100 << 16);
    blob.domains[3].payload = encodeSettings(saved);
    source.profile = encodeProfileBlob(blob).toString("base64");
    source.layers[0].splice(0, 4, 0, 1, 4, 0);
    source.layers[3].splice(0, 4, 1, 0, 5, 1);

    const moved = reorderLayers(source, [3, 1, 2, 0, 4, 5, 6, 7]);
    const rgb = validateSnapshot(moved).rgb;
    assert.deepEqual(moved.layers[0].slice(0, 4), [0, 0, 5, 0], "transparent positions entering base become KC_NO");
    assert.deepEqual(moved.layers[3].slice(0, 4), [1, 1, 4, 1], "KC_NO positions leaving base become transparent");
    assert.deepEqual(rgb.layerColors[3], {layerId: 3, color: {h: 27, s: 180, v: 100}, mode: 0});
    assert.deepEqual(rgb.layerColors[0].color, {h: 180, s: 255, v: 200}, "new base keeps its own lighting");
});

test("Make base leaves unused matrix slots alone through a base swap and back", () => {
    const source = document();
    const physical = new Set(CHARYBDIS_4X6_LAYOUT_MATRIX.map(([row, column]) => row * 6 + column));
    const unused = Array.from({length: 60}, (_, slot) => slot).filter(slot => !physical.has(slot));
    assert.deepEqual(unused, [24, 54, 56, 58]);
    source.layers[0] = source.layers[0].map((_, slot) => physical.has(slot) ? 4 : 0);
    source.layers[2] = source.layers[2].map((_, slot) => physical.has(slot) ? 1 : 0);

    const swap = [2, 1, 0, 3, 4, 5, 6, 7];
    const moved = reorderLayers(source, swap);
    assert.equal(moved.layers[0][0], 0, "a physical transparent key entering base becomes KC_NO");
    assert.equal(moved.layers[2][0], 4, "a physical key leaving base keeps its assignment");
    for (const slot of unused) {
        assert.equal(moved.layers[0][slot], 0, `unused new-base slot ${slot} stays KC_NO`);
        assert.equal(moved.layers[2][slot], 0, `unused old-base slot ${slot} stays KC_NO`);
    }
    const restored = reorderLayers(moved, swap);
    assert.deepEqual(restored.layers, source.layers, "swapping back restores the original matrix");
});

test("Make base keeps an existing old-base colour, while overlay reorders keep empty keys", () => {
    const source = document(), blob = decodeProfileBlob(Buffer.from(source.profile, "base64"));
    const rgb = validateSnapshot(source).rgb;
    rgb.layerColors[0].color = {h: 0, s: 0, v: 120}; // White is a real colour.
    blob.domains[0].payload = require("../../core/schema/rgb-domain-v1").encodeRgbDomainV1(rgb);
    source.profile = encodeProfileBlob(blob).toString("base64");
    source.layers[0][0] = 0;
    source.layers[3][0] = 1;
    const moved = reorderLayers(source, [3, 1, 2, 0, 4, 5, 6, 7]);
    assert.deepEqual(validateSnapshot(moved).rgb.layerColors[3], {layerId: 3, color: {h: 0, s: 0, v: 120}, mode: 0});
    const overlay = reorderLayers(source, [0, 3, 2, 1, 4, 5, 6, 7]);
    assert.equal(overlay.layers[0][0], 0);
    assert.equal(overlay.layers[1][0], 1);
});

test("partial, incompatible, over-capacity and malformed profiles fail before restore", () => {
    const source = document();
    assert.throws(() => validateSnapshot({...source, layers: source.layers.slice(0, 4)}), /matrix/);
    assert.throws(() => validateSnapshot({...source, profile: source.profile + "!"}), /profile data/);
    assert.throws(() => validateSnapshot(source, {compiledLayerCount: 8, supportedDomainMask: 15, actionAbiDigest: 1}), /vocabulary/);
    assert.throws(() => reorderLayers(source, [1, 1, 2, 3, 4, 5, 6, 7]), /every layer once/);
});
module.exports = {settings, document};
test("the five-layer bridge migrates custom triggers and adds transparent space", () => {
    const fixture = fs.readFileSync(path.resolve(__dirname, "../../../../tests/fixtures/compiled_profile_v1.fixture"), "utf8");
    const bytes = Buffer.from(fixture.match(/^profile.full.hex=(.+)$/m)[1], "hex");
    const profile = materializeProfile(bytes, bytes, {version: 1, defaultTermMs: null, holdTermMs: 200, rows: [{id: 0, inputs: [0x7e61, 4], output: 0x7e62, termMs: 50, followsDefault: false}]}, encodeSettings(settings()));
    const layout = Buffer.alloc(600); layout.writeUInt16BE(0x7e61, 0); layout.writeUInt16BE(0x7e60, 2);
    const source = createSnapshot({profile, actionAbiDigest: 0xdcb00959, via: {layers: 5, layout, macros: Buffer.alloc(7551), macroSlots: 64}});
    const result = validateSnapshot(source, {compiledLayerCount: 8, supportedDomainMask: 15, actionAbiDigest: 0xeb80829c});
    assert.deepEqual(result.document.layers[0].slice(0, 2), [0x7e64, 0x7e60]);
    assert.equal(result.document.layers.length, 8); assert.ok(result.document.layers.slice(5).flat().every(code => code === 1));
    assert.equal(result.combos.rows[0].inputs[0].operand, 0x7e64); assert.equal(result.combos.rows[0].output.operand, 0x7e65);
    // Firmware without a stored default keeps every window explicit.
    assert.deepEqual([result.combos.version, result.combos.defaultTermMs, result.combos.holdTermMs, result.combos.rows[0].termMs], [1, null, 200, 50]);
    assert.equal(result.rgb.layerColors.length, 8); assert.equal(result.behaviors.rows.length, 37);
    assert.equal(source.layers.length, 5);
    source.layers[0][0] = 0x7fff;
    assert.throws(() => validateSnapshot(source, {compiledLayerCount: 8, supportedDomainMask: 15, actionAbiDigest: 0xeb80829c}), /no corresponding slot/);
});
