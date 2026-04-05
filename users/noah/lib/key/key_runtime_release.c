// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Release Flow
// ────────────────────────────────────────────────────────────────────────────
//
// Handled-key release resolution, including pending multi-tap holds.
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key.h"
#include "key_runtime_process.h"
#include "key_runtime_state.h"
#include "delayed_action.h"
#include "held_action.h"
#include "../action/action_dispatch.h"
#include "../pointing/pd_modes.h"
#include "../state/layer_ownership.h"
#include "../state/split_runtime_sync.h"

static uint16_t select_release_hold_action(uint16_t elapsed, uint16_t hold_action, hold_behavior_t long_hold, uint16_t longer_hold_term) {
    if (hold_sends_on_release(long_hold) && elapsed >= longer_hold_term) {
        return long_hold.action;
    }
    return hold_action;
}

static void dispatch_released_key_tap(uint16_t keycode, active_key_state_t released_key, key_behavior_view_t behavior) {
    if (behavior.has_multi_tap) {
        multi_tap_begin(&multi_tap, keycode, released_key.tap_action, released_key.tap_hold_term, released_key.multi_tap_term);
    } else if (released_key.tap_action != KC_NO) {
        action_dispatch(released_key.tap_action);
    }
}

static bool dispatch_locked_pd_mode_tap_if_needed(uint16_t keycode, active_key_state_t released_key, uint16_t elapsed, key_behavior_view_t behavior) {
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

    if (pd_mode_is_lockable(mode) && pd_mode_toggle_lock_state(mode)) {
        split_runtime_sync();
    }
    return true;
}

static bool process_pending_multi_tap_hold_release(uint16_t keycode, key_behavior_view_t behavior) {
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
        action = select_release_hold_action(elapsed, cached_hold.action, cached_long_hold, active_key.longer_hold_term);
    }

    for (uint8_t i = 0; i < repeat_count; i++) {
        if (action != KC_NO) {
            dispatch_delayed_action(action, cached_mods);
        }
    }

    if (behavior.is_momentary_layer) {
        layer_ownership_momentary_release(active_key.key_pos);
    }
    active_key_reset();
    return true;
}

static bool process_active_key_release(uint16_t keycode, keyrecord_t *record, key_behavior_view_t behavior) {
    if (behavior.is_momentary_layer) {
        layer_ownership_momentary_release(record->event.key);
    }

    if (!active_key_matches(keycode, record->event.key)) {
        // active_key may already belong to a newer custom press; release any
        // remaining held action still owned by this physical key.
        held_action_release_owned_by_key(record->event.key);
        return true;
    }

    active_key_state_t released_key = active_key;
    active_key_reset();

    uint16_t elapsed              = timer_elapsed(released_key.timer);
    bool     quick_immediate_hold = hold_registers_on_press(released_key.hold) && elapsed < released_key.tap_hold_term && !(behavior.is_momentary_layer && released_key.layer_interrupted);

    if (released_key.hold_fired || released_key.held_action_keycode != KC_NO) {
        if (released_key.held_action_keycode != KC_NO) {
            held_action_unregister(released_key.key_pos, released_key.held_action_keycode);
        }

        if (dispatch_locked_pd_mode_tap_if_needed(keycode, released_key, elapsed, behavior)) {
            return true;
        }

        if (quick_immediate_hold) {
            dispatch_released_key_tap(keycode, released_key, behavior);
            return true;
        }

        if (hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term) {
            action_dispatch(released_key.long_hold.action);
        }
        return true;
    }

    if (elapsed < released_key.tap_hold_term && !(behavior.is_momentary_layer && released_key.layer_interrupted)) {
        if (dispatch_locked_pd_mode_tap_if_needed(keycode, released_key, elapsed, behavior)) {
            return true;
        }
        dispatch_released_key_tap(keycode, released_key, behavior);
        return true;
    }

    if (hold_sends_on_release(released_key.hold)) {
        action_dispatch(select_release_hold_action(elapsed, released_key.hold.action, released_key.long_hold, released_key.longer_hold_term));
    } else if (hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term) {
        action_dispatch(released_key.long_hold.action);
    } else if (!released_key.hold_one_shot_fired && !behavior.is_momentary_layer && released_key.tap_action != KC_NO) {
        action_dispatch(released_key.tap_action);
    }

    return true;
}

bool key_runtime_process_handled_key_release(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    if (process_pending_multi_tap_hold_release(keycode, key.behavior)) {
        return true;
    }

    return process_active_key_release(keycode, record, key.behavior);
}
