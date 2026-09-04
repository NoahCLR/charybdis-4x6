"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const {
    CHARYBDIS_4X6_LAYOUT_MATRIX,
    VIA_LAYOUT_COMMANDS,
    buildViaGetKeycodeRequest,
    buildViaSetKeycodeRequest,
    compileViaLayout,
    decodeViaGetKeycodeResponse,
    synchronizeViaLayout,
} = require("../../core/protocol/via-layout-v1");

function modelWithLayers(layerCount = 1) {
    const keycodes = ["KC_A", "MO(LAYER_BASE)", "LT(LAYER_BASE,KC_B)", "VIA_MACRO_11", "RIGHT_THUMB"];
    return {
        layers: Array.from({length: layerCount}, (_, layer) => ({
            name: layer === 0 ? "LAYER_BASE" : `LAYER_${layer}`,
            positions: CHARYBDIS_4X6_LAYOUT_MATRIX.map((_, index) => ({
                keycode: keycodes[index] || "_______",
            })),
        })),
        customKeycodes: ["RIGHT_THUMB"],
        qmkKeycodeValues: {KC_A: 4, KC_B: 5, KC_TRNS: 1},
    };
}

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

test("compiles authored LAYOUT order into the Charybdis VIA matrix", () => {
    const entries = compileViaLayout(modelWithLayers());
    assert.equal(entries.length, 56);
    assert.deepEqual(entries.slice(0, 5).map(({row, column, keycode}) => ({row, column, keycode})), [
        {row: 0, column: 0, keycode: 4},
        {row: 0, column: 1, keycode: 0x5220},
        {row: 0, column: 2, keycode: 0x4005},
        {row: 0, column: 3, keycode: 0x770b},
        {row: 0, column: 4, keycode: 0x7e5d},
    ]);
    assert.deepEqual(entries[55], {
        layer: 0,
        row: 9,
        column: 5,
        keycode: 1,
        expression: "_______",
        layoutIndex: 55,
    });
});

test("rejects every layout before I/O when an expression is not representable", () => {
    const model = modelWithLayers();
    model.layers[0].positions[8].keycode = "NOT_A_REAL_KEYCODE";
    assert.throws(() => compileViaLayout(model), (error) => {
        assert.equal(error.code, "UNSUPPORTED_LAYOUT_KEYCODE");
        assert.match(error.message, /LAYER_BASE key 9/);
        return true;
    });
});

test("encodes and decodes standard VIA keycode reports", () => {
    const entry = {layer: 2, row: 7, column: 4, keycode: 0x4005};
    const get = buildViaGetKeycodeRequest(entry);
    const set = buildViaSetKeycodeRequest(entry);
    assert.deepEqual(Array.from(get.subarray(0, 6)), [0x04, 2, 7, 4, 0, 0]);
    assert.deepEqual(Array.from(set.subarray(0, 6)), [0x05, 2, 7, 4, 0x40, 0x05]);
    const response = Buffer.from(get);
    response[4] = 0x40;
    response[5] = 0x05;
    assert.equal(decodeViaGetKeycodeResponse(response, get), 0x4005);
});

test("reads the full source layout, writes only differences, and verifies readback", async () => {
    const entries = [
        {layer: 0, row: 0, column: 0, keycode: 4},
        {layer: 0, row: 0, column: 1, keycode: 5},
    ];
    const stored = new Map([["0:0:0", 4], ["0:0:1", 6]]);
    const writes = [];
    const connection = {
        async request(request, options) {
            assert.equal(typeof options.matchResponse, "function");
            const key = `${request[1]}:${request[2]}:${request[3]}`;
            const response = Buffer.from(request);
            if (request[0] === VIA_LAYOUT_COMMANDS.GET_KEYCODE) {
                const keycode = stored.get(key);
                response[4] = keycode >> 8;
                response[5] = keycode & 0xff;
            } else {
                assert.equal(request[0], VIA_LAYOUT_COMMANDS.SET_KEYCODE);
                const keycode = (request[4] << 8) | request[5];
                stored.set(key, keycode);
                writes.push({key, keycode});
            }
            return response;
        },
    };
    const progress = [];
    const result = await synchronizeViaLayout(connection, entries, {onProgress: (value) => progress.push(value)});
    assert.deepEqual(result, {checkedKeys: 2, changedKeys: 1, verifiedKeys: 1});
    assert.deepEqual(writes, [{key: "0:0:1", keycode: 5}]);
    assert.equal(progress.at(-1).phase, "writing-layout");
});
