// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Transitions
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "key_runtime_transition.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "key_runtime_feedback.h"
#include "key_runtime_admission.h"
#include "key_runtime_index_internal.h"
#include "slot/key_runtime_slot_pending_multi_tap.h"
#include "slot/key_runtime_slot_press_reduce.h"
#include "slot/key_runtime_slot_release_active.h"
#include "slot/key_runtime_slot_release_reduce.h"
#include "slot/key_runtime_slot_scan_reduce.h"
#include "slot/key_runtime_slot_policy.h"
#include "key_runtime_internal.h"
#include "key_runtime_trace.h"
#include "../../action/action_dispatch.h"
#include "../../action/action_lifecycle.h"
#include "../../pointing/defs/pd_modes.h"
#include "../../runtime_v2/runtime_v2.h"
#include "../../runtime_v2/runtime_v2_projection.h"
#include "../../state/ownership/layer_ownership.h"
#include "../../state/runtime/split_runtime_sync.h"
#include "../ownership/held_action.h"
#include "../ownership/held_repeat.h"

__attribute__((weak)) bool runtime_v2_blocker_queries_authoritative(void) {
    return false;
}

__attribute__((weak)) bool runtime_v2_has_any_deferred_release_blocker(void) {
    return false;
}

__attribute__((weak)) bool runtime_v2_has_foreign_deferred_release_blocker_except(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

__attribute__((weak)) void runtime_v2_observe_held_action_register(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
}

__attribute__((weak)) void runtime_v2_observe_held_action_unregister(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
}

__attribute__((weak)) void runtime_v2_observe_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
    (void)key_pos;
    (void)action;
    (void)repeat_hz;
}

