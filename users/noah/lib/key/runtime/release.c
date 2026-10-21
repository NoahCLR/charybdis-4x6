// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Release Flow
// ────────────────────────────────────────────────────────────────────────────

#include "../behavior/handled_key.h"
#include "deferred_release.h"
#include "process_internal.h"
#include "transition.h"
#include "planning/effect_plan.h"
#include "reducer/runtime.h"
#include "../../state/modifiers/keyboard_mod_policy.h"

// A handled release the reducer has no token or series for still releases
// what its key may own.
static __attribute__((noinline)) void key_runtime_process_plan_unmatched_release(keypos_t key_pos, const handled_key_resolution_t *resolution, key_runtime_core_effect_plan_t *plan) {
    handled_key_resolution_ctx_t ctx = handled_key_resolution_ctx_live(key_pos);

    if (handled_key_resolution_materializes_momentary_layer(resolution, &ctx)) {
        key_runtime_core_effect_plan_push_layer_release(plan, key_pos);
    }
    key_runtime_core_effect_plan_push_release_owned_state(plan, key_pos);
}

// Planning is not inlined, so the reducer's locals are off the stack before the
// plan is projected beneath the caller's frame.
static __attribute__((noinline)) bool key_runtime_process_plan_handled_key_release(uint16_t keycode, keyrecord_t *record, const handled_key_resolution_t *resolution, key_runtime_core_effect_plan_t *plan) {
    bool handled = key_runtime_core_handle_handled_key_release(keycode, record->event.key, resolution, keyboard_mod_policy_current_state(), plan);

    if (!handled && resolution && handled_key_resolution_is_handled(*resolution)) {
        key_runtime_process_plan_unmatched_release(record->event.key, resolution, plan);
        handled = true;
    }
    if (handled) {
        key_runtime_deferred_release_defer_dispatch_actions_until_release(record->event.key, plan);
    }
    return handled;
}

bool key_runtime_process_handled_key_release(uint16_t keycode, keyrecord_t *record, const handled_key_resolution_t *resolution) {
    key_runtime_core_effect_plan_t plan;
    uint32_t                       feedback_mark = key_runtime_transition_feedback_mark();
    bool                           handled;

    if (!record) {
        return false;
    }

    key_runtime_core_effect_plan_init(&plan);
    handled = key_runtime_process_plan_handled_key_release(keycode, record, resolution, &plan);
    key_runtime_transition_execute_event_plan("release", &plan, feedback_mark);
    return handled;
}
