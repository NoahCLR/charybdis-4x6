"use strict";
const test = require("node:test"), assert = require("node:assert/strict");
const {document} = require("../fixtures/portable-profile");
const pdFixture = require("../../../../tests/fixtures/pd_mode_domain_v1.json");
const {upgradePdSnapshot, validateSnapshot, fingerprint} = require("../../core/model/portable-profile");
const {decodePdDomain, encodePdDomain} = require("../../core/schema/pd-mode-domain-v1");
const {readLegacyPdSource} = require("../../core/protocol/portable-profile-v1");
const {crc32, fnv1a32} = require("../../core/schema/profile-blob-v1");
function source() {
    return {...document(), pdModeSource: {version: 1, actionAbiDigest: 0xeb80829c, compiledDefaultDigest: 42, domain: Buffer.from(pdFixture.hex, "hex").toString("base64")}};
}
test("upgrade preserves custom source tuning, layout, macro banks, RGB and non-PD settings", () => {
    const original = source(), modes = decodePdDomain(Buffer.from(original.pdModeSource.domain, "base64"));
    modes[0].scroll.divisorH = 23; modes[4].thresholdX = 73;
    original.pdModeSource.domain = encodePdDomain(modes).toString("base64");
    const saved = JSON.stringify(original), old = validateSnapshot(original), upgraded = upgradePdSnapshot(original), value = validateSnapshot(upgraded);
    assert.equal(JSON.stringify(original), saved);
    assert.deepEqual(upgraded.layers, original.layers); assert.deepEqual(upgraded.macros, original.macros);
    assert.deepEqual(value.settings.macros, old.settings.macros); assert.deepEqual(value.settings.names, old.settings.names);
    assert.deepEqual(value.rgb.pdModeColors.slice(0, 6), old.rgb.pdModeColors);
    assert.equal(value.pdModes[0].scroll.divisorH, 23); assert.equal(value.pdModes[4].thresholdX, 73);
    assert.deepEqual(value.pdModes.slice(0, 6).map(mode => mode.dpi), [100, 0, 0, 400, 400, 100]);
    assert(!Object.hasOwn(upgraded, "pdModeSource"));
    assert.equal(validateSnapshot(original, {schema: {major: 2}, actionAbiDigest: 0x61072732, compiledLayerCount: 8, supportedDomainMask: 31}).document.version, 2);
    assert.equal(validateSnapshot(original).document.version, 1);
    assert.notEqual(fingerprint(source()), fingerprint(original));
});
test("upgrade refuses missing, truncated or incompatible migration evidence", () => {
    assert.throws(() => upgradePdSnapshot(document()), /does not contain/);
    const invalid = source(); invalid.pdModeSource.domain = "AA==";
    assert.throws(() => upgradePdSnapshot(invalid), /776/);
    invalid.pdModeSource = {...source().pdModeSource, actionAbiDigest: 42};
    assert.throws(() => upgradePdSnapshot(invalid), /migration source/);
});
test("legacy source readback is bounded, checksummed and stable across metadata reads", async () => {
    const bytes = Buffer.from(pdFixture.hex, "hex"), metadata = Buffer.alloc(12); metadata.set([1, 25]); metadata.writeUInt16LE(776, 2); metadata.writeUInt32LE(crc32(bytes), 4); metadata.writeUInt32LE(fnv1a32(bytes), 8);
    let id = 0, headers = 0, corrupt = false;
    const connection = {request: async (request, options) => {
        assert.equal(request[2], 9);
        const page = request[4], payload = page ? bytes.subarray((page - 1) * 25, page * 25) : metadata;
        const response = Buffer.alloc(32); request.copy(response, 0, 0, 5); response[6] = payload.length; payload.copy(response, 7);
        if (!page && ++headers === 2 && corrupt) response[11]++;
        assert(options.matchResponse(response)); return response;
    }};
    assert.deepEqual(await readLegacyPdSource(connection, {next: () => ++id}), bytes);
    corrupt = true; headers = 0;
    await assert.rejects(readLegacyPdSource(connection, {next: () => ++id}), /changed during backup/);
});
