"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {encodeMacroPayload, decodeMacroPayload, macroKeycodes} = require("../../core/schema/macro-payload");

for (const kind of ["via", "user"]) test(`${kind} macros round-trip text, literal braces, Cmd+N, holds and delays`, () => {
    const payload = 'hello {{"key": 1}}\n{KC_LGUI,KC_N}{250}{+KC_LSFT}{KC_A}{-KC_LSFT}{0}';
    const bytes = encodeMacroPayload(payload, kind);
    assert.deepEqual(encodeMacroPayload(decodeMacroPayload(bytes, kind), kind), bytes);
    assert.equal(encodeMacroPayload("", kind).length, 0);
    assert.ok(decodeMacroPayload(bytes, kind).includes('{{"key": 1}}'));
});

test("VIA chords hold keys together and release in reverse order", () => {
    assert.deepEqual([...encodeMacroPayload("{KC_LGUI,KC_N}", "via")], [1, 2, 227, 1, 2, 17, 1, 3, 17, 1, 3, 227]);
    assert.deepEqual([...encodeMacroPayload("{KC_LGUI,KC_N}", "user")], [5, 2, 227, 17]);
});

test("macro validation refuses stuck keys, ambiguous commands and oversized content", () => {
    for (const payload of ["{+KC_A}", "{-KC_A}", "{+KC_A}{KC_A}{-KC_A}", "{KC_A,KC_A}", "{G(KC_N)}", "{MO(1)}", "{65536}", "{KC_A,}", "{", "}", "é", "\u0000"]) {
        for (const kind of ["via", "user"]) assert.throws(() => encodeMacroPayload(payload, kind), undefined, payload);
    }
    assert.throws(() => encodeMacroPayload("a".repeat(512), "user"), /capacity/);
    assert.throws(() => decodeMacroPayload(Buffer.from([1, 4, 50]), "via"), /Truncated/);
    assert.ok(macroKeycodes().includes("KC_LGUI"));
    assert.ok(!macroKeycodes().includes("QK_BOOTLOADER"));
});
