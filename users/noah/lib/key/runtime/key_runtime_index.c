// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Index
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_index.h"

#include <stddef.h>

static bool key_runtime_index_slot_preview_owner_candidate(const active_key_state_t *slot) {
    return key_runtime_slot_active(slot) && !key_runtime_slot_uses_implicit_hold(slot) && !key_runtime_slot_uses_fallback_hold(slot) && slot->lifecycle.held_action_keycode == KC_NO && key_runtime_slot_allows_tap_release(slot) && key_runtime_slot_preview_layer_hint(slot) != UINT8_MAX;
}

static bool key_runtime_index_slot_pending_fallback_candidate(const active_key_state_t *slot) {
    return slot && key_runtime_slot_uses_fallback_hold(slot) && slot->lifecycle.held_action_keycode == KC_NO && slot->owner.keycode != KC_NO;
}

static bool key_runtime_index_contains(const uint8_t *indices, uint8_t count, uint8_t slot_index, uint8_t *found_at) {
    for (uint8_t index = 0; index < count; index++) {
        if (indices[index] != slot_index) {
            continue;
        }

        if (found_at) {
            *found_at = index;
        }
        return true;
    }

    return false;
}

static void key_runtime_index_insert_sorted(uint8_t *indices, uint8_t *count, uint8_t slot_index) {
    uint8_t insert_at;

    if (!(indices && count) || *count >= KEY_RUNTIME_SLOT_TABLE_CAPACITY) {
        return;
    }

    if (key_runtime_index_contains(indices, *count, slot_index, NULL)) {
        return;
    }

    insert_at = *count;
    while (insert_at > 0 && indices[insert_at - 1] > slot_index) {
        indices[insert_at] = indices[insert_at - 1];
        insert_at--;
    }

    indices[insert_at] = slot_index;
    (*count)++;
}

static void key_runtime_index_remove(uint8_t *indices, uint8_t *count, uint8_t slot_index) {
    uint8_t remove_at;

    if (!(indices && count) || !key_runtime_index_contains(indices, *count, slot_index, &remove_at)) {
        return;
    }

    for (uint8_t index = remove_at; index + 1u < *count; index++) {
        indices[index] = indices[index + 1u];
    }

    (*count)--;
}

static int16_t key_runtime_index_slot_index(const active_key_state_t *slot) {
    key_runtime_shared_state_t *state = key_runtime_shared_state();

    if (!(state && slot)) {
        return -1;
    }

    const active_key_state_t *base = &state->slots_by_position[0];
    const active_key_state_t *last = &state->slots_by_position[KEY_RUNTIME_SLOT_TABLE_CAPACITY - 1];

    if (slot < base || slot > last) {
        return -1;
    }

    return (int16_t)(slot - base);
}

static void key_runtime_index_refresh_single_owner_answers(key_runtime_shared_state_t *state) {
    state->index.preview_owner_slot    = UINT8_MAX;
    state->index.pending_fallback_slot = UINT8_MAX;

    for (uint8_t order = 0; order < state->index.active_slot_count; order++) {
        uint8_t            slot_index = state->index.active_slots[order];
        active_key_state_t *slot      = &state->slots_by_position[slot_index];

        if (state->index.preview_owner_slot == UINT8_MAX && key_runtime_index_slot_preview_owner_candidate(slot)) {
            state->index.preview_owner_slot = slot_index;
        }

        if (state->index.pending_fallback_slot == UINT8_MAX && key_runtime_index_slot_pending_fallback_candidate(slot)) {
            state->index.pending_fallback_slot = slot_index;
        }

        if (state->index.preview_owner_slot != UINT8_MAX && state->index.pending_fallback_slot != UINT8_MAX) {
            return;
        }
    }
}

void key_runtime_index_sync_slot(active_key_state_t *slot) {
    key_runtime_shared_state_t *state;
    int16_t                     slot_index;

    state      = key_runtime_shared_state();
    slot_index = key_runtime_index_slot_index(slot);
    if (!(state && slot_index >= 0)) {
        return;
    }

    if (key_runtime_slot_active(slot)) {
        key_runtime_index_insert_sorted(state->index.active_slots, &state->index.active_slot_count, (uint8_t)slot_index);
    } else {
        key_runtime_index_remove(state->index.active_slots, &state->index.active_slot_count, (uint8_t)slot_index);
    }

    if (key_runtime_slot_has_pending_multi_tap(slot)) {
        key_runtime_index_insert_sorted(state->index.pending_multi_tap_slots, &state->index.pending_multi_tap_count, (uint8_t)slot_index);
    } else {
        key_runtime_index_remove(state->index.pending_multi_tap_slots, &state->index.pending_multi_tap_count, (uint8_t)slot_index);
    }

    key_runtime_index_refresh_single_owner_answers(state);
}

const key_runtime_index_state_t *key_runtime_index_state_snapshot(void) {
    key_runtime_shared_state_t *state = key_runtime_shared_state();
    return state ? &state->index : NULL;
}

active_key_state_t *key_runtime_active_slot_by_order(uint8_t order) {
    const key_runtime_index_state_t *index = key_runtime_index_state_snapshot();

    if (!index || order >= index->active_slot_count) {
        return NULL;
    }

    return key_runtime_slot_at(index->active_slots[order]);
}

active_key_state_t *key_runtime_pending_multi_tap_slot_by_order(uint8_t order) {
    const key_runtime_index_state_t *index = key_runtime_index_state_snapshot();

    if (!index || order >= index->pending_multi_tap_count) {
        return NULL;
    }

    return key_runtime_slot_at(index->pending_multi_tap_slots[order]);
}

active_key_state_t *key_runtime_preview_owner_slot(void) {
    const key_runtime_index_state_t *index = key_runtime_index_state_snapshot();
    return (!index || index->preview_owner_slot == UINT8_MAX) ? NULL : key_runtime_slot_at(index->preview_owner_slot);
}

active_key_state_t *key_runtime_pending_fallback_slot(void) {
    const key_runtime_index_state_t *index = key_runtime_index_state_snapshot();
    return (!index || index->pending_fallback_slot == UINT8_MAX) ? NULL : key_runtime_slot_at(index->pending_fallback_slot);
}
