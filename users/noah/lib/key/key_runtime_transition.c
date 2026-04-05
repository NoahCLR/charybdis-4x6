// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Transitions
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_transition.h"

#include "key_runtime_effects.h"
#include "key_runtime_feedback.h"
#include "key_runtime_state.h"
#include "../action/action_dispatch.h"
#include "../action/action_lifecycle.h"
#include "../pointing/pd_modes.h"

typedef enum {
    HOLD_THRESHOLD_DISPATCH_NONE = 0,
    HOLD_THRESHOLD_DISPATCH_TAP,
    HOLD_THRESHOLD_DISPATCH_HELD,
} hold_threshold_dispatch_t;

void key_runtime_transition_plan_init(key_runtime_transition_plan_t *plan) {
    *plan = (key_runtime_transition_plan_t){0};
}

static void key_runtime_transition_plan_push(key_runtime_transition_plan_t *plan, key_runtime_transition_effect_t effect) {
    if (plan->count < ARRAY_SIZE(plan->effects)) {
        plan->effects[plan->count++] = effect;
    }
}

static void key_runtime_transition_plan_dispatch_action(key_runtime_transition_plan_t *plan, uint16_t action) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                            .kind        = KEY_RUNTIME_TRANSITION_EFFECT_DISPATCH_ACTION,
                                            .data.action = action,
                                        });
}

static void key_runtime_transition_plan_held_register(key_runtime_transition_plan_t *plan, keypos_t key_pos, uint16_t action) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                            .kind = KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_REGISTER,
                                            .data.held_action =
                                                {
                                                    .key_pos = key_pos,
                                                    .action  = action,
                                                },
                                        });
}

static void key_runtime_transition_plan_held_unregister(key_runtime_transition_plan_t *plan, keypos_t key_pos, uint16_t action) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                            .kind = KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_UNREGISTER,
                                            .data.held_action =
                                                {
                                                    .key_pos = key_pos,
                                                    .action  = action,
                                                },
                                        });
}

static void key_runtime_transition_plan_release_held_owned_by_key(key_runtime_transition_plan_t *plan, keypos_t key_pos) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                            .kind         = KEY_RUNTIME_TRANSITION_EFFECT_RELEASE_HELD_ACTION_OWNED_BY_KEY,
                                            .data.key_pos = key_pos,
                                        });
}

static void key_runtime_transition_plan_layer_press(key_runtime_transition_plan_t *plan, keypos_t key_pos, uint8_t layer) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                            .kind = KEY_RUNTIME_TRANSITION_EFFECT_LAYER_PRESS,
                                            .data.layer_press =
                                                {
                                                    .key_pos = key_pos,
                                                    .layer   = layer,
                                                },
                                        });
}

static void key_runtime_transition_plan_layer_release(key_runtime_transition_plan_t *plan, keypos_t key_pos) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                            .kind         = KEY_RUNTIME_TRANSITION_EFFECT_LAYER_RELEASE,
                                            .data.key_pos = key_pos,
                                        });
}

static void key_runtime_transition_plan_feedback_pulse(key_runtime_transition_plan_t *plan, bool long_hold_level) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                            .kind                 = KEY_RUNTIME_TRANSITION_EFFECT_FEEDBACK_PULSE,
                                            .data.long_hold_level = long_hold_level,
                                        });
}

static void key_runtime_transition_plan_pd_mode_lock_tap(key_runtime_transition_plan_t *plan, pd_mode_mask_t mode) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                            .kind         = KEY_RUNTIME_TRANSITION_EFFECT_PD_MODE_LOCK_TAP,
                                            .data.pd_mode = mode,
                                        });
}

static void key_runtime_transition_plan_delayed_action(key_runtime_transition_plan_t *plan, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count) {
    if (action == KC_NO || repeat_count == 0) {
        return;
    }

    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                            .kind = KEY_RUNTIME_TRANSITION_EFFECT_DELAYED_ACTION,
                                            .data.delayed_action =
                                                {
                                                    .action       = action,
                                                    .mods         = mods,
                                                    .repeat_count = repeat_count,
                                                },
                                        });
}

