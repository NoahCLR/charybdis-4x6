"use strict";
const assert = require("node:assert/strict");
const test = require("node:test");
const {createBehaviorDraftStore} = require("../../webview/behavior-drafts");

test("switching rows or keyboards never shares field values", () => {
    const drafts = createBehaviorDraftStore();
    const base = {source: "committed", generation: 12, digest: 42, originHalf: 1};
    drafts.capture("keyboard-1/KC_A", [{id: "timing", value: "175"}], base);
    drafts.capture("keyboard-1/KC_B", [{id: "timing", value: "250"}], base);
    assert.equal(drafts.get("keyboard-1/KC_A").controls[0].value, "175");
    assert.equal(drafts.get("keyboard-1/KC_B").controls[0].value, "250");
    assert.equal(drafts.get("keyboard-2/KC_A"), undefined);
    assert.deepEqual(drafts.keys(), ["keyboard-1/KC_A", "keyboard-1/KC_B"]);
    drafts.get("keyboard-1/KC_A").controls[0].value = "999";
    assert.equal(drafts.get("keyboard-1/KC_A").controls[0].value, "175");
    drafts.remove("keyboard-1/KC_A");
    assert.equal(drafts.get("keyboard-1/KC_A"), undefined);
    assert.ok(drafts.get("keyboard-1/KC_B"));
    drafts.clear();
    assert.deepEqual(drafts.keys(), []);
    assert.equal(drafts.get("keyboard-1/KC_B"), undefined);
});

test("further typing cannot silently rebase a stale draft onto a new profile", () => {
    const drafts = createBehaviorDraftStore();
    const base = {source: "compiled", generation: 0, digest: 42};
    drafts.capture("row", [], base);
    base.digest = 99;
    drafts.capture("row", [{value: "250"}], base);
    assert.equal(drafts.get("row").base.digest, 42);
    assert.equal(drafts.stale("row", base), true);
    assert.equal(drafts.stale("row", null), true);
    assert.equal(drafts.stale("row", {source: "compiled", generation: 0, digest: 42}), false);
    assert.equal(drafts.stale("row", {source: "committed", generation: 0, digest: 42}), true);
});
