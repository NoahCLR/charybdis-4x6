// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Preflight
// ────────────────────────────────────────────────────────────────────────────
//
// Physical-event preflight before the handled-key press/release state machine
// runs.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_process.h"
#include "handled_key.h"
#include "key_runtime_state.h"
#include "key_runtime_trace.h"
#include "key_runtime_transition.h"
#include "../action/action_dispatch.h"
#include "../pointing/pd_modes.h"
#include "../state/keyboard_mod_ownership.h"

bool key_runtime_preflight_record(uint16_t keycode, keyrecord_t *record) {
    active_key_state_t *slot = key_runtime_find_slot_by_position(record->event.key);
    bool                other_slot_active = false;

    keyboard_mod_ownership_track_physical_keycode_event(keycode, record);
    if (keyboard_mod_ownership_should_suppress_default(keycode, record)) {
        // Managed modifier releases normally suppress the raw QMK path, but a
        // handled key still needs its own release event so the custom runtime
        // can unregister the held action and clear feedback. That remains true
        // even after another handled key flushes active_key, because the older
        // key's owned held action is still released by physical key position.
        handled_key_view_t handled_key = handled_key_lookup(keycode);
        if (!record->event.pressed && (key_runtime_slot_matches(slot, keycode, record->event.key) || handled_key.behavior.handled)) {
            // Let the handled-key release path run.
        } else {
            key_runtime_trace_message("preflight:suppress_default", "default QMK path suppressed before handled-key runtime");
            return false;
        }
    }

    if (record->event.pressed) {
        for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
            active_key_state_t *candidate = key_runtime_slot_at(index);

            if (!key_runtime_slot_active(candidate) || key_runtime_keypos_equal(candidate->key_pos, record->event.key)) {
                continue;
            }

            other_slot_active = true;
            break;
        }
    }

    if (other_slot_active) {
        key_runtime_transition_plan_t plan;
        key_runtime_transition_plan_init(&plan);
        key_runtime_transition_interrupt_active_keys_on_other_press(record->event.key, &plan);
        key_runtime_trace_plan("preflight:interrupt_active_key", &plan);
        key_runtime_transition_execute_plan(&plan);
    }

    if (multi_tap_active(&multi_tap) && record->event.pressed && !multi_tap_matches(&multi_tap, keycode, record->event.key)) {
        key_runtime_transition_plan_t plan;
        key_runtime_transition_plan_init(&plan);
        key_runtime_transition_flush_multi_tap(&plan);
        key_runtime_trace_plan("preflight:flush_multi_tap", &plan);
        key_runtime_transition_execute_plan(&plan);
    }

    return true;
}

bool key_runtime_process_direct_action_key(uint16_t keycode, keyrecord_t *record) {
    if (!(action_dispatch_is_layer_lock(keycode) || is_pd_mode_lock_action(keycode))) {
        return false;
    }

    if (record->event.pressed) {
        key_runtime_trace_record("preflight:direct_action_dispatch", keycode, record);
        action_dispatch(keycode);
    }

    return true;
}
