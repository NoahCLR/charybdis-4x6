"use strict";
const {test} = require("node:test");
const assert = require("node:assert/strict");
const fixture = require("../../../../tests/fixtures/pd_mode_domain_v1.json");
const {PD_DOMAIN_V1, encodePdDomain, decodePdDomain, isPdTapKey} = require("../../core/schema/pd-mode-domain-v1");
const {decodeProfileBlob, encodeProfileBlob} = require("../../core/schema/profile-blob-v1");
const slots = () => structuredClone(fixture.slots);
const bytes = () => Buffer.from(fixture.hex, "hex");
const reject = (fn, code) => assert.throws(fn, error => error.code === code);

test("deployed native PD, layer-lock and custom identities stay stable during expansion", () => {
    const {resolveNativeQmkExpression} = require("../../core/schema/compiled-profile-v1");
    const model = {layers: Array.from({length: 8}, (_, id) => ({id})), customKeycodes: ["RIGHT_THUMB", "LEFT_THUMB", "CLICK_SPAM", "DRAG_WINDOW"]};
    const names = ["PD_SLOT_0", "PD_SLOT_1", "PD_SLOT_2", "PD_SLOT_3", "PD_SLOT_4", "PD_SLOT_5"];
    names.forEach((name, id) => {
        assert.equal(resolveNativeQmkExpression(name, model), 0x7e50 + id);
        assert.equal(resolveNativeQmkExpression(name + "_LOCK", model), 0x7e56 + id);
    });
    for (let id = 0; id < 8; id++) assert.equal(resolveNativeQmkExpression(`LOCK_LAYER(${id})`, model), 0x7e5c + id);
    model.customKeycodes.forEach((name, id) => assert.equal(resolveNativeQmkExpression(name, model), 0x7e64 + id));
});

test("eight PD slots match the independent byte fixture and preserve the full preset policies", () => {
    const encoded = encodePdDomain(slots());
    assert.equal(encoded.length, 776);
    assert.equal(encoded.toString("hex"), fixture.hex);
    const decoded = decodePdDomain(encoded);
    assert.deepEqual(encodePdDomain(decoded), encoded);
    assert.equal(decoded[4].directions.up.mask, 0x44);
    assert.equal(decoded[4].buttons[0].modifiers, 0x20);
    assert.equal(decoded[5].heldModifiers, 8);
    assert.equal(decoded[7].kind, 0);
});

test("slot seven accepts a custom media mode and byte-bounded Unicode names", () => {
    const value = slots();
    value[7] = {id: 7, kind: 1, axis: 2, name: "Édition ⌘", thresholdX: 40, thresholdY: 60,
        directions: {left: {keycode: 0xac}, right: {keycode: 0xab}, up: {keycode: 0xa9}, down: {keycode: 0xaa}}};
    assert.equal(decodePdDomain(encodePdDomain(value))[7].name, value[7].name);
    value[7].name = "x".repeat(23); encodePdDomain(value);
    value[7].name += "x"; reject(() => encodePdDomain(value), "INVALID_NAME");
    for (const name of ["\ud800", "\u0000", "x\n", "é".repeat(12)]) {
        value[7].name = name; reject(() => encodePdDomain(value), "INVALID_NAME");
    }
});

test("PD decoder rejects malformed framing, hidden disabled state and noncanonical records", () => {
    reject(() => decodePdDomain(bytes().subarray(1)), "INVALID_LENGTH");
    reject(() => decodePdDomain(Buffer.concat([bytes(), Buffer.from([0])])), "INVALID_LENGTH");
    for (const [offset, value, code] of [[0, 2, "INVALID_HEADER"], [3, 1, "INVALID_HEADER"], [8, 1, "INVALID_ID"],
        [15, 1, "RESERVED"], [98, 1, "RESERVED"], [8 + 6 * 96 + 4, 100, "INVALID_PARAMETER"],
        [16, 0xc0, "INVALID_NAME"], [39, 1, "INVALID_NAME"], [9, 3, "INVALID_POLICY"]]) {
        const b = bytes(); b[offset] = value;
        reject(() => decodePdDomain(b), code);
    }
    const value = slots(); [value[6], value[7]] = [value[7], value[6]];
    reject(() => encodePdDomain(value), "INVALID_ID");
    const sparse = slots(); delete sparse[7]; reject(() => encodePdDomain(sparse), "INVALID_ARGUMENT");
    reject(() => encodePdDomain(slots().slice(0, 7)), "INVALID_LENGTH");
});

