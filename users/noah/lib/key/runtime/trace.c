// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Trace
// ────────────────────────────────────────────────────────────────────────────

#include "trace.h"

#include <string.h>

#include "../../state/runtime/runtime_trace.h"

static uint8_t key_runtime_trace_stage_id(const char *stage) {
    if (!stage) {
        return NOAH_TRACE_KEY_RUNTIME_STAGE_UNKNOWN;
    }

    if (strcmp(stage, "preflight:interrupt_active_key") == 0) {
        return NOAH_TRACE_KEY_RUNTIME_STAGE_INTERRUPT_ACTIVE_KEY;
    }

    if (strcmp(stage, "preflight:flush_multi_tap") == 0) {
        return NOAH_TRACE_KEY_RUNTIME_STAGE_FLUSH_MULTI_TAP;
    }

    if (strcmp(stage, "press") == 0) {
        return NOAH_TRACE_KEY_RUNTIME_STAGE_PRESS;
    }

    if (strcmp(stage, "release") == 0) {
        return NOAH_TRACE_KEY_RUNTIME_STAGE_RELEASE;
    }

    if (strcmp(stage, "scan") == 0) {
        return NOAH_TRACE_KEY_RUNTIME_STAGE_SCAN;
    }

    return NOAH_TRACE_KEY_RUNTIME_STAGE_UNKNOWN;
}

static void key_runtime_trace_emit_plan_event(const char *stage, const key_runtime_transition_plan_t *plan) {
    uint16_t detail = 0;

    if (plan) {
        detail = (uint16_t)(plan->count & 0x00FFu);
        if (plan->overflowed) {
            detail |= 0x8000u;
        }
    }

    noah_runtime_trace_emit(NOAH_TRACE_KEY_RUNTIME, NOAH_TRACE_KEY_RUNTIME_EVENT_PLAN, key_runtime_trace_stage_id(stage), detail);
}

static void key_runtime_trace_emit_effect_event(uint8_t index, const key_runtime_effect_t *effect) {
    uint16_t effect_kind = effect ? (uint16_t)effect->kind : (uint16_t)KEY_RUNTIME_EFFECT_NONE;

    noah_runtime_trace_emit(NOAH_TRACE_KEY_RUNTIME, NOAH_TRACE_KEY_RUNTIME_EVENT_EFFECT_EXECUTE, effect_kind, index);
}

static void key_runtime_trace_emit_release_resolution_event(key_runtime_slot_phase_t phase, key_runtime_trace_release_outcome_t outcome, uint16_t flags, uint16_t detail) {
    noah_runtime_trace_emit(NOAH_TRACE_KEY_RUNTIME, NOAH_TRACE_KEY_RUNTIME_EVENT_RELEASE_RESOLUTION, key_runtime_trace_pack_release_resolution(phase, outcome, flags), detail);
}

static void key_runtime_trace_emit_hold_policy_event(key_runtime_trace_hold_policy_decision_t decision, key_runtime_trace_hold_dispatch_t dispatch, uint16_t flags, uint16_t detail) {
    noah_runtime_trace_emit(NOAH_TRACE_KEY_RUNTIME, NOAH_TRACE_KEY_RUNTIME_EVENT_HOLD_POLICY_DECISION, key_runtime_trace_pack_hold_policy_decision(decision, dispatch, flags), detail);
}

static void key_runtime_trace_emit_multi_tap_event(key_runtime_trace_multi_tap_decision_t decision, uint8_t repeat_count, uint16_t detail) {
    noah_runtime_trace_emit(NOAH_TRACE_KEY_RUNTIME, NOAH_TRACE_KEY_RUNTIME_EVENT_MULTI_TAP_DECISION, key_runtime_trace_pack_multi_tap_decision(decision, repeat_count), detail);
}

#if defined(CONSOLE_ENABLE) && defined(NOAH_KEY_RUNTIME_TRACE_ENABLE)

#    include "print.h"

