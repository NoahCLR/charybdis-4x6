"use strict";

const {RAW_HID_REPORT_SIZE, normalizeRawHidReport} = require("../transport/device-adapter");

// Standard VIA RGB Matrix channel. These are GETs of the current QMK
// settings, independent of the generation-owned custom profile.
const VIA_RGB_MATRIX = Object.freeze({GET: 0x08, CHANNEL: 3, UNHANDLED: 0xff});
const VALUES = [["brightness", 1, 1], ["effectId", 2, 1], ["speed", 3, 1], ["color", 4, 2]];

function failure(code, message) {
    return Object.assign(new Error(message), {code});
}

async function readSample(connection) {
    const sample = {};
    for (const [name, id, count] of VALUES) {
        const request = Buffer.alloc(RAW_HID_REPORT_SIZE);
        request[0] = VIA_RGB_MATRIX.GET;
        request[1] = VIA_RGB_MATRIX.CHANNEL;
        request[2] = id;
        const matches = response => response[0] === VIA_RGB_MATRIX.UNHANDLED ||
            (response[0] === request[0] && response[1] === request[1] && response[2] === id);
        const response = normalizeRawHidReport(await connection.request(request, {matchResponse: matches}), "VIA RGB Matrix response");
        if (response[0] === VIA_RGB_MATRIX.UNHANDLED) {
            throw failure("RGB_MATRIX_UNSUPPORTED", "The keyboard does not support base RGB readback.");
        }
        if (!matches(response) || response.subarray(3 + count).some(byte => byte !== 0)) {
            throw failure("RGB_MATRIX_MALFORMED", "The keyboard returned an invalid base RGB response.");
        }
        if (name === "color") {
            sample.hue = response[3];
            sample.saturation = response[4];
        } else sample[name] = response[3];
    }
    return sample;
}

async function readViaRgbMatrix(connection) {
    // VIA has no atomic settings snapshot. Two matching consecutive samples
    // detect ordinary changes during readback; bounded retry avoids chasing
    // a keyboard whose RGB controls are being held down.
    let previous = await readSample(connection);
    for (let attempt = 0; attempt < 3; attempt++) {
        const current = await readSample(connection);
        if (JSON.stringify(previous) === JSON.stringify(current)) return current;
        previous = current;
    }
    throw failure("RGB_MATRIX_CHANGED", "Base RGB settings changed during readback. Read from keyboard again.");
}

module.exports = {readViaRgbMatrix};
