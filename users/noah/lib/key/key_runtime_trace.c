// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Trace
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_trace.h"

#include <string.h>

#include "../state/runtime_trace.h"

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

#endif // defined(CONSOLE_ENABLE) && defined(NOAH_KEY_RUNTIME_TRACE_ENABLE)
