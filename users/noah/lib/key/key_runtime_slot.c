// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slots
// ────────────────────────────────────────────────────────────────────────────
//
// Shared slot storage helpers for the split key runtime modules.
// This module centralizes slot lookup and lifecycle so higher-level code does
// not depend on the raw shared-state layout.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_state.h"
#include "key_behavior_lookup.h"
#include <stddef.h>

bool key_runtime_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

active_key_state_t *key_runtime_primary_slot(void) {
    return &noah_runtime_shared_state.key.active_slots[0];
}

active_key_state_t *key_runtime_slot_at(uint8_t index) {
    if (index >= KEY_RUNTIME_ACTIVE_SLOT_CAPACITY) {
        return NULL;
    }

    return &noah_runtime_shared_state.key.active_slots[index];
}

active_key_state_t *key_runtime_first_active_slot(void) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (key_runtime_slot_active(slot)) {
            return slot;
        }
    }

    return NULL;
}

uint8_t key_runtime_slot_index(const active_key_state_t *slot) {
    if (!slot) {
        return KEY_RUNTIME_ACTIVE_SLOT_CAPACITY;
    }

    const active_key_state_t *base = &noah_runtime_shared_state.key.active_slots[0];

    if (slot < base || slot >= base + KEY_RUNTIME_ACTIVE_SLOT_CAPACITY) {
        return KEY_RUNTIME_ACTIVE_SLOT_CAPACITY;
    }

    return (uint8_t)(slot - base);
}

bool key_runtime_slot_idle(const active_key_state_t *slot) {
    return slot != NULL && slot->keycode == KC_NO && !multi_tap_active(&slot->pending_multi_tap);
}

bool key_runtime_slot_active(const active_key_state_t *slot) {
    return slot != NULL && slot->keycode != KC_NO;
}

bool key_runtime_slot_matches(const active_key_state_t *slot, uint16_t keycode, keypos_t key_pos) {
    return key_runtime_slot_active(slot) && slot->keycode == keycode && key_runtime_keypos_equal(slot->key_pos, key_pos);
}

bool key_runtime_slot_owns_key_position(const active_key_state_t *slot, keypos_t key_pos) {
    return (key_runtime_slot_active(slot) && key_runtime_keypos_equal(slot->key_pos, key_pos)) || (key_runtime_slot_has_pending_multi_tap(slot) && key_runtime_keypos_equal(slot->pending_multi_tap.key_pos, key_pos));
}

bool key_runtime_slot_has_pending_multi_tap(const active_key_state_t *slot) {
    return slot != NULL && multi_tap_active(&slot->pending_multi_tap);
}

bool key_runtime_slot_pending_multi_tap_matches(const active_key_state_t *slot, uint16_t keycode, keypos_t key_pos) {
    return slot != NULL && multi_tap_matches(&slot->pending_multi_tap, keycode, key_pos);
}

bool key_runtime_slot_pending_multi_tap_pending_hold(const active_key_state_t *slot) {
    return slot != NULL && multi_tap_pending_hold(&slot->pending_multi_tap);
}

