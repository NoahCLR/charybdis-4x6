"use strict";

// The keyboard's userspace keycodes: fixed blocks the firmware owns, one per
// family, reserved beyond what is supported today (users/noah/noah_keymap_ids.h,
// Profile Wire feature bit 16).
//
//   0x7e40–0x7e7f  CUSTOM_KEY_0–63   named keys that do what their behaviour says
//   0x7e80–0x7e9f  PD_SLOT_n         hold pointing slot n
//   0x7ea0–0x7ebf  PD_SLOT_n_LOCK    toggle pointing slot n
//   0x7ec0–0x7edf  LOCK_LAYER(n)     toggle layer n's lock

const CUSTOM_KEY_BASE = 0x7e40, PD_HOLD_BASE = 0x7e80, PD_LOCK_BASE = 0x7ea0, LAYER_LOCK_BASE = 0x7ec0;
const CUSTOM_KEY_SLOTS = 64, LAYER_LOCK_SLOTS = 8;

const customKeyCode = slot => CUSTOM_KEY_BASE + slot;
const customKeyOfCode = code => Number.isInteger(code) && code >= CUSTOM_KEY_BASE && code < CUSTOM_KEY_BASE + CUSTOM_KEY_SLOTS ? code - CUSTOM_KEY_BASE : undefined;
const customKeyOfName = name => {
    const match = /^CUSTOM_KEY_(\d+)$/.exec(String(name ?? ""));
    return match && Number(match[1]) < CUSTOM_KEY_SLOTS ? Number(match[1]) : undefined;
};
const layerLockCode = layer => LAYER_LOCK_BASE + layer;
const layerLockOfCode = code => Number.isInteger(code) && code >= LAYER_LOCK_BASE && code < LAYER_LOCK_BASE + LAYER_LOCK_SLOTS ? code - LAYER_LOCK_BASE : undefined;

module.exports = {
    CUSTOM_KEY_BASE, PD_HOLD_BASE, PD_LOCK_BASE, LAYER_LOCK_BASE, CUSTOM_KEY_SLOTS, LAYER_LOCK_SLOTS,
    customKeyCode, customKeyOfCode, customKeyOfName, layerLockCode, layerLockOfCode,
};
