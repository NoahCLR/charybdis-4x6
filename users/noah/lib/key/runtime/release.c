// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Release Flow
// ────────────────────────────────────────────────────────────────────────────

#include "../interaction/handled_key.h"
#include "process_internal.h"
#include "trace.h"
#include "transition.h"
#include "core/runtime.h"

bool key_runtime_process_handled_key_release(uint16_t keycode, keyrecord_t *record, handled_key_resolution_t resolution) {
    key_runtime_transition_plan_t plan;
    bool                          handled;

    key_runtime_transition_plan_init(&plan);
    handled = key_runtime_transition_handled_key_release(keycode, record, resolution, &plan);
    if (handled && record) {
        key_runtime_transition_defer_dispatch_actions_until_release(record->event.key, &plan);
    }
    key_runtime_trace_plan("release", &plan);
    key_runtime_transition_execute_plan(&plan);
    if (handled) {
        key_runtime_release_drain_deferred_dispatches();
    }
    return handled;
}

void key_runtime_release_drain_deferred_dispatches(void) {
    pending_release_t pending[KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY];
    uint8_t           drained = key_runtime_core_take_pending_release_dispatches(pending, ARRAY_SIZE(pending));

    for (uint8_t index = 0; index < drained; index++) {
        key_runtime_core_project_pending_release_dispatch(&pending[index]);
    }
}
