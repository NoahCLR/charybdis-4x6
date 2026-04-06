// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Transitions
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_transition.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

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

typedef enum {
    ACTIVE_KEY_RELEASE_OUTCOME_NONE = 0,
    ACTIVE_KEY_RELEASE_OUTCOME_TAP,
    ACTIVE_KEY_RELEASE_OUTCOME_ACTION,
    ACTIVE_KEY_RELEASE_OUTCOME_PD_MODE_LOCK_TAP,
} active_key_release_outcome_t;

typedef struct {
    bool                        unregister_held_action;
    active_key_release_outcome_t outcome;
    uint16_t                    action;
    pd_mode_mask_t              pd_mode_lock_tap;
} active_key_release_resolution_t;

typedef enum {
    ACTIVE_KEY_SCAN_OUTCOME_NONE = 0,
    ACTIVE_KEY_SCAN_OUTCOME_FIRE_HOLD,
    ACTIVE_KEY_SCAN_OUTCOME_PROMOTE_LONG_HOLD,
} active_key_scan_outcome_t;

typedef struct {
    bool                    commit_immediate_hold;
    bool                    immediate_hold_needs_feedback;
    bool                    immediate_hold_completes_hold;
    bool                    activate_fallback_hold;
    active_key_scan_outcome_t outcome;
    hold_behavior_t         hold;
    hold_behavior_t         long_hold;
} active_key_scan_resolution_t;

typedef enum {
    PENDING_MULTI_TAP_SCAN_OUTCOME_NONE = 0,
    PENDING_MULTI_TAP_SCAN_OUTCOME_FIRE_HOLD,
    PENDING_MULTI_TAP_SCAN_OUTCOME_PROMOTE_LONG_HOLD,
} pending_multi_tap_scan_outcome_t;

typedef struct {
    pending_multi_tap_scan_outcome_t outcome;
    bool                             release_layer_before_action;
    hold_behavior_t                  hold;
    hold_behavior_t                  long_hold;
} pending_multi_tap_scan_resolution_t;

static hold_threshold_dispatch_t key_runtime_transition_hold_threshold_dispatch_kind(hold_behavior_t hold);

void key_runtime_transition_plan_init(key_runtime_transition_plan_t *plan) {
    *plan = (key_runtime_transition_plan_t){0};
}

static void key_runtime_transition_log_plan_overflow(key_runtime_transition_effect_kind_t kind, uint8_t capacity) {
#ifdef CONSOLE_ENABLE
    uprintf("Key runtime transition plan overflow dropping effect kind %u after %u queued effects\n", (unsigned int)kind, (unsigned int)capacity);
#else
    (void)kind;
    (void)capacity;
#endif
}

