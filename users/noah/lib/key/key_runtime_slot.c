// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slots
// ────────────────────────────────────────────────────────────────────────────
//
// Shared slot storage helpers for the split key runtime modules.
// The slot container currently has capacity 1, but this module centralizes
// slot lookup and lifecycle so higher-level code does not depend on the raw
// shared-state layout.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_state.h"

static bool key_runtime_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

active_key_state_t *key_runtime_primary_slot(void) {
    return &noah_runtime_shared_state.key.active_slots[0];
}

bool key_runtime_slot_active(const active_key_state_t *slot) {
    return slot != NULL && slot->keycode != KC_NO;
}

bool key_runtime_slot_matches(const active_key_state_t *slot, uint16_t keycode, keypos_t key_pos) {
    return key_runtime_slot_active(slot) && slot->keycode == keycode && key_runtime_keypos_equal(slot->key_pos, key_pos);
}

active_key_state_t *key_runtime_find_slot_by_position(keypos_t key_pos) {
    active_key_state_t *slot = key_runtime_primary_slot();

    return key_runtime_slot_active(slot) && key_runtime_keypos_equal(slot->key_pos, key_pos) ? slot : NULL;
}

bool active_key_matches(uint16_t keycode, keypos_t key_pos) {
    return key_runtime_slot_matches(key_runtime_primary_slot(), keycode, key_pos);
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
    };
}

void active_key_track(uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired) {
    key_runtime_slot_track(key_runtime_primary_slot(), keycode, key_pos, tap_action, hold, long_hold, tap_hold_term, longer_hold_term, multi_tap_term, hold_fired);
}
