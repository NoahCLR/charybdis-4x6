// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Scan Flow
// ────────────────────────────────────────────────────────────────────────────
//
// Matrix-scan hold promotion, repeat dispatch, and multi-tap expiry handling.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_feedback.h"
#include "held_repeat.h"
#include "key_runtime_trace.h"
#include "key_runtime_transition.h"

void noah_key_runtime_scan(void) {
    key_runtime_transition_plan_t plan;

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);
    key_runtime_trace_plan("scan", &plan);
    key_runtime_transition_execute_plan(&plan);
    held_repeat_tick();
}
