// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Press Flow
// ────────────────────────────────────────────────────────────────────────────

#include "../behavior/handled_key.h"
#include "process_internal.h"
#include "trace.h"
#include "transition.h"

static __attribute__((noinline)) bool key_runtime_process_plan_handled_key_press(uint16_t keycode, keyrecord_t *record, const handled_key_resolution_t *resolution, key_runtime_transition_plan_t *plan) {
    key_runtime_transition_plan_init(plan);
    return key_runtime_transition_handled_key_press(keycode, record->event.key, *resolution, plan);
}

bool key_runtime_process_handled_key_press(uint16_t keycode, keyrecord_t *record, const handled_key_resolution_t *resolution) {
    key_runtime_transition_plan_t plan;
    bool                          handled;

    if (!(record && resolution)) {
        return false;
    }

    handled = key_runtime_process_plan_handled_key_press(keycode, record, resolution, &plan);
    key_runtime_trace_plan("press", &plan);
    key_runtime_transition_execute_plan(&plan);
    return handled;
}