void key_runtime_transition_execute_plan(const key_runtime_transition_plan_t *plan) {
    for (uint8_t i = 0; i < plan->count; i++) {
        const key_runtime_transition_effect_t *effect = &plan->effects[i];

        switch (effect->kind) {
            case KEY_RUNTIME_TRANSITION_EFFECT_DISPATCH_ACTION:
                key_runtime_effects_dispatch_action(effect->data.action);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_REGISTER:
                key_runtime_effects_held_action_register(effect->data.held_action.key_pos, effect->data.held_action.action);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_UNREGISTER:
                key_runtime_effects_held_action_unregister(effect->data.held_action.key_pos, effect->data.held_action.action);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_RELEASE_HELD_ACTION_OWNED_BY_KEY:
                key_runtime_effects_release_held_action_owned_by_key(effect->data.key_pos);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_LAYER_PRESS:
                key_runtime_effects_layer_press(effect->data.layer_press.key_pos, effect->data.layer_press.layer);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_LAYER_RELEASE:
                key_runtime_effects_layer_release(effect->data.key_pos);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_FEEDBACK_PULSE:
                key_runtime_effects_feedback_pulse_arm(effect->data.long_hold_level);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_PD_MODE_LOCK_TAP:
                if (pd_mode_is_lockable(effect->data.pd_mode) && pd_mode_toggle_lock_state(effect->data.pd_mode)) {
                    key_runtime_effects_sync_split_runtime();
                }
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_DELAYED_ACTION:
                for (uint8_t repeat = 0; repeat < effect->data.delayed_action.repeat_count; repeat++) {
                    dispatch_delayed_action(effect->data.delayed_action.action, effect->data.delayed_action.mods);
                }
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_NONE:
            default:
                break;
        }
    }
}

static void key_runtime_transition_flush_active_key(bool active_held_action_survives_flush, key_runtime_transition_plan_t *plan) {
    if (active_key.keycode == KC_NO) return;

    if (active_key.hold_fired || active_key.held_action_keycode != KC_NO) {
        active_key.hold_fired = false;
        if (active_key.held_action_keycode != KC_NO && !active_held_action_survives_flush) {
            key_runtime_transition_plan_held_unregister(plan, active_key.key_pos, active_key.held_action_keycode);
            active_key.held_action_keycode = KC_NO;
        }
    } else if (!is_layer_key(active_key.keycode) && active_key.tap_action != KC_NO) {
        key_runtime_transition_plan_dispatch_action(plan, active_key.tap_action);
    }

    active_key_reset();
}

static void key_runtime_transition_activate_immediate_hold_if_needed(keyrecord_t *record, hold_behavior_t hold, key_runtime_transition_plan_t *plan) {
    if (!hold_registers_on_press(hold)) {
        return;
    }

    key_runtime_transition_plan_held_register(plan, record->event.key, hold.action);
    active_key.held_action_keycode = hold.action;
}

bool key_runtime_transition_handled_key_press(uint16_t keycode, keyrecord_t *record, handled_key_view_t key, bool active_held_action_survives_flush, key_runtime_transition_plan_t *plan) {
    key_behavior_view_t behavior = key.behavior;
    hold_behavior_t     hold     = handled_key_single_hold(key);
    bool                implicit = handled_key_uses_implicit_pd_mode_hold(key);
    pd_mode_mask_t      mode     = pd_mode_for_keycode(keycode);

    if (handled_key_multi_tap_repress(key, keycode)) {
        uint16_t action = handled_key_advance_multi_tap(keycode);
        if (action != KC_NO) {
            key_runtime_transition_plan_dispatch_action(plan, action);
        }

        if (behavior.is_momentary_layer) {
            key_runtime_transition_plan_layer_press(plan, record->event.key, behavior_get_layer(keycode));
        }

        bool pending_hold = multi_tap_pending_hold(&multi_tap);
        if (pending_hold || behavior.is_momentary_layer) {
            active_key_track(keycode, record->event.key, KC_NO, hold_behavior_none(), hold_behavior_none(), behavior.tap_hold_term, behavior.longer_hold_term, behavior.multi_tap_term, !pending_hold);
        }
        return true;
    }

    if (behavior.is_momentary_layer) {
        key_runtime_transition_plan_layer_press(plan, record->event.key, behavior_get_layer(keycode));
    }

    key_runtime_transition_flush_active_key(active_held_action_survives_flush, plan);
    active_key_track(keycode, record->event.key, handled_key_tap_action(key), hold, behavior.single.long_hold, behavior.tap_hold_term, behavior.longer_hold_term, behavior.multi_tap_term, false);
    active_key.implicit_pd_mode_hold       = implicit;
    active_key.pd_mode_was_locked_on_press = mode && pd_mode_locked(mode);
    key_runtime_transition_activate_immediate_hold_if_needed(record, hold, plan);
    return true;
}

