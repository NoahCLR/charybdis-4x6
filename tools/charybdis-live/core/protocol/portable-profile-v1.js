"use strict";
const {buildProfileGetRequest, decodeProfileResponse, profileResponseMatcher} = require("./profile-wire-v1");
const {crc32, fnv1a32} = require("../schema/profile-blob-v1");
const {decodeSettings} = require("../schema/settings-domain-v1");
const fail = message => Object.assign(new Error(message), {code: "PORTABLE_PROFILE_UNAVAILABLE"});
async function page(connection, value, pageIndex, ids) {
    const request = buildProfileGetRequest(value, pageIndex, ids.next());
    const response = await connection.request(request, {matchResponse: response => response[0] === 255 || profileResponseMatcher(response, request)});
    if (response[0] === 255 || response[5] !== 0) throw fail("Update both halves to firmware with complete profile backup support, then read the keyboard again.");
    return Buffer.from(decodeProfileResponse(response, request, {allowShortPayload: true}));
}
async function readSettings(connection, ids) {
    const before = await page(connection, 7, 0, ids);
    if (before.length !== 12 || before[0] !== 1 || before[1] !== 25) throw fail("Invalid settings metadata.");
    const length = before.readUInt16LE(2);
    if (length < 344 || length > 1368) throw fail("Invalid settings capacity.");
    const chunks = [];
    for (let offset = 0; offset < length; offset += 25) {
        const bytes = await page(connection, 7, 1 + offset / 25, ids);
        if (bytes.length !== Math.min(25, length - offset)) throw fail("Truncated settings read.");
        chunks.push(bytes);
    }
    const bytes = Buffer.concat(chunks), after = await page(connection, 7, 0, ids);
    if (!before.equals(after) || crc32(bytes) !== before.readUInt32LE(4) || fnv1a32(bytes) !== before.readUInt32LE(8)) throw fail("Keyboard settings changed during the read. Try again.");
    decodeSettings(bytes); return bytes;
}
async function readStorageStatus(connection, ids) {
    const bytes = await page(connection, 8, 0, ids);
    if (bytes.length !== 25 || bytes[0] !== 1 || (bytes[1] & ~31)) throw fail("Invalid storage status.");
    const result = {flags: bytes[1], generation: bytes.readUInt32LE(2), digest: bytes.readUInt32LE(6), peerGeneration: bytes.readUInt32LE(10), peerDigest: bytes.readUInt32LE(14), acknowledgedGeneration: bytes.readUInt32LE(18), error: bytes[22], conflicts: bytes.readUInt16LE(23)};
    result.ready = result.flags === 4 && !result.error && !result.conflicts && result.generation === result.peerGeneration && result.digest === result.peerDigest;
    return result;
}
async function readSettingsLimits(connection, ids) {
    const request = buildProfileGetRequest(8, 1, ids.next());
    const response = await connection.request(request, {matchResponse: response => profileResponseMatcher(response, request)});
    let bytes;
    try {bytes = decodeProfileResponse(response, request, {allowShortPayload: true});}
    catch (error) {
        // Earlier complete-profile images reject this optional page. Other
        // failures remain errors, including malformed or uncorrelated replies.
        if (error.code === "DEVICE_REJECTED" && error.status === 2 && response[6] === 0) return null;
        throw error;
    }
    if (bytes.length !== 2 || bytes[0] !== 1) throw fail("Unsupported keyboard settings limits.");
    return {brightnessMax: bytes[1]};
}
async function waitForStorage(connection, ids, {timeoutMs = 90000, pollMs = 150} = {}) {
    const deadline = Date.now() + timeoutMs;
    do {
        const status = await readStorageStatus(connection, ids);
        if (status.ready) return status;
        if (status.conflicts || (status.flags & 2)) throw fail("The halves need storage recovery before restoring a profile.");
        await new Promise(resolve => setTimeout(resolve, pollMs));
    } while (Date.now() < deadline);
    throw fail("The two halves have not finished saving. Keep the recovery file and reconnect both halves.");
}
module.exports = {readSettings, readSettingsLimits, readStorageStatus, waitForStorage};
