// ────────────────────────────────────────────────────────────────────────────
// Key Runtime
// ────────────────────────────────────────────────────────────────────────────
//
// Shared state and helper functions for the split key runtime modules.
// ────────────────────────────────────────────────────────────────────────────

#include "../interaction/handled_key.h"
#include "key_runtime_api.h"
#include "key_runtime_index.h"
#include "key_runtime_internal.h"
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

static bool key_runtime_activate_pending_fallback_hold(void) {
    return key_runtime_slot_activate_pending_fallback_hold(key_runtime_pending_fallback_slot());
}

bool noah_key_runtime_settle_pending_fallback_hold(void) {
    return key_runtime_activate_pending_fallback_hold();
}