static const char *key_runtime_trace_effect_name(key_runtime_effect_kind_t kind) {
    switch (kind) {
        case KEY_RUNTIME_EFFECT_DISPATCH_ACTION:
            return "dispatch_action";
        case KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER:
            return "held_register";
        case KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER:
            return "held_unregister";
        case KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY:
            return "release_owned_state";
        case KEY_RUNTIME_EFFECT_REPEAT_START:
            return "repeat_start";
        case KEY_RUNTIME_EFFECT_LAYER_PRESS:
            return "layer_press";
        case KEY_RUNTIME_EFFECT_LAYER_RELEASE:
            return "layer_release";
        case KEY_RUNTIME_EFFECT_FEEDBACK_PULSE:
            return "feedback_pulse";
        case KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP:
            return "pd_mode_lock_tap";
        case KEY_RUNTIME_EFFECT_DELAYED_ACTION:
            return "delayed_action";
        case KEY_RUNTIME_EFFECT_NONE:
        default:
            return "none";
    }
}

static const char *key_runtime_trace_release_outcome_name(key_runtime_trace_release_outcome_t outcome) {
    switch (outcome) {
        case KEY_RUNTIME_TRACE_RELEASE_OUTCOME_TAP:
            return "tap";
        case KEY_RUNTIME_TRACE_RELEASE_OUTCOME_ACTION:
            return "action";
        case KEY_RUNTIME_TRACE_RELEASE_OUTCOME_PD_MODE_LOCK_TAP:
            return "pd_mode_lock_tap";
        case KEY_RUNTIME_TRACE_RELEASE_OUTCOME_NONE:
        default:
            return "none";
    }
}

static const char *key_runtime_trace_hold_policy_name(key_runtime_trace_hold_policy_decision_t decision) {
    switch (decision) {
        case KEY_RUNTIME_TRACE_HOLD_POLICY_ACTIVATE_PENDING_FALLBACK:
            return "activate_pending_fallback";
        case KEY_RUNTIME_TRACE_HOLD_POLICY_COMMIT_IMMEDIATE_HOLD:
            return "commit_immediate_hold";
        case KEY_RUNTIME_TRACE_HOLD_POLICY_FIRE_THRESHOLD:
            return "fire_threshold";
        case KEY_RUNTIME_TRACE_HOLD_POLICY_PROMOTE_LONG_HOLD:
            return "promote_long_hold";
        default:
            return "unknown";
    }
}

static const char *key_runtime_trace_hold_dispatch_name(key_runtime_trace_hold_dispatch_t dispatch) {
    switch (dispatch) {
        case KEY_RUNTIME_TRACE_HOLD_DISPATCH_ACTION:
            return "action";
        case KEY_RUNTIME_TRACE_HOLD_DISPATCH_HELD:
            return "held";
        case KEY_RUNTIME_TRACE_HOLD_DISPATCH_REPEAT:
            return "repeat";
        case KEY_RUNTIME_TRACE_HOLD_DISPATCH_NONE:
        default:
            return "none";
    }
}

static const char *key_runtime_trace_multi_tap_decision_name(key_runtime_trace_multi_tap_decision_t decision) {
    switch (decision) {
        case KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_PRESS_REUSE_CHAIN:
            return "press_reuse_chain";
        case KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_PRESS_FLUSH_CHAIN:
            return "press_flush_chain";
        case KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_RELEASE_DELAYED_ACTION:
            return "release_delayed_action";
        case KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_RELEASE_HELD_LIFECYCLE:
            return "release_held_lifecycle";
        case KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_RELEASE_PRESERVE_CHAIN:
            return "release_preserve_chain";
        case KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_FLUSH_PENDING_CHAIN:
            return "flush_pending_chain";
        case KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_SCAN_HOLD_THRESHOLD:
            return "scan_hold_threshold";
        case KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_SCAN_LONG_HOLD:
            return "scan_long_hold";
        case KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_SCAN_EXPIRED_FLUSH:
            return "scan_expired_flush";
        default:
            return "unknown";
    }
}

void key_runtime_trace_record(const char *stage, uint16_t keycode, const keyrecord_t *record) {
    if (!record) {
        uprintf("Key runtime trace [%s] keycode=0x%04X record=(null)\n", stage, (unsigned int)keycode);
        return;
    }

    uprintf("Key runtime trace [%s] keycode=0x%04X key=(%u,%u) pressed=%u\n", stage, (unsigned int)keycode, (unsigned int)record->event.key.row, (unsigned int)record->event.key.col, record->event.pressed ? 1u : 0u);
}

