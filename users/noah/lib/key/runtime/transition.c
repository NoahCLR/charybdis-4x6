// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Transitions
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "transition.h"
#include "../../rgb/lighting/lighting_events.h"

#include "trace.h"
#include "planning/effect_plan.h"
#include "projection/projection.h"
#include "reducer/runtime.h"
#include "../../split/runtime_sync.h"
#include "../../state/modifiers/keyboard_mod_policy.h"

enum {
    KEY_RUNTIME_TRANSITION_PLAN_FLAG_AUTO_DRAIN = 1u << 0,
};

static void key_runtime_transition_plan_project_items(const key_runtime_transition_plan_t *plan, uint8_t count) {
    if (!plan) {
        return;
    }

    for (uint8_t index = 0; index < count; index++) {
        const key_runtime_effect_t *effect = &plan->items[index];

        key_runtime_trace_effect_execute(index, effect);
        if (noah_lighting_events_observed() && effect->kind == KEY_RUNTIME_EFFECT_DELAYED_ACTION && (effect->data.delayed_action.repeat_count & KEY_RUNTIME_DELAYED_ACTION_FLAG_DEFER_UNTIL_RELEASE) != 0u && index + 1u < count) {
            const key_runtime_effect_t *commit = &plan->items[index + 1u];
            keypos_t key = key_runtime_effect_delayed_action_key_pos(effect);
            if (commit->kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE && (commit->data.feedback_pulse.commit_flags & KEY_RUNTIME_COMMIT_DEFERRED) != 0u && key.row == commit->data.feedback_pulse.key_pos.row && key.col == commit->data.feedback_pulse.key_pos.col) {
                key_runtime_trace_effect_execute(++index, commit);
                key_runtime_core_project_deferred_commit(effect, commit);
                continue;
            }
        }
        key_runtime_core_project_effect(effect);
    }
}

static void key_runtime_transition_plan_drain(key_runtime_transition_plan_t *plan) {
    if (!(plan && plan->count != 0u)) {
        return;
    }

    // A streaming drain must not split an action from its following captured
    // commit. Leave only a trailing potentially annotated action in the plan;
    // its actual bytes remain owned by this stack until the next append.
    const key_runtime_effect_t *tail = &plan->items[plan->count - 1u];
    bool keep = noah_lighting_events_observed() && tail->kind == KEY_RUNTIME_EFFECT_DELAYED_ACTION && (tail->data.delayed_action.repeat_count & KEY_RUNTIME_DELAYED_ACTION_FLAG_DEFER_UNTIL_RELEASE) != 0u;
    key_runtime_transition_plan_project_items(plan, (uint8_t)(plan->count - (keep ? 1u : 0u)));
    if (keep) plan->items[0] = plan->items[plan->count - 1u];
    plan->count = keep ? 1u : 0u;
}

static void key_runtime_transition_plan_push(key_runtime_transition_plan_t *plan, key_runtime_effect_t effect) {
    if (!plan) {
        return;
    }

    if (plan->count >= ARRAY_SIZE(plan->items) && (plan->flags & KEY_RUNTIME_TRANSITION_PLAN_FLAG_AUTO_DRAIN) != 0u) {
        key_runtime_transition_plan_drain(plan);
    }

    if (plan->count < ARRAY_SIZE(plan->items)) {
        plan->items[plan->count++] = effect;
        return;
    }

    plan->overflowed = true;
}

static void key_runtime_transition_plan_append_core_plan(key_runtime_transition_plan_t *plan, const key_runtime_core_effect_plan_t *core_plan) {
    if (!(plan && core_plan)) {
        return;
    }

    uint8_t previous_flags = plan->flags;
    if (noah_lighting_events_observed()) plan->flags |= KEY_RUNTIME_TRANSITION_PLAN_FLAG_AUTO_DRAIN;
    for (uint8_t index = 0; index < core_plan->count; index++) {
        key_runtime_transition_plan_push(plan, core_plan->items[index]);
    }
    plan->flags = previous_flags;

    plan->overflowed |= core_plan->overflowed;
}

