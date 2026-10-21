// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Transitions
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "transition.h"

#include "trace.h"
#include "projection/projection.h"
#include "../../split/runtime_sync.h"

uint32_t key_runtime_transition_feedback_mark(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    return state ? state->next_feedback_sequence : 0u;
}

void key_runtime_transition_drain_plan(key_runtime_core_effect_plan_t *plan) {
    if (!plan) {
        return;
    }

    for (uint8_t index = 0; index < plan->count; index++) {
        const key_runtime_effect_t *effect = &plan->items[index];

        key_runtime_trace_effect_execute(index, effect);
        key_runtime_core_project_effect(effect);
    }
    plan->count = 0u;
}

void key_runtime_transition_execute_plan(const char *stage, key_runtime_core_effect_plan_t *plan) {
    if (!plan) {
        return;
    }

    if (stage) {
        key_runtime_trace_plan(stage, plan);
    }
    if (plan->count != 0u) {
        split_runtime_sync_notify_key_feedback_dirty();
    }
    key_runtime_transition_drain_plan(plan);
}

void key_runtime_transition_execute_event_plan(const char *stage, key_runtime_core_effect_plan_t *plan, uint32_t feedback_mark) {
    if (plan && plan->count == 0u && key_runtime_transition_feedback_mark() != feedback_mark) {
        split_runtime_sync_notify_key_feedback_dirty();
    }
    key_runtime_transition_execute_plan(stage, plan);
}
