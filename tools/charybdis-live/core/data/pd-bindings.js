"use strict";

// The keyboard's pointing-mode keycodes: a fixed registry the firmware owns.
//
// Every slot has one canonical hold and lock name: slot n holds at
// PD_HOLD_BASE + n and toggles at PD_LOCK_BASE + n (user-keycodes.js). Every
// module that names or decodes a slot reads this table. Older portable files
// can still supply the former preset names.

const {PD_HOLD_BASE, PD_LOCK_BASE} = require("./user-keycodes");

const FORMER_NAMES = Object.freeze(["DRAGSCROLL", "VOLUME_MODE", "BRIGHTNESS_MODE", "ZOOM_MODE", "ARROW_MODE", "PINCH_MODE"]);

const PD_BINDINGS = Object.freeze(Array.from({length: 8}, (_, slot) => Object.freeze({
    slot, hold: `PD_SLOT_${slot}`, lock: `PD_SLOT_${slot}_LOCK`,
    holdCode: PD_HOLD_BASE + slot,
    lockCode: PD_LOCK_BASE + slot,
})));

// The slot a native keycode or a binding name reaches, and whether it locks.
const pdBindingOfCode = code => {
    for (const binding of PD_BINDINGS) {
        if (code === binding.holdCode) return {slot: binding.slot, locked: false};
        if (code === binding.lockCode) return {slot: binding.slot, locked: true};
    }
    return undefined;
};
const pdBindingOfName = name => {
    for (const binding of PD_BINDINGS) {
        if (name === binding.hold) return {slot: binding.slot, locked: false};
        if (name === binding.lock) return {slot: binding.slot, locked: true};
    }
    for (let slot = 0; slot < FORMER_NAMES.length; slot++) {
        if (name === FORMER_NAMES[slot]) return {slot, locked: false};
        if (name === `${FORMER_NAMES[slot]}_LOCK`) return {slot, locked: true};
    }
    return undefined;
};

module.exports = {PD_BINDINGS, FORMER_NAMES, pdBindingOfCode, pdBindingOfName};
