// ────────────────────────────────────────────────────────────────────────────
// Held Action Ownership
// ────────────────────────────────────────────────────────────────────────────
//
// Tracks held pure modifiers separately from the tap/hold FSM so those mods
// stay pressed until the owning physical switch is released. Repeat bindings
// share the same physical-key ownership model so authored repeat holds can
// survive active-key flushes and still stop on release.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

void held_action_register(keypos_t key_pos, uint16_t action);
void held_action_unregister(keypos_t key_pos, uint16_t action);
void held_action_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz);
void held_action_repeat_tick(void);
bool held_action_survives_flush(keypos_t key_pos, uint16_t action);
bool held_action_release_owned_by_key(keypos_t key_pos);
bool held_modifier_release_owned_by_key(keypos_t key_pos);
