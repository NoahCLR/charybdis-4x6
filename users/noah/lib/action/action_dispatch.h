// ────────────────────────────────────────────────────────────────────────────
// Action Dispatch
// ────────────────────────────────────────────────────────────────────────────
//
// Dispatches authored key actions, including custom layer-lock and
// pointing-device lock actions.
//
// Actions are keycode-like values. Keymap-local custom keycodes starting at
// NOAH_KEYMAP_SAFE_RANGE are routed back through process_record_user().
// Non-layer QMK behavior keycodes such as OSM()/MT() need explicit handling
// too, because tap_code16/register_code16 only model plain key press/release.
// Raw QMK layer actions are intentionally handled separately so they cannot
// bypass the userspace layer ownership model.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

bool action_dispatch_is_layer_lock(uint16_t action);
bool action_dispatch_is_raw_qmk_layer_action(uint16_t action);
bool action_dispatch_is_layer_action(uint16_t action);
bool action_dispatch_is_macro(uint16_t action);
bool action_dispatch_is_qmk_behavior_keycode(uint16_t action);
bool action_dispatch_layer_is_locked(uint8_t layer);
void action_dispatch(uint16_t action);
