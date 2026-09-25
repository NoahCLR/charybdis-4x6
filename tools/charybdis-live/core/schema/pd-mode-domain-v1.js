"use strict";

// PD domain codec, carried by schema-2 profiles and legacy migration evidence.
const PD_DOMAIN_V1 = Object.freeze({ID: 0x50, VERSION: 1, SLOTS: 8, HEADER_SIZE: 8, RECORD_SIZE: 96, NAME_SIZE: 24, SIZE: 776});
const PD_KIND = Object.freeze({DISABLED: 0, DIRECTIONAL: 1, SCROLLING: 2});
const PD_AXIS = Object.freeze({VERTICAL: 0, HORIZONTAL: 1, DOMINANT: 2, EIGHT: 3});
// What an eight-direction mode does when a diagonal has no shortcut.
const PD_EMPTY_DIRECTION = Object.freeze({NEAREST: 0, BOTH: 1, NOTHING: 2});
const PD_MODIFIERS = Object.freeze({INHERIT: 0, MASK: 1, EXACT: 2});
const PD_BUTTON = Object.freeze({PASS_THROUGH: 0, CONSUME: 1, TAP: 2, HOLD_MODIFIERS: 3});
const HEADER = Buffer.from([1, 8, 96, 0, 0, 0, 0, 0]);
const DIRECTIONS = ["left", "right", "up", "down"];
// Eight directions keep their diagonals in bytes 70..85, which any other
// directional record leaves zero. Byte 86 is what every directional mode does
// with motion toward a direction that has no shortcut.
const DIAGONALS = ["upLeft", "upRight", "downLeft", "downRight"];
const SCROLL_U16 = ["thresholdH", "thresholdV", "divisorH", "divisorV", "intervalMs", "expireMs", "lockMs"];
const SCROLL_U8 = ["startNumerator", "startDenominator", "sustainNumerator", "sustainDenominator", "decayDivisor", "invert"];

function fail(code, offset, message) {
    throw Object.assign(new Error(message), {code, offset});
}
function zero(bytes) { return bytes.every(byte => byte === 0); }
function isPdTapKey(keycode) {
    if (!Number.isInteger(keycode) || keycode < 0 || keycode > 0xffff) return false;
    if (keycode >= 4 && keycode <= 0xc2) return true;
    const mods = keycode >>> 8, key = keycode & 255;
    return mods <= 0x1f && (mods & 15) !== 0 && key >= 4 && key <= 0xa4;
}
function validTap(bytes) {
    if (bytes.readUInt16LE(0) === 0) return zero(bytes);
    return isPdTapKey(bytes.readUInt16LE(0)) && (bytes[2] === PD_MODIFIERS.MASK
        ? bytes[3] !== 0 : [PD_MODIFIERS.INHERIT, PD_MODIFIERS.EXACT].includes(bytes[2]) && bytes[3] === 0);
}
function readName(bytes, offset) {
    const end = bytes.indexOf(0), text = bytes.subarray(0, Math.max(end, 0)).toString("utf8");
    if (end < 0 || !zero(bytes.subarray(end)) || !Buffer.from(text, "utf8").equals(bytes.subarray(0, end)) || /[\u0000-\u001f\u007f]/u.test(text)) {
        fail("INVALID_NAME", offset, "Mode name must be valid UTF-8, terminated and zero-padded.");
    }
    return text;
}
function validateRecord(p, slot, offset) {
    const reject = (code, at, message) => fail(code, offset + at, message);
    if (p[0] !== slot) reject("INVALID_ID", 0, "PD slots must appear exactly once in ID order 0–7.");
    if (p[7] || !zero(p.subarray(90))) reject("RESERVED", p[7] ? 7 : 90, "Reserved PD bytes must be zero.");
    const name = readName(p.subarray(8, 32), offset + 8);
    if (p[1] > 2 || p[2] > 1 || p[3] > PD_AXIS.EIGHT) reject("INVALID_POLICY", 1, "Unknown engine, layer policy or axis policy.");
    if (p[1] === PD_KIND.DISABLED) {
        if (!zero(p.subarray(2, 8)) || !zero(p.subarray(32))) reject("INVALID_PARAMETER", 2, "Disabled slots retain only their ID and name.");
        return;
    }
    if (!name) reject("INVALID_NAME", 8, "Configured modes need a name.");
    for (let at = 36; at < 52; at += 4) {
        if (!validTap(p.subarray(at, at + 4))) reject("INVALID_ACTION", at, "Unsupported directional tap or modifier policy.");
    }
    for (let at = 52; at < 70; at += 6) {
        const kind = p[at], tap = p.subarray(at + 2, at + 6);
        if (kind > 3 || (kind <= 1 && !zero(p.subarray(at + 1, at + 6))) ||
            (kind === 2 && (p[at + 1] !== 0 || tap.readUInt16LE(0) === 0 || !validTap(tap))) ||
            (kind === 3 && (p[at + 1] === 0 || !zero(tap)))) reject("INVALID_ACTION", at, "Invalid button override.");
    }
    const x = p.readUInt16LE(32), y = p.readUInt16LE(34);
    if (p[1] === PD_KIND.DIRECTIONAL && p[3] === PD_AXIS.EIGHT) {
        if (p[6]) reject("INVALID_PARAMETER", 6, "Directional modes cannot carry owned scroll modifiers.");
        if (!x || !y) reject("INVALID_PARAMETER", 32, "Enabled axes need a nonzero movement threshold.");
        for (let at = 70; at < 86; at += 4) {
            if (!validTap(p.subarray(at, at + 4))) reject("INVALID_ACTION", at, "Unsupported diagonal tap or modifier policy.");
        }
        if (p[86] > PD_EMPTY_DIRECTION.NOTHING) reject("INVALID_POLICY", 86, "Unknown empty-direction policy.");
        if (!zero(p.subarray(87, 90))) reject("RESERVED", 87, "Reserved PD bytes must be zero.");
    } else if (p[1] === PD_KIND.DIRECTIONAL) {
        if (p[6] || !zero(p.subarray(70, 86))) reject("INVALID_PARAMETER", 6, "Directional modes cannot carry scroll settings or owned scroll modifiers.");
        if (p[86] > PD_EMPTY_DIRECTION.NOTHING) reject("INVALID_POLICY", 86, "Unknown empty-direction policy.");
        if (!zero(p.subarray(87, 90))) reject("RESERVED", 87, "Reserved PD bytes must be zero.");
        if ((p[3] !== PD_AXIS.VERTICAL && !x) || (p[3] !== PD_AXIS.HORIZONTAL && !y)) reject("INVALID_PARAMETER", 32, "Enabled axes need a nonzero movement threshold.");
        if ((p[3] === PD_AXIS.VERTICAL && (x || !zero(p.subarray(36, 44)))) ||
            (p[3] === PD_AXIS.HORIZONTAL && (y || !zero(p.subarray(44, 52))))) reject("INVALID_PARAMETER", 32, "Unused axes must have zero thresholds and outputs.");
    } else {
        if (p[3] || !zero(p.subarray(32, 52))) reject("INVALID_PARAMETER", 3, "Scrolling modes cannot carry directional settings.");
        if (![70, 72, 74, 76, 82].every(at => p.readUInt16LE(at) > 0) || p.readUInt16LE(80) < p.readUInt16LE(82) ||
            ![84, 85, 86, 87].every(at => p[at] > 0) || p[84] < p[85] || p[86] < p[87] ||
            p[86] * p[85] > p[84] * p[87] || p[88] < 2 || p[89] > 3) reject("INVALID_PARAMETER", 70, "Invalid scrolling thresholds, ratios, divisors or timing.");
    }
}