static void key_runtime_transition_core_effect_sink(void *ctx, key_runtime_effect_t effect) {
    key_runtime_transition_plan_push((key_runtime_transition_plan_t *)ctx, effect);
}

static uint8_t key_runtime_transition_begin_auto_drain(key_runtime_transition_plan_t *plan) {
    uint8_t previous_flags = plan ? plan->flags : 0u;

    if (plan) {
        plan->flags |= KEY_RUNTIME_TRANSITION_PLAN_FLAG_AUTO_DRAIN;
    }

    return previous_flags;
}

static void key_runtime_transition_end_auto_drain(key_runtime_transition_plan_t *plan, uint8_t previous_flags, const key_runtime_core_effect_plan_t *core_plan) {
    if (!plan) {
        return;
    }

    if (core_plan) {
        plan->overflowed |= core_plan->overflowed;
    }
    plan->flags = previous_flags;
}

static void key_runtime_transition_core_plan_init_streaming(key_runtime_core_effect_plan_t *core_plan, key_runtime_transition_plan_t *plan) {
    key_runtime_core_effect_plan_init_with_sink(core_plan, key_runtime_transition_core_effect_sink, plan);
}

static __attribute__((noinline)) void key_runtime_transition_append_unmatched_release_effects(keypos_t key_pos, const handled_key_resolution_t *resolution, key_runtime_transition_plan_t *plan) {
    handled_key_resolution_ctx_t ctx;

    if (!plan) {
        return;
    }

    ctx = handled_key_resolution_ctx_live(key_pos);
    if (handled_key_resolution_materializes_momentary_layer(resolution, &ctx)) {
        key_runtime_transition_plan_push(plan, (key_runtime_effect_t){
                                                   .kind         = KEY_RUNTIME_EFFECT_LAYER_RELEASE,
                                                   .data.key_pos = key_pos,
                                               });
    }

    key_runtime_transition_plan_push(plan, (key_runtime_effect_t){
                                               .kind         = KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY,
                                               .data.key_pos = key_pos,
                                           });
}

void key_runtime_transition_plan_init(key_runtime_transition_plan_t *plan) {
    if (!plan) {
        return;
    }

    *plan = (key_runtime_transition_plan_t){0};
}

void key_runtime_transition_execute_plan(const key_runtime_transition_plan_t *plan) {
    if (plan && plan->count != 0u) {
        split_runtime_sync_notify_key_feedback_dirty();
    }

    key_runtime_transition_plan_project_items(plan, plan ? plan->count : 0u);
}

void key_runtime_transition_flush_multi_tap(key_runtime_transition_plan_t *plan) {
    key_runtime_core_effect_plan_t core_plan;
    uint8_t                        previous_flags;

    previous_flags = key_runtime_transition_begin_auto_drain(plan);
    key_runtime_transition_core_plan_init_streaming(&core_plan, plan);
    key_runtime_core_flush_multi_tap(&core_plan);
    key_runtime_transition_end_auto_drain(plan, previous_flags, &core_plan);
}

void key_runtime_transition_flush_foreign_multi_tap(uint16_t keycode, keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    key_runtime_core_effect_plan_t core_plan;
    uint8_t                        previous_flags;

    previous_flags = key_runtime_transition_begin_auto_drain(plan);
    key_runtime_transition_core_plan_init_streaming(&core_plan, plan);
    key_runtime_core_flush_foreign_multi_tap(keycode, key_pos, &core_plan);
    key_runtime_transition_end_auto_drain(plan, previous_flags, &core_plan);
}

void key_runtime_transition_flush_foreign_dual_role_multi_tap(uint16_t keycode, keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    key_runtime_core_effect_plan_t core_plan;
    uint8_t                        previous_flags;

    previous_flags = key_runtime_transition_begin_auto_drain(plan);
    key_runtime_transition_core_plan_init_streaming(&core_plan, plan);
    key_runtime_core_flush_foreign_dual_role_multi_tap(keycode, key_pos, &core_plan);
    key_runtime_transition_end_auto_drain(plan, previous_flags, &core_plan);
}