void key_runtime_trace_bool_result(const char *stage, uint16_t keycode, const keyrecord_t *record, bool value) {
    if (!record) {
        uprintf("Key runtime trace [%s] keycode=0x%04X result=%u\n", stage, (unsigned int)keycode, value ? 1u : 0u);
        return;
    }

    uprintf("Key runtime trace [%s] keycode=0x%04X key=(%u,%u) pressed=%u result=%u\n", stage, (unsigned int)keycode, (unsigned int)record->event.key.row, (unsigned int)record->event.key.col, record->event.pressed ? 1u : 0u, value ? 1u : 0u);
}

void key_runtime_trace_message(const char *stage, const char *message) {
    uprintf("Key runtime trace [%s] %s\n", stage, message);
}

void key_runtime_trace_plan(const char *stage, const key_runtime_transition_plan_t *plan) {
    key_runtime_trace_emit_plan_event(stage, plan);

    if (!plan) {
        uprintf("Key runtime trace [%s] plan=(null)\n", stage);
        return;
    }

    uprintf("Key runtime trace [%s] plan count=%u overflowed=%u\n", stage, (unsigned int)plan->count, plan->overflowed ? 1u : 0u);

    for (uint8_t i = 0; i < plan->count; i++) {
        const key_runtime_effect_t *effect = &plan->items[i];

        switch (effect->kind) {
            case KEY_RUNTIME_EFFECT_DISPATCH_ACTION:
                uprintf("  [%u] %s action=0x%04X\n", (unsigned int)i, key_runtime_trace_effect_name(effect->kind), (unsigned int)effect->data.action);
                break;
            case KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER:
            case KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER:
                uprintf("  [%u] %s key=(%u,%u) action=0x%04X\n", (unsigned int)i, key_runtime_trace_effect_name(effect->kind), (unsigned int)effect->data.held_action.key_pos.row, (unsigned int)effect->data.held_action.key_pos.col, (unsigned int)effect->data.held_action.action);
                break;
            case KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY:
            case KEY_RUNTIME_EFFECT_LAYER_RELEASE:
                uprintf("  [%u] %s key=(%u,%u)\n", (unsigned int)i, key_runtime_trace_effect_name(effect->kind), (unsigned int)effect->data.key_pos.row, (unsigned int)effect->data.key_pos.col);
                break;
            case KEY_RUNTIME_EFFECT_REPEAT_START:
                uprintf("  [%u] %s key=(%u,%u) action=0x%04X hz=%u\n", (unsigned int)i, key_runtime_trace_effect_name(effect->kind), (unsigned int)effect->data.repeat.key_pos.row, (unsigned int)effect->data.repeat.key_pos.col, (unsigned int)effect->data.repeat.action, (unsigned int)effect->data.repeat.repeat_hz);
                break;
            case KEY_RUNTIME_EFFECT_LAYER_PRESS:
                uprintf("  [%u] %s key=(%u,%u) layer=%u\n", (unsigned int)i, key_runtime_trace_effect_name(effect->kind), (unsigned int)effect->data.layer_press.key_pos.row, (unsigned int)effect->data.layer_press.key_pos.col, (unsigned int)effect->data.layer_press.layer);
                break;
            case KEY_RUNTIME_EFFECT_FEEDBACK_PULSE:
                uprintf("  [%u] %s long=%u\n", (unsigned int)i, key_runtime_trace_effect_name(effect->kind), effect->data.long_hold_level ? 1u : 0u);
                break;
            case KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP:
                uprintf("  [%u] %s mode=0x%04X\n", (unsigned int)i, key_runtime_trace_effect_name(effect->kind), (unsigned int)effect->data.pd_mode);
                break;
            case KEY_RUNTIME_EFFECT_DELAYED_ACTION:
                uprintf("  [%u] %s action=0x%04X repeat=%u\n", (unsigned int)i, key_runtime_trace_effect_name(effect->kind), (unsigned int)effect->data.delayed_action.action, (unsigned int)effect->data.delayed_action.repeat_count);
                break;
            case KEY_RUNTIME_EFFECT_NONE:
            default:
                uprintf("  [%u] %s\n", (unsigned int)i, key_runtime_trace_effect_name(effect->kind));
                break;
        }
    }
}

