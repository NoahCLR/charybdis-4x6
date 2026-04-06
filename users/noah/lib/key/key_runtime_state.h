// ────────────────────────────────────────────────────────────────────────────
// Key Runtime State
// ────────────────────────────────────────────────────────────────────────────
//
// Shared active-key and multi-tap state for the split key runtime modules.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../state/runtime_shared_state.h"

// Transitional aliases while key-runtime modules move from file-local globals
// to the shared runtime-owned state object.
#define active_key (noah_runtime_shared_state.key.active_key)
#define multi_tap (noah_runtime_shared_state.key.multi_tap)

void noah_key_runtime_scan(void);

uint8_t behavior_get_layer(uint16_t keycode);
bool    is_layer_key(uint16_t keycode);
bool    active_key_matches(uint16_t keycode, keypos_t key_pos);
bool    key_runtime_activate_pending_fallback_hold(void);

void active_key_reset(void);
void active_key_track(uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired);
