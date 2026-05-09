// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Scan Flow
// ────────────────────────────────────────────────────────────────────────────

#include "api.h"
#include "deferred_release.h"
#include "trace.h"
#include "transition.h"
#include "../../pointing/defs/pd_modes.h"
#include "../../split/runtime_sync.h"
#include "reducer/state_query.h"

#ifdef NOAH_HOST_TEST_ENV
bool key_runtime_integration_userspace_feeds_core_scan_events(void) {
    return true;
}
#endif

static bool noah_key_runtime_scan_has_core_work(void) {
    return key_runtime_core_active_press_token_count() != 0u || key_runtime_core_pending_multi_tap_count() != 0u;
}

void noah_key_runtime_scan(void) {
    bool core_work = noah_key_runtime_scan_has_core_work();

    if (core_work) {
        key_runtime_transition_plan_t plan;

        key_runtime_transition_plan_init(&plan);
        key_runtime_transition_scan(&plan);
        split_runtime_sync_notify_key_feedback_dirty();
        key_runtime_trace_plan("scan", &plan);
        key_runtime_transition_execute_plan(&plan);
    }

    if (core_work || key_runtime_deferred_release_has_pending_dispatches()) {
        key_runtime_deferred_release_drain_dispatches();
    }
    pd_mode_service_active_dpi_sync();
}
