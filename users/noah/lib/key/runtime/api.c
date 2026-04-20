// ────────────────────────────────────────────────────────────────────────────
// Key Runtime
// ────────────────────────────────────────────────────────────────────────────

#include "api.h"
#include "transition.h"
#include "core/runtime.h"

bool noah_key_runtime_settle_pending_fallback_hold(void) {
    key_runtime_core_effect_plan_t core_plan;
    bool                           settled_any;

    key_runtime_core_effect_plan_init(&core_plan);
    settled_any = key_runtime_core_settle_pending_fallback_hold(&core_plan);
    for (uint8_t index = 0; index < core_plan.count; index++) {
        key_runtime_core_project_effect(&core_plan.items[index]);
    }
    return settled_any;
}
