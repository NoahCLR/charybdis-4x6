"use strict";
const assert = require("node:assert/strict");
const test = require("node:test");
const {LOGICAL_VIA_STAGE_V1, LOGICAL_VIA_STATE} = require("../../core/protocol/logical-via-stage-v1");
const {LogicalViaStageCoordinator} = require("../../core/session/logical-via-stage-coordinator");

class Harness {
    constructor() { this.state = LOGICAL_VIA_STATE.IDLE; this.sequence = 0; this.transactionId = 0; this.generation = 0; this.digest = 0; this.requests = []; }
    async request(value, options) {
        const request = Buffer.from(value); this.requests.push(request);
        if (request[0] === 8) {
            const response = Buffer.from(request); response[5] = 0; response[6] = 18; response[7] = 1; response[8] = this.state; response[9] = 0; response[10] = 0; response.writeUInt16LE(this.transactionId, 11); response.writeUInt16LE(this.sequence, 13); response.writeUInt32LE(this.generation, 15); response.writeUInt32LE(this.digest, 19);
            assert.equal(options.matchResponse(response), true); return response;
        }
        this.transactionId = request.readUInt16LE(3);
        if (request[2] === LOGICAL_VIA_STAGE_V1.VALUE_BEGIN) { this.generation = request.readUInt32LE(5); this.digest = request.readUInt32LE(9); this.state = LOGICAL_VIA_STATE.STAGING; }
        if (request[2] === LOGICAL_VIA_STAGE_V1.VALUE_VERIFY) this.state = LOGICAL_VIA_STATE.STAGED;
        if (request[2] === LOGICAL_VIA_STAGE_V1.VALUE_ABORT) this.state = LOGICAL_VIA_STATE.ABORTED;
        this.sequence += 1;
        const response = Buffer.from(request); response.fill(0, 5); response[7] = 0xff;
        assert.equal(options.matchResponse(response), true); return response;
    }
}

test("coordinator stages only changed VIA blocks and always restores config validity", async () => {
    const connection = new Harness(); let id = 0;
    const coordinator = new LogicalViaStageCoordinator(connection, {requestIds: {next: () => ++id}, pollMs: 0});
    const current = {layout: Buffer.alloc(24), macros: Buffer.alloc(24)};
    const target = {layout: Buffer.from(current.layout), macros: Buffer.from(current.macros)};
    target.layout[13] = 0xa1; target.macros[2] = 0xb2;
    await coordinator.stage({transactionId: 5, generation: 6, digest: 7, current, target});
    const chunks = connection.requests.filter(request => request[2] === LOGICAL_VIA_STAGE_V1.VALUE_CHUNK);
    assert.equal(chunks.length, 5);
    assert.deepEqual(chunks.map(request => request[5]), [1, 2, 2, 4, 4]);
    assert.deepEqual(chunks.map(request => request.readUInt16LE(6)), [0, 0, 12, 0, 12]);
    assert.equal(connection.state, LOGICAL_VIA_STATE.STAGED);
});
