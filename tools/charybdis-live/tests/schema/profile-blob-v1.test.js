"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const {
    PROFILE_ACTION_KINDS,
    PROFILE_BLOB_V1,
    PROFILE_DOMAIN_IDS,
    PROFILE_DOMAIN_VERSIONS,
    crc32,
    decodeDomainEnvelope,
    decodeProfileBlob,
    decodeSemanticAction,
    encodeProfileBlob,
    encodeSemanticAction,
    fnv1a32,
    readDomainEnvelope,
    readSemanticAction,
} = require("../../core/schema/profile-blob-v1");

function fixtures() {
    const values = new Map(fs.readFileSync(path.join(__dirname, "../../../../tests/fixtures/profile_blob_v1.fixture"), "utf8")
        .split(/\r?\n/)
        .filter((line) => line && !line.startsWith("#"))
        .map((line) => {
            const separator = line.indexOf("=");
            assert.notEqual(separator, -1, `Malformed shared fixture line: ${line}`);
            return [line.slice(0, separator), line.slice(separator + 1)];
        }));
    const actionKinds = Object.keys(PROFILE_ACTION_KINDS);
    return {
        empty: {
            hex: values.get("blob.empty.hex"),
            fnv1a32: `0x${values.get("blob.empty.fnv1a32")}`,
            crc32: `0x${values.get("blob.empty.crc32")}`,
        },
        orderedDomains: {
            hex: values.get("blob.ordered.hex"),
            fnv1a32: `0x${values.get("blob.ordered.fnv1a32")}`,
            crc32: `0x${values.get("blob.ordered.crc32")}`,
            domains: [
                {id: PROFILE_DOMAIN_IDS.RGB, version: 1, payload: "deadbeef"},
                {id: PROFILE_DOMAIN_IDS.KEY_BEHAVIORS, version: 1, payload: "000102"},
            ],
        },
        actions: actionKinds.map((kind) => ({
            kind,
            operand: Number.parseInt(values.get(`action.${kind}.operand`), 10),
            hex: values.get(`action.${kind}.hex`),
        })),
    };
}

function hex(value) {
    return Buffer.from(value, "hex");
}

test("empty and ordered-domain blobs match deterministic golden vectors", () => {
    const golden = fixtures();
    const empty = encodeProfileBlob({domains: []});
    assert.equal(empty.toString("hex"), golden.empty.hex);
    assert.equal(fnv1a32(empty), Number.parseInt(golden.empty.fnv1a32, 16));
    assert.equal(crc32(empty), Number.parseInt(golden.empty.crc32, 16));
    assert.deepEqual(decodeProfileBlob(empty), {
        magic: "NLP1",
        schema: {major: 1, minor: 0},
        flags: 1,
        domains: [],
        byteLength: 8,
        digest: Number.parseInt(golden.empty.fnv1a32, 16),
        crc32: Number.parseInt(golden.empty.crc32, 16),
    });

    const encoded = encodeProfileBlob({domains: [
        {id: PROFILE_DOMAIN_IDS.KEY_BEHAVIORS, version: 1, payload: hex("000102")},
        {id: PROFILE_DOMAIN_IDS.RGB, version: 1, payload: hex("deadbeef")},
    ]});
    assert.equal(encoded.toString("hex"), golden.orderedDomains.hex, "encoder must sort domains by id");
    const decoded = decodeProfileBlob(encoded);
    assert.deepEqual(decoded.domains.map((domain) => ({
        id: domain.id,
        version: domain.version,
        payload: domain.payload.toString("hex"),
    })), golden.orderedDomains.domains);
    assert.equal(decoded.digest, Number.parseInt(golden.orderedDomains.fnv1a32, 16));
    assert.equal(decoded.crc32, Number.parseInt(golden.orderedDomains.crc32, 16));
});

