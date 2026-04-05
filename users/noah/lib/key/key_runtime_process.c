// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Process Flow
// ────────────────────────────────────────────────────────────────────────────
//
// Press/release handling and process_record_user integration.
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key.h"
#include "key_runtime_process.h"
#include "key_runtime_state.h"
#include "noah_keymap.h"
#include "../action/macro_dispatch.h"
#include "../pointing/pointing_device_modes.h"
#include "../action/synthetic_record.h"

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

    if (!key_runtime_preflight_record(keycode, record)) {
        return false;
    }

    if (pd_mode_handle_key_event(keycode, record)) {
        return false;
    }

    handled_key_view_t key = handled_key_lookup(keycode);
    if (key.behavior.handled) {
        bool handled = record->event.pressed ? key_runtime_process_handled_key_press(keycode, record, key) : key_runtime_process_handled_key_release(keycode, record, key);
        if (handled) {
            return false;
        }
    }

    if (key_runtime_process_direct_action_key(keycode, record)) {
        return false;
    }

    if (!record->event.pressed) {
        return true;
    }

    if (macro_dispatch(keycode)) return false;
    return true;
}
