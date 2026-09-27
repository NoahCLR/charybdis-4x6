"use strict";
const fs = require("node:fs"), path = require("node:path");
const {settings} = require("./portable-profile");
const {encodeSettings} = require("../../core/schema/settings-domain-v1");
const {materializeProfile, createSnapshot} = require("../../core/model/portable-profile");
function document() {
    const fixture = fs.readFileSync(path.resolve(__dirname, "../../../../tests/fixtures/compiled_profile_pd_v2.fixture"), "utf8");
    const compiled = Buffer.from(fixture.match(/^profile.full.hex=(.+)$/m)[1], "hex");
    const policy = settings(); policy.formatVersion = 2; policy.values.fill(0, 10, 15);
    const profile = materializeProfile(compiled, compiled, {version: 2, defaultTermMs: 50, holdTermMs: 200, rows: []}, encodeSettings(policy));
    return createSnapshot({profile, actionAbiDigest: parseInt(fixture.match(/^profile.action_abi=(.+)$/m)[1], 16), via: {layers: 8, layout: Buffer.alloc(960), macros: Buffer.alloc(7191), macroSlots: 64}});
}
module.exports = {document};
