// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Release Flow
// ────────────────────────────────────────────────────────────────────────────
//
// Handled-key release resolution, including pending multi-tap holds.
// ────────────────────────────────────────────────────────────────────────────

#include "../interaction/handled_key.h"
#include "key_runtime_process_internal.h"
#include "key_runtime_index_internal.h"
#include "key_runtime_trace.h"
#include "key_runtime_transition.h"
#include "../../runtime_v2/runtime_v2.h"
#include "../../runtime_v2/runtime_v2_projection.h"
#include "../../state/runtime/split_runtime_sync.h"

static keyboard_mod_state_t key_runtime_release_keyboard_mod_state_current(void) {
    return (keyboard_mod_state_t){
        .real           = get_mods(),
        .weak           = get_weak_mods(),
        .oneshot        = get_oneshot_mods(),
        .oneshot_locked = get_oneshot_locked_mods(),
    };
}

__attribute__((weak)) void runtime_v2_project_pending_release_dispatch(const pending_release_t *pending) {
    if (!pending) {
        return;
    }

    dispatch_delayed_action(pending->action, pending->mods);
}

static void key_runtime_release_plan_defer_dispatch_actions(key_runtime_transition_plan_t *plan, keypos_t key_pos, keyboard_mod_state_t mods) {
    key_runtime_deferred_release_dispatch_queue_t *queue = &key_runtime_shared_state()->deferred_release_dispatch;
    uint8_t write_index = 0;

    if (!plan) {
        return;
    }

    for (uint8_t read_index = 0; read_index < plan->count; read_index++) {
        const key_runtime_effect_t effect = plan->items[read_index];

        if (effect.kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION && queue->count < ARRAY_SIZE(queue->items)) {
            queue->items[queue->count++] = (key_runtime_deferred_release_dispatch_t){
                .key_pos = key_pos,
                .action = effect.data.action,
                .mods   = mods,
            };
            runtime_v2_observe_release_dispatch_deferred(key_pos, effect.data.action, mods);
            continue;
        }

        plan->items[write_index++] = effect;
    }

    plan->count = write_index;
}

bool key_runtime_process_handled_key_release(uint16_t keycode, keyrecord_t *record, handled_key_resolution_t resolution) {
    key_runtime_transition_plan_t plan;

    key_runtime_transition_plan_init(&plan);
    bool handled = key_runtime_transition_handled_key_release(keycode, record, resolution, &plan);
    if (handled && key_runtime_transition_has_foreign_tap_release_slot_except(record->event.key)) {
        key_runtime_release_plan_defer_dispatch_actions(&plan, record->event.key, key_runtime_release_keyboard_mod_state_current());
    }
    key_runtime_trace_plan("release", &plan);
    key_runtime_transition_execute_plan(&plan);
    if (handled) {
        key_runtime_release_drain_deferred_dispatches();
        split_runtime_sync();
    }
    return handled;
}

void key_runtime_release_drain_deferred_dispatches(void) {
    key_runtime_deferred_release_dispatch_queue_t *queue = &key_runtime_shared_state()->deferred_release_dispatch;
    pending_release_t                              pending[RUNTIME_V2_PENDING_RELEASE_CAPACITY];

    if (queue->count == 0) {
        return;
    }

    if (runtime_v2_blocker_queries_authoritative() && runtime_v2_pending_release_count() == queue->count) {
        uint8_t drained = runtime_v2_take_pending_release_dispatches(pending, ARRAY_SIZE(pending));

        if (drained == 0u) {
            return;
        }

        for (uint8_t index = 0; index < drained; index++) {
            runtime_v2_project_pending_release_dispatch(&pending[index]);
        }

        queue->count = 0;
        return;
    }

    if (key_runtime_transition_has_any_tap_release_slot()) {
        return;
    }

    for (uint8_t index = 0; index < queue->count; index++) {
        runtime_v2_project_pending_release_dispatch(&(pending_release_t){
            .action = queue->items[index].action,
            .mods   = queue->items[index].mods,
        });
        runtime_v2_observe_release_dispatch_drained(queue->items[index].key_pos, queue->items[index].action, queue->items[index].mods);
    }

    queue->count = 0;
}