void key_runtime_trace_effect_execute(uint8_t index, const key_runtime_effect_t *effect) {
    key_runtime_trace_emit_effect_event(index, effect);

    if (!effect) {
        uprintf("Key runtime trace [execute] [%u] effect=(null)\n", (unsigned int)index);
        return;
    }

    uprintf("Key runtime trace [execute] [%u] %s\n", (unsigned int)index, key_runtime_trace_effect_name(effect->kind));
}

void key_runtime_trace_release_resolution(key_runtime_slot_phase_t phase, key_runtime_trace_release_outcome_t outcome, uint16_t flags, uint16_t detail) {
    key_runtime_trace_emit_release_resolution_event(phase, outcome, flags, detail);
    uprintf("Key runtime trace [release] phase=%u outcome=%s flags=0x%04X detail=0x%04X\n", (unsigned int)phase, key_runtime_trace_release_outcome_name(outcome), (unsigned int)flags, (unsigned int)detail);
}

void key_runtime_trace_hold_policy_decision(key_runtime_trace_hold_policy_decision_t decision, key_runtime_trace_hold_dispatch_t dispatch, uint16_t flags, uint16_t detail) {
    key_runtime_trace_emit_hold_policy_event(decision, dispatch, flags, detail);
    uprintf("Key runtime trace [hold] decision=%s dispatch=%s flags=0x%04X detail=0x%04X\n", key_runtime_trace_hold_policy_name(decision), key_runtime_trace_hold_dispatch_name(dispatch), (unsigned int)flags, (unsigned int)detail);
}

void key_runtime_trace_multi_tap_decision(key_runtime_trace_multi_tap_decision_t decision, uint8_t repeat_count, uint16_t detail) {
    key_runtime_trace_emit_multi_tap_event(decision, repeat_count, detail);
    uprintf("Key runtime trace [multi_tap] decision=%s repeat=%u detail=0x%04X\n", key_runtime_trace_multi_tap_decision_name(decision), (unsigned int)repeat_count, (unsigned int)detail);
}

#else

void key_runtime_trace_record(const char *stage, uint16_t keycode, const keyrecord_t *record) {
    (void)stage;
    (void)keycode;
    (void)record;
}

void key_runtime_trace_bool_result(const char *stage, uint16_t keycode, const keyrecord_t *record, bool value) {
    (void)stage;
    (void)keycode;
    (void)record;
    (void)value;
}

void key_runtime_trace_message(const char *stage, const char *message) {
    (void)stage;
    (void)message;
}

void key_runtime_trace_plan(const char *stage, const key_runtime_transition_plan_t *plan) {
    key_runtime_trace_emit_plan_event(stage, plan);
}

void key_runtime_trace_effect_execute(uint8_t index, const key_runtime_effect_t *effect) {
    key_runtime_trace_emit_effect_event(index, effect);
}

void key_runtime_trace_release_resolution(key_runtime_slot_phase_t phase, key_runtime_trace_release_outcome_t outcome, uint16_t flags, uint16_t detail) {
    key_runtime_trace_emit_release_resolution_event(phase, outcome, flags, detail);
}

void key_runtime_trace_hold_policy_decision(key_runtime_trace_hold_policy_decision_t decision, key_runtime_trace_hold_dispatch_t dispatch, uint16_t flags, uint16_t detail) {
    key_runtime_trace_emit_hold_policy_event(decision, dispatch, flags, detail);
}

void key_runtime_trace_multi_tap_decision(key_runtime_trace_multi_tap_decision_t decision, uint8_t repeat_count, uint16_t detail) {
    key_runtime_trace_emit_multi_tap_event(decision, repeat_count, detail);
}

#endif // defined(CONSOLE_ENABLE) && defined(NOAH_KEY_RUNTIME_TRACE_ENABLE)
