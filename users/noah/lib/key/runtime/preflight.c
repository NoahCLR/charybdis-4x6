// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Preflight
// ────────────────────────────────────────────────────────────────────────────

#include "process_internal.h"
#include "../behavior/handled_key.h"
#include "trace.h"
#include "transition.h"
#include "../../action/action_dispatch.h"
#include "reducer/state_query.h"
#include "reducer/ownership_state.h"
#include "../../state/ownership/keyboard_mod_ownership.h"
#include "../../compat/qmk_gesture_timing.h"

// Runs before the runtime observes the release, while the key still holds
// its layer or modifiers: a key that another key's press would settle as a
// tap is settled now, so its tap still gets them (D-F27).
void key_runtime_preflight_settle_before_release(uint16_t keycode, keyrecord_t *record) {
    key_runtime_transition_plan_t plan;

    if (record->event.pressed || !key_runtime_core_has_other_active_press_token(record->event.key) || !key_runtime_core_release_is_mods_or_layer(keycode, record)) {
        return;
    }
    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_settle_active_keys_on_other_release(record->event.key, noah_qmk_gesture_event_time(record), &plan);
    key_runtime_trace_plan("preflight:settle_on_mods_or_layer_release", &plan);
    key_runtime_transition_execute_plan(&plan);
}

bool key_runtime_preflight_record(uint16_t keycode, keyrecord_t *record, bool press_is_handled) {
    const press_token_t *token = key_runtime_core_press_token_at(record->event.key);

    if (keyboard_mod_ownership_should_suppress_default(keycode, record)) {
        if (!record->event.pressed && token && token->handled_key) {
            // Let the handled-key release path run.
        } else {
            if (!record->event.pressed && token && !token->handled_key) {
                // A suppressed native release still retires its reducer
                // leases; finalize separately settles its report components.
                (void)key_runtime_core_finalize_non_handled_release(record->event.key);
            }
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

    if (record->event.pressed && !press_is_handled) {
        key_runtime_transition_plan_t plan;

        key_runtime_transition_plan_init(&plan);
        key_runtime_transition_flush_foreign_multi_tap(keycode, record->event.key, &plan);
        key_runtime_trace_plan("preflight:flush_foreign_multi_tap", &plan);
        key_runtime_transition_execute_plan(&plan);
    } else if (record->event.pressed) {
        // Handled keys keep independent series alive, except a tap/hold key's:
        // its pending tap is typed before the key that followed it.
        key_runtime_transition_plan_t plan;

        key_runtime_transition_plan_init(&plan);
        key_runtime_transition_flush_foreign_dual_role_multi_tap(keycode, record->event.key, &plan);
        key_runtime_trace_plan("preflight:flush_foreign_dual_role_multi_tap", &plan);
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
