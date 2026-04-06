// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Effects
// ────────────────────────────────────────────────────────────────────────────
//
// Thin adapter around the side-effectful services the key runtime uses.
// Keeping these calls behind one small surface makes the tap/hold state
// machine easier to reason about and gives future tests one place to swap or
// instrument runtime effects.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

bool key_runtime_effects_should_suppress_default(uint16_t keycode, keyrecord_t *record);
void key_runtime_effects_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record);
bool key_runtime_effects_activate_pending_fallback_hold(void);

void key_runtime_effects_dispatch_action(uint16_t action);

void key_runtime_effects_held_action_register(keypos_t key_pos, uint16_t action);
void key_runtime_effects_held_action_unregister(keypos_t key_pos, uint16_t action);
bool key_runtime_effects_held_action_survives_flush(keypos_t key_pos, uint16_t action);
bool key_runtime_effects_release_held_action_owned_by_key(keypos_t key_pos);

void key_runtime_effects_layer_press(keypos_t key_pos, uint8_t layer);
void key_runtime_effects_layer_release(keypos_t key_pos);

void key_runtime_effects_feedback_pulse_arm(bool long_hold_level);
void key_runtime_effects_sync_split_runtime(void);
