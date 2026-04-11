// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Press Flow
// ────────────────────────────────────────────────────────────────────────────
//
// Handled-key press transitions and active-key replacement.
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key.h"
#include "key_runtime_process.h"
#include "key_runtime_state.h"
#include "key_runtime_trace.h"
#include "key_runtime_transition.h"
#include "held_action.h"
#include "../state/split_runtime_sync.h"

bool key_runtime_process_handled_key_press(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    active_key_state_t          *slot = key_runtime_select_slot_for_press(record->event.key);
    key_runtime_transition_plan_t plan;
    bool                          active_held_action_survives_flush = slot->held_action_keycode == KC_NO || held_action_survives_flush(slot->key_pos, slot->held_action_keycode);

    key_runtime_transition_plan_init(&plan);
    bool handled = key_runtime_transition_handled_key_press(slot, keycode, record, key, active_held_action_survives_flush, &plan);
    key_runtime_trace_plan("press", &plan);
    key_runtime_transition_execute_plan(&plan);
    if (handled) {
        split_runtime_sync();
    }
    return handled;
}
