// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Press Flow
// ────────────────────────────────────────────────────────────────────────────
//
// Handled-key press transitions and active-key replacement.
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key.h"
#include "key_runtime_process.h"
#include "key_runtime_state.h"
#include "../action/action_dispatch.h"
#include "held_action.h"
#include "../pointing/pd_modes.h"
#include "../state/layer_ownership.h"

static void flush_active_key(void) {
    if (active_key.keycode == KC_NO) return;

    if (active_key.hold_fired || active_key.held_action_keycode != KC_NO) {
        active_key.hold_fired = false;
        if (active_key.held_action_keycode != KC_NO && !held_action_survives_flush(active_key.key_pos, active_key.held_action_keycode)) {
            held_action_unregister(active_key.key_pos, active_key.held_action_keycode);
            active_key.held_action_keycode = KC_NO;
        }
    } else if (!is_layer_key(active_key.keycode) && active_key.tap_action != KC_NO) {
        action_dispatch(active_key.tap_action);
    }

    active_key_reset();
}

static void activate_immediate_hold_if_needed(keyrecord_t *record, hold_behavior_t hold) {
    if (!hold_registers_on_press(hold)) {
        return;
    }

    held_action_register(record->event.key, hold.action);
    active_key.held_action_keycode = hold.action;
}

bool key_runtime_process_handled_key_press(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    key_behavior_view_t behavior = key.behavior;
    hold_behavior_t     hold     = handled_key_single_hold(key);
    bool                implicit = handled_key_uses_implicit_pd_mode_hold(key);
    uint8_t             mode     = pd_mode_for_keycode(keycode);

    if (handled_key_multi_tap_repress(key, keycode)) {
        uint16_t action = handled_key_advance_multi_tap(keycode);
        if (action != KC_NO) {
            action_dispatch(action);
        }

        if (behavior.is_momentary_layer) {
            layer_ownership_momentary_press(record->event.key, behavior_get_layer(keycode));
        }

        bool pending_hold = multi_tap_pending_hold(&multi_tap);
        if (pending_hold || behavior.is_momentary_layer) {
            active_key_track(keycode, record->event.key, KC_NO, hold_behavior_none(), hold_behavior_none(), behavior.tap_hold_term, behavior.longer_hold_term, behavior.multi_tap_term, !pending_hold);
        }
        return true;
    }

    if (behavior.is_momentary_layer) {
        layer_ownership_momentary_press(record->event.key, behavior_get_layer(keycode));
    }

    flush_active_key();
    active_key_track(keycode, record->event.key, handled_key_tap_action(key), hold, behavior.single.long_hold, behavior.tap_hold_term, behavior.longer_hold_term, behavior.multi_tap_term, false);
    active_key.implicit_pd_mode_hold       = implicit;
    active_key.pd_mode_was_locked_on_press = mode && pd_mode_locked(mode);
    activate_immediate_hold_if_needed(record, hold);
    return true;
}
