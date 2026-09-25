import assert from "node:assert/strict";
import test from "node:test";
import {historyEntries, stepTime} from "../webview/view/history.mjs";

const change = (area, title, extra = {}) => ({area, title, status: "changed", fields: [], ...extra});

test("the history reads newest first, each step's items in rail order with their area", () => {
    const entries = historyEntries([
        {step: 0, label: "Read from the keyboard", changes: null, current: false, undone: false},
        {step: 1, label: "Swapped two layers", current: true, undone: false,
            changes: [change("Lighting", "Symbols colour"), change("Layout", "Base · Q"), change("Layers", "Layer priority", {note: "Higher layers win"})]},
        {step: 2, label: "Edited a macro", current: false, undone: true, changes: [change("Macros", "Macro 0")]},
    ]);
    assert.deepEqual(entries.map((entry) => entry.step), [2, 1, 0]);
    assert.deepEqual(entries[1].changes.map((entry) => [entry.title, entry.note]),
        [["Base · Q", "Layout"], ["Layer priority", "Layers · Higher layers win"], ["Symbols colour", "Lighting"]]);
    assert.equal(entries[2].changes, null, "where the draft began stays without a comparison");
});

test("a step's time is how long ago while close, then the clock", () => {
    const now = new Date(2026, 8, 25, 14, 30, 0).getTime();
    assert.equal(stepTime(now - 10_000, now).label, "just now");
    assert.equal(stepTime(now - 5 * 60_000, now).label, "5 min ago");
    assert.equal(stepTime(now - 2 * 3600_000, now).label, "12:30");
    assert.equal(stepTime(now - 2 * 3600_000, now).clock, "12:30:00");
    assert.match(stepTime(now - 26 * 3600_000, now).label, /12:30$/);
    assert.notEqual(stepTime(now - 26 * 3600_000, now).label, "12:30", "another day says which");
});
