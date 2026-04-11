// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Scan
// ────────────────────────────────────────────────────────────────────────────
//
// Scan-specific slot transition contracts for the handled-key runtime.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_state.h"

typedef enum {
    KEY_RUNTIME_SLOT_SCAN_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_SCAN_OUTCOME_FIRE_HOLD,
    KEY_RUNTIME_SLOT_SCAN_OUTCOME_PROMOTE_LONG_HOLD,
} key_runtime_slot_scan_outcome_t;

typedef struct {
    bool                            commit_immediate_hold;
    bool                            immediate_hold_needs_feedback;
    bool                            immediate_hold_completes_hold;
    bool                            activate_fallback_hold;
    key_runtime_slot_scan_outcome_t outcome;
    hold_behavior_t                 hold;
    hold_behavior_t                 long_hold;
} key_runtime_slot_scan_resolution_t;

typedef enum {
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_FIRE_HOLD,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_PROMOTE_LONG_HOLD,
} key_runtime_slot_pending_multi_tap_scan_outcome_t;

typedef struct {
    key_runtime_slot_pending_multi_tap_scan_outcome_t outcome;
    bool                                              release_layer_before_action;
    hold_behavior_t                                   hold;
    hold_behavior_t                                   long_hold;
} key_runtime_slot_pending_multi_tap_scan_resolution_t;

typedef struct {
    bool                           release_layer_before_action;
    key_runtime_slot_effect_request_t effect_request;
} key_runtime_slot_pending_multi_tap_scan_apply_t;

typedef struct {
    key_runtime_slot_effect_request_t immediate_hold_request;
    key_runtime_slot_effect_request_t outcome_request;
} key_runtime_slot_scan_apply_t;

typedef enum {
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_PLAN_NONE = 0,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_PLAN_EFFECT_REQUEST,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_PLAN_FLUSH,
} key_runtime_slot_pending_multi_tap_plan_kind_t;

typedef struct {
    bool                                     handled;
    keypos_t                                 key_pos;
    key_runtime_slot_pending_multi_tap_plan_kind_t kind;
    bool                                     release_layer_before_action;
    key_runtime_slot_effect_request_t        effect_request;
    key_runtime_slot_pending_multi_tap_flush_t flush;
} key_runtime_slot_pending_multi_tap_plan_t;

key_runtime_slot_scan_resolution_t key_runtime_slot_resolve_scan(active_key_state_t active_key_state, uint16_t elapsed);
key_runtime_slot_scan_apply_t key_runtime_slot_apply_scan_resolution(active_key_state_t *slot, key_runtime_slot_scan_resolution_t resolution);
key_runtime_slot_pending_multi_tap_scan_resolution_t key_runtime_slot_resolve_pending_multi_tap_scan(active_key_state_t active_key_state, multi_tap_t multi_tap_state, uint16_t elapsed);
key_runtime_slot_pending_multi_tap_scan_apply_t key_runtime_slot_apply_pending_multi_tap_scan_resolution(active_key_state_t *slot, key_runtime_slot_pending_multi_tap_scan_resolution_t resolution);
key_runtime_slot_pending_multi_tap_plan_t key_runtime_slot_take_pending_multi_tap_plan(active_key_state_t *slot);
