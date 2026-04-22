// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Press Flow
// ────────────────────────────────────────────────────────────────────────────

#include "../interaction/handled_key.h"
#include "process_internal.h"
#include "trace.h"
#include "transition.h"

bool key_runtime_process_handled_key_press(uint16_t keycode, keyrecord_t *record, handled_key_resolution_t resolution) {
    key_runtime_transition_plan_t plan;
    bool                          handled;

    key_runtime_transition_plan_init(&plan);
    handled = key_runtime_transition_handled_key_press(keycode, record->event.key, resolution, &plan);
    key_runtime_trace_plan("press", &plan);
    key_runtime_transition_execute_plan(&plan);
    return handled;
}
