// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Transitions
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_transition.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "key_runtime_feedback.h"
#include "key_runtime_state.h"
#include "key_runtime_trace.h"
#include "../action/action_dispatch.h"
#include "../action/action_lifecycle.h"
#include "../pointing/pd_modes.h"
#include "../state/layer_ownership.h"
#include "../state/split_runtime_sync.h"
#include "held_action.h"

typedef enum {
    HOLD_THRESHOLD_DISPATCH_NONE = 0,
    HOLD_THRESHOLD_DISPATCH_TAP,
    HOLD_THRESHOLD_DISPATCH_HELD,
    HOLD_THRESHOLD_DISPATCH_REPEAT,
} hold_threshold_dispatch_t;

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

static void key_runtime_transition_plan_release_owned_state_by_key(key_runtime_transition_plan_t *plan, keypos_t key_pos) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                               .kind         = KEY_RUNTIME_TRANSITION_EFFECT_RELEASE_OWNED_STATE_BY_KEY,
                                               .data.key_pos = key_pos,
                                           });
}

static void key_runtime_transition_plan_repeat_start(key_runtime_transition_plan_t *plan, keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
    key_runtime_transition_plan_push(plan, (key_runtime_transition_effect_t){
                                               .kind = KEY_RUNTIME_TRANSITION_EFFECT_REPEAT_START,
                                               .data.repeat =
                                                   {
                                                       .key_pos   = key_pos,
                                                       .action    = action,
                                                       .repeat_hz = repeat_hz,
                                                   },
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

static void key_runtime_transition_activate_pending_fallback_hold(active_key_state_t *slot, key_runtime_transition_plan_t *plan) {
    if (!slot || !slot->fallback_hold_pending || slot->held_action_keycode != KC_NO || slot->keycode == KC_NO) {
        return;
    }

    key_runtime_transition_plan_held_register(plan, slot->key_pos, slot->keycode);
    slot->held_action_keycode = slot->keycode;
    slot->hold_fired          = true;
}

static active_key_state_t *key_runtime_transition_select_press_slot(keypos_t key_pos) {
    return key_runtime_select_slot_for_press(key_pos);
}

static void key_runtime_transition_flush_slot_pending_multi_tap(active_key_state_t *slot, key_runtime_transition_plan_t *plan) {
    multi_tap_t *mt = key_runtime_multi_tap_for_slot(slot);

    if (!key_runtime_slot_has_pending_multi_tap(slot)) {
        return;
    }

    delayed_action_mods_t mods = delayed_action_mods_from_multi_tap(mt);

    if (mt->pending_hold) {
        if (mt->tap_action != KC_NO) {
            key_runtime_transition_plan_delayed_action(plan, mt->tap_action, mods, 1);
        } else {
            key_runtime_transition_plan_delayed_action(plan, mt->single_action, mods, mt->count);
        }
        key_runtime_slot_reset_pending_multi_tap(slot);
        return;
    }

    if (mt->count >= 2) {
        key_behavior_step_t step = key_behavior_step_lookup(mt->keycode, mt->count);
        if (step.tap.present && step.tap.action != KC_NO) {
            key_runtime_transition_plan_delayed_action(plan, step.tap.action, mods, 1);
            key_runtime_slot_reset_pending_multi_tap(slot);
            return;
        }
    }

    key_runtime_transition_plan_delayed_action(plan, mt->single_action, mods, mt->count);
    key_runtime_slot_reset_pending_multi_tap(slot);
}

void key_runtime_transition_execute_plan(const key_runtime_transition_plan_t *plan) {
    for (uint8_t i = 0; i < plan->count; i++) {
        const key_runtime_transition_effect_t *effect = &plan->effects[i];
        key_runtime_trace_effect_execute(i, effect);

        switch (effect->kind) {
            case KEY_RUNTIME_TRANSITION_EFFECT_DISPATCH_ACTION:
                action_dispatch(effect->data.action);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_REGISTER:
                held_action_register(effect->data.held_action.key_pos, effect->data.held_action.action);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_UNREGISTER:
                held_action_unregister(effect->data.held_action.key_pos, effect->data.held_action.action);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_RELEASE_OWNED_STATE_BY_KEY:
                held_action_release_owned_by_key(effect->data.key_pos);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_REPEAT_START:
                held_action_repeat_start(effect->data.repeat.key_pos, effect->data.repeat.action, effect->data.repeat.repeat_hz);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_LAYER_PRESS:
                layer_ownership_momentary_press(effect->data.layer_press.key_pos, effect->data.layer_press.layer);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_LAYER_RELEASE:
                layer_ownership_momentary_release(effect->data.key_pos);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_FEEDBACK_PULSE:
                key_feedback_pulse_arm(effect->data.long_hold_level);
                break;
            case KEY_RUNTIME_TRANSITION_EFFECT_PD_MODE_LOCK_TAP:
                if (pd_mode_toggle_lock_state(effect->data.pd_mode)) {
                    split_runtime_sync();
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

static void key_runtime_transition_flush_active_key(active_key_state_t *slot, bool active_held_action_survives_flush, key_runtime_transition_plan_t *plan) {
    if (!key_runtime_slot_active(slot)) return;

    if (slot->hold_fired || slot->held_action_keycode != KC_NO || slot->repeat_binding_active) {
        slot->hold_fired = false;
        if (slot->held_action_keycode != KC_NO && !active_held_action_survives_flush) {
            key_runtime_transition_plan_held_unregister(plan, slot->key_pos, slot->held_action_keycode);
            slot->held_action_keycode = KC_NO;
        }
        slot->repeat_binding_active = false;
    } else if (!is_layer_key(slot->keycode) && slot->tap_action != KC_NO) {
        key_runtime_transition_plan_dispatch_action(plan, slot->tap_action);
    }

    key_runtime_slot_reset(slot);
}

static void key_runtime_transition_activate_immediate_hold_if_needed(active_key_state_t *slot, keyrecord_t *record, hold_behavior_t hold, key_runtime_transition_plan_t *plan) {
    if (!hold_registers_on_press(hold)) {
        return;
    }

    key_runtime_transition_plan_held_register(plan, record->event.key, hold.action);
    slot->held_action_keycode = hold.action;
}

bool key_runtime_transition_handled_key_press(uint16_t keycode, keyrecord_t *record, handled_key_view_t key, bool active_held_action_survives_flush, key_runtime_transition_plan_t *plan) {
    active_key_state_t *slot          = key_runtime_transition_select_press_slot(record->event.key);
    key_behavior_view_t behavior      = key.behavior;
    hold_behavior_t     hold          = handled_key_single_hold(key);
    bool                implicit      = handled_key_uses_implicit_hold(key);
    pd_mode_mask_t      mode          = pd_mode_for_keycode(keycode);

    if (key.behavior.has_multi_tap && key_runtime_slot_pending_multi_tap_matches(slot, keycode, record->event.key)) {
        uint16_t action = key_runtime_slot_advance_pending_multi_tap(slot, keycode);
        if (action != KC_NO) {
            key_runtime_transition_plan_dispatch_action(plan, action);
        }

        if (behavior.is_momentary_layer) {
            key_runtime_transition_plan_layer_press(plan, record->event.key, behavior_get_layer(keycode));
        }

        bool pending_hold = key_runtime_slot_pending_multi_tap_pending_hold(slot);
        if (pending_hold || behavior.is_momentary_layer) {
            key_runtime_slot_track(slot, keycode, record->event.key, KC_NO, hold_behavior_none(), hold_behavior_none(), behavior.tap_hold_term, behavior.longer_hold_term, behavior.multi_tap_term, !pending_hold);
        }
        return true;
    }

    if (key_runtime_slot_has_pending_multi_tap(slot) && !key_runtime_slot_pending_multi_tap_matches(slot, keycode, record->event.key)) {
        key_runtime_transition_flush_slot_pending_multi_tap(slot, plan);
    }

    if (behavior.is_momentary_layer) {
        key_runtime_transition_plan_layer_press(plan, record->event.key, behavior_get_layer(keycode));
    }

    if (key_runtime_slot_active(slot) && !key_runtime_slot_matches(slot, keycode, record->event.key)) {
        key_runtime_transition_flush_active_key(slot, active_held_action_survives_flush, plan);
    }
    key_runtime_slot_track(slot, keycode, record->event.key, handled_key_tap_action(key), hold, behavior.single.long_hold, behavior.tap_hold_term, behavior.longer_hold_term, behavior.multi_tap_term, false);
    slot->implicit_hold               = implicit;
    slot->fallback_hold_pending       = !implicit && handled_key_uses_fallback_hold(key);
    slot->pd_mode_was_locked_on_press = mode && pd_mode_locked(mode);
    key_runtime_transition_activate_immediate_hold_if_needed(slot, record, hold, plan);
    return true;
}

static uint16_t key_runtime_transition_select_release_hold_action(uint16_t elapsed, uint16_t hold_action, hold_behavior_t long_hold, uint16_t longer_hold_term) {
    if (hold_sends_on_release(long_hold) && elapsed >= longer_hold_term) {
        return long_hold.action;
    }
    return hold_action;
}

static void key_runtime_transition_dispatch_released_key_tap(uint16_t keycode, active_key_state_t *slot, active_key_state_t released_key, key_behavior_view_t behavior, key_runtime_transition_plan_t *plan) {
    if (behavior.has_multi_tap) {
        key_runtime_slot_begin_pending_multi_tap(slot, keycode, released_key.key_pos, released_key.tap_action, released_key.tap_hold_term, released_key.multi_tap_term);
    } else if (released_key.tap_action != KC_NO) {
        key_runtime_transition_plan_dispatch_action(plan, released_key.tap_action);
    }
}

static void key_runtime_transition_apply_active_key_release_resolution(uint16_t keycode, active_key_state_t *slot, active_key_state_t released_key, key_behavior_view_t behavior, key_runtime_slot_release_resolution_t resolution, key_runtime_transition_plan_t *plan) {
    if (resolution.release_owned_state) {
        key_runtime_transition_plan_release_owned_state_by_key(plan, released_key.key_pos);
    }

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_TAP:
            key_runtime_transition_dispatch_released_key_tap(keycode, slot, released_key, behavior, plan);
            return;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_ACTION:
            key_runtime_transition_plan_dispatch_action(plan, resolution.action);
            return;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_PD_MODE_LOCK_TAP:
            key_runtime_transition_plan_pd_mode_lock_tap(plan, resolution.pd_mode_lock_tap);
            return;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_NONE:
        default:
            return;
    }
}

void key_runtime_transition_flush_multi_tap(key_runtime_transition_plan_t *plan) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (!key_runtime_slot_has_pending_multi_tap(slot)) {
            continue;
        }

        key_runtime_transition_flush_slot_pending_multi_tap(slot, plan);
    }
}

void key_runtime_transition_interrupt_active_keys_on_other_press(keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (!key_runtime_slot_active(slot) || key_runtime_keypos_equal(slot->key_pos, key_pos)) {
            continue;
        }

        key_runtime_transition_activate_pending_fallback_hold(slot, plan);

        if (is_layer_key(slot->keycode)) {
            slot->layer_interrupted = true;
        }
    }
}

void key_runtime_transition_interrupt_active_key_on_other_press(key_runtime_transition_plan_t *plan) {
    key_runtime_transition_interrupt_active_keys_on_other_press((keypos_t){0xFF, 0xFF}, plan);
}

static bool key_runtime_transition_pending_multi_tap_release_uses_held_lifecycle(const active_key_state_t *slot, hold_behavior_t hold, uint16_t action, uint8_t repeat_count, uint16_t elapsed) {
    if (repeat_count != 1 || elapsed < slot->tap_hold_term) {
        return false;
    }

    if (hold.mode != HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE || action != hold.action) {
        return false;
    }

    return key_runtime_transition_hold_threshold_dispatch_kind(hold) == HOLD_THRESHOLD_DISPATCH_HELD;
}

static bool key_runtime_transition_process_pending_multi_tap_hold_release(uint16_t keycode, keyrecord_t *record, key_behavior_view_t behavior, key_runtime_transition_plan_t *plan) {
    active_key_state_t *slot = key_runtime_find_slot_by_position(record->event.key);

    if (!(slot && key_runtime_slot_pending_multi_tap_pending_hold(slot) && key_runtime_slot_pending_multi_tap_matches(slot, keycode, record->event.key))) {
        return false;
    }

    multi_tap_t             *slot_multi_tap = key_runtime_multi_tap_for_slot(slot);
    uint16_t              elapsed          = timer_elapsed(slot->timer);
    delayed_action_mods_t cached_mods      = delayed_action_mods_from_multi_tap(slot_multi_tap);
    bool                  was_release_hold = hold_sends_on_release(slot_multi_tap->hold);
    hold_behavior_t       cached_hold      = slot_multi_tap->hold;
    hold_behavior_t       cached_long_hold = slot_multi_tap->long_hold;
    uint8_t               repeat_count     = 0;
    uint16_t              action           = key_runtime_slot_resolve_pending_multi_tap_hold(slot, keycode, &repeat_count);

    if (!cached_hold.present && hold_sends_on_release(cached_long_hold) && elapsed >= slot->longer_hold_term) {
        action = cached_long_hold.action;
    } else if (was_release_hold && cached_hold.present && repeat_count == 1 && action == cached_hold.action) {
        action = key_runtime_transition_select_release_hold_action(elapsed, cached_hold.action, cached_long_hold, slot->longer_hold_term);
    }

    if (key_runtime_transition_pending_multi_tap_release_uses_held_lifecycle(slot, cached_hold, action, repeat_count, elapsed)) {
        key_runtime_transition_plan_held_register(plan, slot->key_pos, action);
        key_runtime_transition_plan_held_unregister(plan, slot->key_pos, action);
    } else {
        key_runtime_transition_plan_delayed_action(plan, action, cached_mods, repeat_count);
    }

    if (behavior.is_momentary_layer) {
        key_runtime_transition_plan_layer_release(plan, slot->key_pos);
    }
    key_runtime_slot_reset(slot);
    return true;
}

static bool key_runtime_transition_process_active_key_release(uint16_t keycode, keyrecord_t *record, key_behavior_view_t behavior, key_runtime_transition_plan_t *plan) {
    active_key_state_t *slot = key_runtime_find_slot_by_position(record->event.key);

    if (behavior.is_momentary_layer) {
        key_runtime_transition_plan_layer_release(plan, record->event.key);
    }

    if (!key_runtime_slot_matches(slot, keycode, record->event.key)) {
        key_runtime_transition_plan_release_owned_state_by_key(plan, record->event.key);
        return true;
    }

    active_key_state_t released_key = *slot;
    key_runtime_slot_reset(slot);

    uint16_t                             elapsed    = timer_elapsed(released_key.timer);
    key_runtime_slot_release_resolution_t resolution = key_runtime_slot_resolve_release(keycode, released_key, behavior, elapsed);

    key_runtime_transition_apply_active_key_release_resolution(keycode, slot, released_key, behavior, resolution, plan);

    return true;
}

bool key_runtime_transition_handled_key_release(uint16_t keycode, keyrecord_t *record, handled_key_view_t key, key_runtime_transition_plan_t *plan) {
    if (key_runtime_transition_process_pending_multi_tap_hold_release(keycode, record, key.behavior, plan)) {
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
        case HOLD_BEHAVIOR_REPEAT_WHILE_HELD:
            return HOLD_THRESHOLD_DISPATCH_REPEAT;
        default:
            return HOLD_THRESHOLD_DISPATCH_NONE;
    }
}

static bool key_runtime_transition_hold_activation_needs_pulse(hold_behavior_t hold, bool pulse_momentary_layer_action) {
    if (action_dispatch_is_layer_lock(hold.action)) {
        return true;
    }

    // Keep base momentary layer access quiet; alternate multi-tap layer
    // branches can opt in to a confirmation pulse.
    if (IS_QK_MOMENTARY(hold.action)) {
        return pulse_momentary_layer_action;
    }

    return false;
}

static void key_runtime_transition_clear_active_owned_hold(active_key_state_t *slot, key_runtime_transition_plan_t *plan) {
    if (slot->held_action_keycode != KC_NO || slot->repeat_binding_active) {
        key_runtime_transition_plan_release_owned_state_by_key(plan, slot->key_pos);
        slot->held_action_keycode   = KC_NO;
        slot->repeat_binding_active = false;
    }
}

static void key_runtime_transition_fire_hold_at_threshold(active_key_state_t *slot, hold_behavior_t hold, hold_behavior_t long_hold, bool pulse_momentary_layer_action, key_runtime_transition_plan_t *plan) {
    switch (key_runtime_transition_hold_threshold_dispatch_kind(hold)) {
        case HOLD_THRESHOLD_DISPATCH_TAP:
            key_runtime_transition_clear_active_owned_hold(slot, plan);
            key_runtime_transition_plan_dispatch_action(plan, hold.action);
            key_runtime_transition_plan_feedback_pulse(plan, false);
            if (long_hold.present) {
                slot->hold_fired          = false;
                slot->hold_one_shot_fired = true;
            } else {
                slot->hold_fired          = true;
                slot->hold_one_shot_fired = false;
            }
            return;
        case HOLD_THRESHOLD_DISPATCH_HELD:
            key_runtime_transition_plan_held_register(plan, slot->key_pos, hold.action);
            slot->held_action_keycode = hold.action;
            slot->hold_fired          = !long_hold.present;
            slot->hold_one_shot_fired = false;
            if (key_runtime_transition_hold_activation_needs_pulse(hold, pulse_momentary_layer_action)) {
                key_runtime_transition_plan_feedback_pulse(plan, false);
            }
            return;
        case HOLD_THRESHOLD_DISPATCH_REPEAT:
            key_runtime_transition_clear_active_owned_hold(slot, plan);
            key_runtime_transition_plan_repeat_start(plan, slot->key_pos, hold.action, hold.repeat_hz);
            key_runtime_transition_plan_feedback_pulse(plan, false);
            slot->repeat_binding_active = true;
            slot->hold_fired            = !long_hold.present;
            slot->hold_one_shot_fired   = false;
            return;
        case HOLD_THRESHOLD_DISPATCH_NONE:
            return;
    }
}

static void key_runtime_transition_promote_to_long_hold(active_key_state_t *slot, hold_behavior_t long_hold, bool pulse_momentary_layer_action, key_runtime_transition_plan_t *plan) {
    key_runtime_transition_clear_active_owned_hold(slot, plan);

    switch (key_runtime_transition_hold_threshold_dispatch_kind(long_hold)) {
        case HOLD_THRESHOLD_DISPATCH_TAP:
            key_runtime_transition_plan_dispatch_action(plan, long_hold.action);
            key_runtime_transition_plan_feedback_pulse(plan, true);
            slot->hold_fired = true;
            return;
        case HOLD_THRESHOLD_DISPATCH_HELD:
            key_runtime_transition_plan_held_register(plan, slot->key_pos, long_hold.action);
            slot->held_action_keycode = long_hold.action;
            slot->hold_fired          = true;
            if (key_runtime_transition_hold_activation_needs_pulse(long_hold, pulse_momentary_layer_action)) {
                key_runtime_transition_plan_feedback_pulse(plan, true);
            }
            return;
        case HOLD_THRESHOLD_DISPATCH_REPEAT:
            key_runtime_transition_plan_repeat_start(plan, slot->key_pos, long_hold.action, long_hold.repeat_hz);
            key_runtime_transition_plan_feedback_pulse(plan, true);
            slot->repeat_binding_active = true;
            slot->hold_fired            = true;
            return;
        case HOLD_THRESHOLD_DISPATCH_NONE:
            return;
    }
}

static void key_runtime_transition_apply_active_key_scan_resolution(active_key_state_t *slot, key_runtime_slot_scan_resolution_t resolution, key_runtime_transition_plan_t *plan) {
    if (resolution.commit_immediate_hold) {
        if (resolution.immediate_hold_needs_feedback) {
            key_runtime_transition_plan_feedback_pulse(plan, false);
        }
        slot->hold_one_shot_fired = true;
        if (resolution.immediate_hold_completes_hold) {
            slot->hold_fired = true;
        }
    }

    if (resolution.activate_fallback_hold) {
        key_runtime_transition_activate_pending_fallback_hold(slot, plan);
        return;
    }

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_SCAN_OUTCOME_FIRE_HOLD:
            key_runtime_transition_fire_hold_at_threshold(slot, resolution.hold, resolution.long_hold, false, plan);
            return;
        case KEY_RUNTIME_SLOT_SCAN_OUTCOME_PROMOTE_LONG_HOLD:
            key_runtime_transition_promote_to_long_hold(slot, resolution.long_hold, false, plan);
            return;
        case KEY_RUNTIME_SLOT_SCAN_OUTCOME_NONE:
        default:
            return;
    }
}

static void key_runtime_transition_apply_pending_multi_tap_scan_resolution(active_key_state_t *slot, key_runtime_slot_pending_multi_tap_scan_resolution_t resolution, key_runtime_transition_plan_t *plan) {

    if (resolution.release_layer_before_action) {
        key_runtime_transition_plan_layer_release(plan, slot->key_pos);
    }

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_PROMOTE_LONG_HOLD:
            slot->long_hold = resolution.long_hold;
            key_runtime_transition_promote_to_long_hold(slot, slot->long_hold, true, plan);
            key_runtime_slot_reset_pending_multi_tap(slot);
            return;
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_FIRE_HOLD:
            slot->long_hold = resolution.long_hold;
            key_runtime_transition_fire_hold_at_threshold(slot, resolution.hold, slot->long_hold, true, plan);
            key_runtime_slot_reset_pending_multi_tap(slot);
            return;
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_NONE:
        default:
            return;
    }
}

void key_runtime_transition_scan(key_runtime_transition_plan_t *plan) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (!key_runtime_slot_active(slot) || slot->hold_fired) {
            continue;
        }

        uint16_t                          elapsed    = timer_elapsed(slot->timer);
        key_runtime_slot_scan_resolution_t resolution = key_runtime_slot_resolve_scan(*slot, elapsed);

        key_runtime_transition_apply_active_key_scan_resolution(slot, resolution, plan);
    }

    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot          = key_runtime_slot_at(index);
        multi_tap_t        *slot_multi_tap = key_runtime_multi_tap_slot_at(index);

        if (key_runtime_slot_pending_multi_tap_pending_hold(slot) && key_runtime_slot_active(slot)) {
            uint16_t                                           elapsed    = timer_elapsed(slot_multi_tap->timer);
            key_runtime_slot_pending_multi_tap_scan_resolution_t resolution = key_runtime_slot_resolve_pending_multi_tap_scan(*slot, *slot_multi_tap, elapsed);

            key_runtime_transition_apply_pending_multi_tap_scan_resolution(slot, resolution, plan);
        }

        if (key_runtime_slot_pending_multi_tap_expired(slot)) {
            key_runtime_transition_flush_slot_pending_multi_tap(slot, plan);
        }
    }
}
