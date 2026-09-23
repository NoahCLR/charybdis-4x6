"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const {
    CHARYBDIS_4X6_LAYOUT_MATRIX,
    VIA_LAYOUT_COMMANDS,
    buildViaGetKeycodeRequest,
    decodeViaGetKeycodeResponse,
    readViaLayout,
} = require("../../core/protocol/via-layout-v1");

test("layout matrix stays identical to the pinned QMK keyboard contract", () => {
    const keyboardJsonPath = path.resolve(
        __dirname,
        "../../../../../bastardkb-qmk/keyboards/bastardkb/charybdis/4x6/keyboard.json"
    );
    const keyboard = JSON.parse(fs.readFileSync(keyboardJsonPath, "utf8"));
    assert.deepEqual(
        CHARYBDIS_4X6_LAYOUT_MATRIX.map((entry) => Array.from(entry)),
        keyboard.layouts.LAYOUT.layout.map((entry) => entry.matrix)
    );
});

test("encodes and decodes standard VIA keycode reads", () => {
    const entry = {layer: 2, row: 7, column: 4, keycode: 0x4005};
    const get = buildViaGetKeycodeRequest(entry);
    assert.deepEqual(Array.from(get.subarray(0, 6)), [0x04, 2, 7, 4, 0, 0]);
    const response = Buffer.from(get);
    response[4] = 0x40;
    response[5] = 0x05;
    assert.equal(decodeViaGetKeycodeResponse(response, get), 0x4005);
});

// A keyboard that answers every GET_KEYCODE with a value derived from its
// coordinates, so a wrong layer or position is visible in the result.
function layoutDevice() {
    const reads = [];
    return {
        reads,
        async request(request) {
            assert.equal(request[0], VIA_LAYOUT_COMMANDS.GET_KEYCODE);
            const [layer, row, column] = [request[1], request[2], request[3]];
            reads.push({layer, row, column});
            const keycode = (layer << 12) | (row << 4) | column;
            const response = Buffer.from(request);
            response[4] = keycode >> 8;
            response[5] = keycode & 0xff;
            return response;
        },
    };
}

test("reads every position of every layer from the device", async () => {
    const device = layoutDevice();
    const layers = await readViaLayout(device, {layerCount: 3});

    assert.equal(layers.length, 3);
    assert.equal(device.reads.length, 3 * CHARYBDIS_4X6_LAYOUT_MATRIX.length);
    for (const {layer, positions} of layers) {
        assert.equal(positions.length, CHARYBDIS_4X6_LAYOUT_MATRIX.length);
        positions.forEach((position, layoutIndex) => {
            const [row, column] = CHARYBDIS_4X6_LAYOUT_MATRIX[layoutIndex];
            assert.equal(position.layoutIndex, layoutIndex);
            assert.equal(position.row, row);
            assert.equal(position.column, column);
            assert.equal(position.keycode, (layer << 12) | (row << 4) | column);
        });
    }
});

test("layout reads report monotonic progress against a known total", async () => {
    const seen = [];
    await readViaLayout(layoutDevice(), {layerCount: 2, onProgress: (value) => seen.push(value)});

    const total = 2 * CHARYBDIS_4X6_LAYOUT_MATRIX.length;
    assert.equal(seen.length, total);
    assert.deepEqual(seen.at(0), {done: 1, total, layer: 0});
    assert.deepEqual(seen.at(-1), {done: total, total, layer: 1});
    assert.ok(seen.every((value, index) => value.done === index + 1));
});

test("layout reads refuse an implausible layer count instead of hammering the device", async () => {
    const device = layoutDevice();
    for (const layerCount of [0, -1, 17, 1.5, undefined, "5"]) {
        await assert.rejects(readViaLayout(device, {layerCount}), /layerCount/);
    }
    assert.equal(device.reads.length, 0);
});
