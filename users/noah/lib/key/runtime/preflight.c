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
#include "planning/effect_plan.h"
#include "../../state/ownership/keyboard_mod_ownership.h"
#include "../../compat/qmk_gesture_timing.h"

// Runs before the runtime observes the release, at the reducer's moment
// before a release (key_runtime_core_settle_before_release()). Not inlined:
// process_record_user() calls it for every release, and its plan must not stay
// on the stack beneath the stages that follow.
__attribute__((noinline)) void key_runtime_preflight_settle_before_release(uint16_t keycode, keyrecord_t *record) {
    key_runtime_core_effect_plan_t plan;

    if (record->event.pressed) {
        return;
    }
    key_runtime_core_effect_plan_init_streaming(&plan);
    key_runtime_core_settle_before_release(keycode, record, noah_qmk_gesture_event_time(record), &plan);
    key_runtime_transition_execute_plan("preflight:before_release", &plan);
}

// The reducer's moment before a press (key_runtime_core_settle_before_press()).
// Not inlined, so its plan is off the stack once the press goes on.
static __attribute__((noinline)) void key_runtime_preflight_settle_before_press(uint16_t keycode, keyrecord_t *record, bool press_is_handled) {
    key_runtime_core_effect_plan_t plan;

    key_runtime_core_effect_plan_init_streaming(&plan);
    key_runtime_core_settle_before_press(keycode, record->event.key, press_is_handled, &plan);
    key_runtime_transition_execute_plan("preflight:before_press", &plan);
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

    if (record->event.pressed) {
        key_runtime_preflight_settle_before_press(keycode, record, press_is_handled);
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
