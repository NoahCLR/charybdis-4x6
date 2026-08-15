// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Preflight
// ────────────────────────────────────────────────────────────────────────────

#include "process_internal.h"
#include "../behavior/handled_key.h"
#include "trace.h"
#include "transition.h"
#include "../../action/action_dispatch.h"
#include "../../action/owned_keycode.h"
#include "reducer/state_query.h"
#include "../../state/ownership/keyboard_mod_ownership.h"

bool key_runtime_preflight_record(uint16_t keycode, keyrecord_t *record) {
    handled_key_resolution_t handled_key = handled_key_lookup(keycode);
    const press_token_t     *token       = key_runtime_core_press_token_at(record->event.key);

    if (owned_keycode_should_suppress_default(keycode, record) || keyboard_mod_ownership_should_suppress_default(keycode, record)) {
        if (!record->event.pressed && token && token->handled_key) {
            // Let the handled-key release path run.
        } else {
            key_runtime_trace_message("preflight:suppress_default", "default QMK path suppressed before handled-key runtime");
            return false;
        }
    }

    if (record->event.pressed && key_runtime_core_has_other_active_press_token(record->event.key)) {
        key_runtime_transition_plan_t plan;

        key_runtime_transition_plan_init(&plan);
        key_runtime_transition_interrupt_active_keys_on_other_press(record->event.key, &plan);
        key_runtime_trace_plan("preflight:interrupt_active_key", &plan);
        key_runtime_transition_execute_plan(&plan);
    }

    if (record->event.pressed && !handled_key_resolution_is_handled(handled_key)) {
        key_runtime_transition_plan_t plan;

        key_runtime_transition_plan_init(&plan);
        key_runtime_transition_flush_foreign_multi_tap(keycode, record->event.key, &plan);
        key_runtime_trace_plan("preflight:flush_foreign_multi_tap", &plan);
        key_runtime_transition_execute_plan(&plan);
    }

    return true;
}

bool key_runtime_process_direct_action_key(uint16_t keycode, keyrecord_t *record) {
    noah_action_desc_t desc = noah_action_describe(keycode);

    if (!noah_action_desc_consumes_direct_press(desc)) {
        return false;
    }

    if (record->event.pressed) {
        key_runtime_trace_record("preflight:direct_action_dispatch", keycode, record);
        noah_emit_action_tap_at(record->event.key, keycode, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);
    }

    return true;
}
