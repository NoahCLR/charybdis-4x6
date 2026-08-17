// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Press Flow
// ────────────────────────────────────────────────────────────────────────────

#include "../behavior/handled_key.h"
#include "process_internal.h"
#include "trace.h"
#include "transition.h"
#include "../../split/runtime_sync.h"
#include "reducer/runtime.h"

static __attribute__((noinline)) bool key_runtime_process_plan_handled_key_press(uint16_t keycode, keyrecord_t *record, key_runtime_transition_plan_t *plan) {
    key_runtime_transition_plan_init(plan);
    return key_runtime_transition_handled_key_press(keycode, record->event.key, plan);
}

// A key event can change what the feedback maps render without producing any
// transition plan: opening a multi-tap window makes a pending tap series
// visible while the tap itself is still deferred. execute_plan only announces
// non-empty plans, so without this the split worker never learns and the slave
// renders nothing while the master lights up. Mirrors the same check in
// noah_key_runtime_scan().
void key_runtime_process_notify_planless_feedback_change(const key_runtime_transition_plan_t *plan, uint32_t feedback_sequence_before) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (plan && plan->count == 0u && state && state->next_feedback_sequence != feedback_sequence_before) {
        split_runtime_sync_notify_key_feedback_dirty();
    }
}

bool key_runtime_process_handled_key_press(uint16_t keycode, keyrecord_t *record) {
    key_runtime_transition_plan_t plan;
    key_runtime_core_state_t     *state                    = key_runtime_core_state();
    uint32_t                      feedback_sequence_before = state ? state->next_feedback_sequence : 0u;
    bool                          handled;

    if (!record) {
        return false;
    }

    handled = key_runtime_process_plan_handled_key_press(keycode, record, &plan);
    key_runtime_process_notify_planless_feedback_change(&plan, feedback_sequence_before);
    key_runtime_trace_plan("press", &plan);
    key_runtime_transition_execute_plan(&plan);
    return handled;
}
