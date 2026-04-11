// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Press Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Internal press-planning protocol used behind the shared slot-result surface.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_slot_press.h"

typedef struct {
    bool                              handled;
    key_runtime_slot_pending_multi_tap_flush_t pending_multi_tap_flush;
    uint16_t                          dispatch_action;
    bool                              layer_press;
    uint8_t                           layer;
    keypos_t                          reclaim_key_pos;
    key_runtime_slot_effect_request_t reclaim_request;
    keypos_t                          begin_key_pos;
    key_runtime_slot_effect_request_t begin_request;
} key_runtime_slot_press_plan_t;

key_runtime_slot_press_plan_t key_runtime_slot_take_handled_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_view_t key, bool active_held_action_survives_flush);