function tapFromBytes(bytes, at) {
    return {keycode: bytes.readUInt16LE(at), modifierPolicy: bytes[at + 2], mask: bytes[at + 3]};
}
function decodePdDomain(bytes) {
    if (!Buffer.isBuffer(bytes)) fail("INVALID_ARGUMENT", 0, "PD domain must be a Buffer.");
    if (bytes.length !== PD_DOMAIN_V1.SIZE) fail("INVALID_LENGTH", 0, "PD domain must contain exactly 776 bytes.");
    for (let at = 0; at < HEADER.length; at++) {
        if (bytes[at] !== HEADER[at]) fail("INVALID_HEADER", at, "Unsupported PD domain header.");
    }
    return Array.from({length: PD_DOMAIN_V1.SLOTS}, (_, id) => {
        const offset = PD_DOMAIN_V1.HEADER_SIZE + id * PD_DOMAIN_V1.RECORD_SIZE;
        const p = bytes.subarray(offset, offset + PD_DOMAIN_V1.RECORD_SIZE);
        validateRecord(p, id, offset);
        return {
            id, kind: p[1], pointerLayer: p[2], axis: p[3], dpi: p.readUInt16LE(4), heldModifiers: p[6], name: readName(p.subarray(8, 32), offset + 8),
            thresholdX: p.readUInt16LE(32), thresholdY: p.readUInt16LE(34),
            directions: Object.fromEntries(DIRECTIONS.map((name, i) => [name, tapFromBytes(p, 36 + i * 4)])),
            buttons: Array.from({length: 3}, (_, i) => ({kind: p[52 + i * 6], modifiers: p[53 + i * 6], tap: tapFromBytes(p, 54 + i * 6)})),
            // Bytes 70..90 are scroll settings only in a scrolling mode.
            scroll: Object.fromEntries([...SCROLL_U16.map((name, i) => [name, p[1] === PD_KIND.SCROLLING ? p.readUInt16LE(70 + i * 2) : 0]), ...SCROLL_U8.map((name, i) => [name, p[1] === PD_KIND.SCROLLING ? p[84 + i] : 0])]),
            diagonals: Object.fromEntries(DIAGONALS.map((name, i) => [name, p[1] === PD_KIND.DIRECTIONAL ? tapFromBytes(p, 70 + i * 4) : {keycode: 0, modifierPolicy: 0, mask: 0}])),
            emptyDirection: p[1] === PD_KIND.DIRECTIONAL ? p[86] : 0,
        };
    });
}