test("domain envelope readers support later domain codecs without accepting padding", () => {
    const first = Buffer.from("10010200aabb", "hex");
    const second = Buffer.from("20010100cc", "hex");
    assert.deepEqual(decodeDomainEnvelope(first), {
        id: PROFILE_DOMAIN_IDS.RGB,
        version: 1,
        payload: hex("aabb"),
    });
    assert.throws(() => decodeDomainEnvelope(Buffer.concat([first, Buffer.from([0])])), (error) => error.code === "TRAILING_BYTES");
    const combined = Buffer.concat([Buffer.from([0xff]), first, second]);
    const decodedFirst = readDomainEnvelope(combined, 1);
    const decodedSecond = readDomainEnvelope(combined, decodedFirst.nextOffset);
    assert.equal(decodedFirst.domain.payload.toString("hex"), "aabb");
    assert.equal(decodedSecond.domain.payload.toString("hex"), "cc");
    assert.equal(decodedSecond.nextOffset, combined.length);
});

test("strict blob decode rejects unknown, duplicate, out-of-order, truncated, and trailing domains", () => {
    const header = Buffer.from("4e4c503101000101", "hex");
    assert.throws(() => decodeProfileBlob(Buffer.concat([header, Buffer.from("30010000", "hex")])), (error) => error.code === "UNKNOWN_DOMAIN");
    assert.throws(() => decodeProfileBlob(Buffer.concat([header, Buffer.from("10020000", "hex")])), (error) => error.code === "UNKNOWN_DOMAIN_VERSION");

    const duplicateHeader = Buffer.from("4e4c503101000201", "hex");
    const rgb = Buffer.from("10010000", "hex");
    assert.throws(() => decodeProfileBlob(Buffer.concat([duplicateHeader, rgb, rgb])), (error) => error.code === "DUPLICATE_DOMAIN");
    const behaviors = Buffer.from("20010000", "hex");
    assert.throws(() => decodeProfileBlob(Buffer.concat([duplicateHeader, behaviors, rgb])), (error) => error.code === "DOMAIN_ORDER");
    assert.throws(() => decodeProfileBlob(Buffer.concat([header, Buffer.from("10010400aabb", "hex")])), (error) => error.code === "TRUNCATED");
    assert.throws(() => decodeProfileBlob(Buffer.concat([Buffer.from("4e4c503101000001", "hex"), Buffer.from([0])])), (error) => error.code === "TRAILING_BYTES");
    assert.throws(() => decodeProfileBlob(Buffer.from("4e4c5031010001", "hex")), (error) => error.code === "TRUNCATED");
});

test("strict blob header validation rejects magic, schema, and reserved flag variants", () => {
    const valid = encodeProfileBlob({domains: []});
    const invalidMagic = Buffer.from(valid);
    invalidMagic[0] = 0;
    assert.throws(() => decodeProfileBlob(invalidMagic), (error) => error.code === "INVALID_MAGIC");
    const invalidSchema = Buffer.from(valid);
    invalidSchema[5] = 1;
    assert.throws(() => decodeProfileBlob(invalidSchema), (error) => error.code === "INCOMPATIBLE_SCHEMA");
    const reservedFlag = Buffer.from(valid);
    reservedFlag[7] = 3;
    assert.throws(() => decodeProfileBlob(reservedFlag), (error) => error.code === "RESERVED_FLAGS");
    const missingCanonicalFlag = Buffer.from(valid);
    missingCanonicalFlag[7] = 0;
    assert.throws(() => decodeProfileBlob(missingCanonicalFlag), (error) => error.code === "NONCANONICAL");
});

test("blob encoder rejects duplicate, unknown, oversized, and invalid inputs", () => {
    assert.throws(() => encodeProfileBlob({domains: [
        {id: PROFILE_DOMAIN_IDS.RGB, version: 1, payload: Buffer.alloc(0)},
        {id: PROFILE_DOMAIN_IDS.RGB, version: 1, payload: Buffer.alloc(0)},
    ]}), (error) => error.code === "DUPLICATE_DOMAIN");
    assert.throws(() => encodeProfileBlob({domains: [{id: 0x30, version: 1, payload: Buffer.alloc(0)}]}), (error) => error.code === "UNKNOWN_DOMAIN");
    assert.throws(() => encodeProfileBlob({domains: [{
        id: PROFILE_DOMAIN_IDS.RGB,
        version: 1,
        payload: Buffer.alloc(PROFILE_BLOB_V1.MAX_SIZE),
    }]}), (error) => error.code === "CAPACITY_EXCEEDED");
    assert.throws(() => encodeProfileBlob({domains: [{id: 0x100, version: 1, payload: Buffer.alloc(0)}]}), /8-bit/);
    assert.throws(() => encodeProfileBlob({domains: {}}), /must be an array/);
});

