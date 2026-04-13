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

#include "../runtime/keyboard_mod_state.h"

#define KEYBOARD_MOD_OWNERSHIP_MOD_COUNT 8u

typedef struct {
    keyboard_mod_state_t live_state;
    uint8_t              physical_refcounts[KEYBOARD_MOD_OWNERSHIP_MOD_COUNT];
    uint8_t              managed_refcounts[KEYBOARD_MOD_OWNERSHIP_MOD_COUNT];
} keyboard_mod_ownership_debug_snapshot_t;

void keyboard_mod_ownership_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record);
bool keyboard_mod_ownership_should_suppress_default(uint16_t keycode, keyrecord_t *record);
void keyboard_mod_ownership_register_mods(uint8_t mods);
void keyboard_mod_ownership_unregister_mods(uint8_t mods);
void keyboard_mod_ownership_register(uint16_t keycode);
void keyboard_mod_ownership_unregister(uint16_t keycode);
void keyboard_mod_ownership_debug_snapshot(keyboard_mod_ownership_debug_snapshot_t *out);
void keyboard_mod_ownership_reset_for_test(void);