static void key_runtime_transition_plan_push(key_runtime_transition_plan_t *plan, key_runtime_transition_effect_t effect) {
    if (plan->count < ARRAY_SIZE(plan->effects)) {
        plan->effects[plan->count++] = effect;
        return;
    }

    if (!plan->overflowed) {
        plan->overflowed = true;
        key_runtime_transition_log_plan_overflow(effect.kind, ARRAY_SIZE(plan->effects));
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

static void key_runtime_transition_activate_pending_fallback_hold(key_runtime_transition_plan_t *plan) {
    if (!active_key.fallback_hold_pending || active_key.held_action_keycode != KC_NO || active_key.keycode == KC_NO) {
        return;
    }

    key_runtime_transition_plan_held_register(plan, active_key.key_pos, active_key.keycode);
    active_key.held_action_keycode = active_key.keycode;
    active_key.hold_fired          = true;
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
                if (pd_mode_toggle_lock_state(effect->data.pd_mode)) {
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
    bool                implicit = handled_key_uses_implicit_hold(key);
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
    active_key.implicit_hold = implicit;
    active_key.fallback_hold_pending = !implicit && handled_key_uses_fallback_hold(key);
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

static bool key_runtime_transition_release_is_interrupted_layer_tap(active_key_state_t released_key, key_behavior_view_t behavior) {
    return behavior.is_momentary_layer && released_key.layer_interrupted;
}

static bool key_runtime_transition_release_is_buffered_base_tap(active_key_state_t released_key) {
    return released_key.fallback_hold_pending && released_key.tap_action == KC_NO && released_key.held_action_keycode == KC_NO;
}

static bool key_runtime_transition_release_is_quick_tap(active_key_state_t released_key, key_behavior_view_t behavior, uint16_t elapsed) {
    return elapsed < released_key.tap_hold_term && !key_runtime_transition_release_is_interrupted_layer_tap(released_key, behavior);
}

static pd_mode_mask_t key_runtime_transition_locked_pd_mode_tap_mode(uint16_t keycode, active_key_state_t released_key, uint16_t elapsed, key_behavior_view_t behavior) {
    pd_mode_mask_t mode = pd_mode_for_keycode(keycode);
    if (!mode) {
        return 0;
    }

    if (!released_key.pd_mode_was_locked_on_press || elapsed >= released_key.tap_hold_term) {
        return 0;
    }

    if (key_runtime_transition_release_is_interrupted_layer_tap(released_key, behavior)) {
        return 0;
    }

    return mode;
}

static active_key_release_resolution_t key_runtime_transition_resolve_active_key_release(uint16_t keycode, active_key_state_t released_key, key_behavior_view_t behavior, uint16_t elapsed) {
    bool                  quick_tap            = key_runtime_transition_release_is_quick_tap(released_key, behavior, elapsed);
    bool                  quick_immediate_hold = hold_registers_on_press(released_key.hold) && quick_tap;
    pd_mode_mask_t        lock_tap_mode        = key_runtime_transition_locked_pd_mode_tap_mode(keycode, released_key, elapsed, behavior);
    active_key_release_resolution_t resolution = {
        .unregister_held_action = released_key.held_action_keycode != KC_NO,
    };

    if (released_key.hold_fired || released_key.held_action_keycode != KC_NO) {
        if (lock_tap_mode) {
            resolution.outcome          = ACTIVE_KEY_RELEASE_OUTCOME_PD_MODE_LOCK_TAP;
            resolution.pd_mode_lock_tap = lock_tap_mode;
            return resolution;
        }

        if (quick_immediate_hold) {
            resolution.outcome = ACTIVE_KEY_RELEASE_OUTCOME_TAP;
            return resolution;
        }

        if (hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term) {
            resolution.outcome = ACTIVE_KEY_RELEASE_OUTCOME_ACTION;
            resolution.action  = released_key.long_hold.action;
        }
        return resolution;
    }

    if (key_runtime_transition_release_is_buffered_base_tap(released_key)) {
        resolution.outcome = ACTIVE_KEY_RELEASE_OUTCOME_TAP;
        return resolution;
    }

    if (quick_tap) {
        if (lock_tap_mode) {
            resolution.outcome          = ACTIVE_KEY_RELEASE_OUTCOME_PD_MODE_LOCK_TAP;
            resolution.pd_mode_lock_tap = lock_tap_mode;
            return resolution;
        }

        resolution.outcome = ACTIVE_KEY_RELEASE_OUTCOME_TAP;
        return resolution;
    }

    if (released_key.fallback_hold_pending) {
        return resolution;
    }

    if (hold_sends_on_release(released_key.hold)) {
        resolution.outcome = ACTIVE_KEY_RELEASE_OUTCOME_ACTION;
        resolution.action  = key_runtime_transition_select_release_hold_action(elapsed, released_key.hold.action, released_key.long_hold, released_key.longer_hold_term);
        return resolution;
    }

    if (hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term) {
        resolution.outcome = ACTIVE_KEY_RELEASE_OUTCOME_ACTION;
        resolution.action  = released_key.long_hold.action;
        return resolution;
    }

    if (!released_key.hold_one_shot_fired && !behavior.is_momentary_layer && released_key.tap_action != KC_NO) {
        resolution.outcome = ACTIVE_KEY_RELEASE_OUTCOME_TAP;
    }

    return resolution;
}

static void key_runtime_transition_apply_active_key_release_resolution(uint16_t keycode, active_key_state_t released_key, key_behavior_view_t behavior, active_key_release_resolution_t resolution, key_runtime_transition_plan_t *plan) {
    if (resolution.unregister_held_action) {
        key_runtime_transition_plan_held_unregister(plan, released_key.key_pos, released_key.held_action_keycode);
    }

    switch (resolution.outcome) {
        case ACTIVE_KEY_RELEASE_OUTCOME_TAP:
            key_runtime_transition_dispatch_released_key_tap(keycode, released_key, behavior, plan);
            return;
        case ACTIVE_KEY_RELEASE_OUTCOME_ACTION:
            key_runtime_transition_plan_dispatch_action(plan, resolution.action);
            return;
        case ACTIVE_KEY_RELEASE_OUTCOME_PD_MODE_LOCK_TAP:
            key_runtime_transition_plan_pd_mode_lock_tap(plan, resolution.pd_mode_lock_tap);
            return;
        case ACTIVE_KEY_RELEASE_OUTCOME_NONE:
        default:
            return;
    }
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

void key_runtime_transition_interrupt_active_key_on_other_press(key_runtime_transition_plan_t *plan) {
    if (active_key.keycode == KC_NO) {
        return;
    }

    key_runtime_transition_activate_pending_fallback_hold(plan);

    if (is_layer_key(active_key.keycode)) {
        active_key.layer_interrupted = true;
    }
}

static bool key_runtime_transition_pending_multi_tap_release_uses_held_lifecycle(hold_behavior_t hold, uint16_t action, uint8_t repeat_count, uint16_t elapsed) {
    if (repeat_count != 1 || elapsed < active_key.tap_hold_term) {
        return false;
    }

    if (hold.mode != HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE || action != hold.action) {
        return false;
    }

    return key_runtime_transition_hold_threshold_dispatch_kind(hold) == HOLD_THRESHOLD_DISPATCH_HELD;
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

    if (key_runtime_transition_pending_multi_tap_release_uses_held_lifecycle(cached_hold, action, repeat_count, elapsed)) {
        key_runtime_transition_plan_held_register(plan, active_key.key_pos, action);
        key_runtime_transition_plan_held_unregister(plan, active_key.key_pos, action);
    } else {
        key_runtime_transition_plan_delayed_action(plan, action, cached_mods, repeat_count);
    }

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

    uint16_t                           elapsed    = timer_elapsed(released_key.timer);
    active_key_release_resolution_t resolution = key_runtime_transition_resolve_active_key_release(keycode, released_key, behavior, elapsed);

    key_runtime_transition_apply_active_key_release_resolution(keycode, released_key, behavior, resolution, plan);

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

static active_key_scan_resolution_t key_runtime_transition_resolve_active_key_scan(active_key_state_t active_key_state, uint16_t elapsed) {
    active_key_scan_resolution_t resolution = {0};

    if (hold_registers_on_press(active_key_state.hold) && !active_key_state.hold_one_shot_fired && elapsed >= active_key_state.tap_hold_term) {
        resolution.commit_immediate_hold         = true;
        resolution.immediate_hold_needs_feedback = !active_key_state.implicit_hold;
        resolution.immediate_hold_completes_hold = !active_key_state.long_hold.present;
    }

    if (active_key_state.fallback_hold_pending && active_key_state.held_action_keycode == KC_NO && elapsed >= active_key_state.tap_hold_term) {
        resolution.activate_fallback_hold = true;
        return resolution;
    }

    if (hold_fires_at_threshold(active_key_state.long_hold) && elapsed >= active_key_state.longer_hold_term) {
        resolution.outcome   = ACTIVE_KEY_SCAN_OUTCOME_PROMOTE_LONG_HOLD;
        resolution.long_hold = active_key_state.long_hold;
        return resolution;
    }

    if (hold_fires_at_threshold(active_key_state.hold) && elapsed >= active_key_state.tap_hold_term) {
        resolution.outcome   = ACTIVE_KEY_SCAN_OUTCOME_FIRE_HOLD;
        resolution.hold      = active_key_state.hold;
        resolution.long_hold = active_key_state.long_hold;
    }

    return resolution;
}

static void key_runtime_transition_apply_active_key_scan_resolution(active_key_scan_resolution_t resolution, key_runtime_transition_plan_t *plan) {
    if (resolution.commit_immediate_hold) {
        if (resolution.immediate_hold_needs_feedback) {
            key_runtime_transition_plan_feedback_pulse(plan, false);
        }
        active_key.hold_one_shot_fired = true;
        if (resolution.immediate_hold_completes_hold) {
            active_key.hold_fired = true;
        }
    }

    if (resolution.activate_fallback_hold) {
        key_runtime_transition_activate_pending_fallback_hold(plan);
        return;
    }

    switch (resolution.outcome) {
        case ACTIVE_KEY_SCAN_OUTCOME_FIRE_HOLD:
            key_runtime_transition_fire_hold_at_threshold(resolution.hold, resolution.long_hold, plan);
            return;
        case ACTIVE_KEY_SCAN_OUTCOME_PROMOTE_LONG_HOLD:
            key_runtime_transition_promote_to_long_hold(resolution.long_hold, plan);
            return;
        case ACTIVE_KEY_SCAN_OUTCOME_NONE:
        default:
            return;
    }
}

static bool key_runtime_transition_pending_multi_tap_hold_elapsed(const multi_tap_t *multi_tap_state, uint16_t elapsed) {
    return multi_tap_state->pending_hold && hold_fires_at_threshold(multi_tap_state->hold) && elapsed >= multi_tap_state->tap_hold_term;
}

static pending_multi_tap_scan_resolution_t key_runtime_transition_resolve_pending_multi_tap_scan(active_key_state_t active_key_state, multi_tap_t multi_tap_state, uint16_t elapsed) {
    pending_multi_tap_scan_resolution_t resolution = {0};

    if (!multi_tap_state.pending_hold || active_key_state.keycode == KC_NO) {
        return resolution;
    }

    if (hold_fires_at_threshold(multi_tap_state.long_hold) && elapsed >= active_key_state.longer_hold_term) {
        resolution.outcome                     = PENDING_MULTI_TAP_SCAN_OUTCOME_PROMOTE_LONG_HOLD;
        resolution.long_hold                   = multi_tap_state.long_hold;
        resolution.release_layer_before_action = is_layer_key(active_key_state.keycode) && action_dispatch_is_layer_lock(multi_tap_state.long_hold.action);
        return resolution;
    }

    if (key_runtime_transition_pending_multi_tap_hold_elapsed(&multi_tap_state, elapsed)) {
        resolution.outcome                     = PENDING_MULTI_TAP_SCAN_OUTCOME_FIRE_HOLD;
        resolution.hold                        = multi_tap_state.hold;
        resolution.long_hold                   = multi_tap_state.long_hold;
        resolution.release_layer_before_action = is_layer_key(active_key_state.keycode) && action_dispatch_is_layer_lock(multi_tap_state.hold.action);
    }

    return resolution;
}

static void key_runtime_transition_apply_pending_multi_tap_scan_resolution(pending_multi_tap_scan_resolution_t resolution, key_runtime_transition_plan_t *plan) {
    if (resolution.release_layer_before_action) {
        key_runtime_transition_plan_layer_release(plan, active_key.key_pos);
    }

    switch (resolution.outcome) {
        case PENDING_MULTI_TAP_SCAN_OUTCOME_PROMOTE_LONG_HOLD:
            active_key.long_hold = resolution.long_hold;
            key_runtime_transition_promote_to_long_hold(active_key.long_hold, plan);
            multi_tap_reset(&multi_tap);
            return;
        case PENDING_MULTI_TAP_SCAN_OUTCOME_FIRE_HOLD:
            active_key.long_hold = resolution.long_hold;
            key_runtime_transition_fire_hold_at_threshold(resolution.hold, active_key.long_hold, plan);
            multi_tap_reset(&multi_tap);
            return;
        case PENDING_MULTI_TAP_SCAN_OUTCOME_NONE:
        default:
            return;
    }
}

void key_runtime_transition_scan(key_runtime_transition_plan_t *plan) {
    if (active_key.keycode != KC_NO && !active_key.hold_fired) {
        uint16_t                     elapsed     = timer_elapsed(active_key.timer);
        active_key_scan_resolution_t resolution = key_runtime_transition_resolve_active_key_scan(active_key, elapsed);

        key_runtime_transition_apply_active_key_scan_resolution(resolution, plan);
    }

    if (multi_tap_pending_hold(&multi_tap) && active_key.keycode != KC_NO) {
        uint16_t                           elapsed     = timer_elapsed(multi_tap.timer);
        pending_multi_tap_scan_resolution_t resolution = key_runtime_transition_resolve_pending_multi_tap_scan(active_key, multi_tap, elapsed);

        key_runtime_transition_apply_pending_multi_tap_scan_resolution(resolution, plan);
    }

    if (multi_tap_expired(&multi_tap)) {
        key_runtime_transition_flush_multi_tap_impl(plan);
    }
}
