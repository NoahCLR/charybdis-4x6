"use strict";
const test = require("node:test"), assert = require("node:assert/strict");
const {PD_BINDINGS, FORMER_NAMES, pdBindingOfCode, pdBindingOfName} = require("../../core/data/pd-bindings");
const {resolveNativeQmkExpression} = require("../../core/schema/compiled-profile-v1");

test("every pointing slot has a hold and a lock binding, found by name and by keycode", () => {
    assert.equal(PD_BINDINGS.length, 8);
    assert.equal(FORMER_NAMES.length, 6, "the six factory presets had names");
    assert.deepEqual(PD_BINDINGS.map((binding) => binding.hold), Array.from({length: 8}, (_, slot) => `PD_SLOT_${slot}`));
    for (const binding of PD_BINDINGS) {
        assert.deepEqual(pdBindingOfCode(binding.holdCode), {slot: binding.slot, locked: false});
        assert.deepEqual(pdBindingOfCode(binding.lockCode), {slot: binding.slot, locked: true});
        assert.deepEqual(pdBindingOfName(binding.lock), {slot: binding.slot, locked: true});
        assert.equal(resolveNativeQmkExpression(binding.hold, {}), binding.holdCode, "the resolver agrees with the registry");
        assert.equal(resolveNativeQmkExpression(binding.lock, {}), binding.lockCode);
    }
    for (let slot = 0; slot < FORMER_NAMES.length; slot++) {
        assert.deepEqual(pdBindingOfName(FORMER_NAMES[slot]), {slot, locked: false});
        assert.deepEqual(pdBindingOfName(`${FORMER_NAMES[slot]}_LOCK`), {slot, locked: true});
        assert.equal(resolveNativeQmkExpression(FORMER_NAMES[slot], {}), PD_BINDINGS[slot].holdCode);
        assert.equal(resolveNativeQmkExpression(`${FORMER_NAMES[slot]}_LOCK`, {}), PD_BINDINGS[slot].lockCode);
    }
    assert.equal(pdBindingOfCode(0x0004), undefined);
    assert.deepEqual([PD_BINDINGS[0].holdCode, PD_BINDINGS[0].lockCode, PD_BINDINGS[6].holdCode, PD_BINDINGS[7].lockCode], [0x7e80, 0x7ea0, 0x7e86, 0x7ea7],
        "the firmware's fixed keycode blocks");
});
