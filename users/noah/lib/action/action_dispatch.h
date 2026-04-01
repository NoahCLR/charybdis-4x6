// ────────────────────────────────────────────────────────────────────────────
// Action Dispatch
// ────────────────────────────────────────────────────────────────────────────
//
// Dispatches authored key actions, including custom layer-lock and
// pointing-device lock actions.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // QMK

bool action_dispatch_is_layer_lock(uint16_t action);
bool action_dispatch_layer_is_locked(uint8_t layer);
void action_dispatch(uint16_t action);
