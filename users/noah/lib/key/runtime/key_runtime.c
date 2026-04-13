// ────────────────────────────────────────────────────────────────────────────
// Key Runtime
// ────────────────────────────────────────────────────────────────────────────
//
// Shared state and helper functions for the split key runtime modules.
// ────────────────────────────────────────────────────────────────────────────

#include "../interaction/handled_key.h"
#include "key_runtime_state.h"
#include "../ownership/held_action.h"

bool key_runtime_slot_activate_pending_fallback_hold(active_key_state_t *slot) {
    if (!(slot && key_runtime_slot_uses_fallback_hold(slot) && slot->lifecycle.held_action_keycode == KC_NO && slot->owner.keycode != KC_NO)) {
        return false;
    }

    slot->lifecycle.held_action_keycode = slot->owner.keycode;
    key_runtime_slot_commit_hold_phase(slot, true);
    held_action_register(slot->owner.key_pos, slot->owner.keycode);
    return true;
}

bool key_runtime_activate_pending_fallback_hold(void) {
    for (uint8_t index = 0; index < KEY_RUNTIME_SLOT_TABLE_CAPACITY; index++) {
        if (key_runtime_slot_activate_pending_fallback_hold(key_runtime_slot_at(index))) {
            return true;
        }
    }

    return false;
}
