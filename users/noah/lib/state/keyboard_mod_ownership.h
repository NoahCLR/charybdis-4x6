// ────────────────────────────────────────────────────────────────────────────
// Keyboard Modifier Ownership
// ────────────────────────────────────────────────────────────────────────────
//
// Tracks physical modifier keys separately from userspace-owned synthetic
// modifier holds so releasing one owner does not clear another.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

void keyboard_mod_ownership_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record);
bool keyboard_mod_ownership_should_suppress_default(uint16_t keycode, keyrecord_t *record);
void keyboard_mod_ownership_register(uint16_t keycode);
void keyboard_mod_ownership_unregister(uint16_t keycode);
