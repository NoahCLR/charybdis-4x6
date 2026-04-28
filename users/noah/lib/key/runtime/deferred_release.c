// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Deferred Release Transport
// ────────────────────────────────────────────────────────────────────────────

#include "deferred_release.h"

#include "core/pending_release_queue.h"
#include "core/projection.h"
#include "core/state_query.h"

static keyboard_mod_state_t key_runtime_deferred_release_keyboard_mod_state_current(void) {
    return (keyboard_mod_state_t){
        .real           = get_mods(),
        .weak           = get_weak_mods(),
        .oneshot        = get_oneshot_mods(),
        .oneshot_locked = get_oneshot_locked_mods(),
    };
}

static bool key_runtime_deferred_release_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static bool key_runtime_deferred_release_effect_is_tap_commit_feedback_for_key(const key_runtime_effect_t *effect, keypos_t key_pos) {
    return effect && effect->kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE && effect->data.feedback_pulse.kind == KEY_FEEDBACK_PULSE_TAP_COMMITTED && key_runtime_deferred_release_keypos_equal(effect->data.feedback_pulse.key_pos, key_pos);
}

void key_runtime_deferred_release_defer_dispatch_actions_until_release(keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    keyboard_mod_state_t mods;
    uint8_t              write_index = 0u;

    if (!(plan && key_runtime_core_has_foreign_deferred_release_blocker_except(key_pos))) {
        return;
    }

    mods = key_runtime_deferred_release_keyboard_mod_state_current();
    for (uint8_t read_index = 0; read_index < plan->count; read_index++) {
        const key_runtime_effect_t effect = plan->items[read_index];

        if (effect.kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION) {
            keypos_t dispatch_key_pos    = key_runtime_effect_dispatch_action_key_pos(&effect);
            bool     tap_commit_feedback = (read_index + 1u) < plan->count && key_runtime_deferred_release_effect_is_tap_commit_feedback_for_key(&plan->items[read_index + 1u], dispatch_key_pos);

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

void key_runtime_deferred_release_drain_dispatches(void) {
    pending_release_t pending[KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY];
    uint8_t           drained = key_runtime_core_take_pending_release_dispatches(pending, ARRAY_SIZE(pending));

    for (uint8_t index = 0; index < drained; index++) {
        key_runtime_core_project_pending_release_dispatch(&pending[index]);
    }
}
