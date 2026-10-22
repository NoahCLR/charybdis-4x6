// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Release Flow
// ────────────────────────────────────────────────────────────────────────────

#include "../behavior/handled_key.h"
#include "deferred_release.h"
#include "process_internal.h"
#include "transition.h"
#include "planning/effect_plan.h"
#include "reducer/runtime.h"
#include "../../state/modifiers/keyboard_mod_policy.h"

// Planning is not inlined, so the reducer's locals are off the stack before the
// plan is projected beneath the caller's frame.
static __attribute__((noinline)) bool key_runtime_process_plan_handled_key_release(uint16_t keycode, keyrecord_t *record, const handled_key_resolution_t *resolution, key_runtime_core_effect_plan_t *plan) {
    // The reducer plans every handled release at a matrix position, one it
    // has no token or series for included. Every release reaching here has
    // one: QMK puts a combo's records at one, and this keyboard sends no
    // encoder or DIP-switch events, whose positions lie outside the matrix.
    bool handled = key_runtime_core_handle_handled_key_release(keycode, record->event.key, resolution, keyboard_mod_policy_current_state(), plan);

    if (handled) {
        key_runtime_deferred_release_defer_dispatch_actions_until_release(record->event.key, plan);
    }
    return handled;
}

bool key_runtime_process_handled_key_release(uint16_t keycode, keyrecord_t *record, const handled_key_resolution_t *resolution) {
    key_runtime_core_effect_plan_t plan;
    uint32_t                       feedback_mark = key_runtime_transition_feedback_mark();
    bool                           handled;

    if (!record) {
        return false;
    }

    key_runtime_core_effect_plan_init(&plan);
    handled = key_runtime_process_plan_handled_key_release(keycode, record, resolution, &plan);
    key_runtime_transition_execute_event_plan("release", &plan, feedback_mark);
    return handled;
}
