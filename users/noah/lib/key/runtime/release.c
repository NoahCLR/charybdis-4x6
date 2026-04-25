// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Release Flow
// ────────────────────────────────────────────────────────────────────────────

#include "../interaction/handled_key.h"
#include "process_internal.h"
#include "trace.h"
#include "transition.h"
#include "core/runtime.h"

static keyboard_mod_state_t key_runtime_release_keyboard_mod_state_current(void) {
    return (keyboard_mod_state_t){
        .real           = get_mods(),
        .weak           = get_weak_mods(),
        .oneshot        = get_oneshot_mods(),
        .oneshot_locked = get_oneshot_locked_mods(),
    };
}

static bool key_runtime_release_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static bool key_runtime_release_effect_is_tap_commit_feedback_for_key(const key_runtime_effect_t *effect, keypos_t key_pos) {
    return effect && effect->kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE && effect->data.feedback_pulse.kind == KEY_FEEDBACK_PULSE_TAP_COMMITTED && key_runtime_release_keypos_equal(effect->data.feedback_pulse.key_pos, key_pos);
}

static void key_runtime_release_plan_defer_dispatch_actions(key_runtime_transition_plan_t *plan, keypos_t key_pos, keyboard_mod_state_t mods) {
    uint8_t write_index = 0u;

    if (!plan) {
        return;
    }

    (void)key_pos;

    for (uint8_t read_index = 0; read_index < plan->count; read_index++) {
        const key_runtime_effect_t effect = plan->items[read_index];

        if (effect.kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION) {
            keypos_t dispatch_key_pos    = key_runtime_effect_dispatch_action_key_pos(&effect);
            bool     tap_commit_feedback = (read_index + 1u) < plan->count && key_runtime_release_effect_is_tap_commit_feedback_for_key(&plan->items[read_index + 1u], dispatch_key_pos);

            if (key_runtime_core_queue_pending_release_dispatch(dispatch_key_pos, effect.data.dispatch_action.action, mods, tap_commit_feedback)) {
                if (tap_commit_feedback) {
                    read_index++;
                }
                continue;
            }
        }

        plan->items[write_index++] = effect;
    }

    plan->count = write_index;
}

bool key_runtime_process_handled_key_release(uint16_t keycode, keyrecord_t *record, handled_key_resolution_t resolution) {
    key_runtime_transition_plan_t plan;
    bool                          handled;

    key_runtime_transition_plan_init(&plan);
    handled = key_runtime_transition_handled_key_release(keycode, record, resolution, &plan);
    if (handled && key_runtime_transition_has_foreign_tap_release_slot_except(record->event.key)) {
        key_runtime_release_plan_defer_dispatch_actions(&plan, record->event.key, key_runtime_release_keyboard_mod_state_current());
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