static uint16_t key_runtime_transition_select_release_hold_action(uint16_t elapsed, uint16_t hold_action, hold_behavior_t long_hold, uint16_t longer_hold_term) {
    if (hold_sends_on_release(long_hold) && elapsed >= longer_hold_term) {
        return long_hold.action;
    }
    return hold_action;
}

static void key_runtime_transition_dispatch_released_key_tap(uint16_t keycode, active_key_state_t released_key, key_behavior_view_t behavior, key_runtime_transition_plan_t *plan) {
    if (behavior.has_multi_tap) {
        multi_tap_begin(&multi_tap, keycode, released_key.tap_action, released_key.tap_hold_term, released_key.multi_tap_term);
    } else if (released_key.tap_action != KC_NO) {
        key_runtime_transition_plan_dispatch_action(plan, released_key.tap_action);
    }
}

static bool key_runtime_transition_queue_locked_pd_mode_tap_if_needed(uint16_t keycode, active_key_state_t released_key, uint16_t elapsed, key_behavior_view_t behavior, key_runtime_transition_plan_t *plan) {
    pd_mode_mask_t mode = pd_mode_for_keycode(keycode);
    if (!mode) {
        return false;
    }

    if (!released_key.pd_mode_was_locked_on_press || elapsed >= released_key.tap_hold_term) {
        return false;
    }

    if (behavior.is_momentary_layer && released_key.layer_interrupted) {
        return false;
    }

    key_runtime_transition_plan_pd_mode_lock_tap(plan, mode);
    return true;
}

static void key_runtime_transition_flush_multi_tap_impl(key_runtime_transition_plan_t *plan) {
    delayed_action_mods_t mods = delayed_action_mods_from_multi_tap(&multi_tap);

    if (multi_tap.pending_hold) {
        if (multi_tap.tap_action != KC_NO) {
            key_runtime_transition_plan_delayed_action(plan, multi_tap.tap_action, mods, 1);
        } else {
            key_runtime_transition_plan_delayed_action(plan, multi_tap.single_action, mods, multi_tap.count);
        }
        multi_tap_reset(&multi_tap);
        return;
    }

    if (multi_tap.count >= 2) {
        key_behavior_step_t step = key_behavior_step_lookup(multi_tap.keycode, multi_tap.count);
        if (step.tap.present && step.tap.action != KC_NO) {
            key_runtime_transition_plan_delayed_action(plan, step.tap.action, mods, 1);
            multi_tap_reset(&multi_tap);
            return;
        }
    }

    key_runtime_transition_plan_delayed_action(plan, multi_tap.single_action, mods, multi_tap.count);
    multi_tap_reset(&multi_tap);
}

void key_runtime_transition_flush_multi_tap(key_runtime_transition_plan_t *plan) {
    if (!multi_tap_active(&multi_tap)) {
        return;
    }

    key_runtime_transition_flush_multi_tap_impl(plan);
}

static bool key_runtime_transition_process_pending_multi_tap_hold_release(uint16_t keycode, key_behavior_view_t behavior, key_runtime_transition_plan_t *plan) {
    if (!(multi_tap_pending_hold(&multi_tap) && multi_tap.keycode == keycode)) {
        return false;
    }

    uint16_t              elapsed          = timer_elapsed(active_key.timer);
    delayed_action_mods_t cached_mods      = delayed_action_mods_from_multi_tap(&multi_tap);
    bool                  was_release_hold = hold_sends_on_release(multi_tap.hold);
    hold_behavior_t       cached_hold      = multi_tap.hold;
    hold_behavior_t       cached_long_hold = multi_tap.long_hold;
    uint8_t               repeat_count     = 0;
    uint16_t              action           = multi_tap_resolve_hold(&multi_tap, keycode, key_behavior_has_more_taps, &repeat_count);

    if (!cached_hold.present && hold_sends_on_release(cached_long_hold) && elapsed >= active_key.longer_hold_term) {
        action = cached_long_hold.action;
    } else if (was_release_hold && cached_hold.present && repeat_count == 1 && action == cached_hold.action) {
        action = key_runtime_transition_select_release_hold_action(elapsed, cached_hold.action, cached_long_hold, active_key.longer_hold_term);
    }

    key_runtime_transition_plan_delayed_action(plan, action, cached_mods, repeat_count);

    if (behavior.is_momentary_layer) {
        key_runtime_transition_plan_layer_release(plan, active_key.key_pos);
    }
    active_key_reset();
    return true;
}

