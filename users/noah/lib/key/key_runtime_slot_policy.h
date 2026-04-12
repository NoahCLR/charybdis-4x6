// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Policy
// ────────────────────────────────────────────────────────────────────────────
//
// Internal policy helpers for handled-key slot lifecycle effects. These
// helpers keep hold/flush/interrupt policy decisions out of the main reducer
// loop so `key_runtime_slot_step.c` can stay focused on event routing.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_slot_effect.h"

uint16_t key_runtime_slot_policy_select_release_hold_action(uint16_t elapsed, uint16_t hold_action, hold_behavior_t long_hold, uint16_t longer_hold_term);
key_runtime_slot_effect_request_t key_runtime_slot_policy_activate_pending_fallback_hold(active_key_state_t *slot);
key_runtime_slot_effect_request_t key_runtime_slot_policy_interrupt_on_other_press(active_key_state_t *slot, keypos_t other_key_pos);
key_runtime_slot_effect_request_t key_runtime_slot_policy_commit_immediate_hold(active_key_state_t *slot, bool needs_feedback, bool completes_hold);
key_runtime_slot_effect_request_t key_runtime_slot_policy_take_flush(active_key_state_t *slot, bool active_held_action_survives_flush);
key_runtime_slot_effect_request_t key_runtime_slot_policy_fire_hold_at_threshold(active_key_state_t *slot, hold_behavior_t hold, hold_behavior_t long_hold, bool pulse_momentary_layer_action);
key_runtime_slot_effect_request_t key_runtime_slot_policy_promote_to_long_hold(active_key_state_t *slot, hold_behavior_t long_hold, bool pulse_momentary_layer_action);
