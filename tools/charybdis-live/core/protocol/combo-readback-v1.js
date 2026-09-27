"use strict";
const {isUnhandledEcho, orUnhandled} = require("./via-unhandled-v1");
const {buildProfileGetRequest, decodeProfileResponse, profileResponseMatcher} = require("./profile-wire-v1");
const {fnv1a32} = require("../schema/profile-blob-v1");

const COMBO_READBACK_V1 = Object.freeze({VALUE: 0x06, MAX_ROWS: 32, MAX_INPUTS: 4});
const fail = (code, message) => Object.assign(new Error(message), {code});

// Version 2 reports the default window and the hold threshold once, in
// metadata bytes 18-21, and marks each row that follows the default (flag bit
// 3). Version 1 repeats the hold threshold on every row and has no default.
function decodeComboPages(metadata, pages) {
    if (!(metadata instanceof Uint8Array) || metadata.length !== 25) throw fail("COMBO_MALFORMED", "Invalid combo metadata length.");
    const data = Buffer.from(metadata), version = data[0];
    if (version !== 1 && version !== 2) throw fail("COMBO_INCOMPATIBLE", "Unsupported combo readback version.");
    const [count, maxInputs, layerCount, enabled, flags] = data.subarray(1, 6);
    if (count > 32 || maxInputs !== 4 || layerCount < 1 || layerCount > 8 || enabled > 1 || flags & ~63 || data.subarray(6 + layerCount, 14).some(Boolean) || data.subarray(version === 2 ? 22 : 18).some(Boolean)) {
        throw fail("COMBO_MALFORMED", "Invalid combo limits, policy or reserved bytes.");
    }
    const layerReferences = [...data.subarray(6, 6 + layerCount)];
    if (layerReferences.some(layer => layer >= layerCount) || pages.length !== count) throw fail("COMBO_MALFORMED", "Invalid combo layer references or row count.");
    if ((flags & 32) && layerReferences.some(layer => layer !== layerReferences[0])) throw fail("COMBO_MALFORMED", "A fixed combo reference must use the same layer throughout.");
    const rowFlags = version === 2 ? ~15 : ~7;
    let holdTermMs = version === 2 ? data.readUInt16LE(20) : null;
    const rows = pages.map((page, id) => {
        if (!(page instanceof Uint8Array) || page.length !== 25) throw fail("COMBO_MALFORMED", "Invalid combo row length.");
        const row = Buffer.from(page);
        const inputCount = row[1];
        if (row[0] !== id || inputCount < 2 || inputCount > 4 || row[8] & rowFlags || row.subarray(9 + 2 * inputCount).some(Boolean) || (version === 2 && row.readUInt16LE(6))) {
            throw fail("COMBO_MALFORMED", "Invalid combo row index, input count, flags or padding.");
        }
        if (version === 1) {
            if (id && row.readUInt16LE(6) !== holdTermMs) throw fail("COMBO_MALFORMED", "Combos report more than one hold threshold.");
            holdTermMs = row.readUInt16LE(6);
        }
        const inputs = Array.from({length: inputCount}, (_, index) => row.readUInt16LE(9 + 2 * index));
        if (inputs.includes(0) || new Set(inputs).size !== inputs.length) throw fail("COMBO_MALFORMED", "Combo inputs must be distinct nonzero keycodes.");
        return {id, inputs, output: row.readUInt16LE(2), termMs: row.readUInt16LE(4), followsDefault: Boolean(row[8] & 8), mustHold: Boolean(row[8] & 1), mustTap: Boolean(row[8] & 2), ordered: Boolean(row[8] & 4)};
    });
    const defaultTermMs = version === 2 ? data.readUInt16LE(18) : null;
    if (rows.some(row => row.followsDefault && row.termMs !== defaultTermMs)) throw fail("COMBO_MALFORMED", "A combo that follows the default window reports another.");
    const digest = data.readUInt32LE(14);
    const covered = version === 2 ? [data.subarray(0, 14), data.subarray(18)] : [data.subarray(0, 14)];
    if (fnv1a32(Buffer.concat([...covered, ...pages.map(page => Buffer.from(page))])) !== digest) {
        throw fail("COMBO_CORRUPT", "Combo readback does not match the keyboard's digest.");
    }
    return {version, defaultTermMs, holdTermMs, rows, enabled: Boolean(enabled), noTimer: Boolean(flags & 1), strictTimer: Boolean(flags & 2), customTrigger: Boolean(flags & 4), customRelease: Boolean(flags & 8), customRepress: Boolean(flags & 16), fixedReference: Boolean(flags & 32), layerReferences, digest};
}

async function readDeviceCombos(connection, options = {}) {
    let nextId = 0;
    const read = async page => {
        const id = options.nextRequestId ? options.nextRequestId() : (nextId = nextId % 255 + 1);
        const request = buildProfileGetRequest(COMBO_READBACK_V1.VALUE, page, id);
        const response = await connection.request(request, {
            matchResponse: orUnhandled(profileResponseMatcher, request),
        });
        if (isUnhandledEcho(response, request)) throw fail("COMBO_UNSUPPORTED", "This firmware does not expose combos. Flash the updated firmware pair to read them.");
        return Buffer.from(decodeProfileResponse(response, request));
    };
    for (let attempt = 0; attempt < 2; attempt++) {
        const before = await read(0);
        if (![1, 2].includes(before[0]) || before[1] > COMBO_READBACK_V1.MAX_ROWS) throw fail("COMBO_INCOMPATIBLE", "Unsupported combo readback version or capacity.");
        const pages = [];
        for (let index = 0; index < before[1]; index++) pages.push(await read(index + 1));
        if (!before.equals(await read(0))) continue;
        return decodeComboPages(before, pages);
    }
    throw fail("COMBO_CHANGED", "Combo settings changed during readback. Read from keyboard again.");
}

module.exports = {COMBO_READBACK_V1, decodeComboPages, readDeviceCombos};
