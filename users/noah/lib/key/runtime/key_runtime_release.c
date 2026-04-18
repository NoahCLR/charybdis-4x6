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
#include "../../state/runtime/split_runtime_sync.h"

static keyboard_mod_state_t key_runtime_release_keyboard_mod_state_current(void) {
    return (keyboard_mod_state_t){
        .real           = get_mods(),
        .weak           = get_weak_mods(),
        .oneshot        = get_oneshot_mods(),
        .oneshot_locked = get_oneshot_locked_mods(),
    };
}

static void key_runtime_release_plan_defer_dispatch_actions(key_runtime_transition_plan_t *plan, keyboard_mod_state_t mods) {
    key_runtime_deferred_release_dispatch_queue_t *queue = &key_runtime_shared_state()->deferred_release_dispatch;
    uint8_t write_index = 0;

    if (!plan) {
        return;
    }

    for (uint8_t read_index = 0; read_index < plan->count; read_index++) {
        const key_runtime_effect_t effect = plan->items[read_index];

        if (effect.kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION && queue->count < ARRAY_SIZE(queue->items)) {
            queue->items[queue->count++] = (key_runtime_deferred_release_dispatch_t){
                .action = effect.data.action,
                .mods   = mods,
            };
            continue;
        }

        plan->items[write_index++] = effect;
    }

    plan->count = write_index;
}

bool key_runtime_process_handled_key_release(uint16_t keycode, keyrecord_t *record, handled_key_resolution_t resolution) {
    key_runtime_transition_plan_t plan;

    key_runtime_transition_plan_init(&plan);
#if defined(NOAH_DIAGNOSTIC_FLUSH_FOREIGN_ACTIVE_ON_RELEASE)
    key_runtime_transition_flush_active_keys_except(record->event.key, &plan);
#elif defined(NOAH_DIAGNOSTIC_FLUSH_FOREIGN_TAP_RELEASE_ON_RELEASE)
    key_runtime_transition_flush_foreign_tap_release_slots_except(record->event.key, &plan);
#endif
    bool handled = key_runtime_transition_handled_key_release(keycode, record, resolution, &plan);
    if (handled && key_runtime_transition_has_foreign_tap_release_slot_except(record->event.key)) {
        key_runtime_release_plan_defer_dispatch_actions(&plan, key_runtime_release_keyboard_mod_state_current());
    }
    key_runtime_trace_plan("release", &plan);
    key_runtime_transition_execute_plan(&plan);
    if (handled) {
#if defined(NOAH_DIAGNOSTIC_SKIP_RELEASE_SPLIT_SYNC_WITH_ACTIVE_SIBLING)
        if (key_runtime_active_slot_count() == 0) {
            split_runtime_sync();
        }
#else
        split_runtime_sync();
#endif
    }
    return handled;
}

void key_runtime_release_drain_deferred_dispatches(void) {
    key_runtime_deferred_release_dispatch_queue_t *queue = &key_runtime_shared_state()->deferred_release_dispatch;

    if (queue->count == 0 || key_runtime_transition_has_any_tap_release_slot()) {
        return;
    }

    for (uint8_t index = 0; index < queue->count; index++) {
        dispatch_delayed_action(queue->items[index].action, queue->items[index].mods);
    }

    queue->count = 0;
}
