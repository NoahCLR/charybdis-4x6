// ────────────────────────────────────────────────────────────────────────────
// Runtime Debug Snapshot
// ────────────────────────────────────────────────────────────────────────────

#include <string.h>

#include "runtime_debug.h"

#include "../../key/runtime/key_runtime_state.h"
#include "runtime_context_internal.h"

static keypos_t noah_runtime_debug_slot_index_key_pos(uint8_t slot_index) {
    return (keypos_t){
        .row = (uint8_t)(slot_index / MATRIX_COLS),
        .col = (uint8_t)(slot_index % MATRIX_COLS),
    };
}

static bool noah_runtime_debug_key_pos_valid(keypos_t key_pos) {
    return key_pos.row < MATRIX_ROWS && key_pos.col < MATRIX_COLS;
}

static uint16_t noah_runtime_debug_slot_table_index(keypos_t key_pos) {
    return (uint16_t)((uint16_t)key_pos.row * (uint16_t)MATRIX_COLS + (uint16_t)key_pos.col);
}

static const noah_runtime_debug_slot_snapshot_t *noah_runtime_debug_snapshot_slot(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos) {
    if (!(snapshot) || !noah_runtime_debug_key_pos_valid(key_pos)) {
        return NULL;
    }

    return &snapshot->slots_by_position[noah_runtime_debug_slot_table_index(key_pos)];
}

static noah_runtime_debug_slot_snapshot_t noah_runtime_debug_slot_snapshot_from_runtime(const active_key_state_t *slot) {
    if (!slot) {
        return (noah_runtime_debug_slot_snapshot_t){0};
    }

    return (noah_runtime_debug_slot_snapshot_t){
        .owner_keycode             = slot->owner.keycode,
        .tap_action                = slot->interaction.binding.tap_action,
        .held_action_keycode       = slot->lifecycle.held_action_keycode,
        .pending_multi_tap_count   = slot->pending_multi_tap.count,
        .pending_multi_tap_holding = key_runtime_slot_pending_multi_tap_pending_hold(slot),
        .has_pending_multi_tap     = key_runtime_slot_has_pending_multi_tap(slot),
        .hold_complete             = key_runtime_slot_hold_is_complete(slot),
    };
}

static void noah_runtime_context_debug_snapshot(const noah_runtime_context_t *ctx, noah_runtime_debug_snapshot_t *out) {
    const runtime_shared_state_t *state = ctx ? &ctx->shared : NULL;

    if (!out) {
        return;
    }

    *out = (noah_runtime_debug_snapshot_t){0};

    if (!state) {
        return;
    }

    out->feedback_active = state->key.feedback.active;

    for (uint16_t index = 0; index < NOAH_RUNTIME_DEBUG_SLOT_CAPACITY; index++) {
        out->slots_by_position[index] = noah_runtime_debug_slot_snapshot_from_runtime(&state->key.slots_by_position[index]);
    }

    out->active_slot_count = state->key.index.active_slot_count;
    for (uint8_t index = 0; index < state->key.index.active_slot_count; index++) {
        out->active_slots[index] = noah_runtime_debug_slot_index_key_pos(state->key.index.active_slots[index]);
    }

    out->pending_multi_tap_count = state->key.index.pending_multi_tap_count;
    for (uint8_t index = 0; index < state->key.index.pending_multi_tap_count; index++) {
        out->pending_multi_tap_slots[index] = noah_runtime_debug_slot_index_key_pos(state->key.index.pending_multi_tap_slots[index]);
    }

    if (state->key.index.preview_owner_slot != UINT8_MAX) {
        out->preview_owner_slot_present = true;
        out->preview_owner_slot         = noah_runtime_debug_slot_index_key_pos(state->key.index.preview_owner_slot);
    }

    if (state->key.index.pending_fallback_slot != UINT8_MAX) {
        out->pending_fallback_slot_present = true;
        out->pending_fallback_slot         = noah_runtime_debug_slot_index_key_pos(state->key.index.pending_fallback_slot);
    }
}

void noah_runtime_debug_snapshot(noah_runtime_debug_snapshot_t *out) {
    noah_runtime_context_debug_snapshot(noah_runtime_context(), out);
}

bool noah_runtime_debug_feedback_active(const noah_runtime_debug_snapshot_t *snapshot) {
    return snapshot ? snapshot->feedback_active : false;
}

uint16_t noah_runtime_debug_slot_owner_keycode(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos) {
    const noah_runtime_debug_slot_snapshot_t *slot = noah_runtime_debug_snapshot_slot(snapshot, key_pos);
    return slot ? slot->owner_keycode : KC_NO;
}

uint16_t noah_runtime_debug_slot_tap_action(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos) {
    const noah_runtime_debug_slot_snapshot_t *slot = noah_runtime_debug_snapshot_slot(snapshot, key_pos);
    return slot ? slot->tap_action : KC_NO;
}

uint16_t noah_runtime_debug_slot_held_action_keycode(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos) {
    const noah_runtime_debug_slot_snapshot_t *slot = noah_runtime_debug_snapshot_slot(snapshot, key_pos);
    return slot ? slot->held_action_keycode : KC_NO;
}

uint8_t noah_runtime_debug_slot_pending_multi_tap_count(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos) {
    const noah_runtime_debug_slot_snapshot_t *slot = noah_runtime_debug_snapshot_slot(snapshot, key_pos);
    return slot ? slot->pending_multi_tap_count : 0;
}

bool noah_runtime_debug_slot_pending_multi_tap_holding(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos) {
    const noah_runtime_debug_slot_snapshot_t *slot = noah_runtime_debug_snapshot_slot(snapshot, key_pos);
    return slot ? slot->pending_multi_tap_holding : false;
}

bool noah_runtime_debug_slot_has_pending_multi_tap(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos) {
    const noah_runtime_debug_slot_snapshot_t *slot = noah_runtime_debug_snapshot_slot(snapshot, key_pos);
    return slot ? slot->has_pending_multi_tap : false;
}

bool noah_runtime_debug_slot_hold_is_complete(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos) {
    const noah_runtime_debug_slot_snapshot_t *slot = noah_runtime_debug_snapshot_slot(snapshot, key_pos);
    return slot ? slot->hold_complete : false;
}

uint8_t noah_runtime_debug_active_slot_count(const noah_runtime_debug_snapshot_t *snapshot) {
    return snapshot ? snapshot->active_slot_count : 0;
}

bool noah_runtime_debug_active_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, uint8_t order, keypos_t *out) {
    if (!(snapshot && out) || order >= snapshot->active_slot_count) {
        return false;
    }

    *out = snapshot->active_slots[order];
    return true;
}

uint8_t noah_runtime_debug_pending_multi_tap_slot_count(const noah_runtime_debug_snapshot_t *snapshot) {
    return snapshot ? snapshot->pending_multi_tap_count : 0;
}

bool noah_runtime_debug_pending_multi_tap_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, uint8_t order, keypos_t *out) {
    if (!(snapshot && out) || order >= snapshot->pending_multi_tap_count) {
        return false;
    }

    *out = snapshot->pending_multi_tap_slots[order];
    return true;
}

bool noah_runtime_debug_preview_owner_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, keypos_t *out) {
    if (!(snapshot && out) || !snapshot->preview_owner_slot_present) {
        return false;
    }

    *out = snapshot->preview_owner_slot;
    return true;
}

bool noah_runtime_debug_pending_fallback_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, keypos_t *out) {
    if (!(snapshot && out) || !snapshot->pending_fallback_slot_present) {
        return false;
    }

    *out = snapshot->pending_fallback_slot;
    return true;
}
