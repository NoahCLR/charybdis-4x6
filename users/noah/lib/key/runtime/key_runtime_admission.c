// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Admission
// ────────────────────────────────────────────────────────────────────────────
//
// Slot lookup and admission policy for handled-key presses.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_admission.h"
#include "key_runtime_index.h"

active_key_state_t *key_runtime_first_active_slot(void) {
    key_runtime_index_rebuild();
    return key_runtime_active_slot_by_order(0);
}

active_key_state_t *key_runtime_find_slot_by_position(keypos_t key_pos) {
    active_key_state_t *slot = key_runtime_slot_for_position(key_pos);

    if (key_runtime_slot_active(slot) && key_runtime_keypos_equal(slot->owner.key_pos, key_pos)) {
        return slot;
    }

    return NULL;
}

active_key_state_t *key_runtime_find_slot_with_pending_multi_tap(keypos_t key_pos) {
    active_key_state_t *slot = key_runtime_slot_for_position(key_pos);

    if (!key_runtime_slot_active(slot) && key_runtime_slot_has_pending_multi_tap(slot) && key_runtime_keypos_equal(slot->pending_multi_tap.key_pos, key_pos)) {
        return slot;
    }

    return NULL;
}

active_key_state_t *key_runtime_select_slot_for_press(keypos_t key_pos) {
    return key_runtime_slot_for_position(key_pos);
}

bool active_key_matches(uint16_t keycode, keypos_t key_pos) {
    return key_runtime_slot_matches(key_runtime_find_slot_by_position(key_pos), keycode, key_pos);
}
