// ────────────────────────────────────────────────────────────────────────────
// Key Runtime
// ────────────────────────────────────────────────────────────────────────────

#include "api.h"
#include "transition.h"
#include "reducer/runtime.h"

bool noah_key_runtime_settle_pending_fallback_hold(void) {
    key_runtime_transition_plan_t plan;
    bool                          settled_any;

    key_runtime_transition_plan_init(&plan);
    settled_any = key_runtime_transition_settle_pending_fallback_hold(&plan);
    key_runtime_transition_execute_plan(&plan);
    return settled_any;
}

void noah_key_runtime_activity_snapshot(noah_key_runtime_activity_snapshot_t *out) {
    key_runtime_core_state_t *state;

    if (!out) {
        return;
    }

    *out  = (noah_key_runtime_activity_snapshot_t){0};
    state = key_runtime_core_state();
    if (!state) {
        return;
    }

    *out = (noah_key_runtime_activity_snapshot_t){
        .press_token_count       = state->press_token_count,
        .tap_series_count        = state->tap_series_count,
        .lease_count             = state->lease_count,
        .pending_release_count   = state->pending_release_count,
        .persistent_intent_count = state->persistent_intent_count,
    };
}
