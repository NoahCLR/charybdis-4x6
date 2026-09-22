"use strict";
const {test} = require("node:test");
const assert = require("node:assert/strict");
const {VIA_STORAGE, viaStorageDigest, readViaStorage, readRegion, writeRegion, changedRanges, writeChangedRegion, writeViaMacros} = require("../../core/protocol/via-storage-v1");
function keyboard() {
    const layout = Buffer.alloc(960, 1), macros = Buffer.alloc(7191);
    return {layout, macros, async request(request) {
        const response = Buffer.from(request), command = request[0];
        if (command === 0x11) response[1] = 8;
        else if (command === 0x0c) response[1] = 64;
        else if (command === 0x0d) response.writeUInt16BE(macros.length, 1);
        else {
            const bytes = [0x12, 0x13].includes(command) ? layout : macros;
            const offset = request.readUInt16BE(1), count = request[3];
            if ([0x0e, 0x12].includes(command)) bytes.copy(response, 4, offset, offset + count);
            else request.copy(bytes, offset, 4, 4 + count);
        }
        return response;
    }};
}
test("reads every matrix position and the entire macro bank, including unused storage", async () => {
    const device = keyboard(); device.macros[7000] = 23;
    const read = await readViaStorage(device);
    assert.equal(read.layers, 8); assert.equal(read.macroSlots, 64);
    assert.deepEqual(read.layout, device.layout); assert.deepEqual(read.macros, device.macros);
});
test("computes the firmware's canonical digest for the complete VIA store", () => {
    const device = keyboard();
    assert.equal(viaStorageDigest(device), 1170160570);
    assert.throws(() => viaStorageDigest({layout: Buffer.alloc(1), macros: device.macros}), /geometry/);
});
test("writes across chunk and final partial boundaries with exact acknowledgements", async () => {
    const device = keyboard(), bytes = Buffer.alloc(960, 17), progress = [];
    await writeRegion(device, VIA_STORAGE.LAYOUT_WRITE, bytes, {onProgress: value => progress.push(value)});
    assert.deepEqual(device.layout, bytes); assert.equal(progress.at(-1).completed, 960);
});
test("writes only changed byte ranges and reads them back at their absolute offsets", async () => {
    const device = keyboard(), current = Buffer.from(device.layout), target = Buffer.from(current), requests = [];
    target[10] = 4; target[11] = 5; target[900] = 6;
    const request = device.request.bind(device);
    device.request = async bytes => {requests.push(Buffer.from(bytes)); return request(bytes);};
    assert.deepEqual(changedRanges(current, target).map(range => [range.offset, range.bytes.length]), [[0, 28], [896, 28]]);
    await writeChangedRegion(device, VIA_STORAGE.LAYOUT_WRITE, target, current);
    assert.deepEqual(requests.map(bytes => [bytes.readUInt16BE(1), bytes[3]]), [[0, 28], [896, 28]]);
    assert.deepEqual(await readRegion(device, VIA_STORAGE.LAYOUT_READ, 2, {startOffset: 10}), target.subarray(10, 12));
});
test("rejects an incomplete macro write and corrupted response headers", async () => {
    const device = keyboard(); device.macros[7190] = 1;
    await assert.rejects(readViaStorage(device), /incomplete/);
    assert.deepEqual((await readViaStorage(device, {allowIncomplete: true})).macros, device.macros);
    await assert.rejects(readRegion({request: async request => {const r = Buffer.from(request); r[2]++; return r;}}, 0x12, 30), /chunk/);
});
test("macro execution stays invalidated until the last acknowledged byte", async () => {
    const device = keyboard(), request = device.request.bind(device), sentinels = [];
    device.request = async bytes => {const response = await request(bytes); sentinels.push(device.macros.at(-1)); return response;};
    const target = Buffer.alloc(7191); target.write("hello");
    await writeViaMacros(device, target);
    assert.ok(sentinels.slice(0, -1).every(value => value === 1));
    assert.equal(sentinels.at(-1), 0); assert.deepEqual(device.macros, target);
});
test("a small macro edit does not transfer the unused macro capacity", async () => {
    const device = keyboard(), current = Buffer.from(device.macros), target = Buffer.from(current), requests = [];
    target.write("hello");
    const request = device.request.bind(device);
    device.request = async bytes => {requests.push(Buffer.from(bytes)); const response = await request(bytes); return response;};
    await writeViaMacros(device, target, {current});
    assert.deepEqual(requests.map(bytes => [bytes.readUInt16BE(1), bytes[3]]), [[7190, 1], [0, 28], [7190, 1]]);
    assert.deepEqual(device.macros, target);
});
test("an interrupted macro transfer stays invalid and can be replaced on retry", async () => {
    const device = keyboard(), request = device.request.bind(device); let writes = 0;
    device.request = async bytes => {if (++writes === 4) throw Error("disconnected"); return request(bytes);};
    const target = Buffer.alloc(7191); target.write("replacement");
    await assert.rejects(writeViaMacros(device, target), /disconnected/);
    assert.equal(device.macros.at(-1), 1);
    device.request = request; await writeViaMacros(device, target);
    assert.deepEqual((await readViaStorage(device)).macros, target);
});
test("rejects unhandled commands, bad capacities and malformed write echoes", async () => {
    await assert.rejects(readViaStorage({request: async () => Buffer.alloc(32, 255)}), /not support/);
    const device = keyboard(), request = device.request.bind(device);
    device.request = async data => {const r = await request(data); if (r[0] === 0x11) r[1] = 9; return r;};
    await assert.rejects(readViaStorage(device), /geometry/);
    await assert.rejects(writeRegion({request: async data => {const r = Buffer.from(data); r[4]++; return r;}}, 0x13, Buffer.alloc(30)), /acknowledge/);
});
