"use strict";
const SETTINGS = Object.freeze({COUNT: 28, LAYERS: 8, NAME_BYTES: 24, MACROS: 16, FIXED_SIZE: 312, MAX_SIZE: 1368});
const fail = message => Object.assign(new Error(message), {code: "INVALID_SETTINGS"});
function validSetting(id, v, layers = 8) {
    if (!Number.isInteger(v) || v < 0 || v > 0xffffffff) return false;
    if ([4, 8, 20].includes(id)) return v <= 1;
    if ([5, 9].includes(id)) return v < layers;
    if (id === 7) return v <= 255;
    if (id === 15) return v > 0 && v <= 65535;
    if (id === 17) return v <= 86400000;
    if (id === 18) return v >= 400 && v <= 3400 && v % 200 === 0;
    if (id === 19) return v >= 100 && v <= 400 && v % 100 === 0;
    if (id === 21) return (v & 255) <= 1;
    if (id === 22) return v <= 0xffffff;
    if (id === 23) return v > 0 && v < 2 ** layers;
    if (id === 27) return Array.from({length: 8}, (_, i) => (v >>> (i * 4)) & 15).every(layer => layer < layers);
    return v <= 65535;
}
function validateMacroIr(bytes) {
    if (!Buffer.isBuffer(bytes) || bytes.length > 512) throw fail("A macro exceeds its instruction capacity.");
    const held = new Set();
    const key = byte => (byte >= 4 && byte <= 0xa4) || (byte >= 0xe0 && byte <= 0xe7);
    let offset = 0;
    while (offset < bytes.length) {
        const op = bytes[offset++];
        if (op === 2) {if (offset + 2 > bytes.length) throw fail("Truncated macro delay."); offset += 2; continue;}
        if (op === 3 || op === 4) {
            const k = bytes[offset++];
            if (!key(k) || (op === 3 ? held.has(k) || held.size === 16 : !held.has(k))) throw fail("Unbalanced macro key presses.");
            if (op === 3) held.add(k); else held.delete(k);
            continue;
        }
        if (op !== 1 && op !== 5) throw fail("Unknown macro instruction.");
        const count = bytes[offset++];
        if (!count || offset + count > bytes.length || (op === 5 && count > 16)) throw fail("Invalid macro instruction length.");
        const keys = new Set();
        for (const k of bytes.subarray(offset, offset + count)) {
            if (op === 1 ? !(k === 9 || k === 10 || (k >= 32 && k <= 126)) : !key(k) || held.has(k) || keys.has(k)) throw fail("Invalid macro content.");
            keys.add(k);
        }
        offset += count;
    }
    if (held.size) throw fail("A macro leaves keys held down.");
    return bytes;
}
function encodeSettings(value, layers = 8) {
    if (!value || !Array.isArray(value.values) || value.values.length !== 28 || !value.values.every((v, id) => validSetting(id, v, layers)) || value.values[6] <= value.values[16]) throw fail("Invalid keyboard settings.");
    if (!Array.isArray(value.names) || value.names.length !== 8 || !Array.isArray(value.macros) || value.macros.length !== 16) throw fail("Missing layer names or macros.");
    const fixed = Buffer.alloc(312); Buffer.from([1, 8, 28, 16]).copy(fixed);
    value.values.forEach((v, id) => fixed.writeUInt32LE(v, 8 + id * 4));
    value.names.forEach((name, id) => {
        if (typeof name !== "string" || /[\u0000-\u001f\u007f]/u.test(name) || Buffer.byteLength(name) > 23 || Buffer.from(name).toString() !== name) throw fail("Layer names must fit 23 UTF-8 bytes and contain no control characters.");
        fixed.write(name, 120 + id * 24, 23, "utf8");
    });
    const macros = value.macros.map(bytes => {validateMacroIr(bytes); const size = Buffer.alloc(2); size.writeUInt16LE(bytes.length); return Buffer.concat([size, bytes]);});
    const output = Buffer.concat([fixed, ...macros]);
    if (output.length > SETTINGS.MAX_SIZE) throw fail("The macro bank exceeds the portable profile capacity.");
    return output;
}
function decodeSettings(bytes, layers = 8) {
    if (!Buffer.isBuffer(bytes) || bytes.length < 344 || bytes.length > 1368 || !bytes.subarray(0, 8).equals(Buffer.from([1, 8, 28, 16, 0, 0, 0, 0]))) throw fail("Unsupported settings format.");
    const values = Array.from({length: 28}, (_, id) => bytes.readUInt32LE(8 + id * 4));
    const names = Array.from({length: 8}, (_, id) => {
        const field = bytes.subarray(120 + id * 24, 144 + id * 24), end = field.indexOf(0);
        if (end < 0 || field.subarray(end).some(v => v)) throw fail("Invalid layer name padding.");
        const name = field.subarray(0, end).toString("utf8");
        if (!Buffer.from(name).equals(field.subarray(0, end))) throw fail("Invalid layer name encoding.");
        return name;
    });
    let offset = 312;
    const macros = Array.from({length: 16}, () => {
        if (offset + 2 > bytes.length) throw fail("Missing macro slot.");
        const length = bytes.readUInt16LE(offset); offset += 2;
        if (offset + length > bytes.length) throw fail("Truncated macro slot.");
        const result = Buffer.from(bytes.subarray(offset, offset + length)); offset += length; return result;
    });
    if (offset !== bytes.length) throw fail("Unexpected settings data.");
    const result = {values, names, macros}; encodeSettings(result, layers); return result;
}
module.exports = {SETTINGS, validSetting, validateMacroIr, encodeSettings, decodeSettings};
