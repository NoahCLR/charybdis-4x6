// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Admission
// ────────────────────────────────────────────────────────────────────────────
//
// Slot lookup and admission policy for handled-key presses.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_admission.h"

#include <stddef.h>

active_key_state_t *key_runtime_first_active_slot(void) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (key_runtime_slot_active(slot)) {
            return slot;
        }
    }

    return NULL;
}

active_key_state_t *key_runtime_find_slot_by_position(keypos_t key_pos) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (key_runtime_slot_active(slot) && key_runtime_keypos_equal(slot->key_pos, key_pos)) {
            return slot;
        }
    }

    return NULL;
}

active_key_state_t *key_runtime_find_slot_with_pending_multi_tap(keypos_t key_pos) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (!key_runtime_slot_active(slot) && key_runtime_slot_has_pending_multi_tap(slot) && key_runtime_keypos_equal(slot->pending_multi_tap.key_pos, key_pos)) {
            return slot;
        }
    }

    return NULL;
}

active_key_state_t *key_runtime_find_free_slot(void) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (key_runtime_slot_idle(slot)) {
            return slot;
        }
    }

    return NULL;
}

active_key_state_t *key_runtime_find_reclaimable_slot(void) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (!key_runtime_slot_active(slot) && key_runtime_slot_has_pending_multi_tap(slot)) {
            return slot;
        }
    }

    return NULL;
}

active_key_state_t *key_runtime_select_slot_for_press(keypos_t key_pos) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (key_runtime_slot_owns_key_position(slot, key_pos)) {
            return slot;
        }
    }

    active_key_state_t *slot = key_runtime_find_free_slot();
    if (slot) {
        return slot;
    }

    slot = key_runtime_find_reclaimable_slot();
    if (slot) {
        return slot;
    }

    return key_runtime_primary_slot();
}

bool active_key_matches(uint16_t keycode, keypos_t key_pos) {
    return key_runtime_slot_matches(key_runtime_find_slot_by_position(key_pos), keycode, key_pos);
}