bool key_runtime_transition_flush_foreign_layer_multi_tap(keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    key_runtime_core_effect_plan_t core_plan;
    uint8_t                        previous_flags;
    bool                           flushed;

    previous_flags = key_runtime_transition_begin_auto_drain(plan);
    key_runtime_transition_core_plan_init_streaming(&core_plan, plan);
    flushed = key_runtime_core_flush_foreign_layer_multi_tap(key_pos, &core_plan);
    key_runtime_transition_end_auto_drain(plan, previous_flags, &core_plan);
    return flushed;
}

void key_runtime_transition_flush_active_keys_except(keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    key_runtime_core_effect_plan_t core_plan;
    uint8_t                        previous_flags;

    previous_flags = key_runtime_transition_begin_auto_drain(plan);
    key_runtime_transition_core_plan_init_streaming(&core_plan, plan);
    key_runtime_core_flush_active_keys_except(key_pos, &core_plan);
    key_runtime_transition_end_auto_drain(plan, previous_flags, &core_plan);
}

void key_runtime_transition_interrupt_active_keys_on_other_press(keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    key_runtime_core_effect_plan_t core_plan;
    uint8_t                        previous_flags;

    previous_flags = key_runtime_transition_begin_auto_drain(plan);
    key_runtime_transition_core_plan_init_streaming(&core_plan, plan);
    key_runtime_core_interrupt_active_keys_on_other_press(key_pos, &core_plan);
    key_runtime_transition_end_auto_drain(plan, previous_flags, &core_plan);
}

bool key_runtime_transition_handled_key_press(uint16_t keycode, keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    key_runtime_core_effect_plan_t core_plan;

    key_runtime_core_effect_plan_init(&core_plan);
    if (!key_runtime_core_handle_handled_key_press(keycode, key_pos, &core_plan)) {
        return false;
    }

    key_runtime_transition_plan_append_core_plan(plan, &core_plan);
    return true;
}

bool key_runtime_transition_handled_key_release(uint16_t keycode, keyrecord_t *record, const handled_key_resolution_t *resolution, key_runtime_transition_plan_t *plan) {
    key_runtime_core_effect_plan_t core_plan;

    if (!(record && plan)) {
        return false;
    }

    key_runtime_core_effect_plan_init(&core_plan);
    if (key_runtime_core_handle_handled_key_release(keycode, record->event.key, resolution, keyboard_mod_policy_current_state(), &core_plan)) {
        key_runtime_transition_plan_append_core_plan(plan, &core_plan);
        return true;
    }

    if (resolution && handled_key_resolution_is_handled(*resolution)) {
        key_runtime_transition_append_unmatched_release_effects(record->event.key, resolution, plan);
        return true;
    }

    return false;
}

void key_runtime_transition_scan(key_runtime_transition_plan_t *plan) {
    key_runtime_core_effect_plan_t core_plan;
    uint8_t                        previous_flags;

    previous_flags = key_runtime_transition_begin_auto_drain(plan);
    key_runtime_transition_core_plan_init_streaming(&core_plan, plan);
    key_runtime_core_scan(&core_plan, timer_read());
    key_runtime_transition_end_auto_drain(plan, previous_flags, &core_plan);
}

bool key_runtime_transition_settle_pending_fallback_hold(key_runtime_transition_plan_t *plan) {
    key_runtime_core_effect_plan_t core_plan;
    uint8_t                        previous_flags;
    bool                           settled_any;

    previous_flags = key_runtime_transition_begin_auto_drain(plan);
    key_runtime_transition_core_plan_init_streaming(&core_plan, plan);
    settled_any = key_runtime_core_settle_pending_fallback_hold(&core_plan);
    key_runtime_transition_end_auto_drain(plan, previous_flags, &core_plan);
    return settled_any;
}
