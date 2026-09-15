"use strict";
const assert = require("node:assert/strict");
const test = require("node:test");
const {LOGICAL_VIA_STAGE_V1, LOGICAL_VIA_STATE, buildLogicalViaBeginRequest, buildLogicalViaChunkRequest, buildLogicalViaStatusRequest, decodeLogicalViaAcknowledgement, decodeLogicalViaStatus} = require("../../core/protocol/logical-via-stage-v1");

test("logical VIA frames bind every sparse chunk to one identity", () => {
    const begin = buildLogicalViaBeginRequest(0x1234, 9, 0x89abcdef);
    assert.deepEqual([...begin.subarray(0, 13)], [7, 0, 0x15, 0x34, 0x12, 9, 0, 0, 0, 0xef, 0xcd, 0xab, 0x89]);
    const chunk = buildLogicalViaChunkRequest(0x1234, {region: 2, offset: 24, regionLength: 960, bytes: Buffer.from([1, 2, 3]), generation: 9, digest: 0x89abcdef});
    assert.equal(chunk[2], LOGICAL_VIA_STAGE_V1.VALUE_CHUNK);
    assert.equal(chunk.readUInt16LE(6), 24);
    assert.equal(chunk.readUInt16LE(8), 960);
    assert.deepEqual([...chunk.subarray(11, 14)], [1, 2, 3]);
    assert.equal(chunk.readUInt32LE(23), 9);
    assert.equal(chunk.readUInt32LE(27), 0x89abcdef);
});

test("logical VIA acknowledgement and status reject malformed responses", () => {
    const request = buildLogicalViaBeginRequest(1, 2, 3);
    const ack = Buffer.from(request); ack.fill(0, 5); ack[7] = 0xff;
    assert.deepEqual(decodeLogicalViaAcknowledgement(ack, request), {admission: 0});
    const statusRequest = buildLogicalViaStatusRequest(7);
    const response = Buffer.from(statusRequest); response[5] = 0; response[6] = 18; response[7] = 1; response[8] = LOGICAL_VIA_STATE.STAGED; response.writeUInt16LE(12, 11); response.writeUInt16LE(4, 13); response.writeUInt32LE(6, 15); response.writeUInt32LE(0x12345678, 19);
    assert.deepEqual(decodeLogicalViaStatus(response, statusRequest), {state: LOGICAL_VIA_STATE.STAGED, lastStatus: 0, pending: false, transactionId: 12, operationSequence: 4, generation: 6, digest: 0x12345678});
    response[31] = 1;
    assert.throws(() => decodeLogicalViaStatus(response, statusRequest), {code: "NONCANONICAL_RESPONSE"});
});
