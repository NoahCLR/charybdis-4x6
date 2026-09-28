"use strict";
// v1/v2 carry 16 user-macro IR records after the fixed part; v3 instead names
// the 64 macros (u8 length 0..23, then UTF-8), in the same 1,368 bytes. v4
// keeps those records but gives every name up to 20 printable ASCII
// characters, and its ceiling holds all 64 at full length. v5 follows them
// with the 64 custom keys' names in the same form, counted in header byte 4.
const SETTINGS = Object.freeze({COUNT: 28, LAYERS: 8, NAME_BYTES: 24, MACROS: 16, MACRO_NAMES: 64, MACRO_NAME_BYTES: 23, MACRO_NAME_CHARS: 20,
    CUSTOM_KEY_NAMES: 64, FIXED_SIZE: 312, MAX_SIZE: 1368, V4_MAX_SIZE: 312 + 64 * 21, V5_MAX_SIZE: 312 + 128 * 21});
const CURRENT_VERSION = 5;
// The version a macro-name edit writes: every schema-2 keyboard reads v4.
const MACRO_NAME_VERSION = 4;
const maxSize = version => version >= 5 ? SETTINGS.V5_MAX_SIZE : version >= 4 ? SETTINGS.V4_MAX_SIZE : SETTINGS.MAX_SIZE;
const asciiName = name => typeof name === "string" && /^[\x20-\x7e]*$/.test(name) && name.length <= SETTINGS.MACRO_NAME_CHARS;
function validName(name, maxBytes) {
    return typeof name === "string" && !/[\u0000-\u001f\u007f]/u.test(name) && Buffer.byteLength(name) <= maxBytes && Buffer.from(name).toString() === name;
}
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
    const version = value?.formatVersion ?? 1;
    if (![1, 2, 3, 4, 5].includes(version) || (version >= 2 && value?.values?.slice(10, 15).some(v => v !== 0))) throw fail("Unsupported settings format or retired PD settings.");
    if (!value || !Array.isArray(value.values) || value.values.length !== 28 || !value.values.every((v, id) => validSetting(id, v, layers)) || value.values[6] <= value.values[16]) throw fail("Invalid keyboard settings.");
    if (!Array.isArray(value.names) || value.names.length !== 8) throw fail("Missing layer names.");
    if (version >= 3 ? !Array.isArray(value.macroNames) || value.macroNames.length !== SETTINGS.MACRO_NAMES : !Array.isArray(value.macros) || value.macros.length !== 16) throw fail(version >= 3 ? "Missing macro names." : "Missing layer names or macros.");
    if (version >= 5 && (!Array.isArray(value.customKeyNames) || value.customKeyNames.length !== SETTINGS.CUSTOM_KEY_NAMES)) throw fail("Missing custom key names.");
    const fixed = Buffer.alloc(312); Buffer.from([version, 8, 28, version >= 3 ? SETTINGS.MACRO_NAMES : 16, version >= 5 ? SETTINGS.CUSTOM_KEY_NAMES : 0]).copy(fixed);
    value.values.forEach((v, id) => fixed.writeUInt32LE(v, 8 + id * 4));
    value.names.forEach((name, id) => {
        if (typeof name !== "string" || /[\u0000-\u001f\u007f]/u.test(name) || Buffer.byteLength(name) > 23 || Buffer.from(name).toString() !== name) throw fail("Layer names must fit 23 UTF-8 bytes and contain no control characters.");
        fixed.write(name, 120 + id * 24, 23, "utf8");
    });
    if (version >= 3) {
        const records = value.macroNames.map(name => {
            if (version >= 4 ? !asciiName(name) : !validName(name, SETTINGS.MACRO_NAME_BYTES)) throw fail(version >= 4 ? "A macro name is up to 20 plain characters: letters, digits, spaces and punctuation." : "Macro names must fit 23 UTF-8 bytes and contain no control characters.");
            const text = Buffer.from(name, "utf8");
            return Buffer.concat([Buffer.from([text.length]), text]);
        });
        if (version >= 5) for (const name of value.customKeyNames) {
            if (!asciiName(name)) throw fail("A custom key name is up to 20 plain characters: letters, digits, spaces and punctuation.");
            records.push(Buffer.concat([Buffer.from([name.length]), Buffer.from(name, "ascii")]));
        }
        const output = Buffer.concat([fixed, ...records]);
        if (output.length > maxSize(version)) throw fail(`The macro names need ${output.length - SETTINGS.FIXED_SIZE - SETTINGS.MACRO_NAMES} bytes; they share ${SETTINGS.MAX_SIZE - SETTINGS.FIXED_SIZE - SETTINGS.MACRO_NAMES}. Shorten some names.`);
        return output;
    }
    const macros = value.macros.map(bytes => {validateMacroIr(bytes); const size = Buffer.alloc(2); size.writeUInt16LE(bytes.length); return Buffer.concat([size, bytes]);});
    const output = Buffer.concat([fixed, ...macros]);
    if (output.length > SETTINGS.MAX_SIZE) throw fail("The macro bank exceeds the portable profile capacity.");
    return output;
}
function decodeSettings(bytes, layers = 8) {
    const count = bytes?.[0] >= 3 ? SETTINGS.MACRO_NAMES : 16, customKeys = bytes?.[0] >= 5 ? SETTINGS.CUSTOM_KEY_NAMES : 0;
    if (!Buffer.isBuffer(bytes) || bytes.length < SETTINGS.FIXED_SIZE + (count === 16 ? 32 : count + customKeys) || bytes.length > maxSize(bytes[0]) || ![1, 2, 3, 4, 5].includes(bytes[0]) || !bytes.subarray(0, 8).equals(Buffer.from([bytes[0], 8, 28, count, customKeys, 0, 0, 0]))) throw fail("Unsupported settings format.");
    const values = Array.from({length: 28}, (_, id) => bytes.readUInt32LE(8 + id * 4));
    const names = Array.from({length: 8}, (_, id) => {
        const field = bytes.subarray(120 + id * 24, 144 + id * 24), end = field.indexOf(0);
        if (end < 0 || field.subarray(end).some(v => v)) throw fail("Invalid layer name padding.");
        const name = field.subarray(0, end).toString("utf8");
        if (!Buffer.from(name).equals(field.subarray(0, end))) throw fail("Invalid layer name encoding.");
        return name;
    });
    let offset = 312;
    if (bytes[0] >= 3) {
        const nameRecord = () => {
            if (offset >= bytes.length) throw fail("Missing macro name.");
            const length = bytes[offset++];
            if (length > (bytes[0] >= 4 ? SETTINGS.MACRO_NAME_CHARS : SETTINGS.MACRO_NAME_BYTES) || offset + length > bytes.length) throw fail("Invalid macro name length.");
            const raw = bytes.subarray(offset, offset + length), name = raw.toString("utf8");
            offset += length;
            if (!Buffer.from(name).equals(raw)) throw fail("Invalid macro name encoding.");
            return name;
        };
        const macroNames = Array.from({length: SETTINGS.MACRO_NAMES}, nameRecord);
        const customKeyNames = Array.from({length: customKeys}, nameRecord);
        if (offset !== bytes.length) throw fail("Unexpected settings data.");
        const result = {values, names, macroNames, ...(customKeys ? {customKeyNames} : {}), formatVersion: bytes[0]}; encodeSettings(result, layers); return result;
    }
    const macros = Array.from({length: 16}, () => {
        if (offset + 2 > bytes.length) throw fail("Missing macro slot.");
        const length = bytes.readUInt16LE(offset); offset += 2;
        if (offset + length > bytes.length) throw fail("Truncated macro slot.");
        const result = Buffer.from(bytes.subarray(offset, offset + length)); offset += length; return result;
    });
    if (offset !== bytes.length) throw fail("Unexpected settings data.");
    const result = {values, names, macros, ...(bytes[0] === 2 ? {formatVersion: 2} : {})}; encodeSettings(result, layers); return result;
}
// The same settings as v4, or v5 with a target of 5. From v1/v2 the retired
// user macros go and the 64 names start empty; from v3 each name keeps its
// printable ASCII characters, cut to 20, so a name v4 cannot hold changes
// visibly in review rather than failing the edit. v5 adds the custom keys'
// names, empty. Only a name edit upgrades a profile: a macro's to v4, a custom
// key's to v5; everything else keeps the version it was read in.
function upgradeSettings(value, target = MACRO_NAME_VERSION) {
    const version = value?.formatVersion ?? 1;
    if (version >= target) return value;
    const {macros, ...rest} = value;
    const macroNames = version >= 4 ? value.macroNames
        : version >= 3 ? value.macroNames.map(name => name.replace(/[^\x20-\x7e]/g, "").trim().slice(0, SETTINGS.MACRO_NAME_CHARS).trim())
        : Array(SETTINGS.MACRO_NAMES).fill("");
    return {...rest, formatVersion: target, macroNames, ...(target >= 5 ? {customKeyNames: Array(SETTINGS.CUSTOM_KEY_NAMES).fill("")} : {})};
}
function macroNamesOf(value) {
    return (value?.formatVersion ?? 1) >= 3 ? value.macroNames.slice() : Array(SETTINGS.MACRO_NAMES).fill("");
}
function customKeyNamesOf(value) {
    return (value?.formatVersion ?? 1) >= 5 ? value.customKeyNames.slice() : Array(SETTINGS.CUSTOM_KEY_NAMES).fill("");
}
module.exports = {SETTINGS, CURRENT_VERSION, MACRO_NAME_VERSION, maxSize, asciiName, validSetting, validateMacroIr, encodeSettings, decodeSettings, upgradeSettings, macroNamesOf, customKeyNamesOf};
