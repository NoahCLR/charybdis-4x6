// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Process Flow
// ────────────────────────────────────────────────────────────────────────────
//
// Press/release handling and process_record_user integration.
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key.h"
#include "key_runtime_process.h"
#include "key_runtime_state.h"
#include "key_runtime_trace.h"
#include "noah_keymap.h"
#include "../action/macro_dispatch.h"
#include "../pointing/pd_modes.h"
#include "../action/synthetic_record.h"

bool noah_get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

bool noah_process_record_user(uint16_t keycode, keyrecord_t *record) {
    key_runtime_trace_record("process:entry", keycode, record);

    // Synthetic records are dispatched by the key_behavior action system to
    // route keymap-local custom keycodes back through process_record_user().
    // Skip the physical key runtime so the keymap-local handler can run.
    if (noah_synthetic_record_active()) {
        key_runtime_trace_message("process:synthetic_passthrough", "synthetic record bypassed physical runtime");
        return true;
    }

    if (!key_runtime_preflight_record(keycode, record)) {
        key_runtime_trace_bool_result("process:preflight", keycode, record, false);
        return false;
    }
    key_runtime_trace_bool_result("process:preflight", keycode, record, true);

    if (pd_mode_handle_key_event(keycode, record)) {
        key_runtime_trace_message("process:pd_mode_key_handler", "key event consumed by active pd mode");
        return false;
    }

    handled_key_view_t key = handled_key_lookup(keycode);
    if (key.behavior.handled) {
        bool handled = record->event.pressed ? key_runtime_process_handled_key_press(keycode, record, key) : key_runtime_process_handled_key_release(keycode, record, key);
        key_runtime_trace_bool_result("process:handled_key", keycode, record, handled);
        if (handled) {
            return false;
        }
    }

    if (key_runtime_process_direct_action_key(keycode, record)) {
        key_runtime_trace_message("process:direct_action", "direct action key consumed");
        return false;
    }

    if (!record->event.pressed) {
        key_runtime_trace_bool_result("process:return", keycode, record, true);
        return true;
    }

    if (macro_dispatch(keycode)) {
        key_runtime_trace_message("process:macro_dispatch", "macro keycode consumed");
        return false;
    }

    key_runtime_trace_bool_result("process:return", keycode, record, true);
    return true;
}
