"use strict";
const test = require("node:test"), assert = require("node:assert/strict");
const keycodes = require("../../core/data/user-keycodes");

test("each userspace family has its fixed block, as the firmware allocates them", () => {
    assert.deepEqual([keycodes.CUSTOM_KEY_BASE, keycodes.PD_HOLD_BASE, keycodes.PD_LOCK_BASE, keycodes.LAYER_LOCK_BASE], [0x7e40, 0x7e80, 0x7ea0, 0x7ec0]);
    assert.equal(keycodes.customKeyCode(63), 0x7e7f);
    assert.equal(keycodes.customKeyOfCode(0x7e40), 0);
    assert.equal(keycodes.customKeyOfCode(0x7e7f), 63);
    assert.equal(keycodes.customKeyOfCode(0x7e80), undefined, "a pointing hold is not a custom key");
    assert.equal(keycodes.customKeyOfName("CUSTOM_KEY_12"), 12);
    assert.equal(keycodes.customKeyOfName("CUSTOM_KEY_64"), undefined);
    assert.equal(keycodes.layerLockCode(7), 0x7ec7);
    assert.equal(keycodes.layerLockOfCode(0x7ec8), undefined, "only eight layers have a lock");
});
