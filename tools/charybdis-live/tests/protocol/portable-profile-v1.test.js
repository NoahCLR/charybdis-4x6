"use strict";
const {test} = require("node:test");
const assert = require("node:assert/strict");
const {readSettings, readStorageStatus, waitForStorage} = require("../../core/protocol/portable-profile-v1");
const {encodeSettings} = require("../../core/schema/settings-domain-v1");
const {crc32, fnv1a32} = require("../../core/schema/profile-blob-v1");
const ids = () => {let value = 0; return {next: () => value = value % 255 + 1};};
function response(request, payload, status = 0) {const bytes = Buffer.alloc(32); request.copy(bytes, 0, 0, 5); bytes[5] = status; bytes[6] = payload.length; payload.copy(bytes, 7); return bytes;}
const settings = () => encodeSettings({values: [200,150,400,150,1,4,1200,25,1,3,100,0,0,400,400,200,400,900000,1200,200,1,257,0xc8ff00,1,0,200,10,0x76543210], names: Array(8).fill(""), macros: Array.from({length:16}, () => Buffer.alloc(0))});
test("reads coherent settings using correlated bounded pages", async () => {
    const bytes = settings(), metadata = Buffer.alloc(12); metadata.set([1,25]); metadata.writeUInt16LE(bytes.length, 2); metadata.writeUInt32LE(crc32(bytes),4); metadata.writeUInt32LE(fnv1a32(bytes),8);
    const connection = {request: async (request, options) => {assert.equal(request[2],7); const page = request[4]; const reply = response(request, page ? bytes.subarray((page-1)*25, page*25) : metadata); assert(options.matchResponse(reply)); return reply;}};
    assert.deepEqual(await readSettings(connection, ids()), bytes);
    let headers = 0; const original = connection.request;
    connection.request = async (...args) => {const reply = await original(...args); if (!args[0][4] && ++headers === 2) reply[11]++; return reply;};
    await assert.rejects(readSettings(connection, ids()), /changed/);
});
test("old firmware cannot silently produce a partial backup", async () => {
    await assert.rejects(readSettings({request: async () => Buffer.alloc(32,255)},ids()), /Update both halves/);
});
test("convergence requires matching peer identity, valid digest and no pending writes", async () => {
    const bytes = Buffer.alloc(25); bytes.set([1,4]); bytes.writeUInt32LE(4,2); bytes.writeUInt32LE(99,6); bytes.writeUInt32LE(4,10); bytes.writeUInt32LE(99,14);
    const connection = {request: async request => response(request, bytes)};
    assert.equal((await waitForStorage(connection,ids())).ready,true);
    bytes[1] |= 8; assert.equal((await readStorageStatus(connection,ids())).ready,false);
    await assert.rejects(waitForStorage(connection,ids(),{timeoutMs:1,pollMs:1}),/not finished/);
    bytes[1] = 6; await assert.rejects(waitForStorage(connection,ids()),/recovery/);
});
