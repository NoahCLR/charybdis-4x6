// ────────────────────────────────────────────────────────────────────────────
// Key Runtime
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_api.h"
#include "key_runtime_transition.h"
#include "../../runtime_v2/runtime_v2.h"

bool noah_key_runtime_settle_pending_fallback_hold(void) {
    runtime_v2_effect_plan_t v2_plan;
    bool                     settled_any;

    runtime_v2_effect_plan_init(&v2_plan);
    settled_any = runtime_v2_settle_pending_fallback_hold(&v2_plan);
    for (uint8_t index = 0; index < v2_plan.count; index++) {
        runtime_v2_project_effect(&v2_plan.items[index]);
    }
    return settled_any;
}
