// ────────────────────────────────────────────────────────────────────────────
// Action Lifecycle
// ────────────────────────────────────────────────────────────────────────────
//
// Shared authored-action semantics across tap dispatch and held-action
// press/release ownership.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

void noah_action_tap(uint16_t action);
void noah_action_tap_at(keypos_t key_pos, uint16_t action);
void noah_action_press(keypos_t key_pos, uint16_t action);
void noah_action_release(keypos_t key_pos, uint16_t action);
