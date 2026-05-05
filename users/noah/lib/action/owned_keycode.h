// ────────────────────────────────────────────────────────────────────────────
// Owned Keycode Dispatch
// ────────────────────────────────────────────────────────────────────────────
//
// Shared literal-keycode helpers for userspace-owned synthetic dispatch.
// This surface intentionally covers only:
//   - plain 8-bit keycodes
//   - plain modifier keycodes
//   - QK_MODS keycodes such as S(KC_1) or G(KC_C)
//
// Higher QMK behavior keycodes stay with their existing dedicated handlers.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

bool owned_keycode_register(uint16_t keycode);
bool owned_keycode_unregister(uint16_t keycode);
bool owned_keycode_tap(uint16_t keycode);
