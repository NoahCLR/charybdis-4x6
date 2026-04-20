// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Transitions
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "key_runtime_transition.h"

#include "key_runtime_trace.h"
#include "../../runtime_v2/runtime_v2.h"

static void key_runtime_transition_plan_push(key_runtime_transition_plan_t *plan, key_runtime_effect_t effect) {
    if (!plan) {
        return;
    }

    if (plan->count < ARRAY_SIZE(plan->items)) {
        plan->items[plan->count++] = effect;
        return;
    }

    plan->overflowed = true;
}

static void key_runtime_transition_plan_append_runtime_v2_plan(key_runtime_transition_plan_t *plan, const runtime_v2_effect_plan_t *v2_plan) {
    if (!(plan && v2_plan)) {
        return;
    }

    for (uint8_t index = 0; index < v2_plan->count; index++) {
        key_runtime_transition_plan_push(plan, v2_plan->items[index]);
    }

    plan->overflowed |= v2_plan->overflowed;
}

static void key_runtime_transition_append_unmatched_release_effects(keypos_t key_pos, handled_key_resolution_t resolution, key_runtime_transition_plan_t *plan) {
    handled_key_materialized_t materialized = handled_key_materialize(resolution, handled_key_resolution_ctx_live(key_pos));

    if (!plan) {
        return;
    }

    if ((materialized.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0) {
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

static keyboard_mod_state_t key_runtime_transition_keyboard_mod_state_current(void) {
    return (keyboard_mod_state_t){
        .real           = get_mods(),
        .weak           = get_weak_mods(),
        .oneshot        = get_oneshot_mods(),
        .oneshot_locked = get_oneshot_locked_mods(),
    };
}

void key_runtime_transition_plan_init(key_runtime_transition_plan_t *plan) {
    if (!plan) {
        return;
    }

    *plan = (key_runtime_transition_plan_t){0};
}

void key_runtime_transition_execute_plan(const key_runtime_transition_plan_t *plan) {
    if (!plan) {
        return;
    }

    for (uint8_t index = 0; index < plan->count; index++) {
        const key_runtime_effect_t *effect = &plan->items[index];

        key_runtime_trace_effect_execute(index, effect);
        runtime_v2_project_effect(effect);
    }
}

void key_runtime_transition_flush_multi_tap(key_runtime_transition_plan_t *plan) {
    runtime_v2_effect_plan_t v2_plan;

    runtime_v2_effect_plan_init(&v2_plan);
    runtime_v2_flush_multi_tap(&v2_plan);
    key_runtime_transition_plan_append_runtime_v2_plan(plan, &v2_plan);
}

void key_runtime_transition_flush_foreign_multi_tap(uint16_t keycode, keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    runtime_v2_effect_plan_t v2_plan;

    runtime_v2_effect_plan_init(&v2_plan);
    runtime_v2_flush_foreign_multi_tap(keycode, key_pos, &v2_plan);
    key_runtime_transition_plan_append_runtime_v2_plan(plan, &v2_plan);
}

void key_runtime_transition_flush_active_keys_except(keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    runtime_v2_effect_plan_t v2_plan;

    runtime_v2_effect_plan_init(&v2_plan);
    runtime_v2_flush_active_keys_except(key_pos, &v2_plan);
    key_runtime_transition_plan_append_runtime_v2_plan(plan, &v2_plan);
}

bool key_runtime_transition_has_foreign_tap_release_slot_except(keypos_t key_pos) {
    return runtime_v2_has_foreign_deferred_release_blocker_except(key_pos);
}

bool key_runtime_transition_has_any_tap_release_slot(void) {
    return runtime_v2_has_any_deferred_release_blocker();
}

void key_runtime_transition_interrupt_active_keys_on_other_press(keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    runtime_v2_effect_plan_t v2_plan;

    runtime_v2_effect_plan_init(&v2_plan);
    runtime_v2_interrupt_active_keys_on_other_press(key_pos, &v2_plan);
    key_runtime_transition_plan_append_runtime_v2_plan(plan, &v2_plan);
}

void key_runtime_transition_interrupt_active_key_on_other_press(key_runtime_transition_plan_t *plan) {
    key_runtime_transition_interrupt_active_keys_on_other_press((keypos_t){0xFF, 0xFF}, plan);
}

bool key_runtime_transition_handled_key_press(uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution, key_runtime_transition_plan_t *plan) {
    runtime_v2_effect_plan_t v2_plan;

    runtime_v2_effect_plan_init(&v2_plan);
    if (!runtime_v2_handle_handled_key_press(keycode, key_pos, resolution, &v2_plan)) {
        return false;
    }

    key_runtime_transition_plan_append_runtime_v2_plan(plan, &v2_plan);
    return true;
}

bool key_runtime_transition_handled_key_release(uint16_t keycode, keyrecord_t *record, handled_key_resolution_t resolution, key_runtime_transition_plan_t *plan) {
    runtime_v2_effect_plan_t v2_plan;

    if (!(record && plan)) {
        return false;
    }

    runtime_v2_effect_plan_init(&v2_plan);
    if (runtime_v2_handle_handled_key_release(keycode, record->event.key, resolution, key_runtime_transition_keyboard_mod_state_current(), &v2_plan)) {
        key_runtime_transition_plan_append_runtime_v2_plan(plan, &v2_plan);
        return true;
    }

    key_runtime_transition_append_unmatched_release_effects(record->event.key, resolution, plan);
    return true;
}

void key_runtime_transition_scan(key_runtime_transition_plan_t *plan) {
    runtime_v2_effect_plan_t v2_plan;

    runtime_v2_effect_plan_init(&v2_plan);
    runtime_v2_scan(&v2_plan, timer_read());
    key_runtime_transition_plan_append_runtime_v2_plan(plan, &v2_plan);
}