__attribute__((weak)) bool runtime_v2_release_owned_state_by_key(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

__attribute__((weak)) void runtime_v2_project_effect(const key_runtime_effect_t *effect) {
    if (!effect) {
        return;
    }

    switch (effect->kind) {
        case KEY_RUNTIME_EFFECT_DISPATCH_ACTION:
            noah_emit_action_tap(effect->data.action, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);
            return;
        case KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER:
            runtime_v2_observe_held_action_register(effect->data.held_action.key_pos, effect->data.held_action.action);
            held_action_register(effect->data.held_action.key_pos, effect->data.held_action.action);
            return;
        case KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER:
            held_action_unregister(effect->data.held_action.key_pos, effect->data.held_action.action);
            runtime_v2_observe_held_action_unregister(effect->data.held_action.key_pos, effect->data.held_action.action);
            return;
        case KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY:
            runtime_v2_release_owned_state_by_key(effect->data.key_pos);
            held_action_release_owned_by_key(effect->data.key_pos);
            return;
        case KEY_RUNTIME_EFFECT_REPEAT_START:
            runtime_v2_observe_repeat_start(effect->data.repeat.key_pos, effect->data.repeat.action, effect->data.repeat.repeat_hz);
            held_repeat_start(effect->data.repeat.key_pos, effect->data.repeat.action, effect->data.repeat.repeat_hz);
            return;
        case KEY_RUNTIME_EFFECT_LAYER_PRESS:
            layer_ownership_momentary_press(effect->data.layer_press.key_pos, effect->data.layer_press.layer);
            return;
        case KEY_RUNTIME_EFFECT_LAYER_RELEASE:
            layer_ownership_momentary_release(effect->data.key_pos);
            return;
        case KEY_RUNTIME_EFFECT_FEEDBACK_PULSE:
            key_feedback_pulse_arm(effect->data.long_hold_level);
            return;
        case KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP:
            if (pd_mode_toggle_lock_state(effect->data.pd_mode)) {
                split_runtime_sync();
            }
            return;
        case KEY_RUNTIME_EFFECT_DELAYED_ACTION:
            for (uint8_t repeat = 0; repeat < effect->data.delayed_action.repeat_count; repeat++) {
                dispatch_delayed_action(effect->data.delayed_action.action, effect->data.delayed_action.mods);
            }
            return;
        case KEY_RUNTIME_EFFECT_NONE:
        default:
            return;
    }
}

void key_runtime_transition_plan_init(key_runtime_transition_plan_t *plan) {
    *plan = (key_runtime_transition_plan_t){0};
}

static void key_runtime_transition_log_plan_overflow(key_runtime_effect_kind_t kind, uint8_t capacity) {
#ifdef CONSOLE_ENABLE
    uprintf("Key runtime transition plan overflow dropping effect kind %u after %u queued effects\n", (unsigned int)kind, (unsigned int)capacity);
#else
    (void)kind;
    (void)capacity;
#endif
}

static void key_runtime_transition_fail_host_overflow(key_runtime_effect_kind_t kind, uint8_t capacity) {
#ifdef NOAH_HOST_TEST_ENV
    noah_host_test_fail_runtime_overflow("key runtime transition plan", (unsigned int)kind, (unsigned int)capacity);
#else
    (void)kind;
    (void)capacity;
#endif
}

static void key_runtime_transition_plan_push(key_runtime_transition_plan_t *plan, key_runtime_effect_t effect) {
    if (plan->count < ARRAY_SIZE(plan->items)) {
        plan->items[plan->count++] = effect;
        return;
    }

    if (!plan->overflowed) {
        plan->overflowed = true;
        key_runtime_transition_log_plan_overflow(effect.kind, ARRAY_SIZE(plan->items));
        key_runtime_transition_fail_host_overflow(effect.kind, ARRAY_SIZE(plan->items));
    }
}

static void key_runtime_transition_plan_push_builder_if_present(key_runtime_transition_plan_t *plan, keypos_t key_pos, key_runtime_effect_builder_t builder) {
    if (!key_runtime_slot_direct_plan_builder_has_effect(builder)) {
        return;
    }

    if (builder.release_owned_state) {
        key_runtime_transition_plan_push(plan, (key_runtime_effect_t){
                                                   .kind         = KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY,
                                                   .data.key_pos = key_pos,
                                               });
    }

    switch (builder.kind) {
        case KEY_RUNTIME_EFFECT_BUILDER_DISPATCH_ACTION:
            key_runtime_transition_plan_push(plan, (key_runtime_effect_t){
                                                       .kind        = KEY_RUNTIME_EFFECT_DISPATCH_ACTION,
                                                       .data.action = builder.action,
                                                   });
            break;
        case KEY_RUNTIME_EFFECT_BUILDER_HELD_REGISTER:
            key_runtime_transition_plan_push(plan, (key_runtime_effect_t){
                                                       .kind = KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER,
                                                       .data.held_action =
                                                           {
                                                               .key_pos = key_pos,
                                                               .action  = builder.action,
                                                           },
                                                   });
            break;
        case KEY_RUNTIME_EFFECT_BUILDER_HELD_UNREGISTER:
            key_runtime_transition_plan_push(plan, (key_runtime_effect_t){
                                                       .kind = KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER,
                                                       .data.held_action =
                                                           {
                                                               .key_pos = key_pos,
                                                               .action  = builder.action,
                                                           },
                                                   });
            break;
        case KEY_RUNTIME_EFFECT_BUILDER_REPEAT_START:
            key_runtime_transition_plan_push(plan, (key_runtime_effect_t){
                                                       .kind = KEY_RUNTIME_EFFECT_REPEAT_START,
                                                       .data.repeat =
                                                           {
                                                               .key_pos   = key_pos,
                                                               .action    = builder.action,
                                                               .repeat_hz = builder.repeat_hz,
                                                           },
                                                   });
            break;
        case KEY_RUNTIME_EFFECT_BUILDER_NONE:
        default:
            break;
    }

    if (builder.feedback_pulse) {
        key_runtime_transition_plan_push(plan, (key_runtime_effect_t){
                                                   .kind                 = KEY_RUNTIME_EFFECT_FEEDBACK_PULSE,
                                                   .data.long_hold_level = builder.feedback_long_hold_level,
                                               });
    }
}

static void key_runtime_transition_plan_push_delayed_action(key_runtime_transition_plan_t *plan, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count) {
    if (!(plan && action != KC_NO && repeat_count != 0u)) {
        return;
    }

    key_runtime_transition_plan_push(plan, (key_runtime_effect_t){
                                               .kind = KEY_RUNTIME_EFFECT_DELAYED_ACTION,
                                               .data.delayed_action =
                                                   {
                                                       .action       = action,
                                                       .mods         = mods,
                                                       .repeat_count = repeat_count,
                                                   },
                                           });
}

static void key_runtime_transition_apply_slot_result(const key_runtime_slot_result_t *result, key_runtime_transition_plan_t *plan) {
    if (!result || !result->handled) {
        return;
    }

    for (uint8_t index = 0; index < result->count; index++) {
        key_runtime_transition_plan_push(plan, result->items[index]);
    }
}

static void key_runtime_transition_append_release_effect_plan(const runtime_v2_release_effect_plan_t *release_plan, key_runtime_transition_plan_t *plan) {
    if (!(release_plan && plan)) {
        return;
    }

    for (uint8_t index = 0; index < release_plan->count; index++) {
        key_runtime_transition_plan_push(plan, release_plan->items[index]);
    }
}

static void key_runtime_transition_append_direct_plan(const key_runtime_slot_direct_plan_t *direct_plan, key_runtime_transition_plan_t *plan) {
    if (!(direct_plan && plan)) {
        return;
    }

    for (uint8_t index = 0; index < direct_plan->count; index++) {
        key_runtime_transition_plan_push(plan, direct_plan->items[index]);
    }
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

static bool key_runtime_transition_apply_release_reduce(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution, key_runtime_transition_plan_t *plan) {
    key_runtime_slot_result_t result = key_runtime_slot_reduce_handled_release(slot, keycode, key_pos, resolution);

    if (!result.handled) {
        return false;
    }

    key_runtime_transition_apply_slot_result(&result, plan);
    return true;
}

void key_runtime_transition_execute_plan(const key_runtime_transition_plan_t *plan) {
    for (uint8_t i = 0; i < plan->count; i++) {
        const key_runtime_effect_t *effect = &plan->items[i];
        key_runtime_trace_effect_execute(i, effect);
        runtime_v2_project_effect(effect);
    }
}

bool key_runtime_transition_handled_key_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution, bool active_held_action_survives_flush, key_runtime_transition_plan_t *plan) {
    key_runtime_slot_direct_plan_t direct_plan = key_runtime_slot_take_handled_press_plan(slot, keycode, key_pos, resolution, active_held_action_survives_flush);

    if (!direct_plan.handled) {
        return false;
    }

    key_runtime_transition_append_direct_plan(&direct_plan, plan);
    return true;
}

void key_runtime_transition_flush_multi_tap(key_runtime_transition_plan_t *plan) {
    active_key_state_t *slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t             pending_count = key_runtime_index_snapshot_pending_multi_tap_slots(slots, ARRAY_SIZE(slots));

    for (uint8_t index = 0; index < pending_count; index++) {
        key_runtime_slot_pending_multi_tap_flush_t flush = key_runtime_slot_take_pending_multi_tap_flush(slots[index]);

        if (!flush.handled) {
            continue;
        }

        key_runtime_trace_multi_tap_decision(KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_FLUSH_PENDING_CHAIN, flush.repeat_count, flush.action);
        key_runtime_transition_plan_push_delayed_action(plan, flush.action, flush.mods, flush.repeat_count);
    }
}

void key_runtime_transition_flush_foreign_multi_tap(uint16_t keycode, keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    active_key_state_t *slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t             pending_count = key_runtime_index_snapshot_pending_multi_tap_slots(slots, ARRAY_SIZE(slots));

    for (uint8_t index = 0; index < pending_count; index++) {
        active_key_state_t *slot = slots[index];

        if (!slot || key_runtime_slot_pending_multi_tap_matches(slot, keycode, key_pos)) {
            continue;
        }

        key_runtime_slot_pending_multi_tap_flush_t flush = key_runtime_slot_take_pending_multi_tap_flush(slot);

        if (!flush.handled) {
            continue;
        }

        key_runtime_trace_multi_tap_decision(KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_FLUSH_PENDING_CHAIN, flush.repeat_count, flush.action);
        key_runtime_transition_plan_push_delayed_action(plan, flush.action, flush.mods, flush.repeat_count);
    }
}

void key_runtime_transition_flush_active_keys_except(keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    active_key_state_t *slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t             active_count = key_runtime_index_snapshot_active_slots(slots, ARRAY_SIZE(slots));

    for (uint8_t index = 0; index < active_count; index++) {
        active_key_state_t         *slot = slots[index];
        key_runtime_effect_builder_t builder;
        bool                        active_held_action_survives_flush;
        keypos_t                    owner_key_pos;

        if (!slot || key_runtime_keypos_equal(slot->owner.key_pos, key_pos)) {
            continue;
        }

        owner_key_pos                     = slot->owner.key_pos;
        active_held_action_survives_flush = slot->lifecycle.held_action_keycode == KC_NO || held_action_survives_flush(slot->owner.key_pos, slot->lifecycle.held_action_keycode);
        builder                           = key_runtime_slot_policy_take_flush(slot, active_held_action_survives_flush);
        if (key_runtime_slot_direct_plan_builder_has_effect(builder)) {
            key_runtime_transition_plan_push_builder_if_present(plan, owner_key_pos, builder);
        }
    }
}

bool key_runtime_transition_has_foreign_tap_release_slot_except(keypos_t key_pos) {
    if (runtime_v2_blocker_queries_authoritative()) {
        return runtime_v2_has_foreign_deferred_release_blocker_except(key_pos);
    }

    return key_runtime_index_has_foreign_deferred_release_blocker_except(key_pos);
}

bool key_runtime_transition_has_any_tap_release_slot(void) {
    if (runtime_v2_blocker_queries_authoritative()) {
        return runtime_v2_has_any_deferred_release_blocker();
    }

    return key_runtime_index_has_any_deferred_release_blocker();
}

void key_runtime_transition_interrupt_active_keys_on_other_press(keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    active_key_state_t *slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t             active_count = key_runtime_index_snapshot_active_slots(slots, ARRAY_SIZE(slots));

    for (uint8_t index = 0; index < active_count; index++) {
        active_key_state_t         *slot = slots[index];
        key_runtime_effect_builder_t builder;

        if (!slot) {
            continue;
        }

        builder = key_runtime_slot_policy_interrupt_on_other_press(slot, key_pos);
        key_runtime_transition_plan_push_builder_if_present(plan, slot->owner.key_pos, builder);
    }
}

void key_runtime_transition_interrupt_active_key_on_other_press(key_runtime_transition_plan_t *plan) {
    key_runtime_transition_interrupt_active_keys_on_other_press((keypos_t){0xFF, 0xFF}, plan);
}

static bool key_runtime_transition_process_active_key_release(uint16_t keycode, keyrecord_t *record, handled_key_resolution_t resolution, key_runtime_transition_plan_t *plan) {
    active_key_state_t      *slot            = key_runtime_find_slot_by_position(record->event.key);
    uint16_t                 release_keycode = keycode;
    handled_key_resolution_t release_key     = resolution;
    runtime_v2_release_effect_plan_t release_plan = {0};

    if (key_runtime_slot_active(slot) && slot->owner.keycode != keycode) {
        release_keycode = slot->owner.keycode;
        release_key     = handled_key_lookup(release_keycode);
    }

    if (runtime_v2_blocker_queries_authoritative()) {
        if (slot && key_runtime_slot_take_v2_pending_multi_tap_release_plan(slot, release_keycode, timer_elapsed(slot->timer), &release_plan)) {
            key_runtime_transition_append_release_effect_plan(&release_plan, plan);
            return true;
        }

        if (key_runtime_slot_matches(slot, release_keycode, record->event.key) && key_runtime_slot_take_v2_active_release_plan(slot, release_keycode, &release_plan)) {
            key_runtime_transition_append_release_effect_plan(&release_plan, plan);
            return true;
        }

        key_runtime_transition_append_unmatched_release_effects(record->event.key, release_key, plan);
        return true;
    }

    return key_runtime_transition_apply_release_reduce(slot, release_keycode, record->event.key, release_key, plan);
}

bool key_runtime_transition_handled_key_release(uint16_t keycode, keyrecord_t *record, handled_key_resolution_t resolution, key_runtime_transition_plan_t *plan) {
    return key_runtime_transition_process_active_key_release(keycode, record, resolution, plan);
}

void key_runtime_transition_scan(key_runtime_transition_plan_t *plan) {
    active_key_state_t *active_slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    active_key_state_t *pending_slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t             active_count  = key_runtime_index_snapshot_active_slots(active_slots, ARRAY_SIZE(active_slots));
    uint8_t             pending_count = key_runtime_index_snapshot_pending_multi_tap_slots(pending_slots, ARRAY_SIZE(pending_slots));

    for (uint8_t index = 0; index < active_count; index++) {
        active_key_state_t *slot = active_slots[index];
        key_runtime_slot_direct_plan_t direct_plan;

        direct_plan = key_runtime_slot_take_active_scan_plan(slot);
        key_runtime_transition_append_direct_plan(&direct_plan, plan);
        if (key_runtime_slot_has_pending_multi_tap(slot)) {
            direct_plan = key_runtime_slot_take_pending_multi_tap_scan_plan(slot);
            key_runtime_transition_append_direct_plan(&direct_plan, plan);
        }
    }

    for (uint8_t index = 0; index < pending_count; index++) {
        key_runtime_slot_direct_plan_t direct_plan = key_runtime_slot_take_pending_multi_tap_scan_plan(pending_slots[index]);
        key_runtime_transition_append_direct_plan(&direct_plan, plan);
    }
}
