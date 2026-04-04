// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Process Flow
// ────────────────────────────────────────────────────────────────────────────
//
// Press/release handling and process_record_user integration.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_internal.h"
#include "synthetic_record.h"

static inline void deactivate_momentary_layer_if_unlocked(uint16_t keycode) {
    uint8_t layer = behavior_get_layer(keycode);
    if (!action_dispatch_layer_is_locked(layer)) {
        layer_off(layer);
    }
}

static uint16_t pd_mode_key_runtime_advance_multi_tap(void *context, uint16_t keycode) {
    (void)context;
    return handled_key_advance_multi_tap(keycode);
}

static void pd_mode_key_runtime_dispatch_tap_or_begin_multi_tap(void *context, uint16_t keycode, keyrecord_t *record) {
    handled_key_view_t *key = context;
    handled_key_dispatch_tap_or_begin_multi_tap(keycode, *key, record);
}

static void pd_mode_key_runtime_dispatch_action(void *context, uint16_t action) {
    (void)context;
    action_dispatch(action);
}

static bool pd_mode_key_runtime_capture_pending_multi_tap_hold(void *context, uint16_t *tap_action, uint16_t *hold_action) {
    (void)context;

    if (!multi_tap_pending_hold(&multi_tap)) {
        return false;
    }

    *tap_action  = multi_tap.tap_action;
    *hold_action = multi_tap.hold.action;
    multi_tap_reset(&multi_tap);
    return true;
}

static uint16_t select_release_hold_action(uint16_t elapsed, uint16_t hold_action, hold_behavior_t long_hold, uint16_t longer_hold_term) {
    if (hold_sends_on_release(long_hold) && elapsed >= longer_hold_term) {
        return long_hold.action;
    }
    return hold_action;
}

static void flush_active_key(void) {
    if (active_key.keycode == KC_NO) return;

    if (active_key.hold_fired || active_key.held_action_keycode != KC_NO) {
        active_key.hold_fired = false;
        if (active_key.held_action_keycode != KC_NO && !held_action_survives_flush(active_key.held_action_keycode)) {
            held_action_unregister(active_key.key_pos, active_key.held_action_keycode);
            active_key.held_action_keycode = KC_NO;
        }
    } else if (!is_layer_key(active_key.keycode) && active_key.tap_action != KC_NO) {
        action_dispatch(active_key.tap_action);
    }

    active_key_reset();
}

static bool process_pd_mode_key(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    const pd_mode_key_runtime_hooks_t hooks = {
        .context                         = &key,
        .advance_multi_tap               = pd_mode_key_runtime_advance_multi_tap,
        .dispatch_tap_or_begin_multi_tap = pd_mode_key_runtime_dispatch_tap_or_begin_multi_tap,
        .dispatch_action                 = pd_mode_key_runtime_dispatch_action,
        .capture_pending_multi_tap_hold  = pd_mode_key_runtime_capture_pending_multi_tap_hold,
    };

    return pd_mode_key_runtime_process(keycode, record, key.pd_mode, key.behavior.tap_hold_term, handled_key_multi_tap_repress(key, keycode), &hooks);
}

static bool process_key_behavior_press(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    key_behavior_view_t behavior = key.behavior;

    if (handled_key_multi_tap_repress(key, keycode)) {
        uint16_t action = handled_key_advance_multi_tap(keycode);
        if (action != KC_NO) {
            action_dispatch(action);
        }

        if (behavior.is_momentary_layer) {
            layer_on(behavior_get_layer(keycode));
        }

        bool pending_hold = multi_tap_pending_hold(&multi_tap);
        if (pending_hold || behavior.is_momentary_layer) {
            active_key_track(keycode, record->event.key, KC_NO, hold_behavior_none(), hold_behavior_none(), behavior.tap_hold_term, behavior.longer_hold_term, behavior.multi_tap_term, !pending_hold);
        }
        return true;
    }

    if (behavior.is_momentary_layer) {
        layer_on(behavior_get_layer(keycode));
    }

    flush_active_key();
    active_key_track(keycode, record->event.key, handled_key_tap_action(key, record), behavior.single.hold, behavior.single.long_hold, behavior.tap_hold_term, behavior.longer_hold_term, behavior.multi_tap_term, false);
    return true;
}

