// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Preflight
// ────────────────────────────────────────────────────────────────────────────
//
// Physical-event preflight before the handled-key press/release state machine
// runs.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_process.h"
#include "key_runtime_effects.h"
#include "key_runtime_state.h"
#include "delayed_action.h"
#include "key_behavior_lookup.h"
#include "../action/action_dispatch.h"
#include "../pointing/pd_modes.h"

bool key_runtime_preflight_record(uint16_t keycode, keyrecord_t *record) {
    key_runtime_effects_track_physical_keycode_event(keycode, record);
    if (key_runtime_effects_should_suppress_default(keycode, record)) {
        return false;
    }

    if (record->event.pressed && active_key.keycode != KC_NO && !active_key_matches(keycode, record->event.key) && is_layer_key(active_key.keycode)) {
        active_key.layer_interrupted = true;
    }

    if (multi_tap_active(&multi_tap) && record->event.pressed && keycode != multi_tap.keycode) {
        multi_tap_flush(&multi_tap, key_behavior_step_lookup, dispatch_multi_tap_action);
    }

    return true;
}

bool key_runtime_process_direct_action_key(uint16_t keycode, keyrecord_t *record) {
    if (!(action_dispatch_is_layer_lock(keycode) || is_pd_mode_lock_action(keycode))) {
        return false;
    }

    if (record->event.pressed) {
        key_runtime_effects_dispatch_action(keycode);
    }

    return true;
}
