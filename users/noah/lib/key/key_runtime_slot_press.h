// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Press
// ────────────────────────────────────────────────────────────────────────────
//
// Press-specific slot transition helpers that remain externally useful outside
// the internal slot-result adapter.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "handled_key.h"
#include "key_runtime_slot_effect.h"

key_runtime_slot_effect_request_t key_runtime_slot_begin_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, key_runtime_slot_phase_t phase, key_runtime_slot_hold_strategy_t hold_strategy, bool pd_mode_was_locked_on_press);
