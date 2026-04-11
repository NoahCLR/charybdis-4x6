// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Effects
// ────────────────────────────────────────────────────────────────────────────
//
// Effect-request contracts and mutation helpers for slot transitions.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_state.h"

typedef enum {
    KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE = 0,
    KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION,
    KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER,
    KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_UNREGISTER,
    KEY_RUNTIME_SLOT_EFFECT_REQUEST_REPEAT_START,
} key_runtime_slot_effect_request_kind_t;

typedef struct {
    bool                                   release_owned_state;
    bool                                   feedback_pulse;
    bool                                   feedback_long_hold_level;
    key_runtime_slot_effect_request_kind_t kind;
    uint16_t                               action;
    uint16_t                               repeat_hz;
} key_runtime_slot_effect_request_t;

key_runtime_slot_effect_request_t key_runtime_slot_activate_pending_fallback_hold_request(active_key_state_t *slot);
key_runtime_slot_effect_request_t key_runtime_slot_interrupt_on_other_press(active_key_state_t *slot, keypos_t other_key_pos);
key_runtime_slot_effect_request_t key_runtime_slot_commit_immediate_hold(active_key_state_t *slot, bool needs_feedback, bool completes_hold);
key_runtime_slot_effect_request_t key_runtime_slot_take_flush(active_key_state_t *slot, bool active_held_action_survives_flush);
key_runtime_slot_effect_request_t key_runtime_slot_fire_hold_at_threshold(active_key_state_t *slot, hold_behavior_t hold, hold_behavior_t long_hold, bool pulse_momentary_layer_action);
key_runtime_slot_effect_request_t key_runtime_slot_promote_to_long_hold(active_key_state_t *slot, hold_behavior_t long_hold, bool pulse_momentary_layer_action);
