"use strict";
const {buildProfileGetRequest, decodeProfileResponse, profileResponseMatcher} = require("./profile-wire-v1");
const {fnv1a32} = require("../schema/profile-blob-v1");

const COMBO_READBACK_V1 = Object.freeze({VALUE: 0x06, MAX_ROWS: 32, MAX_INPUTS: 4});
const fail = (code, message) => Object.assign(new Error(message), {code});

function decodeComboPages(metadata, pages) {
    if (!(metadata instanceof Uint8Array) || metadata.length !== 25) throw fail("COMBO_MALFORMED", "Invalid combo metadata length.");
    const data = Buffer.from(metadata);
    if (data[0] !== 1) throw fail("COMBO_INCOMPATIBLE", "Unsupported combo readback version.");
    const [count, maxInputs, layerCount, enabled, flags] = data.subarray(1, 6);
    if (count > 32 || maxInputs !== 4 || layerCount < 1 || layerCount > 8 || enabled > 1 || flags & ~63 || data.subarray(6 + layerCount, 14).some(Boolean) || data.subarray(18).some(Boolean)) {
        throw fail("COMBO_MALFORMED", "Invalid combo limits, policy or reserved bytes.");
    }
    const layerReferences = [...data.subarray(6, 6 + layerCount)];
    if (layerReferences.some(layer => layer >= layerCount) || pages.length !== count) throw fail("COMBO_MALFORMED", "Invalid combo layer references or row count.");
    if ((flags & 32) && layerReferences.some(layer => layer !== layerReferences[0])) throw fail("COMBO_MALFORMED", "A fixed combo reference must use the same layer throughout.");
    const rows = pages.map((page, id) => {
        if (!(page instanceof Uint8Array) || page.length !== 25) throw fail("COMBO_MALFORMED", "Invalid combo row length.");
        const row = Buffer.from(page);
        const inputCount = row[1];
        if (row[0] !== id || inputCount < 2 || inputCount > 4 || row[8] & ~7 || row.subarray(9 + 2 * inputCount).some(Boolean)) {
            throw fail("COMBO_MALFORMED", "Invalid combo row index, input count, flags or padding.");
        }
        const inputs = Array.from({length: inputCount}, (_, index) => row.readUInt16LE(9 + 2 * index));
        if (inputs.includes(0) || new Set(inputs).size !== inputs.length) throw fail("COMBO_MALFORMED", "Combo inputs must be distinct nonzero keycodes.");
        return {id, inputs, output: row.readUInt16LE(2), termMs: row.readUInt16LE(4), holdTermMs: row.readUInt16LE(6), mustHold: Boolean(row[8] & 1), mustTap: Boolean(row[8] & 2), ordered: Boolean(row[8] & 4)};
    });
    const digest = data.readUInt32LE(14);
    if (fnv1a32(Buffer.concat([data.subarray(0, 14), ...pages.map(page => Buffer.from(page))])) !== digest) {
        throw fail("COMBO_CORRUPT", "Combo readback does not match the keyboard's digest.");
    }
    return {rows, enabled: Boolean(enabled), noTimer: Boolean(flags & 1), strictTimer: Boolean(flags & 2), customTrigger: Boolean(flags & 4), customRelease: Boolean(flags & 8), customRepress: Boolean(flags & 16), fixedReference: Boolean(flags & 32), layerReferences, digest};
}

async function readDeviceCombos(connection, options = {}) {
    let nextId = 0;
    const read = async page => {
        const id = options.nextRequestId ? options.nextRequestId() : (nextId = nextId % 255 + 1);
        const request = buildProfileGetRequest(COMBO_READBACK_V1.VALUE, page, id);
        const response = await connection.request(request, {
            matchResponse: (actual, expected) => actual[0] === 0xff || profileResponseMatcher(actual, expected),
        });
        if (response[0] === 0xff) throw fail("COMBO_UNSUPPORTED", "This firmware does not expose combos. Flash the updated firmware pair to read them.");
        return Buffer.from(decodeProfileResponse(response, request));
    };
    for (let attempt = 0; attempt < 2; attempt++) {
        const before = await read(0);
        if (before[0] !== 1 || before[1] > COMBO_READBACK_V1.MAX_ROWS) throw fail("COMBO_INCOMPATIBLE", "Unsupported combo readback version or capacity.");
        const pages = [];
        for (let index = 0; index < before[1]; index++) pages.push(await read(index + 1));
        if (!before.equals(await read(0))) continue;
        return decodeComboPages(before, pages);
    }
    throw fail("COMBO_CHANGED", "Combo settings changed during readback. Read from keyboard again.");
}

module.exports = {COMBO_READBACK_V1, decodeComboPages, readDeviceCombos};
