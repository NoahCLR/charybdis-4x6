// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Scan Flow
// ────────────────────────────────────────────────────────────────────────────

#include "api.h"
#include "process_internal.h"
#include "trace.h"
#include "transition.h"
#include "../../pointing/defs/pd_modes.h"

#ifdef NOAH_HOST_TEST_ENV
bool key_runtime_integration_userspace_feeds_runtime_v2_scan_events(void) {
    return true;
}
#endif

void noah_key_runtime_scan(void) {
    key_runtime_transition_plan_t plan;

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);
    key_runtime_trace_plan("scan", &plan);
    key_runtime_transition_execute_plan(&plan);
    key_runtime_release_drain_deferred_dispatches();
    pd_mode_service_active_dpi_sync();
}