test("callers can explicitly register a future domain without weakening v1 defaults", () => {
    const domainVersions = {...PROFILE_DOMAIN_VERSIONS, 0x30: 1};
    const encoded = encodeProfileBlob({domains: [{id: 0x30, version: 1, payload: hex("aabb")}]}, {domainVersions});
    assert.throws(() => decodeProfileBlob(encoded), (error) => error.code === "UNKNOWN_DOMAIN");
    assert.equal(decodeProfileBlob(encoded, {domainVersions}).domains[0].payload.toString("hex"), "aabb");
});

test("semantic action encodings match golden vectors", () => {
    for (const vector of fixtures().actions) {
        const action = {kind: PROFILE_ACTION_KINDS[vector.kind], operand: vector.operand};
        assert.equal(encodeSemanticAction(action).toString("hex"), vector.hex);
        assert.deepEqual(decodeSemanticAction(hex(vector.hex)), {...action, flags: 0});
    }
    assert.equal(encodeSemanticAction({kind: PROFILE_ACTION_KINDS.QMK_KEYCODE, operand: 0xffff}).toString("hex"), "0100ffff");
});

test("semantic action decode rejects kinds, flags, operands, truncation, and capacity overflow", () => {
    assert.throws(() => decodeSemanticAction(Buffer.from("08000000", "hex")), (error) => error.code === "UNKNOWN_ACTION_KIND");
    assert.throws(() => decodeSemanticAction(Buffer.from("01010000", "hex")), (error) => error.code === "RESERVED_FLAGS");
    assert.throws(() => decodeSemanticAction(Buffer.from("00000100", "hex")), (error) => error.code === "INVALID_OPERAND");
    assert.throws(() => decodeSemanticAction(Buffer.from("02000800", "hex")), (error) => error.code === "INVALID_OPERAND");
    assert.throws(() => decodeSemanticAction(Buffer.from("04000600", "hex")), (error) => error.code === "INVALID_OPERAND");
    assert.throws(() => decodeSemanticAction(Buffer.from("06004000", "hex")), (error) => error.code === "INVALID_OPERAND");
    assert.throws(() => decodeSemanticAction(Buffer.from("07001000", "hex")), (error) => error.code === "INVALID_OPERAND");
    assert.throws(() => decodeSemanticAction(Buffer.alloc(3)), (error) => error.code === "INVALID_LENGTH");
    assert.throws(() => encodeSemanticAction({kind: 1, operand: 0x10000}), /16-bit/);
});

test("semantic action stream reader returns an exact next offset", () => {
    const prefix = Buffer.from([0xaa]);
    const action = encodeSemanticAction({kind: PROFILE_ACTION_KINDS.QMK_KEYCODE, operand: 0x4321});
    const suffix = Buffer.from([0xbb]);
    const stream = Buffer.concat([prefix, action, suffix]);
    const decoded = readSemanticAction(stream, 1);
    assert.deepEqual(decoded.action, {kind: PROFILE_ACTION_KINDS.QMK_KEYCODE, flags: 0, operand: 0x4321});
    assert.equal(decoded.nextOffset, 5);
    assert.throws(() => readSemanticAction(stream, 3), (error) => error.code === "TRUNCATED");
});

test("checksum helpers match published reference input and reject non-byte data", () => {
    const reference = Buffer.from("123456789", "ascii");
    assert.equal(fnv1a32(reference), 0xbb86b11c);
    assert.equal(crc32(reference), 0xcbf43926);
    assert.throws(() => fnv1a32("123456789"), /Buffer or Uint8Array/);
    assert.throws(() => crc32([1, 2, 3]), /Buffer or Uint8Array/);
});
