"use strict";
const fs = require("node:fs"), path = require("node:path");
const {settings} = require("./portable-profile");
const {encodeSettings} = require("../../core/schema/settings-domain-v1");
const {materializeProfile, createSnapshot} = require("../../core/model/portable-profile");
const {decodeProfileBlob, encodeProfileBlob} = require("../../core/schema/profile-blob-v1");
const {decodePdDomain, encodePdDomain} = require("../../core/schema/pd-mode-domain-v1");
const {decodeKeyBehaviorDomain, encodeKeyBehaviorDomain} = require("../../core/schema/key-behavior-domain-v1");
function document() {
    const fixture = fs.readFileSync(path.resolve(__dirname, "../../../../tests/fixtures/compiled_profile_pd_v2.fixture"), "utf8");
    const blob = decodeProfileBlob(Buffer.from(fixture.match(/^profile.full.hex=(.+)$/m)[1], "hex"));
    // Creation/duplication tests require two empty destinations, independently
    // of the user's current compiled profile (which now configures slot 6).
    const compiled = encodeProfileBlob({schema: blob.schema, domains: blob.domains.map(domain => {
        if (domain.id === 0x50) return {...domain,
            payload: encodePdDomain(decodePdDomain(domain.payload).map(slot => slot.id < 6 ? slot : {id: slot.id, kind: 0, name: ""})),
        };
        if (domain.id === 0x20) {
            const options = {actionLimits: {maxPdModes: 8}};
            const table = decodeKeyBehaviorDomain(domain.payload, options);
            // Remove the authored Undo / Redo branch too: deliberately empty
            // destinations must not introduce an unrelated inert-action warning.
            const reachesExtension = action => action && [4, 5].includes(action.kind) && action.operand >= 6;
            table.rows = table.rows.map(row => ({...row, steps: row.steps.filter(step =>
                ![step.tap, step.hold?.action, step.longHold?.action].some(reachesExtension))}));
            return {...domain, payload: encodeKeyBehaviorDomain(table, options)};
        }
        return domain;
    })});
    const policy = settings(); policy.formatVersion = 2; policy.values.fill(0, 10, 15);
    const profile = materializeProfile(compiled, compiled, {version: 2, defaultTermMs: 50, holdTermMs: 200, rows: []}, encodeSettings(policy));
    return createSnapshot({profile, actionAbiDigest: parseInt(fixture.match(/^profile.action_abi=(.+)$/m)[1], 16), via: {layers: 8, layout: Buffer.alloc(960), macros: Buffer.alloc(7191), macroSlots: 64}});
}
module.exports = {document};
