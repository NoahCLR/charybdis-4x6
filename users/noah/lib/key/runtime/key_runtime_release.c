// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Release Flow
// ────────────────────────────────────────────────────────────────────────────
//
// Handled-key release resolution, including pending multi-tap holds.
// ────────────────────────────────────────────────────────────────────────────

#include "../interaction/handled_key.h"
#include "key_runtime_process.h"
#include "key_runtime_trace.h"
#include "key_runtime_transition.h"
#include "../../state/runtime/split_runtime_sync.h"

bool key_runtime_process_handled_key_release(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    key_runtime_transition_plan_t plan;

    key_runtime_transition_plan_init(&plan);
    bool handled = key_runtime_transition_handled_key_release(keycode, record, key, &plan);
    key_runtime_trace_plan("release", &plan);
    key_runtime_transition_execute_plan(&plan);
    if (handled) {
        split_runtime_sync();
    }
    return handled;
}
