"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {DPI_STEP, DPI_MAX_CHOICE, POINTER_DPI, dpiChoices} = require("../../core/model/pointer-dpi");

test("the app's one DPI list moves in the sensor's steps up to the top of the normal range", () => {
    assert.equal(POINTER_DPI[0], DPI_STEP);
    assert.equal(POINTER_DPI.at(-1), DPI_MAX_CHOICE);
    assert.ok(POINTER_DPI.every((dpi, index) => dpi === DPI_STEP * (index + 1)));
    assert.deepEqual(dpiChoices().map(choice => choice.value), [...POINTER_DPI]);
    assert.deepEqual(dpiChoices()[0], {value: 100, label: "100 DPI"});
});

test("a mode's list leads with normal speed, and a limited field offers only the part the keyboard accepts", () => {
    assert.deepEqual(dpiChoices({normalSpeed: true})[0], {value: 0, label: "Normal pointer speed"});
    assert.deepEqual(dpiChoices({accepts: v => v <= 300}).map(choice => choice.value), [100, 200, 300]);
    assert.throws(() => { POINTER_DPI.push(1); }, TypeError, "the shared list cannot be changed by one field");
});
