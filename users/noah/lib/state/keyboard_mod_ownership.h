// ────────────────────────────────────────────────────────────────────────────
// Keyboard Modifier Ownership
// ────────────────────────────────────────────────────────────────────────────
//
// Tracks direct physical modifier keys separately from managed modifier holds
// so releasing one owner does not clear another. Managed holds include both
// userspace-owned registrations and QMK paths that flow through
// register_mods()/unregister_mods(), such as held MT()/OSM() modifiers.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

void keyboard_mod_ownership_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record);
bool keyboard_mod_ownership_should_suppress_default(uint16_t keycode, keyrecord_t *record);
void keyboard_mod_ownership_register_mods(uint8_t mods);
void keyboard_mod_ownership_unregister_mods(uint8_t mods);
void keyboard_mod_ownership_register(uint16_t keycode);
void keyboard_mod_ownership_unregister(uint16_t keycode);
