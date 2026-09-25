"use strict";

// The keyboard's pointing-mode keycodes: a fixed registry the firmware owns.
//
// Slots 0–5 carry the names they shipped with (DRAGSCROLL, VOLUME_MODE…) in
// the user-keycode block, hold then lock; slots 6 and 7 came later and sit in
// their own block, hold and lock side by side. Every module that names, finds
// or decodes a pointing-mode key reads this one table.

const LEGACY = ["DRAGSCROLL", "VOLUME_MODE", "BRIGHTNESS_MODE", "ZOOM_MODE", "ARROW_MODE", "PINCH_MODE"];
const LEGACY_BASE = 0x7e50, SLOT_BASE = 0x7ef0;

const PD_BINDINGS = Object.freeze(Array.from({length: 8}, (_, slot) => Object.freeze(slot < LEGACY.length
    ? {slot, hold: LEGACY[slot], lock: `${LEGACY[slot]}_LOCK`, holdCode: LEGACY_BASE + slot, lockCode: LEGACY_BASE + LEGACY.length + slot}
    : {slot, hold: `PD_SLOT_${slot}`, lock: `PD_SLOT_${slot}_LOCK`, holdCode: SLOT_BASE + (slot - LEGACY.length) * 2, lockCode: SLOT_BASE + (slot - LEGACY.length) * 2 + 1})));

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
    return undefined;
};

// The legacy slots own a hold and a lock keycode each in the user block,
// which is where the keycodes after them (layer locks, custom keys) start.
const LEGACY_PD_SLOTS = LEGACY.length;

module.exports = {PD_BINDINGS, LEGACY_PD_SLOTS, pdBindingOfCode, pdBindingOfName};
