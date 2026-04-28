#pragma once

#include "runtime.h"

typedef enum {
    KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_NONE = 0,
    KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_HOLD_THRESHOLD,
    KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_LONG_HOLD,
    KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_FLUSH,
    KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_RELEASE_HOLD_PENDING,
} key_runtime_core_pending_multi_tap_scan_outcome_t;

typedef struct {
    key_runtime_core_pending_multi_tap_scan_outcome_t outcome;
    hold_behavior_t                                   hold;
    handled_key_hold_semantics_t                      semantics;
    bool                                              completes_hold;
    uint16_t                                          action;
    uint8_t                                           repeat_count;
    uint8_t                                           tap_count;
} key_runtime_core_pending_multi_tap_scan_resolution_t;

bool key_runtime_core_resolve_pending_multi_tap_scan(keypos_t key_pos, key_runtime_core_pending_multi_tap_scan_resolution_t *out);
void key_runtime_core_press_token_commit_hold_phase(key_runtime_core_state_t *state, press_token_t *token, bool completes_hold, bool long_hold_level);
void key_runtime_core_press_token_mark_release_hold_pending(key_runtime_core_state_t *state, press_token_t *token);
void key_runtime_core_press_token_refresh_slot_phase_for_scan(key_runtime_core_state_t *state, press_token_t *token, uint16_t now);
void key_runtime_core_plan_fallback_hold_activation(key_runtime_core_state_t *state, press_token_t *token, key_runtime_core_effect_plan_t *plan);
void key_runtime_core_plan_active_scan_for_token(key_runtime_core_state_t *state, press_token_t *token, key_runtime_core_effect_plan_t *plan);
void key_runtime_core_plan_pending_multi_tap_scan_for_key(key_runtime_core_state_t *state, keypos_t key_pos, key_runtime_core_effect_plan_t *plan);
