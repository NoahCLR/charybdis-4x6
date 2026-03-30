// ────────────────────────────────────────────────────────────────────────────
// Key Runtime
// ────────────────────────────────────────────────────────────────────────────
//
// Custom key processing, tap/hold resolution, and matrix-scan hold logic.
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#include "../../keymap_defs.h"
#include "key_behavior.h"
#include "multi_tap_engine.h"
#include "../pointing/pointing_device_modes.h"
#include "../pointing/split_sync.h"

// ─── Tracked Press State ─────────────────────────────────────────────────────
//
// The key runtime tracks two independent presses:
//   - the currently active key-behavior key
//   - the currently active pointing-device mode key
//
// They keep separate timers so quick overlaps do not stomp each other's
// tap/hold bookkeeping.

typedef struct {
    uint16_t timer;
    uint16_t keycode;
    uint8_t  mode;
    bool     was_locked;
    bool     activated;
} pd_mode_press_state_t;

#define PD_MODE_PRESS_STATE_INIT \
    { .keycode = KC_NO }

typedef struct {
    uint16_t        timer;
    uint16_t        keycode;
    bool            hold_fired;
    uint16_t        tap_action;
    uint16_t        tap_hold_term;
    uint16_t        longer_hold_term;
    uint16_t        multi_tap_term;
    bool            hold_one_shot_fired;
    bool            layer_interrupted;
    hold_behavior_t hold;
    hold_behavior_t long_hold;
} active_key_state_t;

#define ACTIVE_KEY_STATE_INIT \
    {                         \
        .keycode          = KC_NO,                  \
        .tap_hold_term    = CUSTOM_TAP_HOLD_TERM,   \
        .longer_hold_term = CUSTOM_LONGER_HOLD_TERM,\
        .multi_tap_term   = CUSTOM_MULTI_TAP_TERM,  \
    }

static pd_mode_press_state_t pd_mode_press = PD_MODE_PRESS_STATE_INIT;
static active_key_state_t    active_key    = ACTIVE_KEY_STATE_INIT;

static inline uint8_t behavior_get_layer(uint16_t keycode) {
    return IS_QK_LAYER_TAP(keycode) ? QK_LAYER_TAP_GET_LAYER(keycode) : QK_MOMENTARY_GET_LAYER(keycode);
}