function object(value, keys, label) {
    if (!value || typeof value !== "object" || Array.isArray(value) || Object.keys(value).some(key => !keys.includes(key))) {
        fail("INVALID_ARGUMENT", 0, `${label} must be an object with supported fields.`);
    }
    return value;
}
function optional(value, fallback) { return value === undefined ? fallback : value; }
function integer(value, max) {
    if (!Number.isInteger(value) || value < 0 || value > max) fail("INVALID_PARAMETER", 0, `PD value must be an integer from 0 to ${max}.`);
    return value;
}
function writeTap(p, at, value = {}) {
    object(value, ["keycode", "modifierPolicy", "mask"], "Tap");
    p.writeUInt16LE(integer(optional(value.keycode, 0), 65535), at);
    p[at + 2] = integer(optional(value.modifierPolicy, 0), 255);
    p[at + 3] = integer(optional(value.mask, 0), 255);
}
function encodePdDomain(slots) {
    if (!Array.isArray(slots) || slots.length !== PD_DOMAIN_V1.SLOTS) fail("INVALID_LENGTH", 0, "Exactly eight PD slots are required.");
    const bytes = Buffer.alloc(PD_DOMAIN_V1.SIZE);
    HEADER.copy(bytes);
    for (let id = 0; id < PD_DOMAIN_V1.SLOTS; id++) {
        const slot = object(slots[id], ["id", "kind", "pointerLayer", "axis", "dpi", "heldModifiers", "name", "thresholdX", "thresholdY", "directions", "buttons", "scroll", "diagonals", "emptyDirection"], "Slot");
        const p = bytes.subarray(8 + id * 96, 8 + (id + 1) * 96);
        p[0] = integer(slot.id, 7);
        for (const [at, name] of [[1, "kind"], [2, "pointerLayer"], [3, "axis"], [6, "heldModifiers"]]) p[at] = integer(optional(slot[name], 0), 255);
        for (const [at, name] of [[4, "dpi"], [32, "thresholdX"], [34, "thresholdY"]]) p.writeUInt16LE(integer(optional(slot[name], 0), 65535), at);
        if (typeof slot.name !== "string" || Buffer.byteLength(slot.name, "utf8") >= 24 || /[\u0000-\u001f\u007f]/u.test(slot.name) || Buffer.from(slot.name).toString("utf8") !== slot.name) {
            fail("INVALID_NAME", 8 + id * 96 + 8, "Names must fit 23 UTF-8 bytes and contain no ASCII controls.");
        }
        p.write(slot.name, 8, 23, "utf8");
        const directions = object(optional(slot.directions, {}), DIRECTIONS, "Directions");
        DIRECTIONS.forEach((name, i) => writeTap(p, 36 + i * 4, directions[name]));
        const buttons = optional(slot.buttons, [{}, {}, {}]);
        if (!Array.isArray(buttons) || buttons.length !== 3) fail("INVALID_LENGTH", 0, "Exactly three button overrides are required.");
        for (let i = 0; i < 3; i++) {
            const button = object(buttons[i], ["kind", "modifiers", "tap"], "Button");
            p[52 + i * 6] = integer(optional(button.kind, 0), 255);
            p[53 + i * 6] = integer(optional(button.modifiers, 0), 255);
            writeTap(p, 54 + i * 6, button.tap);
        }
        const scroll = object(optional(slot.scroll, {}), [...SCROLL_U16, ...SCROLL_U8], "Scroll settings");
        const diagonals = object(optional(slot.diagonals, {}), DIAGONALS, "Diagonals");
        if (slot.kind === PD_KIND.DIRECTIONAL) {
            // A directional record's bytes 70..90 hold diagonals or nothing.
            DIAGONALS.forEach((name, i) => writeTap(p, 70 + i * 4, diagonals[name]));
            p[86] = integer(optional(slot.emptyDirection, 0), 255);
        } else {
            SCROLL_U16.forEach((name, i) => p.writeUInt16LE(integer(optional(scroll[name], 0), 65535), 70 + i * 2));
            SCROLL_U8.forEach((name, i) => { p[84 + i] = integer(optional(scroll[name], 0), 255); });
        }
    }
    decodePdDomain(bytes);
    return bytes;
}

module.exports = {PD_DOMAIN_V1, PD_KIND, PD_AXIS, PD_EMPTY_DIRECTION, PD_MODIFIERS, PD_BUTTON, DIAGONALS, isPdTapKey, encodePdDomain, decodePdDomain};
