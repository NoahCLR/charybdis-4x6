// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Trace
// ────────────────────────────────────────────────────────────────────────────
//
// Optional debug tracing for handled-key event flow, transition plans, and
// executed effects. This stays compiled out unless both CONSOLE_ENABLE and
// NOAH_KEY_RUNTIME_TRACE_ENABLE are defined.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_transition.h"

typedef enum {
    KEY_RUNTIME_TRACE_RELEASE_OUTCOME_NONE = 0,
    KEY_RUNTIME_TRACE_RELEASE_OUTCOME_TAP,
    KEY_RUNTIME_TRACE_RELEASE_OUTCOME_ACTION,
    KEY_RUNTIME_TRACE_RELEASE_OUTCOME_PD_MODE_LOCK_TAP,
} key_runtime_trace_release_outcome_t;

typedef enum {
    KEY_RUNTIME_TRACE_RELEASE_FLAG_INTERRUPTED_LAYER_TAP = 1u << 0,
    KEY_RUNTIME_TRACE_RELEASE_FLAG_QUICK_TAP             = 1u << 1,
    KEY_RUNTIME_TRACE_RELEASE_FLAG_QUICK_IMMEDIATE_HOLD  = 1u << 2,
    KEY_RUNTIME_TRACE_RELEASE_FLAG_BUFFERED_BASE_TAP     = 1u << 3,
    KEY_RUNTIME_TRACE_RELEASE_FLAG_LOCK_TAP_AVAILABLE    = 1u << 4,
    KEY_RUNTIME_TRACE_RELEASE_FLAG_RELEASE_OWNED_STATE   = 1u << 5,
    KEY_RUNTIME_TRACE_RELEASE_FLAG_FALLBACK_SUPPRESSED   = 1u << 6,
} key_runtime_trace_release_flag_t;

typedef enum {
    KEY_RUNTIME_TRACE_HOLD_POLICY_ACTIVATE_PENDING_FALLBACK = 0,
    KEY_RUNTIME_TRACE_HOLD_POLICY_COMMIT_IMMEDIATE_HOLD,
    KEY_RUNTIME_TRACE_HOLD_POLICY_FIRE_THRESHOLD,
    KEY_RUNTIME_TRACE_HOLD_POLICY_PROMOTE_LONG_HOLD,
} key_runtime_trace_hold_policy_decision_t;

typedef enum {
    KEY_RUNTIME_TRACE_HOLD_DISPATCH_NONE = 0,
    KEY_RUNTIME_TRACE_HOLD_DISPATCH_ACTION,
    KEY_RUNTIME_TRACE_HOLD_DISPATCH_HELD,
    KEY_RUNTIME_TRACE_HOLD_DISPATCH_REPEAT,
} key_runtime_trace_hold_dispatch_t;

typedef enum {
    KEY_RUNTIME_TRACE_HOLD_POLICY_FLAG_COMPLETES_HOLD    = 1u << 0,
    KEY_RUNTIME_TRACE_HOLD_POLICY_FLAG_FEEDBACK_PULSE    = 1u << 1,
    KEY_RUNTIME_TRACE_HOLD_POLICY_FLAG_FEEDBACK_LONG     = 1u << 2,
    KEY_RUNTIME_TRACE_HOLD_POLICY_FLAG_RELEASE_OWNED_KEY = 1u << 3,
} key_runtime_trace_hold_policy_flag_t;

typedef enum {
    KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_PRESS_REUSE_CHAIN = 0,
    KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_PRESS_FLUSH_CHAIN,
    KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_RELEASE_DELAYED_ACTION,
    KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_RELEASE_HELD_LIFECYCLE,
    KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_RELEASE_PRESERVE_CHAIN,
    KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_FLUSH_PENDING_CHAIN,
    KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_SCAN_EXPIRED_FLUSH,
} key_runtime_trace_multi_tap_decision_t;

static inline uint16_t key_runtime_trace_pack_release_resolution(key_runtime_slot_phase_t phase, key_runtime_trace_release_outcome_t outcome, uint16_t flags) {
    return (uint16_t)(((uint16_t)phase & 0x0007u) | (((uint16_t)outcome & 0x0007u) << 3) | ((flags & 0x03FFu) << 6));
}

static inline uint16_t key_runtime_trace_pack_hold_policy_decision(key_runtime_trace_hold_policy_decision_t decision, key_runtime_trace_hold_dispatch_t dispatch, uint16_t flags) {
    return (uint16_t)(((uint16_t)decision & 0x000Fu) | (((uint16_t)dispatch & 0x000Fu) << 4) | ((flags & 0x00FFu) << 8));
}

static inline uint16_t key_runtime_trace_pack_multi_tap_decision(key_runtime_trace_multi_tap_decision_t decision, uint8_t repeat_count) {
    return (uint16_t)(((uint16_t)decision & 0x001Fu) | (((uint16_t)repeat_count & 0x00FFu) << 5));
}

void key_runtime_trace_record(const char *stage, uint16_t keycode, const keyrecord_t *record);
void key_runtime_trace_bool_result(const char *stage, uint16_t keycode, const keyrecord_t *record, bool value);
void key_runtime_trace_message(const char *stage, const char *message);
void key_runtime_trace_plan(const char *stage, const key_runtime_transition_plan_t *plan);
void key_runtime_trace_effect_execute(uint8_t index, const key_runtime_effect_t *effect);
void key_runtime_trace_release_resolution(key_runtime_slot_phase_t phase, key_runtime_trace_release_outcome_t outcome, uint16_t flags, uint16_t detail);
void key_runtime_trace_hold_policy_decision(key_runtime_trace_hold_policy_decision_t decision, key_runtime_trace_hold_dispatch_t dispatch, uint16_t flags, uint16_t detail);
void key_runtime_trace_multi_tap_decision(key_runtime_trace_multi_tap_decision_t decision, uint8_t repeat_count, uint16_t detail);
