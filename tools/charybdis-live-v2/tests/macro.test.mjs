import assert from "node:assert/strict";
import test from "node:test";
import {describeStep, macroPeek, parseMacro, serializeMacro, unreleased} from "../webview/view/macro.mjs";

test("a payload reads back as the steps the keyboard will play", () => {
    const {steps, error} = parseMacro("noah@xomnia.com{120}{KC_ENT}");
    assert.equal(error, "");
    assert.deepEqual(steps, [
        {kind: "text", text: "noah@xomnia.com"},
        {kind: "delay", delay: 120},
        {kind: "tap", keys: ["KC_ENT"]},
    ]);
    assert.equal(describeStep(steps[0]), "noah@xomnia.com");
    assert.equal(describeStep(steps[1]), "120 ms");
});

test("presses, releases and chords keep their shape", () => {
    const {steps} = parseMacro("{+KC_LGUI}{KC_LSFT,KC_4}{-KC_LGUI}");
    assert.deepEqual(steps.map((step) => step.kind), ["press", "tap", "release"]);
    assert.deepEqual(steps[1].keys, ["KC_LSFT", "KC_4"]);
    assert.equal(describeStep(steps[1]), "KC_LSFT + KC_4");
});

test("literal braces are doubled, exactly as the keyboard stores them", () => {
    const {steps, error} = parseMacro("{{not a command}}");
    assert.equal(error, "");
    assert.deepEqual(steps, [{kind: "text", text: "{not a command}"}]);
});

test("parsed steps serialize losslessly for reordering and removal", () => {
    const source = "hello {{world}}{120}{+KC_LGUI}{KC_D,KC_ENT}{-KC_LGUI}";
    const {steps, error} = parseMacro(source);
    assert.equal(error, "");
    assert.equal(serializeMacro(steps), source);
    assert.equal(serializeMacro([steps[1], steps[0]]), "{120}hello {{world}}");
});

test("a payload this cannot read says so instead of guessing", () => {
    assert.match(parseMacro("{KC_A").error, /missing its }/);
    assert.match(parseMacro("stray }").error, /Unexpected }/);
    assert.match(parseMacro("{}").error, /empty/i);
});

test("keys left held are reported, because the keyboard would hold them", () => {
    assert.deepEqual(unreleased(parseMacro("{+KC_LGUI}{KC_D}").steps), ["KC_LGUI"]);
    assert.deepEqual(unreleased(parseMacro("{+KC_LGUI}{KC_D}{-KC_LGUI}").steps), []);
});

test("a slot's cell shows what the macro sends, not the modifier they all start with", () => {
    // Twelve real slots all opened with {+KC_LEFT_GUI}, so every cell read
    // "KC_LEFT_…" and no slot could be told from another.
    const label = (name) => ({KC_SPACE: "Space", KC_DOT: ".", KC_X: "X", KC_LEFT_GUI: "Left GUI"})[name] || name;
    assert.equal(macroPeek("{+KC_LEFT_GUI}{KC_SPACE}{-KC_LEFT_GUI}", label), "Space");
    assert.equal(macroPeek("{+KC_LEFT_GUI}{+KC_LEFT_SHIFT}{KC_DOT}{-KC_LEFT_SHIFT}{-KC_LEFT_GUI}", label), ".");
    assert.equal(macroPeek("hello", label), "hello", "text still speaks for itself");
    assert.equal(macroPeek("{+KC_LEFT_GUI}", label), "Left GUI", "a press with no tap is all there is to show");
    assert.equal(macroPeek("", label), "");
});

test("the preview refuses exactly what the host refuses", async () => {
    const {createRequire} = await import("node:module");
    const require = createRequire(import.meta.url);
    const {parsePayload, macroKeycodes} = require("../core/schema/macro-payload.js");
    const keys = macroKeycodes();
    const hostAccepts = (payload) => { try { parsePayload(payload); return true; } catch { return false; } };
    const previewAccepts = (payload) => { const {steps, error} = parseMacro(payload, {keys}); return !error && !unreleased(steps).length; };
    for (const payload of [
        "hello{120}{KC_ENT}", "{+KC_LGUI}{KC_LSFT,KC_4}{-KC_LGUI}", "{65535}", "a{{b}}c",
        "{+KC_A,KC_B}", "{-KC_A,KC_B}", "{65536}", "{KC_A,KC_A}", "{-KC_A}", "{+KC_A}{+KC_A}{-KC_A}",
        "{+KC_A}", "{KC_NOT_A_KEY}", "café", "{KC_A", "a}", "x".repeat(32769),
        `{${Array.from({length: 17}, (_, index) => ["KC_A", "KC_B", "KC_C", "KC_D", "KC_E", "KC_F", "KC_G", "KC_H", "KC_I", "KC_J", "KC_K", "KC_L", "KC_M", "KC_N", "KC_O", "KC_P", "KC_Q"][index]).join(",")}}`,
    ]) {
        assert.equal(previewAccepts(payload), hostAccepts(payload), payload.length > 40 ? `${payload.slice(0, 40)}…` : payload);
    }
    assert.equal(parseMacro("{KC_NOT_A_KEY}").error, "", "without the host's key list, names are not judged");
});
