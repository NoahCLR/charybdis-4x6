"use strict";
const {buildProfileGetRequest, decodeProfileResponse, profileResponseMatcher} = require("./profile-wire-v1");
const fail = message => Object.assign(new Error(message), {code: "INVALID_KEYBOARD_OPTIONS"});

async function readPage(connection, ids, page, optional = false) {
    const request = buildProfileGetRequest(8, page, ids.next());
    const response = await connection.request(request, {matchResponse: response => profileResponseMatcher(response, request)});
    try {return Buffer.from(decodeProfileResponse(response, request, {allowShortPayload: true}));}
    catch (error) {
        if (optional && error.code === "DEVICE_REJECTED" && error.status === 2 && response[6] === 0) return null;
        throw error;
    }
}
async function readKeyboardOptions(connection, ids) {
    const metadata = await readPage(connection, ids, 2, true);
    if (!metadata) return null;
    if (metadata.length !== 9 || metadata[0] !== 1 || metadata[1] !== 25 || metadata[5] !== 13 || !metadata[4] || metadata.readUInt16LE(6) & ~0x1fff || metadata[8] & ~15) throw fail("Unsupported keyboard options metadata.");
    const length = metadata.readUInt16LE(2), count = metadata[4];
    if (length !== 26 + 64 * count || length > 253 * 25) throw fail("Invalid keyboard options capacity.");
    const chunks = [];
    for (let offset = 0; offset < length; offset += 25) {
        const bytes = await readPage(connection, ids, 3 + offset / 25);
        if (bytes.length !== Math.min(25, length - offset)) throw fail("Truncated keyboard options.");
        chunks.push(bytes);
    }
    if (!metadata.equals(await readPage(connection, ids, 2))) throw fail("Keyboard options changed during readback. Read the keyboard again.");
    const bytes = Buffer.concat(chunks), masks = Array.from({length: 13}, (_, i) => bytes.readUInt16LE(i * 2));
    if (masks.some(mask => !mask || (mask & (mask - 1))) || new Set(masks).size !== 13) throw fail("Invalid keyboard option bit assignments.");
    const effects = Array.from({length: count}, (_, i) => {
        const field = bytes.subarray(26 + i * 64, 26 + (i + 1) * 64), end = field.indexOf(0);
        if (end < 1 || field.subarray(end).some(Boolean)) throw fail("Invalid lighting effect name padding.");
        const name = field.subarray(0, end).toString("ascii");
        if (!/^[A-Z][A-Z0-9_]*$/.test(name) || !Buffer.from(name, "ascii").equals(field.subarray(0, end))) throw fail("Invalid lighting effect name.");
        return {id: i + 1, name};
    });
    return {effects, keymapMasks: masks, supportedKeymapOptions: metadata.readUInt16LE(6), ledFlags: metadata[8]};
}
module.exports = {readKeyboardOptions};
