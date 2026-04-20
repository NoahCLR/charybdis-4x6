// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Release Flow
// ────────────────────────────────────────────────────────────────────────────

#include "../interaction/handled_key.h"
#include "process_internal.h"
#include "trace.h"
#include "transition.h"
#include "core/runtime.h"
#include "../../state/runtime/split_runtime_sync.h"

static keyboard_mod_state_t key_runtime_release_keyboard_mod_state_current(void) {
    return (keyboard_mod_state_t){
        .real           = get_mods(),
        .weak           = get_weak_mods(),
        .oneshot        = get_oneshot_mods(),
        .oneshot_locked = get_oneshot_locked_mods(),
    };
}

static void key_runtime_release_plan_defer_dispatch_actions(key_runtime_transition_plan_t *plan, keypos_t key_pos, keyboard_mod_state_t mods) {
    uint8_t write_index = 0u;

    if (!plan) {
        return;
    }

    for (uint8_t read_index = 0; read_index < plan->count; read_index++) {
        const key_runtime_effect_t effect = plan->items[read_index];

        if (effect.kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION && runtime_v2_queue_pending_release_dispatch(key_pos, effect.data.action, mods)) {
            continue;
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
        split_runtime_sync_request();
    }
    return handled;
}

void key_runtime_release_drain_deferred_dispatches(void) {
    pending_release_t pending[RUNTIME_V2_PENDING_RELEASE_CAPACITY];
    uint8_t           drained = runtime_v2_take_pending_release_dispatches(pending, ARRAY_SIZE(pending));

    for (uint8_t index = 0; index < drained; index++) {
        runtime_v2_project_pending_release_dispatch(&pending[index]);
    }
}
