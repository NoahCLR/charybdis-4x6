// ────────────────────────────────────────────────────────────────────────────
// Runtime V2 Release Planning Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Internal reducer-owned release-resolution helpers used while the production
// active-release path migrates away from slot-owned release semantics.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "runtime_v2.h"

#include "../key/runtime/effects/key_runtime_effect_queue.h"
#include "../key/runtime/slot/key_runtime_slot_release_resolver.h"

#define RUNTIME_V2_RELEASE_EFFECT_PLAN_CAPACITY 6u

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
} runtime_v2_active_release_resolution_t;

typedef enum {
    RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_NONE = 0,
    RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION,
    RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_HELD_LIFECYCLE,
    RUNTIME_V2_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN,
} runtime_v2_pending_multi_tap_release_outcome_t;

typedef struct {
    runtime_v2_pending_multi_tap_release_outcome_t outcome;
    uint16_t                                       action;
    uint8_t                                        repeat_count;
} runtime_v2_pending_multi_tap_release_resolution_t;

typedef enum {
    RUNTIME_V2_PENDING_MULTI_TAP_SCAN_OUTCOME_NONE = 0,
    RUNTIME_V2_PENDING_MULTI_TAP_SCAN_OUTCOME_HOLD_THRESHOLD,
    RUNTIME_V2_PENDING_MULTI_TAP_SCAN_OUTCOME_LONG_HOLD,
    RUNTIME_V2_PENDING_MULTI_TAP_SCAN_OUTCOME_FLUSH,
} runtime_v2_pending_multi_tap_scan_outcome_t;

typedef struct {
    runtime_v2_pending_multi_tap_scan_outcome_t outcome;
    hold_behavior_t                             hold;
    handled_key_hold_semantics_t                semantics;
    bool                                        completes_hold;
    uint16_t                                    action;
    uint8_t                                     repeat_count;
} runtime_v2_pending_multi_tap_scan_resolution_t;

typedef enum {
    RUNTIME_V2_RELEASE_SLOT_SETTLEMENT_NONE = 0,
    RUNTIME_V2_RELEASE_SLOT_SETTLEMENT_RESET,
    RUNTIME_V2_RELEASE_SLOT_SETTLEMENT_CLEAR_ACTIVE_PRESERVE_PENDING_MULTI_TAP,
} runtime_v2_release_slot_settlement_t;

typedef struct {
    bool     active;
    uint16_t keycode;
    keypos_t key_pos;
    uint8_t  tap_count;
    uint16_t tap_action;
    uint8_t  tap_repeat_count;
    uint16_t tap_hold_term;
    uint16_t multi_tap_term;
    bool     has_more_taps;
} runtime_v2_pending_multi_tap_seed_t;

typedef struct {
    runtime_v2_release_slot_settlement_t settlement;
    runtime_v2_pending_multi_tap_seed_t  pending_multi_tap_seed;
    KEY_RUNTIME_EFFECT_QUEUE_FIELDS(RUNTIME_V2_RELEASE_EFFECT_PLAN_CAPACITY);
} runtime_v2_release_effect_plan_t;

bool runtime_v2_resolve_active_release(keypos_t key_pos, runtime_v2_active_release_resolution_t *out);
bool runtime_v2_plan_active_release_effects(keypos_t key_pos, uint16_t keycode, const runtime_v2_active_release_resolution_t *resolution, runtime_v2_release_effect_plan_t *out);
bool runtime_v2_resolve_pending_multi_tap_release(keypos_t key_pos, uint16_t tap_action, uint8_t tap_repeat_count, bool preserve_chain_available, runtime_v2_pending_multi_tap_release_resolution_t *out);
bool runtime_v2_plan_pending_multi_tap_release_effects(keypos_t key_pos, bool is_momentary_layer, const runtime_v2_pending_multi_tap_release_resolution_t *resolution, delayed_action_mods_t mods, runtime_v2_release_effect_plan_t *out);
bool runtime_v2_resolve_pending_multi_tap_scan(keypos_t key_pos, runtime_v2_pending_multi_tap_scan_resolution_t *out);
