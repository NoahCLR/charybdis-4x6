// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Release Flow
// ────────────────────────────────────────────────────────────────────────────

#include "../behavior/handled_key.h"
#include "deferred_release.h"
#include "process_internal.h"
#include "trace.h"
#include "transition.h"
#include "../../split/runtime_sync.h"
#include "reducer/runtime.h"

static __attribute__((noinline)) bool key_runtime_process_plan_handled_key_release(uint16_t keycode, keyrecord_t *record, const handled_key_resolution_t *resolution, key_runtime_transition_plan_t *plan) {
    bool handled;

    key_runtime_transition_plan_init(plan);
    handled = key_runtime_transition_handled_key_release(keycode, record, resolution, plan);
    if (handled) {
        key_runtime_deferred_release_defer_dispatch_actions_until_release(record->event.key, plan);
    }

    return handled;
}

bool key_runtime_process_handled_key_release(uint16_t keycode, keyrecord_t *record, const handled_key_resolution_t *resolution) {
    key_runtime_transition_plan_t plan;
    key_runtime_core_state_t     *state                    = key_runtime_core_state();
    uint32_t                      feedback_sequence_before = state ? state->next_feedback_sequence : 0u;
    bool                          handled;

    if (!record) {
        return false;
    }

    handled = key_runtime_process_plan_handled_key_release(keycode, record, resolution, &plan);
    key_runtime_process_notify_planless_feedback_change(&plan, feedback_sequence_before);
    key_runtime_trace_plan("release", &plan);
    key_runtime_transition_execute_plan(&plan);
    return handled;
}
