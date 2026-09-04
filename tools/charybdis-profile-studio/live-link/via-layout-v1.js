"use strict";

const {RAW_HID_REPORT_SIZE, normalizeRawHidReport} = require("./device-adapter");
const {resolveNativeQmkExpression} = require("./compiled-profile-v1");

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

function compileViaLayout(model) {
    if (!model || !Array.isArray(model.layers) || !model.layers.length) {
        throw new ViaLayoutError("INVALID_LAYOUT_MODEL", "The active profile has no parsed layout layers to apply.");
    }
    if (model.layers.length > 0xff) {
        throw new ViaLayoutError("LAYOUT_LAYER_LIMIT", "The active profile has too many layers for the VIA keymap protocol.");
    }

    const entries = [];
    for (const [layerIndex, layer] of model.layers.entries()) {
        if (!Array.isArray(layer.positions) || layer.positions.length !== CHARYBDIS_4X6_LAYOUT_MATRIX.length) {
            throw new ViaLayoutError(
                "LAYOUT_GEOMETRY_MISMATCH",
                `${layer?.name || `Layer ${layerIndex}`} has ${layer?.positions?.length || 0} keys; the Charybdis 4x6 VIA layout requires ${CHARYBDIS_4X6_LAYOUT_MATRIX.length}.`
            );
        }
        for (const [layoutIndex, position] of layer.positions.entries()) {
            const expression = String(position?.keycode || "").trim();
            const keycode = resolveNativeQmkExpression(expression, model);
            if (!Number.isInteger(keycode) || keycode < 0 || keycode > 0xffff) {
                throw new ViaLayoutError(
                    "UNSUPPORTED_LAYOUT_KEYCODE",
                    `${layer?.name || `Layer ${layerIndex}`} key ${layoutIndex + 1} (${expression || "empty"}) cannot be encoded as a VIA keycode.`,
                    {expression, layer: layerIndex, layoutIndex}
                );
            }
            const [row, column] = CHARYBDIS_4X6_LAYOUT_MATRIX[layoutIndex];
            entries.push({layer: layerIndex, row, column, keycode, expression, layoutIndex});
        }
    }
    return entries;
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

function buildViaSetKeycodeRequest(entry) {
    const normalized = normalizeEntry(entry, {requireKeycode: true});
    const report = Buffer.alloc(RAW_HID_REPORT_SIZE);
    report[0] = VIA_LAYOUT_COMMANDS.SET_KEYCODE;
    report[1] = normalized.layer;
    report[2] = normalized.row;
    report[3] = normalized.column;
    report[4] = normalized.keycode >> 8;
    report[5] = normalized.keycode & 0xff;
    return report;
}

function viaKeycodeResponseMatcher(response, request) {
    const actual = normalizeRawHidReport(response, "VIA keycode response");
    const expected = normalizeRawHidReport(request, "VIA keycode request");
    return actual[0] === VIA_LAYOUT_COMMANDS.UNHANDLED
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

function decodeViaSetKeycodeResponse(response, request) {
    const report = handledResponse(response, request, VIA_LAYOUT_COMMANDS.SET_KEYCODE);
    const expected = normalizeRawHidReport(request, "VIA set-keycode request");
    if (report[4] !== expected[4] || report[5] !== expected[5]) {
        throw new ViaLayoutError("VIA_LAYOUT_CORRELATION_MISMATCH", "VIA set-keycode response did not echo the requested keycode.");
    }
    assertZeroRange(report, 6, RAW_HID_REPORT_SIZE, "VIA set-keycode response");
}

async function readViaKeycode(connection, entry, options = {}) {
    assertConnection(connection);
    const request = buildViaGetKeycodeRequest(entry);
    const response = await connection.request(request, requestOptions(options));
    return decodeViaGetKeycodeResponse(response, request);
}

async function writeViaKeycode(connection, entry, options = {}) {
    assertConnection(connection);
    const request = buildViaSetKeycodeRequest(entry);
    const response = await connection.request(request, requestOptions(options));
    decodeViaSetKeycodeResponse(response, request);
}

async function synchronizeViaLayout(connection, entries, options = {}) {
    assertConnection(connection);
    if (!Array.isArray(entries)) throw new TypeError("VIA layout entries must be an array.");
    const onProgress = typeof options.onProgress === "function" ? options.onProgress : () => {};
    const changed = [];
    for (const [index, entry] of entries.entries()) {
        const current = await readViaKeycode(connection, entry, options);
        if (current !== entry.keycode) changed.push({...entry, previousKeycode: current});
        onProgress({phase: "reading-layout", completed: index + 1, total: entries.length, changed: changed.length});
    }

    for (const [index, entry] of changed.entries()) {
        await writeViaKeycode(connection, entry, options);
        const verified = await readViaKeycode(connection, entry, options);
        if (verified !== entry.keycode) {
            throw new ViaLayoutError(
                "VIA_LAYOUT_VERIFICATION_FAILED",
                `VIA readback for layer ${entry.layer}, row ${entry.row}, column ${entry.column} returned 0x${verified.toString(16).padStart(4, "0")} instead of 0x${entry.keycode.toString(16).padStart(4, "0")}.`,
                {entry, actualKeycode: verified}
            );
        }
        onProgress({phase: "writing-layout", completed: index + 1, total: changed.length, changed: changed.length});
    }
    return {checkedKeys: entries.length, changedKeys: changed.length, verifiedKeys: changed.length};
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
    buildViaSetKeycodeRequest,
    compileViaLayout,
    decodeViaGetKeycodeResponse,
    decodeViaSetKeycodeResponse,
    readViaKeycode,
    synchronizeViaLayout,
    viaKeycodeResponseMatcher,
    writeViaKeycode,
};
