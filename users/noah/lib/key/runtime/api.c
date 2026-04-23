// ────────────────────────────────────────────────────────────────────────────
// Key Runtime
// ────────────────────────────────────────────────────────────────────────────

#include "api.h"
#include "transition.h"
#include "core/runtime.h"

bool noah_key_runtime_settle_pending_fallback_hold(void) {
    key_runtime_transition_plan_t plan;
    bool                          settled_any;

    key_runtime_transition_plan_init(&plan);
    settled_any = key_runtime_transition_settle_pending_fallback_hold(&plan);
    key_runtime_transition_execute_plan(&plan);
    return settled_any;
}
