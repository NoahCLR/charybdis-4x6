"use strict";
const assert = require("node:assert/strict");
const test = require("node:test");
const {readViaRgbMatrix} = require("../../core/protocol/via-rgb-matrix-v1");

function device(sample = () => ({brightness: 255, effectId: 1, speed: 32, hue: 0, saturation: 255}), mutate) {
    const requests = [];
    return {requests, async request(request, options) {
        assert.equal(request.length, 32);
        assert.deepEqual([...request.subarray(0, 2)], [0x08, 3], "only standard RGB GETs are allowed");
        assert.ok(request.subarray(3).every(byte => byte === 0));
        const state = sample(Math.floor(requests.length / 4));
        requests.push(request);
        const response = Buffer.from(request);
        const data = {1: [state.brightness], 2: [state.effectId], 3: [state.speed], 4: [state.hue, state.saturation]}[request[2]];
        response.set(data, 3);
        assert.equal(options.matchResponse(response), true);
        assert.equal(options.matchResponse(Buffer.alloc(32)), false);
        mutate?.(response);
        return response;
    }};
}

test("reads base RGB through matching consecutive samples, preserving all zeros", async () => {
    const keyboard = device();
    assert.deepEqual(await readViaRgbMatrix(keyboard), {brightness: 255, effectId: 1, speed: 32, hue: 0, saturation: 255});
    assert.deepEqual(keyboard.requests.map(request => request[2]), [1, 2, 3, 4, 1, 2, 3, 4]);
    const zeros = {brightness: 0, effectId: 0, speed: 0, hue: 0, saturation: 0};
    assert.deepEqual(await readViaRgbMatrix(device(() => zeros)), zeros);
});

test("a change mid-read retries, while continuous changes stop within sixteen GETs", async () => {
    const sample = index => ({brightness: index === 0 ? 5 : 20, effectId: 1, speed: 0, hue: 0, saturation: 255});
    const keyboard = device(sample);
    assert.equal((await readViaRgbMatrix(keyboard)).brightness, 20);
    assert.equal(keyboard.requests.length, 12);
    const changing = device(index => ({...sample(index), hue: index}));
    await assert.rejects(readViaRgbMatrix(changing), {code: "RGB_MATRIX_CHANGED"});
    assert.equal(changing.requests.length, 16);
});

test("unsupported and malformed responses cannot become an RGB colour", async () => {
    await assert.rejects(readViaRgbMatrix(device(undefined, response => {response[0] = 0xff;})), {code: "RGB_MATRIX_UNSUPPORTED"});
    for (const change of [response => {response[1] = 2;}, response => {response[31] = 1;}]) {
        await assert.rejects(readViaRgbMatrix(device(undefined, change)), {code: "RGB_MATRIX_MALFORMED"});
    }
    await assert.rejects(readViaRgbMatrix({request: async () => Buffer.alloc(10)}));
});
