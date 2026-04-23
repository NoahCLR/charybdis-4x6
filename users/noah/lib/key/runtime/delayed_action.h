// ────────────────────────────────────────────────────────────────────────────
// Delayed Action Dispatch
// ────────────────────────────────────────────────────────────────────────────
//
// Replays authored actions using the modifier state captured when a multi-tap
// sequence started resolving.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../state/runtime/keyboard_mod_state.h"

typedef keyboard_mod_state_t delayed_action_mods_t;

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods);
void dispatch_delayed_action_at(keypos_t key_pos, uint16_t action, delayed_action_mods_t mods);
