"use strict";
const fs = require("node:fs");
const path = require("node:path");
const {createSnapshot, validateSnapshot, materializeProfile, reorderLayers, fingerprint} = require("../../core/model/portable-profile");
const {encodeSettings, decodeSettings} = require("../../core/schema/settings-domain-v1");
const {decodeProfileBlob, encodeProfileBlob} = require("../../core/schema/profile-blob-v1");
const {decodeKeyBehaviorDomain, encodeKeyBehaviorDomain} = require("../../core/schema/key-behavior-domain-v1");
const {ACTION_ABI} = require("../../core/schema/actions");
const pdFixture = require("../../../../tests/fixtures/pd_mode_domain_v1.json");
function settings() {
    const values = [200, 150, 400, 150, 1, 4, 1200, 25, 1, 3, 100, 0, 0, 400, 400, 200, 400, 900000, 1200, 200, 1, 257, 0xc8ff00, 1, 0, 200, 10, 0x76543210];
    return {values, names: ["Base", "Numbers", "Symbols", "Navigation", "Pointer", "Extra 1", "Extra 2", "Extra 3"], macros: Array.from({length: 16}, () => Buffer.alloc(0))};
}
// A backup from the eight-layer firmware before the pointing domain and the
// keycode blocks: schema 1, action ABI 0xeb80829c, LOCK_LAYER(4) at 0x7e60.
function legacyDocument() {
    const fixture = fs.readFileSync(path.resolve(__dirname, "../../../../tests/fixtures/compiled_profile_eight_v1.fixture"), "utf8");
    const active = Buffer.from(fixture.match(/^profile.full.hex=(.+)$/m)[1], "hex");
    const profile = materializeProfile(active, active, {version: 2, defaultTermMs: 50, holdTermMs: 200, rows: [{id: 0, inputs: [4, 5], output: 8, termMs: 50, followsDefault: true, mustHold: false, mustTap: false, ordered: false}]}, encodeSettings(settings()));
    const layout = Buffer.alloc(960); layout.writeUInt16BE(0x5221, 0); layout.writeUInt16BE(0x4131, 2); layout.writeUInt16BE(0x7e60, 4); layout.writeUInt16BE(0x5022, 6); layout.writeUInt16BE(0x52c1, 8);
    return createSnapshot({profile, actionAbiDigest: 0xeb80829c, via: {layers: 8, layout, macros: Buffer.alloc(7191), macroSlots: 64}});
}

// The same backup as the current firmware holds it, upgraded the way the app
// imports it: pointing domain first, then the keycode blocks.
function document() {
    const pdModeSource = {version: 1, actionAbiDigest: 0xeb80829c, compiledDefaultDigest: 42, domain: Buffer.from(pdFixture.hex, "hex").toString("base64")};
    return validateSnapshot({...legacyDocument(), pdModeSource}, CURRENT_CAPABILITIES).document;
}
const CURRENT_CAPABILITIES = Object.freeze({compiledLayerCount: 8, supportedDomainMask: 31, schema: {major: 2}, actionAbiDigest: ACTION_ABI});

module.exports = {settings, document, legacyDocument, CURRENT_CAPABILITIES};
