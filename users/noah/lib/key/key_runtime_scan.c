// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Scan Flow
// ────────────────────────────────────────────────────────────────────────────
//
// Matrix-scan hold promotion and multi-tap expiry handling.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_feedback.h"
#include "key_runtime_effects.h"
#include "key_runtime_state.h"
#include "delayed_action.h"
#include "key_behavior_lookup.h"
#include "../action/action_lifecycle.h"
#include "../action/action_dispatch.h"

typedef enum {
    HOLD_THRESHOLD_DISPATCH_NONE = 0,
    HOLD_THRESHOLD_DISPATCH_TAP,
    HOLD_THRESHOLD_DISPATCH_HELD,
} hold_threshold_dispatch_t;

static hold_threshold_dispatch_t hold_threshold_dispatch_kind(hold_behavior_t hold) {
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

static bool hold_activation_needs_pulse(hold_behavior_t hold) {
    return action_dispatch_is_layer_action(hold.action);
}

static void clear_active_held_action(void) {
    if (active_key.held_action_keycode != KC_NO) {
        key_runtime_effects_held_action_unregister(active_key.key_pos, active_key.held_action_keycode);
        active_key.held_action_keycode = KC_NO;
    }
}

static void fire_hold_at_threshold(hold_behavior_t hold, hold_behavior_t long_hold) {
    switch (hold_threshold_dispatch_kind(hold)) {
        case HOLD_THRESHOLD_DISPATCH_TAP:
            clear_active_held_action();
            key_runtime_effects_dispatch_action(hold.action);
            key_runtime_effects_feedback_pulse_arm(false);
            if (long_hold.present) {
                active_key.hold_fired          = false;
                active_key.hold_one_shot_fired = true;
            } else {
                active_key.hold_fired          = true;
                active_key.hold_one_shot_fired = false;
            }
            return;
        case HOLD_THRESHOLD_DISPATCH_HELD:
            key_runtime_effects_held_action_register(active_key.key_pos, hold.action);
            active_key.held_action_keycode = hold.action;
            active_key.hold_fired          = !long_hold.present;
            active_key.hold_one_shot_fired = false;
            if (hold_activation_needs_pulse(hold)) {
                key_runtime_effects_feedback_pulse_arm(false);
            }
            return;
        case HOLD_THRESHOLD_DISPATCH_NONE:
            return;
    }
}

static void promote_to_long_hold(hold_behavior_t long_hold) {
    clear_active_held_action();

    switch (hold_threshold_dispatch_kind(long_hold)) {
        case HOLD_THRESHOLD_DISPATCH_TAP:
            key_runtime_effects_dispatch_action(long_hold.action);
            key_runtime_effects_feedback_pulse_arm(true);
            active_key.hold_fired = true;
            return;
        case HOLD_THRESHOLD_DISPATCH_HELD:
            key_runtime_effects_held_action_register(active_key.key_pos, long_hold.action);
            active_key.held_action_keycode = long_hold.action;
            active_key.hold_fired          = true;
            if (hold_activation_needs_pulse(long_hold)) {
                key_runtime_effects_feedback_pulse_arm(true);
            }
            return;
        case HOLD_THRESHOLD_DISPATCH_NONE:
            return;
    }
}

static inline void deactivate_pending_multi_tap_layer_before_lock(uint16_t action) {
    if (is_layer_key(active_key.keycode) && action_dispatch_is_layer_lock(action)) {
        key_runtime_effects_layer_release(active_key.key_pos);
    }
}

static void commit_immediate_hold_threshold(void) {
    if (!hold_registers_on_press(active_key.hold) || active_key.hold_one_shot_fired || timer_elapsed(active_key.timer) < active_key.tap_hold_term) {
        return;
    }

    if (!active_key.implicit_pd_mode_hold) {
        key_runtime_effects_feedback_pulse_arm(false);
    }
    active_key.hold_one_shot_fired = true;
    if (!active_key.long_hold.present) {
        active_key.hold_fired = true;
    }
}

void noah_key_runtime_scan(void) {
    if (active_key.keycode != KC_NO && !active_key.hold_fired) {
        commit_immediate_hold_threshold();

        uint16_t elapsed = timer_elapsed(active_key.timer);
        if (hold_fires_at_threshold(active_key.long_hold) && elapsed >= active_key.longer_hold_term) {
            promote_to_long_hold(active_key.long_hold);
        } else if (hold_fires_at_threshold(active_key.hold) && elapsed >= active_key.tap_hold_term) {
            fire_hold_at_threshold(active_key.hold, active_key.long_hold);
        }
    }

    if (multi_tap_pending_hold(&multi_tap) && active_key.keycode != KC_NO) {
        uint16_t elapsed = timer_elapsed(multi_tap.timer);

        if (hold_fires_at_threshold(multi_tap.long_hold) && elapsed >= active_key.longer_hold_term) {
            deactivate_pending_multi_tap_layer_before_lock(multi_tap.long_hold.action);
            active_key.long_hold = multi_tap.long_hold;
            promote_to_long_hold(active_key.long_hold);
            multi_tap_reset(&multi_tap);
        } else if (multi_tap_hold_elapsed(&multi_tap)) {
            deactivate_pending_multi_tap_layer_before_lock(multi_tap.hold.action);
            active_key.long_hold = multi_tap.long_hold;
            fire_hold_at_threshold(multi_tap.hold, active_key.long_hold);
            multi_tap_reset(&multi_tap);
        }
    }

    if (multi_tap_expired(&multi_tap)) {
        multi_tap_flush(&multi_tap, key_behavior_step_lookup, dispatch_multi_tap_action);
    }
}
