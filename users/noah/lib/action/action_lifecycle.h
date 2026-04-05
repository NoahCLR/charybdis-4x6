// ────────────────────────────────────────────────────────────────────────────
// Action Lifecycle
// ────────────────────────────────────────────────────────────────────────────
//
// Shared authored-action semantics across tap dispatch and held-action
// press/release ownership.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

typedef enum {
    NOAH_ACTION_HOLD_KIND_SHARED = 0, // first press / last release
    NOAH_ACTION_HOLD_KIND_PER_KEY,    // each physical key needs its own press/release
    NOAH_ACTION_HOLD_KIND_PRESS_ONLY, // fire once on press; release is a no-op
} noah_action_hold_kind_t;

noah_action_hold_kind_t noah_action_hold_kind(uint16_t action);
void                    noah_action_tap(uint16_t action);
void                    noah_action_press(keypos_t key_pos, uint16_t action);
void                    noah_action_release(keypos_t key_pos, uint16_t action);
