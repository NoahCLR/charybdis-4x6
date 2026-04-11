// ────────────────────────────────────────────────────────────────────────────
// Handled Key View
// ────────────────────────────────────────────────────────────────────────────
//
// Runtime helpers that adapt a resolved key_behavior view into the press and
// release engine's handled-key decisions.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_behavior_lookup.h"

typedef struct {
    key_behavior_view_t behavior;
} handled_key_view_t;

handled_key_view_t handled_key_lookup(uint16_t keycode);
bool               handled_key_uses_implicit_hold(handled_key_view_t key);
bool               handled_key_uses_fallback_hold(handled_key_view_t key);
hold_behavior_t    handled_key_single_hold(handled_key_view_t key);
uint16_t           handled_key_tap_action(handled_key_view_t key);
bool               handled_key_multi_tap_repress(handled_key_view_t key, uint16_t keycode, keypos_t key_pos);
uint16_t           handled_key_advance_multi_tap(uint16_t keycode);