static bool key_runtime_transition_process_active_key_release(uint16_t keycode, keyrecord_t *record, key_behavior_view_t behavior, key_runtime_transition_plan_t *plan) {
    if (behavior.is_momentary_layer) {
        key_runtime_transition_plan_layer_release(plan, record->event.key);
    }

    if (!active_key_matches(keycode, record->event.key)) {
        key_runtime_transition_plan_release_held_owned_by_key(plan, record->event.key);
        return true;
    }

    active_key_state_t released_key = active_key;
    active_key_reset();

    uint16_t elapsed              = timer_elapsed(released_key.timer);
    bool     quick_immediate_hold = hold_registers_on_press(released_key.hold) && elapsed < released_key.tap_hold_term && !(behavior.is_momentary_layer && released_key.layer_interrupted);

    if (released_key.hold_fired || released_key.held_action_keycode != KC_NO) {
        if (released_key.held_action_keycode != KC_NO) {
            key_runtime_transition_plan_held_unregister(plan, released_key.key_pos, released_key.held_action_keycode);
        }

        if (key_runtime_transition_queue_locked_pd_mode_tap_if_needed(keycode, released_key, elapsed, behavior, plan)) {
            return true;
        }

        if (quick_immediate_hold) {
            key_runtime_transition_dispatch_released_key_tap(keycode, released_key, behavior, plan);
            return true;
        }

        if (hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term) {
            key_runtime_transition_plan_dispatch_action(plan, released_key.long_hold.action);
        }
        return true;
    }

    if (elapsed < released_key.tap_hold_term && !(behavior.is_momentary_layer && released_key.layer_interrupted)) {
        if (key_runtime_transition_queue_locked_pd_mode_tap_if_needed(keycode, released_key, elapsed, behavior, plan)) {
            return true;
        }
        key_runtime_transition_dispatch_released_key_tap(keycode, released_key, behavior, plan);
        return true;
    }

    if (hold_sends_on_release(released_key.hold)) {
        key_runtime_transition_plan_dispatch_action(plan, key_runtime_transition_select_release_hold_action(elapsed, released_key.hold.action, released_key.long_hold, released_key.longer_hold_term));
    } else if (hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term) {
        key_runtime_transition_plan_dispatch_action(plan, released_key.long_hold.action);
    } else if (!released_key.hold_one_shot_fired && !behavior.is_momentary_layer && released_key.tap_action != KC_NO) {
        key_runtime_transition_plan_dispatch_action(plan, released_key.tap_action);
    }

    return true;
}

bool key_runtime_transition_handled_key_release(uint16_t keycode, keyrecord_t *record, handled_key_view_t key, key_runtime_transition_plan_t *plan) {
    if (key_runtime_transition_process_pending_multi_tap_hold_release(keycode, key.behavior, plan)) {
        return true;
    }

    return key_runtime_transition_process_active_key_release(keycode, record, key.behavior, plan);
}

static hold_threshold_dispatch_t key_runtime_transition_hold_threshold_dispatch_kind(hold_behavior_t hold) {
    if (!hold.present) {
        return HOLD_THRESHOLD_DISPATCH_NONE;
    }

    switch (hold.mode) {
        case HOLD_BEHAVIOR_TAP_AT_HOLD_THRESHOLD:
            return HOLD_THRESHOLD_DISPATCH_TAP;
        case HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE:
            return noah_action_hold_kind(hold.action) == NOAH_ACTION_HOLD_KIND_PRESS_ONLY ? HOLD_THRESHOLD_DISPATCH_TAP : HOLD_THRESHOLD_DISPATCH_HELD;
        default:
            return HOLD_THRESHOLD_DISPATCH_NONE;
    }
}

static bool key_runtime_transition_hold_activation_needs_pulse(hold_behavior_t hold) {
    return action_dispatch_is_layer_action(hold.action);
}

static void key_runtime_transition_clear_active_held_action(key_runtime_transition_plan_t *plan) {
    if (active_key.held_action_keycode != KC_NO) {
        key_runtime_transition_plan_held_unregister(plan, active_key.key_pos, active_key.held_action_keycode);
        active_key.held_action_keycode = KC_NO;
    }
}