bool key_runtime_slot_pending_multi_tap_expired(const active_key_state_t *slot) {
    return slot != NULL && multi_tap_expired(&slot->pending_multi_tap);
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

multi_tap_t *key_runtime_primary_multi_tap(void) {
    return &noah_runtime_shared_state.key.active_slots[0].pending_multi_tap;
}

multi_tap_t *key_runtime_multi_tap_slot_at(uint8_t index) {
    active_key_state_t *slot = key_runtime_slot_at(index);

    if (!slot) {
        return NULL;
    }

    return &slot->pending_multi_tap;
}

multi_tap_t *key_runtime_multi_tap_for_slot(const active_key_state_t *slot) {
    if (!slot) {
        return NULL;
    }

    return (multi_tap_t *)&slot->pending_multi_tap;
}

active_key_state_t *key_runtime_slot_for_multi_tap(const multi_tap_t *mt) {
    if (!mt) {
        return NULL;
    }

    const active_key_state_t *base = &noah_runtime_shared_state.key.active_slots[0];
    const multi_tap_t        *min  = &base[0].pending_multi_tap;
    const multi_tap_t        *max  = &base[KEY_RUNTIME_ACTIVE_SLOT_CAPACITY - 1].pending_multi_tap;

    if (mt < min || mt > max) {
        return NULL;
    }

    return (active_key_state_t *)((char *)mt - offsetof(active_key_state_t, pending_multi_tap));
}

bool key_runtime_multi_tap_slot_active(const multi_tap_t *mt) {
    return mt != NULL && multi_tap_active(mt);
}

multi_tap_t *key_runtime_first_active_multi_tap(void) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        multi_tap_t *mt = key_runtime_multi_tap_slot_at(index);

        if (key_runtime_multi_tap_slot_active(mt)) {
            return mt;
        }
    }

    return NULL;
}

multi_tap_t *key_runtime_find_multi_tap_by_position(keypos_t key_pos) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (key_runtime_slot_has_pending_multi_tap(slot) && key_runtime_keypos_equal(slot->pending_multi_tap.key_pos, key_pos)) {
            return &slot->pending_multi_tap;
        }
    }

    return NULL;
}

void key_runtime_slot_begin_pending_multi_tap(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t single_action, uint16_t tap_hold_term, uint16_t multi_tap_term) {
    if (!slot) {
        return;
    }

    multi_tap_begin(&slot->pending_multi_tap, keycode, key_pos, single_action, tap_hold_term, multi_tap_term);
}

uint16_t key_runtime_slot_advance_pending_multi_tap(active_key_state_t *slot, uint16_t keycode) {
    if (!slot) {
        return KC_NO;
    }

    return multi_tap_advance(&slot->pending_multi_tap, keycode, key_behavior_step_lookup, key_behavior_has_more_taps);
}

uint16_t key_runtime_slot_resolve_pending_multi_tap_hold(active_key_state_t *slot, uint16_t keycode, uint8_t *repeat_count) {
    if (!slot) {
        if (repeat_count) {
            *repeat_count = 0;
        }
        return KC_NO;
    }

    return multi_tap_resolve_hold(&slot->pending_multi_tap, keycode, key_behavior_has_more_taps, repeat_count);
}

void key_runtime_slot_reset_pending_multi_tap(active_key_state_t *slot) {
    if (!slot) {
        return;
    }

    multi_tap_reset(&slot->pending_multi_tap);
}

bool active_key_matches(uint16_t keycode, keypos_t key_pos) {
    return key_runtime_slot_matches(key_runtime_find_slot_by_position(key_pos), keycode, key_pos);
}

void key_runtime_slot_reset(active_key_state_t *slot) {
    if (!slot) {
        return;
    }

    *slot = (active_key_state_t)ACTIVE_KEY_STATE_INIT;
}

void active_key_reset(void) {
    key_runtime_slot_reset(key_runtime_primary_slot());
}

void key_runtime_slot_track(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired) {
    if (!slot) {
        return;
    }

    multi_tap_t pending_multi_tap = slot->pending_multi_tap;

    *slot = (active_key_state_t){
        .timer               = timer_read(),
        .keycode             = keycode,
        .key_pos             = key_pos,
        .hold_fired          = hold_fired,
        .held_action_keycode = KC_NO,
        .tap_action          = tap_action,
        .tap_hold_term       = tap_hold_term,
        .longer_hold_term    = longer_hold_term,
        .multi_tap_term      = multi_tap_term,
        .hold                = hold,
        .long_hold           = long_hold,
        .pending_multi_tap   = pending_multi_tap,
    };
}

void active_key_track(uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired) {
    key_runtime_slot_track(key_runtime_primary_slot(), keycode, key_pos, tap_action, hold, long_hold, tap_hold_term, longer_hold_term, multi_tap_term, hold_fired);
}
