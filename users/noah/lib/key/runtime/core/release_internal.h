// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Core Release Planning Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Internal reducer-owned release-resolution helpers used while the production
// active-release path migrates away from slot-owned release semantics.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "runtime.h"

#include "../effects/effect_queue.h"
#include "../slot/release_resolver.h"

#define KEY_RUNTIME_CORE_RELEASE_EFFECT_PLAN_CAPACITY 6u

typedef struct {
    key_runtime_slot_interaction_t      interaction;
    key_runtime_slot_phase_t            phase;
    uint16_t                            elapsed;
    bool                                held_action_active;
    bool                                repeat_active;
    bool                                momentary_layer_tap_interrupted;
    bool                                quick_tap;
    bool                                quick_immediate_hold;
    bool                                buffered_base_tap;
    pd_mode_mask_t                      lock_tap_mode;
    key_runtime_slot_release_decision_t decision;
} key_runtime_core_active_release_resolution_t;

typedef enum {
    KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_NONE = 0,
    KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
    KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
    KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN,
} key_runtime_core_pending_multi_tap_release_outcome_t;

typedef struct {
    key_runtime_core_pending_multi_tap_release_outcome_t outcome;
    uint16_t                                             action;
    uint8_t                                              repeat_count;
    bool                                                 tap_branch_feedback;
    bool                                                 tap_commit_feedback;
    bool                                                 action_feedback;
    key_feedback_pulse_kind_t                            action_feedback_kind;
    uint8_t                                              tap_count;
} key_runtime_core_pending_multi_tap_release_resolution_t;

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

typedef enum {
    KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_NONE = 0,
    KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_RESET,
    KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_CLEAR_ACTIVE_PRESERVE_PENDING_MULTI_TAP,
} key_runtime_core_release_slot_settlement_t;

typedef struct {
    bool     active;
    uint16_t keycode;
    keypos_t key_pos;
    uint8_t  tap_count;
    uint16_t tap_action;
    uint8_t  tap_repeat_count;
    uint16_t tap_hold_term;
    uint16_t multi_tap_term;
    uint16_t branch_confirm_term;
    bool     has_more_taps;
} key_runtime_core_pending_multi_tap_seed_t;

typedef struct {
    key_runtime_core_release_slot_settlement_t settlement;
    key_runtime_core_pending_multi_tap_seed_t  pending_multi_tap_seed;
    KEY_RUNTIME_EFFECT_QUEUE_FIELDS(KEY_RUNTIME_CORE_RELEASE_EFFECT_PLAN_CAPACITY);
} key_runtime_core_release_effect_plan_t;

_Static_assert(sizeof(key_runtime_core_release_effect_plan_t) <= 196u, "key_runtime_core_release_effect_plan_t must stay within the approved stack budget");

bool key_runtime_core_resolve_active_release(keypos_t key_pos, key_runtime_core_active_release_resolution_t *out);
bool key_runtime_core_plan_active_release_effects(keypos_t key_pos, uint16_t keycode, const key_runtime_core_active_release_resolution_t *resolution, key_runtime_core_release_effect_plan_t *out);
bool key_runtime_core_resolve_pending_multi_tap_release(keypos_t key_pos, uint16_t tap_action, uint8_t tap_repeat_count, bool preserve_chain_available, key_runtime_core_pending_multi_tap_release_resolution_t *out);
bool key_runtime_core_plan_pending_multi_tap_release_effects(keypos_t key_pos, bool is_momentary_layer, const key_runtime_core_pending_multi_tap_release_resolution_t *resolution, delayed_action_mods_t mods, key_runtime_core_release_effect_plan_t *out);
bool key_runtime_core_resolve_pending_multi_tap_scan(keypos_t key_pos, key_runtime_core_pending_multi_tap_scan_resolution_t *out);
