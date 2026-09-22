"use strict";

const {RAW_HID_REPORT_SIZE, normalizeRawHidReport} = require("../transport/device-adapter");

const VIA_STORAGE = Object.freeze({MACRO_COUNT: 0x0c, MACRO_SIZE: 0x0d, MACRO_READ: 0x0e, MACRO_WRITE: 0x0f, LAYER_COUNT: 0x11, LAYOUT_READ: 0x12, LAYOUT_WRITE: 0x13, CHUNK: 28});
const fail = message => Object.assign(new Error(message), {code: "VIA_STORAGE_INVALID"});
function viaStorageDigest({layout, macros}) {
    if (!Buffer.isBuffer(layout) || layout.length !== 960 || !Buffer.isBuffer(macros) || macros.length !== 7191) throw fail("Unsupported keyboard storage geometry.");
    let hash = 2166136261;
    const hashByte = byte => {hash = Math.imul((hash ^ byte) >>> 0, 16777619) >>> 0;};
    for (const [id, bytes] of [[1, Buffer.from([1, 0])], [2, layout], [3, Buffer.alloc(0)], [4, macros]]) {
        hashByte(id); hashByte(bytes.length & 255); hashByte(bytes.length >>> 8);
        for (const byte of bytes) hashByte(byte);
    }
    return hash >>> 0;
}

async function exchange(connection, request) {
    const response = normalizeRawHidReport(await connection.request(request, {matchResponse: data => data[0] === request[0] || data[0] === 0xff}), "VIA storage response");
    if (response[0] !== request[0]) throw fail("The keyboard does not support this storage operation.");
    return response;
}

async function scalar(connection, command, width) {
    const request = Buffer.alloc(RAW_HID_REPORT_SIZE); request[0] = command;
    const response = await exchange(connection, request);
    if (response.subarray(1 + width).some(byte => byte !== 0)) throw fail("Invalid storage capacity response.");
    return width === 1 ? response[1] : response.readUInt16BE(1);
}

async function readRegion(connection, command, length, {startOffset = 0} = {}) {
    if (!Number.isInteger(length) || length < 1 || length > 8192 || !Number.isInteger(startOffset) || startOffset < 0 || startOffset + length > 8192) throw fail("Invalid storage size.");
    const bytes = Buffer.alloc(length);
    for (let offset = 0; offset < length; offset += VIA_STORAGE.CHUNK) {
        const count = Math.min(VIA_STORAGE.CHUNK, length - offset);
        const request = Buffer.alloc(RAW_HID_REPORT_SIZE);
        request[0] = command; request.writeUInt16BE(startOffset + offset, 1); request[3] = count;
        const response = await exchange(connection, request);
        if (!response.subarray(0, 4).equals(request.subarray(0, 4)) || response.subarray(4 + count).some(byte => byte !== 0)) throw fail("Invalid storage chunk response.");
        response.copy(bytes, offset, 4, 4 + count);
    }
    return bytes;
}

function changedRanges(current, target, {end = target?.length} = {}) {
    if (!Buffer.isBuffer(current) || !Buffer.isBuffer(target) || current.length !== target.length || !Number.isInteger(end) || end < 0 || end > target.length) throw fail("Invalid storage comparison.");
    const ranges = [];
    for (let offset = 0; offset < end; offset += VIA_STORAGE.CHUNK) {
        const rangeEnd = Math.min(offset + VIA_STORAGE.CHUNK, end);
        if (!current.subarray(offset, rangeEnd).equals(target.subarray(offset, rangeEnd))) {
            const prior = ranges.at(-1);
            if (prior && prior.offset + prior.bytes.length === offset) prior.bytes = target.subarray(prior.offset, rangeEnd);
            else ranges.push({offset, bytes: target.subarray(offset, rangeEnd)});
        }
    }
    return ranges;
}

