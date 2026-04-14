// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Index
// ────────────────────────────────────────────────────────────────────────────
//
// Explicit registry helpers for cross-slot coordination. The slot table
// remains the source of truth for storage; this layer keeps the active/pending
// working sets and common single-owner answers cached in shared state.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_state.h"

static inline bool key_runtime_index_slot_preview_owner_candidate(const active_key_state_t *slot) {
    return key_runtime_slot_active(slot) && !key_runtime_slot_uses_implicit_hold(slot) && !key_runtime_slot_uses_fallback_hold(slot) && slot->lifecycle.held_action_keycode == KC_NO && key_runtime_slot_allows_tap_release(slot) && key_runtime_slot_preview_layer_hint(slot) != UINT8_MAX;
}

static inline bool key_runtime_index_slot_pending_fallback_candidate(const active_key_state_t *slot) {
    return slot && key_runtime_slot_uses_fallback_hold(slot) && slot->lifecycle.held_action_keycode == KC_NO && slot->owner.keycode != KC_NO;
}

static inline const key_runtime_index_state_t *key_runtime_index_state_snapshot(void) {
    return &key_runtime_shared_state()->index;
}

static inline void key_runtime_index_rebuild(void) {
    key_runtime_shared_state_t *state = key_runtime_shared_state();

    if (!state) {
        return;
    }

    state->index = (key_runtime_index_state_t){
        .preview_owner_slot    = UINT8_MAX,
        .pending_fallback_slot = UINT8_MAX,
    };

    for (uint8_t index = 0; index < KEY_RUNTIME_SLOT_TABLE_CAPACITY; index++) {
        active_key_state_t *slot = &state->slots_by_position[index];

        if (key_runtime_slot_active(slot)) {
            state->index.active_slots[state->index.active_slot_count++] = index;
        }

        if (key_runtime_slot_has_pending_multi_tap(slot)) {
            state->index.pending_multi_tap_slots[state->index.pending_multi_tap_count++] = index;
        }

        if (state->index.preview_owner_slot == UINT8_MAX && key_runtime_index_slot_preview_owner_candidate(slot)) {
            state->index.preview_owner_slot = index;
        }

        if (state->index.pending_fallback_slot == UINT8_MAX && key_runtime_index_slot_pending_fallback_candidate(slot)) {
            state->index.pending_fallback_slot = index;
        }
    }
}

static inline active_key_state_t *key_runtime_active_slot_by_order(uint8_t order) {
    const key_runtime_index_state_t *index = key_runtime_index_state_snapshot();

    if (!index || order >= index->active_slot_count) {
        return NULL;
    }

    return key_runtime_slot_at(index->active_slots[order]);
}

static inline active_key_state_t *key_runtime_pending_multi_tap_slot_by_order(uint8_t order) {
    const key_runtime_index_state_t *index = key_runtime_index_state_snapshot();

    if (!index || order >= index->pending_multi_tap_count) {
        return NULL;
    }

    return key_runtime_slot_at(index->pending_multi_tap_slots[order]);
}

static inline active_key_state_t *key_runtime_preview_owner_slot(void) {
    const key_runtime_index_state_t *index = key_runtime_index_state_snapshot();
    return (!index || index->preview_owner_slot == UINT8_MAX) ? NULL : key_runtime_slot_at(index->preview_owner_slot);
}

static inline active_key_state_t *key_runtime_pending_fallback_slot(void) {
    const key_runtime_index_state_t *index = key_runtime_index_state_snapshot();
    return (!index || index->pending_fallback_slot == UINT8_MAX) ? NULL : key_runtime_slot_at(index->pending_fallback_slot);
}
