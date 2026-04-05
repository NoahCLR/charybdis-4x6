// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Press Flow
// ────────────────────────────────────────────────────────────────────────────
//
// Handled-key press transitions and active-key replacement.
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key.h"
#include "key_runtime_effects.h"
#include "key_runtime_process.h"
#include "key_runtime_state.h"
#include "key_runtime_transition.h"

bool key_runtime_process_handled_key_press(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    key_runtime_transition_plan_t plan;
    bool                          active_held_action_survives_flush =
        active_key.held_action_keycode == KC_NO || key_runtime_effects_held_action_survives_flush(active_key.key_pos, active_key.held_action_keycode);

    key_runtime_transition_plan_init(&plan);
    bool handled = key_runtime_transition_handled_key_press(keycode, record, key, active_held_action_survives_flush, &plan);
    key_runtime_transition_execute_plan(&plan);
    return handled;
}
