// ────────────────────────────────────────────────────────────────────────────
// Key Action Router
// ────────────────────────────────────────────────────────────────────────────
//
// Dispatches authored key actions, including custom layer-lock and
// pointing-device lock actions.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // QMK

bool action_router_is_layer_lock_action(uint16_t action);
bool action_router_layer_is_locked(uint8_t layer);
void action_router_dispatch(uint16_t action);
