"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {readKeyboardOptions} = require("../../core/protocol/keyboard-options-v1");
const {options, wire} = require("../fixtures/keyboard-options");
function connection(value = wire(), edit = () => {}) {
    const requests = [];
    return {requests, async request(request, match) {
        requests.push(request); assert.equal(request[0], 8); assert.equal(request[2], 8);
        const offset = (request[4] - 3) * 25, payload = request[4] === 2 ? value.metadata : value.bytes.subarray(offset, offset + 25);
        const response = Buffer.alloc(32); request.copy(response, 0, 0, 5); response[6] = payload.length; payload.copy(response, 7);
        edit(response, request, requests.length); assert(match.matchResponse(response)); return response;
    }};
}
function ids() {let next = 0; return {next: () => ++next};}
test("reads the keyboard's actual effect inventory, feature flags and option masks using bounded pages", async () => {
    const c = connection(); assert.deepEqual(await readKeyboardOptions(c, ids()), options());
    assert.equal(c.requests.length, 11); assert.equal(c.requests.at(-1)[4], 2);
});
test("old firmware is distinguished from corrupt optional metadata", async () => {
    const unsupported = connection(wire(), response => {response.fill(0, 5); response[5] = 2;});
    assert.equal(await readKeyboardOptions(unsupported, ids()), null);
    assert.equal(unsupported.requests.length, 1);
    for (const update of [r => {r[7] = 2;}, r => {r[6] = 8; r[15] = 0;}, r => {r[15] = 128;}, r => {r[10] = 255;}]) await assert.rejects(readKeyboardOptions(connection(wire(), update), ids()));
    await assert.rejects(readKeyboardOptions(connection(wire(), (r, q, count) => {if (q[4] === 2 && count > 1) r[15] = 4;}), ids()), /changed/);
});
test("rejects duplicate masks, non-bit masks, invalid names and truncation", async () => {
    for (const edit of [b => {b.writeUInt16LE(1,2);}, b => {b.writeUInt16LE(3,0);}, b => {b[26] = 0xff;}, b => {b[26+63] = 1;}, b => {b.fill(65,26,26+64);}]) {
        const value = wire(); edit(value.bytes); await assert.rejects(readKeyboardOptions(connection(value), ids()));
    }
    await assert.rejects(readKeyboardOptions(connection(wire(), (r, q) => {if (q[4] === 3) {r[6] = 24; r[31] = 0;}}), ids()), /Truncated/);
});
