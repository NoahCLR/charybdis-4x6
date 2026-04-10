// ────────────────────────────────────────────────────────────────────────────
// Layer Ownership
// ────────────────────────────────────────────────────────────────────────────
//
// Tracks momentary and locked layer owners so overlapping layer holds and
// layer locks can coexist without raw layer_on()/layer_off() races.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

bool layer_ownership_is_locked(uint8_t layer);
bool layer_ownership_set_lock_state(uint8_t layer, bool locked);
bool layer_ownership_toggle_lock_state(uint8_t layer);

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer);
bool layer_ownership_momentary_release(keypos_t key_pos);

#ifdef NOAH_HOST_TESTS
void layer_ownership_reset_for_test(void);
#endif
