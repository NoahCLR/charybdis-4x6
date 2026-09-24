"use strict";
const test = require("node:test"), assert = require("node:assert/strict");
const {PD_BINDINGS, LEGACY_PD_SLOTS, pdBindingOfCode, pdBindingOfName} = require("../../core/data/pd-bindings");
const {resolveNativeQmkExpression} = require("../../core/schema/compiled-profile-v1");

test("every pointing slot has a hold and a lock binding, found by name and by keycode", () => {
    assert.equal(PD_BINDINGS.length, 8);
    assert.equal(LEGACY_PD_SLOTS, 6);
    assert.deepEqual(PD_BINDINGS.slice(0, 6).map((binding) => binding.hold), ["DRAGSCROLL", "VOLUME_MODE", "BRIGHTNESS_MODE", "ZOOM_MODE", "ARROW_MODE", "PINCH_MODE"]);
    for (const binding of PD_BINDINGS) {
        assert.deepEqual(pdBindingOfCode(binding.holdCode), {slot: binding.slot, locked: false});
        assert.deepEqual(pdBindingOfCode(binding.lockCode), {slot: binding.slot, locked: true});
        assert.deepEqual(pdBindingOfName(binding.lock), {slot: binding.slot, locked: true});
        assert.equal(resolveNativeQmkExpression(binding.hold, {}), binding.holdCode, "the resolver agrees with the registry");
        assert.equal(resolveNativeQmkExpression(binding.lock, {}), binding.lockCode);
    }
    assert.equal(pdBindingOfCode(0x0004), undefined);
    assert.deepEqual([PD_BINDINGS[0].holdCode, PD_BINDINGS[0].lockCode, PD_BINDINGS[6].holdCode, PD_BINDINGS[7].lockCode], [0x7e50, 0x7e56, 0x7ef0, 0x7ef3],
        "the firmware's fixed keycodes");
});
