// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Press
// ────────────────────────────────────────────────────────────────────────────
//
// Press-specific slot transition contracts and planning helpers.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "handled_key.h"
#include "key_runtime_slot_effect.h"

typedef struct {
    bool                               handled;
    key_runtime_slot_pending_multi_tap_flush_t pending_multi_tap_flush;
    uint16_t                           dispatch_action;
    bool                               layer_press;
    uint8_t                            layer;
    keypos_t                           reclaim_key_pos;
    key_runtime_slot_effect_request_t  reclaim_request;
    keypos_t                           begin_key_pos;
    key_runtime_slot_effect_request_t  begin_request;
} key_runtime_slot_press_plan_t;

key_runtime_slot_effect_request_t key_runtime_slot_begin_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, key_runtime_slot_phase_t phase, key_runtime_slot_hold_strategy_t hold_strategy, bool pd_mode_was_locked_on_press);
key_runtime_slot_press_plan_t key_runtime_slot_take_handled_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_view_t key, bool active_held_action_survives_flush);
