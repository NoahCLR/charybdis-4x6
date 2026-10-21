// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Press Flow
// ────────────────────────────────────────────────────────────────────────────

#include "../behavior/handled_key.h"
#include "process_internal.h"
#include "transition.h"
#include "planning/effect_plan.h"
#include "reducer/runtime.h"

bool key_runtime_process_handled_key_press(uint16_t keycode, keyrecord_t *record) {
    key_runtime_core_effect_plan_t plan;
    uint32_t                       feedback_mark = key_runtime_transition_feedback_mark();
    bool                           handled;

    if (!record) {
        return false;
    }

    key_runtime_core_effect_plan_init(&plan);
    handled = key_runtime_core_handle_handled_key_press(keycode, record->event.key, &plan);
    key_runtime_transition_execute_event_plan("press", &plan, feedback_mark);
    return handled;
}
