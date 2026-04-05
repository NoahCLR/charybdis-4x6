// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Release Flow
// ────────────────────────────────────────────────────────────────────────────
//
// Handled-key release resolution, including pending multi-tap holds.
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key.h"
#include "key_runtime_process.h"
#include "key_runtime_transition.h"

bool key_runtime_process_handled_key_release(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    key_runtime_transition_plan_t plan;

    key_runtime_transition_plan_init(&plan);
    bool handled = key_runtime_transition_handled_key_release(keycode, record, key, &plan);
    key_runtime_transition_execute_plan(&plan);
    return handled;
}
