"use strict";
const {test} = require("node:test");
const assert = require("node:assert/strict");
const {VIA_STORAGE, readViaStorage, readRegion, writeRegion, writeViaMacros} = require("../../core/protocol/via-storage-v1");
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
test("writes across chunk and final partial boundaries with exact acknowledgements", async () => {
    const device = keyboard(), bytes = Buffer.alloc(960, 17), progress = [];
    await writeRegion(device, VIA_STORAGE.LAYOUT_WRITE, bytes, {onProgress: value => progress.push(value)});
    assert.deepEqual(device.layout, bytes); assert.equal(progress.at(-1).completed, 960);
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
