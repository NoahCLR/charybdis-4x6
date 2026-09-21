"use strict";
const {test} = require("node:test");
const assert = require("node:assert/strict");
const {document} = require("../fixtures/portable-profile");
const {PD_UPGRADE_STORAGE, measurePdProfileUpgrade} = require("../../core/model/pd-profile-upgrade-budget");
const {PROFILE_BLOB_V1} = require("../../core/schema/profile-blob-v1");
const {PD_DOMAIN_V1} = require("../../core/schema/pd-mode-domain-v1");
const {validateSnapshot} = require("../../core/model/portable-profile");
const {decodeProfileBlob, encodeProfileBlob} = require("../../core/schema/profile-blob-v1");
const {encodeComboDomainV1} = require("../../core/schema/combo-domain-v1");
const {decodeSettings, encodeSettings} = require("../../core/schema/settings-domain-v1");
const fixture = require("../../../../tests/fixtures/pd_mode_domain_v1.json");

function textProgram(length) {
    const result = [];
    while (length > 0) {
        // Each text instruction has two header bytes and at least one character.
        const size = Math.min(length, 257);
        assert.ok(size >= 3);
        result.push(Buffer.from([1, size - 2]), Buffer.alloc(size - 2, 65));
        length -= size;
    }
    return Buffer.concat(result);
}
function populatedDocument(macroBytes) {
    const value = document();
    const domains = decodeProfileBlob(Buffer.from(value.profile, "base64")).domains;
    const key = operand => ({kind: 1, operand});
    domains[2].payload = encodeComboDomainV1(Array.from({length: 32}, (_, id) => ({
        inputs: [key(4), key(5 + id)], output: key(41), termMs: 50, holdTermMs: 200,
    })));
    const settings = decodeSettings(domains[3].payload);
    settings.macros[0] = textProgram(Math.min(512, macroBytes));
    if (macroBytes > 512) settings.macros[1] = textProgram(macroBytes - 512);
    domains[3].payload = encodeSettings(settings);
    value.profile = encodeProfileBlob({domains}).toString("base64");
    validateSnapshot(value); // Capacity examples must be real valid complete old profiles.
    return value;
}

test("PD upgrade fits the complete portable fixture without changing its domains or macros", () => {
    const value = document(), before = JSON.stringify(value);
    assert.deepEqual(measurePdProfileUpgrade(value, fixture.slots), {
        legacyBytes: 1488, pdDomainBytes: 776, domainEnvelopeBytes: 4, additionalRgbBytes: 10,
        requiredBytes: 2278, capacityBytes: 4064, remainingBytes: 1786, fits: true,
        plannedStorage: {capacityBytes: 5088, remainingBytes: 2810, fits: true},
    });
    assert.equal(JSON.stringify(value), before);
});

test("valid full-profile capacity fixtures hit the exact migration limit and one byte beyond", () => {
    const exact = measurePdProfileUpgrade(populatedDocument(918), fixture.slots);
    assert.equal(exact.legacyBytes, 3274);
    assert.equal(exact.requiredBytes, 4064);
    assert.equal(exact.remainingBytes, 0);
    assert.equal(exact.fits, true);
    const above = measurePdProfileUpgrade(populatedDocument(919), fixture.slots);
    assert.equal(above.requiredBytes, 4065);
    assert.equal(above.fits, false);
    assert.equal(above.remainingBytes, -1);
    const fullMacros = measurePdProfileUpgrade(populatedDocument(1024), fixture.slots);
    assert.equal(fullMacros.requiredBytes, 4170);
    assert.equal(fullMacros.fits, false);
    assert.deepEqual(fullMacros.plannedStorage, {capacityBytes: 5088, remainingBytes: 918, fits: true});
});

test("planned geometry preserves VIA and fits any maximum-size legacy payload plus eight slots", () => {
    const geometry = PD_UPGRADE_STORAGE;
    assert.equal(geometry.viaEnd + 1, geometry.slotAStart);
    assert.equal(geometry.slotAStart + geometry.slotBytes, geometry.slotBStart);
    assert.equal(geometry.slotBStart + geometry.slotBytes, geometry.logicalBytes);
    assert.equal(geometry.slotBytes - geometry.headerBytes, geometry.payloadBytes);
    // Audited fork constraints: backing >= 2*logical, integral logical multiples,
    // 4 KiB erase sectors, and uint16 wire/EEPROM addressing.
    assert.equal(geometry.backingBytes, 2 * geometry.logicalBytes);
    assert.equal(geometry.backingBytes % 4096, 0);
    assert.ok(geometry.logicalBytes <= 65536);
    assert.equal(geometry.logicalBytes - 16384, 2048);
    const growth = PD_DOMAIN_V1.SIZE + PROFILE_BLOB_V1.DOMAIN_HEADER_SIZE + 10;
    const maximumMigrated = PROFILE_BLOB_V1.MAX_SIZE + growth;
    assert.equal(maximumMigrated, 4854);
    assert.equal(geometry.payloadBytes - maximumMigrated, 234);
    // Existing 25-byte readback chunks still fit the one-byte page index.
    assert.ok(Math.ceil(geometry.payloadBytes / 25) <= 255);
});

test("size preflight requires complete legacy data and explicitly supplied PD records", () => {
    assert.throws(() => measurePdProfileUpgrade(document(), undefined));
    const value = document(); value.macros.pop();
    assert.throws(() => measurePdProfileUpgrade(value, fixture.slots));
});
