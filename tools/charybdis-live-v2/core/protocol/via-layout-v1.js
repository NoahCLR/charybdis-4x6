"use strict";
const {isUnhandledEcho} = require("./via-unhandled-v1");

const {RAW_HID_REPORT_SIZE, normalizeRawHidReport} = require("../transport/device-adapter");

const VIA_LAYOUT_COMMANDS = Object.freeze({
    GET_KEYCODE: 0x04,
    SET_KEYCODE: 0x05,
    UNHANDLED: 0xff,
});

// QMK keyboard.json -> layouts.LAYOUT.layout, in the same order as the
// authored LAYOUT(...) arguments. Keeping this contract beside the wire codec
// makes it impossible to confuse visual order with matrix row/column order.
const CHARYBDIS_4X6_LAYOUT_MATRIX = Object.freeze([
    [0, 0], [0, 1], [0, 2], [0, 3], [0, 4], [0, 5],
    [5, 5], [5, 4], [5, 3], [5, 2], [5, 1], [5, 0],
    [1, 0], [1, 1], [1, 2], [1, 3], [1, 4], [1, 5],
    [6, 5], [6, 4], [6, 3], [6, 2], [6, 1], [6, 0],
    [2, 0], [2, 1], [2, 2], [2, 3], [2, 4], [2, 5],
    [7, 5], [7, 4], [7, 3], [7, 2], [7, 1], [7, 0],
    [3, 0], [3, 1], [3, 2], [3, 3], [3, 4], [3, 5],
    [8, 5], [8, 4], [8, 3], [8, 2], [8, 1], [8, 0],
    [4, 3], [4, 4], [4, 1], [9, 1], [9, 3], [4, 5], [4, 2], [9, 5],
].map((position) => Object.freeze(position)));

class ViaLayoutError extends Error {
    constructor(code, message, details = {}) {
        super(message);
        this.name = "ViaLayoutError";
        this.code = code;
        Object.assign(this, details);
    }
}

function buildViaGetKeycodeRequest(entry) {
    const normalized = normalizeEntry(entry);
    const report = Buffer.alloc(RAW_HID_REPORT_SIZE);
    report[0] = VIA_LAYOUT_COMMANDS.GET_KEYCODE;
    report[1] = normalized.layer;
    report[2] = normalized.row;
    report[3] = normalized.column;
    return report;
}

function viaKeycodeResponseMatcher(response, request) {
    const actual = normalizeRawHidReport(response, "VIA keycode response");
    const expected = normalizeRawHidReport(request, "VIA keycode request");
    return isUnhandledEcho(actual, expected)
        || (actual[0] === expected[0]
            && actual[1] === expected[1]
            && actual[2] === expected[2]
            && actual[3] === expected[3]);
}

function decodeViaGetKeycodeResponse(response, request) {
    const report = handledResponse(response, request, VIA_LAYOUT_COMMANDS.GET_KEYCODE);
    assertZeroRange(report, 6, RAW_HID_REPORT_SIZE, "VIA get-keycode response");
    return (report[4] << 8) | report[5];
}

async function readViaKeycode(connection, entry, options = {}) {
    assertConnection(connection);
    const request = buildViaGetKeycodeRequest(entry);
    const response = await connection.request(request, requestOptions(options));
    return decodeViaGetKeycodeResponse(response, request);
}

// Reads the whole layout out of the keyboard, one position at a time, because
// standard VIA has no bulk keycode read. 56 positions per layer, so a five
// layer board is 280 sequential round trips; the coordinator serialises them
// and onProgress lets the UI show that it is working rather than hung.
async function readViaLayout(connection, options = {}) {
    assertConnection(connection);
    const layerCount = options.layerCount;
    if (!Number.isInteger(layerCount) || layerCount < 1 || layerCount > 16) {
        throw new ViaLayoutError("VIA_LAYOUT_INVALID_ARGUMENT", "layerCount must be an integer from 1 through 16.");
    }

    const total = layerCount * CHARYBDIS_4X6_LAYOUT_MATRIX.length;
    const layers = [];
    let done = 0;

    for (let layer = 0; layer < layerCount; layer += 1) {
        const positions = [];
        for (let layoutIndex = 0; layoutIndex < CHARYBDIS_4X6_LAYOUT_MATRIX.length; layoutIndex += 1) {
            const [row, column] = CHARYBDIS_4X6_LAYOUT_MATRIX[layoutIndex];
            const keycode = await readViaKeycode(connection, {layer, row, column}, options);
            positions.push({layoutIndex, row, column, keycode});
            done += 1;
            if (typeof options.onProgress === "function") {
                options.onProgress({done, total, layer});
            }
        }
        layers.push({layer, positions});
    }
    return layers;
}

function normalizeEntry(entry, options = {}) {
    const normalized = {};
    for (const name of ["layer", "row", "column"]) {
        const value = Number(entry?.[name]);
        if (!Number.isInteger(value) || value < 0 || value > 0xff) {
            throw new RangeError(`${name} must be an 8-bit integer.`);
        }
        normalized[name] = value;
    }
    if (options.requireKeycode) {
        const keycode = Number(entry?.keycode);
        if (!Number.isInteger(keycode) || keycode < 0 || keycode > 0xffff) {
            throw new RangeError("keycode must be a 16-bit integer.");
        }
        normalized.keycode = keycode;
    }
    return normalized;
}

function handledResponse(response, request, command) {
    const report = Buffer.from(normalizeRawHidReport(response, "VIA keycode response"));
    if (report[0] === VIA_LAYOUT_COMMANDS.UNHANDLED) {
        throw new ViaLayoutError("VIA_LAYOUT_REJECTED", "Firmware rejected the standard VIA keymap command.");
    }
    if (!viaKeycodeResponseMatcher(report, request) || report[0] !== command) {
        throw new ViaLayoutError("VIA_LAYOUT_CORRELATION_MISMATCH", "VIA keycode response does not match its request.");
    }
    return report;
}

function requestOptions(options) {
    return {
        matchResponse: viaKeycodeResponseMatcher,
        signal: options.signal,
        timeoutMs: options.timeoutMs,
    };
}

function assertConnection(connection) {
    if (!connection || typeof connection.request !== "function") {
        throw new TypeError("connection must provide request(report, options).");
    }
}

function assertZeroRange(buffer, start, end, label) {
    for (let index = start; index < end; index += 1) {
        if (buffer[index] !== 0) throw new ViaLayoutError("VIA_LAYOUT_NONCANONICAL_RESPONSE", `${label} has nonzero reserved bytes.`);
    }
}

module.exports = {
    CHARYBDIS_4X6_LAYOUT_MATRIX,
    VIA_LAYOUT_COMMANDS,
    ViaLayoutError,
    buildViaGetKeycodeRequest,
    decodeViaGetKeycodeResponse,
    readViaKeycode,
    readViaLayout,
    viaKeycodeResponseMatcher,
};
