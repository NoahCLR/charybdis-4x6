// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Scan Flow
// ────────────────────────────────────────────────────────────────────────────
//
// Matrix-scan hold promotion and multi-tap expiry handling.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_internal.h"

static void fire_hold_at_threshold(hold_behavior_t hold, hold_behavior_t long_hold) {
    if (action_dispatch_is_layer_lock(hold.action) || !hold_registers_while_held(hold)) {
        if (active_key.held_action_keycode != KC_NO) {
            held_action_unregister(active_key.key_pos, active_key.held_action_keycode);
            active_key.held_action_keycode = KC_NO;
        }
        action_dispatch(hold.action);
        if (long_hold.present) {
            active_key.hold_fired          = false;
            active_key.hold_one_shot_fired = true;
        } else {
            active_key.hold_fired          = true;
            active_key.hold_one_shot_fired = false;
        }
        return;
    }

    held_action_register(active_key.key_pos, hold.action);
    active_key.held_action_keycode = hold.action;
    active_key.hold_fired          = !long_hold.present;
    active_key.hold_one_shot_fired = false;
}

static void promote_to_long_hold(hold_behavior_t long_hold) {
    if (active_key.held_action_keycode != KC_NO) {
        held_action_unregister(active_key.key_pos, active_key.held_action_keycode);
        active_key.held_action_keycode = KC_NO;
    }

    if (action_dispatch_is_layer_lock(long_hold.action) || !hold_registers_while_held(long_hold)) {
        action_dispatch(long_hold.action);
        active_key.hold_fired = true;
        return;
    }

    held_action_register(active_key.key_pos, long_hold.action);
    active_key.held_action_keycode = long_hold.action;
    active_key.hold_fired          = true;
}

static inline void deactivate_pending_multi_tap_layer_before_lock(uint16_t action) {
    if (is_layer_key(active_key.keycode) && action_dispatch_is_layer_lock(action)) {
        layer_off(behavior_get_layer(active_key.keycode));
    }
}

void noah_matrix_scan_user(void) {
    pd_mode_key_runtime_scan();

    if (active_key.keycode != KC_NO && !active_key.hold_fired) {
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
