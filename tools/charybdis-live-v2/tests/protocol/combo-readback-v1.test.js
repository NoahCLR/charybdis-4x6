"use strict";
const assert = require("node:assert/strict");
const test = require("node:test");
const {decodeComboPages, readDeviceCombos} = require("../../core/protocol/combo-readback-v1");
const {fixturePages, rehash, responseFor} = require("../fixtures/device-combos");
const decode = pages => decodeComboPages(pages[0], pages.slice(1));

test("the firmware fixture decodes native inputs, output and timing without a repository lookup", async () => {
    const pages = fixturePages();
    const requests = [];
    const actual = await readDeviceCombos({request: async (request, options) => {
        requests.push(request);
        assert.deepEqual([...request.subarray(0, 3)], [8, 0, 6]);
        assert.ok(request[3] && request.subarray(5).every(byte => byte === 0));
        const response = responseFor(request, pages);
        assert.equal(options.matchResponse(response, request), true);
        const stale = Buffer.from(response); stale[3]++;
        assert.equal(options.matchResponse(stale, request), false);
        return response;
    }});
    assert.deepEqual(actual.rows[0], {id: 0, inputs: [7, 0x4109], output: 0x2b, termMs: 50, holdTermMs: 200, mustHold: false, mustTap: false, ordered: false});
    assert.deepEqual(actual.rows[1].inputs, [0x0806, 0x0819]);
    assert.equal(actual.rows[1].output, 0x0804);
    assert.deepEqual(actual.layerReferences, [0, 1, 2, 3, 4]);
    assert.deepEqual(requests.map(request => request[4]), [0, 1, 2, 0]);
    assert.equal(new Set(requests.map(request => request[3])).size, 4);
});

test("combo policy flags, callback output, timing zeros and a disabled empty table survive decoding", () => {
    const pages = fixturePages();
    pages[0][4] = 0; pages[0][5] = 63; pages[0].fill(0, 6, 11);
    pages[1].fill(0, 2, 8); pages[1][8] = 7;
    const actual = decode(rehash(pages));
    assert.equal(actual.enabled, false);
    for (const key of ["noTimer", "strictTimer", "customTrigger", "customRelease", "customRepress", "fixedReference"]) assert.equal(actual[key], true);
    assert.deepEqual(actual.rows[0], {id: 0, inputs: [7, 0x4109], output: 0, termMs: 0, holdTermMs: 0, mustHold: true, mustTap: true, ordered: true});
    pages[0][1] = 0;
    assert.deepEqual(decode(rehash([pages[0]])).rows, []);
});

test("malformed limits, row shape, duplicates and reserved bytes are rejected before display", () => {
    for (const mutate of [
        pages => {pages[0][1] = 33;}, pages => {pages[0][2] = 5;}, pages => {pages[0][3] = 0;},
        pages => {pages[0][4] = 2;}, pages => {pages[0][5] = 64;}, pages => {pages[0][6] = 5;},
        pages => {pages[0][13] = 1;}, pages => {pages[0][24] = 1;}, pages => {pages.pop();},
        pages => {pages[0][5] = 32;},
        pages => {pages[1][0] = 1;}, pages => {pages[1][1] = 1;}, pages => {pages[1][1] = 5;},
        pages => {pages[1][8] = 8;}, pages => {pages[1][13] = 1;}, pages => {pages[1][24] = 1;},
        pages => {pages[1].writeUInt16LE(7, 11);}, pages => {pages[1].writeUInt16LE(0, 9);},
    ]) {
        const pages = fixturePages(); mutate(pages);
        assert.throws(() => decode(pages), {code: "COMBO_MALFORMED"});
    }
    const corrupt = fixturePages(); corrupt[1][2]++;
    assert.throws(() => decode(corrupt), {code: "COMBO_CORRUPT"});
    assert.throws(() => decodeComboPages(Buffer.alloc(24), []), {code: "COMBO_MALFORMED"});
    const version = fixturePages(); version[0][0] = 2;
    assert.throws(() => decode(version), {code: "COMBO_INCOMPATIBLE"});
});

test("older firmware is explicitly unsupported and changing readout has a bounded retry", async () => {
    await assert.rejects(readDeviceCombos({request: async () => Buffer.alloc(32, 0xff)}), {code: "COMBO_UNSUPPORTED"});
    let calls = 0;
    const pages = fixturePages();
    const once = {request: async request => {
        calls++;
        const response = responseFor(request, pages);
        if (calls === 4) response[11] = 0;
        return response;
    }};
    assert.equal((await readDeviceCombos(once)).rows.length, 2);
    assert.equal(calls, 8);
    calls = 0;
    await assert.rejects(readDeviceCombos({request: async request => {
        calls++;
        const response = responseFor(request, pages);
        if (calls % 4 === 0) response[11] = 0;
        return response;
    }}), {code: "COMBO_CHANGED"});
    assert.equal(calls, 8);
});