test("directional output vocabulary excludes controls, mouse actions, macros and recursive mode actions", () => {
    for (const key of [4, 0xa4, 0xa5, 0xc2, 0x082e, 0x114f, 0x1f50]) assert.ok(isPdTapKey(key));
    for (const key of [0, 1, 3, 0xc3, 0xd1, 0xe0, 0x1004, 0x08aa, 0x2004, 0x5220, 0x7700, 0x7e50, 0x7c00, -1, 65536, NaN]) assert.equal(isPdTapKey(key), false);
    for (const tap of [{keycode: 0x7e50}, {keycode: 0, mask: 1}, {keycode: 4, modifierPolicy: 1},
        {keycode: 4, modifierPolicy: 2, mask: 8}, {keycode: 4, modifierPolicy: 3}]) {
        const value = slots(); value[1].directions.up = tap;
        reject(() => encodePdDomain(value), "INVALID_ACTION");
    }
    const value = slots(); value[1].directions.left = {keycode: 4};
    reject(() => encodePdDomain(value), "INVALID_PARAMETER");
});

test("scroll validation rejects bad timing and ratios before they reach arithmetic", () => {
    for (const patch of [{divisorH: 0}, {thresholdV: 0}, {lockMs: 0}, {expireMs: 54}, {startDenominator: 0},
        {startNumerator: 1}, {sustainNumerator: 8}, {decayDivisor: 1}, {invert: 4}, {intervalMs: 65536}, {thresholdH: -1}]) {
        const value = slots(); Object.assign(value[0].scroll, patch);
        reject(() => encodePdDomain(value), "INVALID_PARAMETER");
    }
    const value = slots(); value[0].scroll.intervalMs = 0; value[0].scroll.expireMs = 55;
    encodePdDomain(value); // Inclusive expiry/lock equality and unthrottled wheel output.
    value[0].directions = {up: {keycode: 4}};
    reject(() => encodePdDomain(value), "INVALID_PARAMETER");
});

test("button override validation requires balanced, explicit output kinds", () => {
    for (const button of [{kind: 4}, {kind: 0, modifiers: 8}, {kind: 1, tap: {keycode: 4}},
        {kind: 2}, {kind: 2, modifiers: 8, tap: {keycode: 4}}, {kind: 3}, {kind: 3, modifiers: 8, tap: {keycode: 4}}]) {
        const value = slots(); value[4].buttons[0] = button;
        reject(() => encodePdDomain(value), "INVALID_ACTION");
    }
    const value = slots(); value[4].buttons = new Array(3);
    reject(() => encodePdDomain(value), "INVALID_ARGUMENT");
});

test("encoder rejects unknown fields and numeric coercion rather than silently dropping edits", () => {
    for (const patch of [{dpi: "400"}, {dpi: null}, {directions: null}, {buttons: null}, {scroll: null}, {thresholdY: 0.5}, {name: null}, {kind: NaN}, {dpi: Infinity}, {unknown: true}]) {
        const value = slots(); Object.assign(value[1], patch);
        assert.throws(() => encodePdDomain(value));
    }
});

test("standalone PD codec does not enable a new domain in current live writers", () => {
    reject(() => encodeProfileBlob({domains: [{id: PD_DOMAIN_V1.ID, version: 1, payload: bytes()}]}), "UNKNOWN_DOMAIN");
    const envelope = Buffer.alloc(12);
    Buffer.from([0x4e, 0x4c, 0x50, 0x31, 1, 0, 1, 1, 0x50, 1, 8, 3]).copy(envelope);
    reject(() => decodeProfileBlob(Buffer.concat([envelope, bytes()])), "UNKNOWN_DOMAIN");
});