async function writeChangedRegion(connection, command, target, current, {onProgress = () => {}, end = target?.length} = {}) {
    const ranges = changedRanges(current, target, {end});
    const total = ranges.reduce((sum, range) => sum + range.bytes.length, 0);
    let completed = 0;
    for (const range of ranges) {
        await writeRegion(connection, command, range.bytes, {startOffset: range.offset, onProgress: progress => onProgress({completed: completed + progress.completed, total})});
        completed += range.bytes.length;
    }
    return ranges;
}

async function writeRegion(connection, command, bytes, {onProgress = () => {}, startOffset = 0} = {}) {
    if (!Buffer.isBuffer(bytes) || bytes.length < 1 || bytes.length > 8192 || !Number.isInteger(startOffset) || startOffset < 0 || startOffset + bytes.length > 8192) throw fail("Invalid storage payload.");
    for (let offset = 0; offset < bytes.length; offset += VIA_STORAGE.CHUNK) {
        const count = Math.min(VIA_STORAGE.CHUNK, bytes.length - offset);
        const request = Buffer.alloc(RAW_HID_REPORT_SIZE);
        request[0] = command; request.writeUInt16BE(startOffset + offset, 1); request[3] = count;
        bytes.copy(request, 4, offset, offset + count);
        const response = await exchange(connection, request);
        if (!response.equals(request)) throw fail("The keyboard did not acknowledge the storage write.");
        onProgress({completed: offset + count, total: bytes.length});
    }
}

async function readViaStorage(connection, {matrixRows = 10, matrixColumns = 6, allowIncomplete = false} = {}) {
    const layers = await scalar(connection, VIA_STORAGE.LAYER_COUNT, 1);
    const macroSlots = await scalar(connection, VIA_STORAGE.MACRO_COUNT, 1);
    const macroCapacity = await scalar(connection, VIA_STORAGE.MACRO_SIZE, 2);
    if (layers < 1 || layers > 8 || macroSlots < 1 || macroSlots > 64 || macroCapacity < macroSlots + 1 || macroCapacity > 8192 || matrixRows !== 10 || matrixColumns !== 6) throw fail("Unsupported keyboard storage geometry.");
    const layout = await readRegion(connection, VIA_STORAGE.LAYOUT_READ, layers * matrixRows * matrixColumns * 2);
    const macros = await readRegion(connection, VIA_STORAGE.MACRO_READ, macroCapacity);
    if (!allowIncomplete && macros[macros.length - 1] !== 0) throw fail("A macro write is incomplete. Import your recovery profile before taking another backup.");
    return {layers, macroSlots, macroCapacity, matrixRows, matrixColumns, layout, macros};
}

async function writeViaMacros(connection, bytes, {current, onProgress = () => {}} = {}) {
    if (!Buffer.isBuffer(bytes) || bytes.length < 2 || bytes.at(-1) !== 0) throw fail("Invalid macro bank.");
    if (current !== undefined && (!Buffer.isBuffer(current) || current.length !== bytes.length)) throw fail("Invalid current macro bank.");
    if (current?.equals(bytes)) return [];
    await writeRegion(connection, VIA_STORAGE.MACRO_WRITE, Buffer.from([1]), {startOffset: bytes.length - 1});
    let ranges;
    if (current) {
        ranges = await writeChangedRegion(connection, VIA_STORAGE.MACRO_WRITE, bytes, current, {end: bytes.length - 1, onProgress});
    } else {
        const pending = Buffer.from(bytes); pending[pending.length - 1] = 1;
        await writeRegion(connection, VIA_STORAGE.MACRO_WRITE, pending, {onProgress});
        ranges = [{offset: 0, bytes: bytes.subarray(0, -1)}];
    }
    await writeRegion(connection, VIA_STORAGE.MACRO_WRITE, Buffer.from([0]), {startOffset: bytes.length - 1});
    return ranges;
}

module.exports = {VIA_STORAGE, viaStorageDigest, readViaStorage, readRegion, writeRegion, changedRanges, writeChangedRegion, writeViaMacros};
