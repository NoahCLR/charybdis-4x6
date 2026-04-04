// ────────────────────────────────────────────────────────────────────────────
// Delayed Action Dispatch
// ────────────────────────────────────────────────────────────────────────────
//
// Replays authored actions using the modifier state captured when a multi-tap
// sequence started resolving.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../state/keyboard_mod_state.h"
#include "multi_tap_engine.h"

typedef keyboard_mod_state_t delayed_action_mods_t;

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt);
void                  dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods);
void                  dispatch_multi_tap_action(uint16_t action, const multi_tap_t *mt);