static inline bool is_layer_key(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

static inline void pd_mode_press_reset(void) {
    pd_mode_press = (pd_mode_press_state_t)PD_MODE_PRESS_STATE_INIT;
}

// ─── Key Behavior State ──────────────────────────────────────────────────────
static multi_tap_t multi_tap = {0};
static uint8_t     locked_layer = 0;
static uint16_t    held_action_keycode = KC_NO;

static inline void active_key_reset(void) {
    active_key = (active_key_state_t)ACTIVE_KEY_STATE_INIT;
}

static inline void active_key_track(uint16_t keycode, uint16_t tap_action,
                                    hold_behavior_t hold, hold_behavior_t long_hold,
                                    uint16_t tap_hold_term, uint16_t longer_hold_term,
                                    uint16_t multi_tap_term, bool hold_fired) {
    active_key = (active_key_state_t){
        .timer              = timer_read(),
        .keycode            = keycode,
        .hold_fired         = hold_fired,
        .tap_action         = tap_action,
        .tap_hold_term      = tap_hold_term,
        .longer_hold_term   = longer_hold_term,
        .multi_tap_term     = multi_tap_term,
        .hold               = hold,
        .long_hold          = long_hold,
    };
}

static inline void deactivate_momentary_layer_if_unlocked(uint16_t keycode) {
    uint8_t layer = behavior_get_layer(keycode);
    if (locked_layer != layer) {
        layer_off(layer);
    }
}

static inline bool is_layer_lock_action(uint16_t action) {
    return action >= LAYER_LOCK_BASE && action < LAYER_LOCK_BASE + LAYER_COUNT;
}

static void dispatch_action(uint16_t action) {
    if (is_layer_lock_action(action)) {
        uint8_t layer = action - LAYER_LOCK_BASE;
        if (locked_layer == layer) {
            layer_off(layer);
            locked_layer = 0;
        } else {
            if (locked_layer) layer_off(locked_layer);
            layer_on(layer);
            locked_layer = layer;
        }
        return;
    }
    if (is_pd_mode_lock_action(action)) {
        const pd_mode_def_t *def = pd_mode_lock_action_lookup(action);
        if (def && pd_mode_toggle_lock_state(def->mode_flag)) {
            pd_state_sync();
        }
        return;
    }
    tap_code16(action);
}

typedef struct {
    key_behavior_view_t behavior;
    uint8_t             pd_mode;
} handled_key_view_t;

static inline handled_key_view_t handled_key_lookup(uint16_t keycode) {
    return (handled_key_view_t){
        .behavior = key_behavior_lookup(keycode),
        .pd_mode  = pd_mode_for_keycode(keycode),
    };
}

static inline uint16_t handled_key_tap_action(handled_key_view_t key, keyrecord_t *record) {
    if (key.behavior.single.tap.present) return key.behavior.single.tap.action;
    if (key.pd_mode)                     return keymap_key_to_keycode(LAYER_BASE, record->event.key);
    if (key.behavior.is_layer_tap)       return QK_LAYER_TAP_GET_TAP_KEYCODE(key.behavior.keycode);
    if (key.behavior.is_momentary_layer) return KC_NO;
    if (key.behavior.keycode >= SAFE_RANGE) return keymap_key_to_keycode(LAYER_BASE, record->event.key);
    return key.behavior.keycode;
}

static inline bool handled_key_multi_tap_repress(handled_key_view_t key, uint16_t keycode) {
    return multi_tap_active(&multi_tap) && multi_tap.keycode == keycode && key.behavior.has_multi_tap;
}

static inline uint16_t handled_key_advance_multi_tap(uint16_t keycode) {
    return multi_tap_advance(&multi_tap, keycode, key_behavior_step_lookup, key_behavior_has_more_taps);
}

static inline void handled_key_dispatch_tap_or_begin_multi_tap(uint16_t keycode, handled_key_view_t key, keyrecord_t *record) {
    uint16_t tap_action = handled_key_tap_action(key, record);
    if (key.behavior.has_multi_tap) {
        multi_tap_begin(&multi_tap, keycode, tap_action, key.behavior.tap_hold_term, key.behavior.multi_tap_term);
    } else if (tap_action != KC_NO) {
        dispatch_action(tap_action);
    }
}

static uint16_t select_release_hold_action(uint16_t elapsed, uint16_t hold_action,
                                           hold_behavior_t long_hold, uint16_t longer_hold_term) {
    if (hold_sends_on_release(long_hold) && elapsed >= longer_hold_term) {
        return long_hold.action;
    }
    return hold_action;
}

static void fire_hold_at_threshold(hold_behavior_t hold, hold_behavior_t long_hold) {
    if (is_layer_lock_action(hold.action) || !hold_registers_while_held(hold)) {
        if (held_action_keycode != KC_NO) {
            unregister_code16(held_action_keycode);
            held_action_keycode = KC_NO;
        }
        dispatch_action(hold.action);
        if (long_hold.present) {
            active_key.hold_fired          = false;
            active_key.hold_one_shot_fired = true;
        } else {
            active_key.hold_fired          = true;
            active_key.hold_one_shot_fired = false;
        }
        return;
    }

    register_code16(hold.action);
    held_action_keycode         = hold.action;
    active_key.hold_fired       = !long_hold.present;
    active_key.hold_one_shot_fired = false;
}

static void promote_to_long_hold(hold_behavior_t long_hold) {
    if (held_action_keycode != KC_NO) {
        unregister_code16(held_action_keycode);
        held_action_keycode = KC_NO;
    }

    if (is_layer_lock_action(long_hold.action) || !hold_registers_while_held(long_hold)) {
        dispatch_action(long_hold.action);
        active_key.hold_fired = true;
        return;
    }

    register_code16(long_hold.action);
    held_action_keycode = long_hold.action;
    active_key.hold_fired = true;
}

static void flush_active_key(void) {
    if (active_key.keycode == KC_NO) return;

    if (active_key.hold_fired || held_action_keycode != KC_NO) {
        active_key.hold_fired = false;
        if (held_action_keycode != KC_NO) {
            unregister_code16(held_action_keycode);
            held_action_keycode = KC_NO;
        }
    } else if (!is_layer_key(active_key.keycode) && active_key.tap_action != KC_NO) {
        dispatch_action(active_key.tap_action);
    }

    active_key_reset();
}

static bool process_pd_mode_key(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    uint8_t mode = key.pd_mode;
    if (!mode) return false;

    if (record->event.pressed) {
        if (pd_mode_unlock_other_locks(mode)) {
            pd_state_sync();
        }

        if (handled_key_multi_tap_repress(key, keycode)) {
            pd_mode_press.mode       = mode;
            pd_mode_press.was_locked = pd_mode_locked(mode);
            pd_mode_press.activated  = false;
            uint16_t action = handled_key_advance_multi_tap(keycode);
            if (action != KC_NO) dispatch_action(action);
            pd_mode_press.keycode = KC_NO;
            return true;
        }

        pd_mode_press.timer      = timer_read();
        pd_mode_press.keycode    = keycode;
        pd_mode_press.mode       = mode;
        pd_mode_press.was_locked = pd_mode_locked(mode);
        pd_mode_press.activated  = !pd_mode_press.was_locked;
        pd_mode_update(mode, true);
    } else {
        bool     pressed_this_key = pd_mode_press.keycode == keycode;
        uint16_t elapsed          = pressed_this_key ? timer_elapsed(pd_mode_press.timer) : 0;
        bool     locked_press =
            pd_mode_press.mode == mode && pd_mode_press.was_locked && pd_mode_is_lockable(mode) && pressed_this_key;

        if (pd_mode_press.mode == mode && pd_mode_press.activated) {
            pd_mode_update(mode, false);
        }

        if (locked_press) {
            if (pd_mode_toggle_lock_state(mode)) {
                pd_state_sync();
            }
        } else if (pressed_this_key && elapsed < CUSTOM_TAP_HOLD_TERM) {
            handled_key_dispatch_tap_or_begin_multi_tap(keycode, key, record);
        }
        pd_mode_press_reset();
    }

    pd_state_sync();
    return true;
}

static bool process_key_behavior(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    key_behavior_view_t behavior = key.behavior;
    if (!behavior.handled) return false;

    if (record->event.pressed) {
        if (handled_key_multi_tap_repress(key, keycode)) {
            uint16_t action = handled_key_advance_multi_tap(keycode);
            if (action != KC_NO) dispatch_action(action);
            if (behavior.is_momentary_layer) layer_on(behavior_get_layer(keycode));
            bool pending_hold = multi_tap_pending_hold(&multi_tap);
            if (pending_hold || behavior.is_momentary_layer) {
                active_key_track(keycode, KC_NO, hold_behavior_none(), hold_behavior_none(),
                                 behavior.tap_hold_term, behavior.longer_hold_term,
                                 behavior.multi_tap_term, !pending_hold);
            }
            return true;
        }

        if (behavior.is_momentary_layer) {
            layer_on(behavior_get_layer(keycode));
        }

        flush_active_key();

        active_key_track(keycode, handled_key_tap_action(key, record),
                         behavior.single.hold, behavior.single.long_hold,
                         behavior.tap_hold_term, behavior.longer_hold_term,
                         behavior.multi_tap_term, false);
        return true;
    }

    if (multi_tap_pending_hold(&multi_tap) && multi_tap.keycode == keycode) {
        bool            was_release_hold = hold_sends_on_release(multi_tap.hold);
        hold_behavior_t cached_hold      = multi_tap.hold;
        hold_behavior_t cached_long_hold = multi_tap.long_hold;
        uint8_t         repeat_count     = 0;
        uint16_t action = multi_tap_resolve_hold(&multi_tap, keycode, key_behavior_has_more_taps, &repeat_count);

        if (was_release_hold && cached_hold.present && repeat_count == 1 && action == cached_hold.action) {
            action = select_release_hold_action(timer_elapsed(active_key.timer), cached_hold.action,
                                                cached_long_hold, active_key.longer_hold_term);
        }

        for (uint8_t i = 0; i < repeat_count; i++) {
            if (action != KC_NO) dispatch_action(action);
        }

        if (behavior.is_momentary_layer) {
            deactivate_momentary_layer_if_unlocked(keycode);
        }
        active_key_reset();
        return true;
    }

    if (behavior.is_momentary_layer) {
        deactivate_momentary_layer_if_unlocked(keycode);
    }

    if (active_key.keycode != keycode) return true;

    active_key_state_t released_key = active_key;

    active_key_reset();

    if (released_key.hold_fired || held_action_keycode != KC_NO) {
        uint16_t elapsed = timer_elapsed(released_key.timer);
        if (held_action_keycode != KC_NO) {
            unregister_code16(held_action_keycode);
            held_action_keycode = KC_NO;
        }
        if (hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term) {
            dispatch_action(released_key.long_hold.action);
        }
        return true;
    }

    uint16_t elapsed = timer_elapsed(released_key.timer);

    if (elapsed < released_key.tap_hold_term && !(behavior.is_momentary_layer && released_key.layer_interrupted)) {
        if (behavior.is_momentary_layer && locked_layer) {
            layer_off(locked_layer);
            locked_layer = 0;
        } else if (behavior.has_multi_tap) {
            multi_tap_begin(&multi_tap, keycode, released_key.tap_action,
                            released_key.tap_hold_term, released_key.multi_tap_term);
        } else if (released_key.tap_action != KC_NO) {
            dispatch_action(released_key.tap_action);
        }
    } else {
        if (hold_sends_on_release(released_key.hold)) {
            dispatch_action(select_release_hold_action(elapsed, released_key.hold.action,
                                                      released_key.long_hold, released_key.longer_hold_term));
        } else if (hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term) {
            dispatch_action(released_key.long_hold.action);
        } else if (!released_key.hold_one_shot_fired && !behavior.is_momentary_layer &&
                   released_key.tap_action != KC_NO) {
            dispatch_action(released_key.tap_action);
        }
    }

    return true;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed && active_key.keycode != KC_NO && keycode != active_key.keycode &&
        is_layer_key(active_key.keycode)) {
        active_key.layer_interrupted = true;
    }

    if (multi_tap_active(&multi_tap) && record->event.pressed && keycode != multi_tap.keycode) {
        multi_tap_flush(&multi_tap, key_behavior_step_lookup, dispatch_action);
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

    if (!record->event.pressed) {
        return true;
    }

    if (macro_dispatch(keycode)) return false;
    return true;
}

void matrix_scan_user(void) {
    if (active_key.keycode != KC_NO && !active_key.hold_fired) {
        uint16_t elapsed = timer_elapsed(active_key.timer);
        if (hold_fires_at_threshold(active_key.long_hold) && elapsed >= active_key.longer_hold_term) {
            promote_to_long_hold(active_key.long_hold);
        } else if (hold_fires_at_threshold(active_key.hold) && elapsed >= active_key.tap_hold_term) {
            fire_hold_at_threshold(active_key.hold, active_key.long_hold);
        }
    }

    if (multi_tap_hold_elapsed(&multi_tap)) {
        if (is_layer_key(active_key.keycode) && is_layer_lock_action(multi_tap.hold.action)) {
            layer_off(behavior_get_layer(active_key.keycode));
        }
        active_key.long_hold = multi_tap.long_hold;
        fire_hold_at_threshold(multi_tap.hold, active_key.long_hold);
        multi_tap_reset(&multi_tap);
    }

    if (multi_tap_expired(&multi_tap)) {
        multi_tap_flush(&multi_tap, key_behavior_step_lookup, dispatch_action);
    }
}
