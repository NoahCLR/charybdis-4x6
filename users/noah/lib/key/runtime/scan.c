// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Scan Flow
// ────────────────────────────────────────────────────────────────────────────

#include "api.h"
#include "deferred_release.h"
#include "trace.h"
#include "transition.h"
#include "../../pointing/defs/pd_modes.h"
#include "planning/effect_plan.h"
#include "reducer/state_query.h"

#ifdef NOAH_HOST_TEST_ENV
bool key_runtime_integration_userspace_feeds_core_scan_events(void) {
    return true;
}
#endif

static bool noah_key_runtime_scan_has_core_work(void) {
    return key_runtime_core_active_press_token_count() != 0u || key_runtime_core_pending_multi_tap_count() != 0u;
}

// Not inlined, so the scan's plan is off the stack before the deferred
// releases it may have queued are dispatched.
static __attribute__((noinline)) void noah_key_runtime_scan_core(void) {
    key_runtime_core_effect_plan_t plan;
    uint32_t                       feedback_mark = key_runtime_transition_feedback_mark();

    key_runtime_core_effect_plan_init_streaming(&plan);
    key_runtime_core_scan(&plan, timer_read());
    key_runtime_transition_execute_event_plan("scan", &plan, feedback_mark);
}

// Not inlined into matrix_scan(), which would otherwise carry its locals on
// every scan path, the key runtime's or not.
__attribute__((noinline)) void noah_key_runtime_scan(void) {
    bool core_work = noah_key_runtime_scan_has_core_work();

    if (core_work) {
        noah_key_runtime_scan_core();
    }

    if (core_work || key_runtime_deferred_release_has_pending_dispatches()) {
        key_runtime_deferred_release_drain_dispatches();
    }
    pd_mode_service_active_dpi_sync();
}
