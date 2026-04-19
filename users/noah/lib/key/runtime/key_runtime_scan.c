// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Scan Flow
// ────────────────────────────────────────────────────────────────────────────
//
// Matrix-scan hold promotion and multi-tap expiry handling.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_api.h"
#include "key_runtime_feedback.h"
#include "key_runtime_index_internal.h"
#include "key_runtime_process_internal.h"
#include "key_runtime_trace.h"
#include "key_runtime_transition.h"

__attribute__((weak)) void runtime_v2_observe_scan_cycle(uint16_t now) {
    (void)now;
}

#ifdef NOAH_HOST_TEST_ENV
bool key_runtime_integration_userspace_feeds_runtime_v2_scan_events(void) {
    return true;
}
#endif

void noah_key_runtime_scan(void) {
    key_runtime_transition_plan_t plan;

    runtime_v2_observe_scan_cycle(timer_read());
    key_runtime_index_refresh_timed_deferred_release_blockers();
    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);
    key_runtime_trace_plan("scan", &plan);
    key_runtime_transition_execute_plan(&plan);
    key_runtime_release_drain_deferred_dispatches();
}