static bool process_key_behavior_release_pending_multi_tap_hold(uint16_t keycode, key_behavior_view_t behavior) {
    if (!(multi_tap_pending_hold(&multi_tap) && multi_tap.keycode == keycode)) {
        return false;
    }

    delayed_action_mods_t cached_mods      = delayed_action_mods_from_multi_tap(&multi_tap);
    bool                  was_release_hold = hold_sends_on_release(multi_tap.hold);
    hold_behavior_t       cached_hold      = multi_tap.hold;
    hold_behavior_t       cached_long_hold = multi_tap.long_hold;
    uint8_t               repeat_count     = 0;
    uint16_t              action           = multi_tap_resolve_hold(&multi_tap, keycode, key_behavior_has_more_taps, &repeat_count);

    if (was_release_hold && cached_hold.present && repeat_count == 1 && action == cached_hold.action) {
        action = select_release_hold_action(timer_elapsed(active_key.timer), cached_hold.action, cached_long_hold, active_key.longer_hold_term);
    }

    for (uint8_t i = 0; i < repeat_count; i++) {
        if (action != KC_NO) {
            dispatch_delayed_action(action, cached_mods);
        }
    }

    if (behavior.is_momentary_layer) {
        deactivate_momentary_layer_if_unlocked(keycode);
    }
    active_key_reset();
    return true;
}

static bool process_key_behavior_release_active_key(uint16_t keycode, keyrecord_t *record, key_behavior_view_t behavior) {
    if (behavior.is_momentary_layer) {
        deactivate_momentary_layer_if_unlocked(keycode);
    }

    if (!active_key_matches(keycode, record->event.key)) {
        // active_key may already belong to a newer custom press; release any
        // pure modifier still owned by this physical key.
        held_modifier_release_owned_by_key(record->event.key);
        return true;
    }

    active_key_state_t released_key = active_key;
    active_key_reset();

    if (released_key.hold_fired || released_key.held_action_keycode != KC_NO) {
        uint16_t elapsed = timer_elapsed(released_key.timer);

        if (released_key.held_action_keycode != KC_NO) {
            held_action_unregister(released_key.key_pos, released_key.held_action_keycode);
        }

        if (hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term) {
            action_dispatch(released_key.long_hold.action);
        }
        return true;
    }

    uint16_t elapsed = timer_elapsed(released_key.timer);

    if (elapsed < released_key.tap_hold_term && !(behavior.is_momentary_layer && released_key.layer_interrupted)) {
        if (behavior.has_multi_tap) {
            multi_tap_begin(&multi_tap, keycode, released_key.tap_action, released_key.tap_hold_term, released_key.multi_tap_term);
        } else if (released_key.tap_action != KC_NO) {
            action_dispatch(released_key.tap_action);
        }
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

static bool process_key_behavior_release(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    if (process_key_behavior_release_pending_multi_tap_hold(keycode, key.behavior)) {
        return true;
    }

    return process_key_behavior_release_active_key(keycode, record, key.behavior);
}

static bool process_key_behavior(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    if (!key.behavior.handled) {
        return false;
    }

    if (record->event.pressed) {
        return process_key_behavior_press(keycode, record, key);
    }

    return process_key_behavior_release(keycode, record, key);
}

static bool process_direct_action_key(uint16_t keycode, keyrecord_t *record) {
    if (!(action_dispatch_is_layer_lock(keycode) || is_pd_mode_lock_action(keycode))) {
        return false;
    }

    if (record->event.pressed) {
        action_dispatch(keycode);
    }

    return true;
}

bool noah_get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record) {
    (void)record;

    switch (keycode) {
        case MT(MOD_LSFT, KC_CAPS):
            return true;
        default:
            return false;
    }
}

bool noah_process_record_user(uint16_t keycode, keyrecord_t *record) {
    // Synthetic records are dispatched by the key_behavior action system to
    // route keymap-local custom keycodes back through process_record_user().
    // Skip the physical key runtime so the keymap-local handler can run.
    if (noah_synthetic_record_active()) {
        return true;
    }

    if (record->event.pressed && active_key.keycode != KC_NO && !active_key_matches(keycode, record->event.key) && is_layer_key(active_key.keycode)) {
        active_key.layer_interrupted = true;
    }

    if (multi_tap_active(&multi_tap) && record->event.pressed && keycode != multi_tap.keycode) {
        multi_tap_flush(&multi_tap, key_behavior_step_lookup, dispatch_multi_tap_action);
    }

    if (pd_mode_handle_key_event(keycode, record)) {
        return false;
    }

    handled_key_view_t key = handled_key_lookup(keycode);

    if (process_pd_mode_key(keycode, record, key)) {
        return false;
    }

    if (process_key_behavior(keycode, record, key)) {
        return false;
    }

    if (process_direct_action_key(keycode, record)) {
        return false;
    }

    if (!record->event.pressed) {
        return true;
    }

    if (macro_dispatch(keycode)) return false;
    return true;
}
