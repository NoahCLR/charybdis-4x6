// ────────────────────────────────────────────────────────────────────────────
// Action Dispatch
// ────────────────────────────────────────────────────────────────────────────
//
// Dispatches authored key actions, including custom layer-lock and
// pointing-device lock actions.
//
// Actions are keycode-like values. Keymap-local custom keycodes starting at
// NOAH_KEYMAP_SAFE_RANGE are routed back through process_record_user().
// Other new semantic action values still need explicit handling here.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

bool action_dispatch_is_layer_lock(uint16_t action);
bool action_dispatch_is_macro(uint16_t action);
bool action_dispatch_layer_is_locked(uint8_t layer);
void action_dispatch(uint16_t action);