static void key_runtime_transition_fire_hold_at_threshold(hold_behavior_t hold, hold_behavior_t long_hold, key_runtime_transition_plan_t *plan) {
    switch (key_runtime_transition_hold_threshold_dispatch_kind(hold)) {
        case HOLD_THRESHOLD_DISPATCH_TAP:
            key_runtime_transition_clear_active_held_action(plan);
            key_runtime_transition_plan_dispatch_action(plan, hold.action);
            key_runtime_transition_plan_feedback_pulse(plan, false);
            if (long_hold.present) {
                active_key.hold_fired          = false;
                active_key.hold_one_shot_fired = true;
            } else {
                active_key.hold_fired          = true;
                active_key.hold_one_shot_fired = false;
            }
            return;
        case HOLD_THRESHOLD_DISPATCH_HELD:
            key_runtime_transition_plan_held_register(plan, active_key.key_pos, hold.action);
            active_key.held_action_keycode = hold.action;
            active_key.hold_fired          = !long_hold.present;
            active_key.hold_one_shot_fired = false;
            if (key_runtime_transition_hold_activation_needs_pulse(hold)) {
                key_runtime_transition_plan_feedback_pulse(plan, false);
            }
            return;
        case HOLD_THRESHOLD_DISPATCH_NONE:
            return;
    }
}

static void key_runtime_transition_promote_to_long_hold(hold_behavior_t long_hold, key_runtime_transition_plan_t *plan) {
    key_runtime_transition_clear_active_held_action(plan);

    switch (key_runtime_transition_hold_threshold_dispatch_kind(long_hold)) {
        case HOLD_THRESHOLD_DISPATCH_TAP:
            key_runtime_transition_plan_dispatch_action(plan, long_hold.action);
            key_runtime_transition_plan_feedback_pulse(plan, true);
            active_key.hold_fired = true;
            return;
        case HOLD_THRESHOLD_DISPATCH_HELD:
            key_runtime_transition_plan_held_register(plan, active_key.key_pos, long_hold.action);
            active_key.held_action_keycode = long_hold.action;
            active_key.hold_fired          = true;
            if (key_runtime_transition_hold_activation_needs_pulse(long_hold)) {
                key_runtime_transition_plan_feedback_pulse(plan, true);
            }
            return;
        case HOLD_THRESHOLD_DISPATCH_NONE:
            return;
    }
}

static void key_runtime_transition_deactivate_pending_multi_tap_layer_before_lock(uint16_t action, key_runtime_transition_plan_t *plan) {
    if (is_layer_key(active_key.keycode) && action_dispatch_is_layer_lock(action)) {
        key_runtime_transition_plan_layer_release(plan, active_key.key_pos);
    }
}

static void key_runtime_transition_commit_immediate_hold_threshold(key_runtime_transition_plan_t *plan) {
    if (!hold_registers_on_press(active_key.hold) || active_key.hold_one_shot_fired || timer_elapsed(active_key.timer) < active_key.tap_hold_term) {
        return;
    }

    if (!active_key.implicit_pd_mode_hold) {
        key_runtime_transition_plan_feedback_pulse(plan, false);
    }
    active_key.hold_one_shot_fired = true;
    if (!active_key.long_hold.present) {
        active_key.hold_fired = true;
    }
}

void key_runtime_transition_scan(key_runtime_transition_plan_t *plan) {
    if (active_key.keycode != KC_NO && !active_key.hold_fired) {
        key_runtime_transition_commit_immediate_hold_threshold(plan);

        uint16_t elapsed = timer_elapsed(active_key.timer);
        if (hold_fires_at_threshold(active_key.long_hold) && elapsed >= active_key.longer_hold_term) {
            key_runtime_transition_promote_to_long_hold(active_key.long_hold, plan);
        } else if (hold_fires_at_threshold(active_key.hold) && elapsed >= active_key.tap_hold_term) {
            key_runtime_transition_fire_hold_at_threshold(active_key.hold, active_key.long_hold, plan);
        }
    }

    if (multi_tap_pending_hold(&multi_tap) && active_key.keycode != KC_NO) {
        uint16_t elapsed = timer_elapsed(multi_tap.timer);

        if (hold_fires_at_threshold(multi_tap.long_hold) && elapsed >= active_key.longer_hold_term) {
            key_runtime_transition_deactivate_pending_multi_tap_layer_before_lock(multi_tap.long_hold.action, plan);
            active_key.long_hold = multi_tap.long_hold;
            key_runtime_transition_promote_to_long_hold(active_key.long_hold, plan);
            multi_tap_reset(&multi_tap);
        } else if (multi_tap_hold_elapsed(&multi_tap)) {
            key_runtime_transition_deactivate_pending_multi_tap_layer_before_lock(multi_tap.hold.action, plan);
            active_key.long_hold = multi_tap.long_hold;
            key_runtime_transition_fire_hold_at_threshold(multi_tap.hold, active_key.long_hold, plan);
            multi_tap_reset(&multi_tap);
        }
    }

    if (multi_tap_expired(&multi_tap)) {
        key_runtime_transition_flush_multi_tap_impl(plan);
    }
}
