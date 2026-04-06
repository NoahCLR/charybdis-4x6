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
#include "key_runtime_transition.h"
#include "../action/action_dispatch.h"
#include "../pointing/pd_modes.h"

bool key_runtime_preflight_record(uint16_t keycode, keyrecord_t *record) {
    key_runtime_effects_track_physical_keycode_event(keycode, record);
    if (key_runtime_effects_should_suppress_default(keycode, record)) {
        // Managed modifier releases normally suppress the raw QMK path, but a
        // handled key still needs its own release event so the custom runtime
        // can unregister the held action and clear feedback.
        if (!record->event.pressed && active_key_matches(keycode, record->event.key)) {
            // Let the handled-key release path run.
        } else {
            return false;
        }
    }

    if (record->event.pressed && active_key.keycode != KC_NO && !active_key_matches(keycode, record->event.key)) {
        key_runtime_effects_activate_pending_fallback_hold();

        if (is_layer_key(active_key.keycode)) {
            active_key.layer_interrupted = true;
        }
    }

    if (multi_tap_active(&multi_tap) && record->event.pressed && keycode != multi_tap.keycode) {
        key_runtime_transition_plan_t plan;
        key_runtime_transition_plan_init(&plan);
        key_runtime_transition_flush_multi_tap(&plan);
        key_runtime_transition_execute_plan(&plan);
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
