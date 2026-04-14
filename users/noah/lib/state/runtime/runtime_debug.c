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

    return &snapshot->key.slots_by_position[noah_runtime_debug_slot_table_index(key_pos)];
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

static void noah_runtime_debug_snapshot_layer_ownership(const noah_runtime_context_t *ctx, layer_ownership_debug_snapshot_t *out) {
    if (!(ctx && out)) {
        return;
    }

    *out = (layer_ownership_debug_snapshot_t){
        .applied_layer_state = layer_state,
        .locked_mask         = ctx->layer_ownership.locked_mask,
    };

    memcpy(out->momentary_refcounts, ctx->layer_ownership.momentary_refcounts, sizeof(ctx->layer_ownership.momentary_refcounts));
    memcpy(out->bindings, ctx->layer_ownership.bindings, sizeof(ctx->layer_ownership.bindings));
}

static void noah_runtime_debug_snapshot_held_actions(const noah_runtime_context_t *ctx, held_action_debug_snapshot_t *out) {
    if (!(ctx && out)) {
        return;
    }

    memcpy(out->modifiers, ctx->held_actions.modifiers, sizeof(ctx->held_actions.modifiers));
    memcpy(out->modifier_refcounts, ctx->held_actions.modifier_refcounts, sizeof(ctx->held_actions.modifier_refcounts));
    memcpy(out->actions, ctx->held_actions.actions, sizeof(ctx->held_actions.actions));
}

static void noah_runtime_debug_snapshot_held_repeats(const noah_runtime_context_t *ctx, held_repeat_debug_snapshot_t *out) {
    if (!(ctx && out)) {
        return;
    }

    memcpy(out->bindings, ctx->held_repeats.bindings, sizeof(ctx->held_repeats.bindings));
}

static void noah_runtime_debug_snapshot_keyboard_mod_ownership(const noah_runtime_context_t *ctx, keyboard_mod_ownership_debug_snapshot_t *out) {
    if (!(ctx && out)) {
        return;
    }

    *out = (keyboard_mod_ownership_debug_snapshot_t){
        .live_state =
            {
                .real           = get_mods(),
                .weak           = get_weak_mods(),
                .oneshot        = get_oneshot_mods(),
                .oneshot_locked = get_oneshot_locked_mods(),
            },
    };

    memcpy(out->physical_refcounts, ctx->keyboard_mod_ownership.physical_refcounts, sizeof(ctx->keyboard_mod_ownership.physical_refcounts));
    memcpy(out->managed_refcounts, ctx->keyboard_mod_ownership.managed_refcounts, sizeof(ctx->keyboard_mod_ownership.managed_refcounts));
}

static void noah_runtime_debug_snapshot_trace(const noah_runtime_context_t *ctx, noah_runtime_trace_snapshot_t *out) {
    const noah_runtime_trace_state_t *state;

    if (!(ctx && out)) {
        return;
    }

    state = &ctx->trace;
    *out  = (noah_runtime_trace_snapshot_t){
        .count      = state->count,
        .overflowed = state->overflowed,
    };

    uint8_t start = (uint8_t)((state->next_index + NOAH_RUNTIME_TRACE_CAPACITY - state->count) % NOAH_RUNTIME_TRACE_CAPACITY);

    for (uint8_t index = 0; index < state->count; index++) {
        out->entries[index] = state->entries[(uint8_t)((start + index) % NOAH_RUNTIME_TRACE_CAPACITY)];
    }
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

    out->key.feedback_active           = state->key.feedback.active;
    out->pd.local_active_mode          = state->pd.local_active_mode;
    out->pd.local_locked_mode          = state->pd.local_locked_mode;
    out->pd.remote_display_active_mode = state->pd.remote_display_active_mode;
    out->pd.remote_display_locked_mode = state->pd.remote_display_locked_mode;

    for (uint16_t index = 0; index < NOAH_RUNTIME_DEBUG_SLOT_CAPACITY; index++) {
        out->key.slots_by_position[index] = noah_runtime_debug_slot_snapshot_from_runtime(&state->key.slots_by_position[index]);
    }

    out->key.active_slot_count = state->key.index.active_slot_count;
    for (uint8_t index = 0; index < state->key.index.active_slot_count; index++) {
        out->key.active_slots[index] = noah_runtime_debug_slot_index_key_pos(state->key.index.active_slots[index]);
    }

    out->key.pending_multi_tap_count = state->key.index.pending_multi_tap_count;
    for (uint8_t index = 0; index < state->key.index.pending_multi_tap_count; index++) {
        out->key.pending_multi_tap_slots[index] = noah_runtime_debug_slot_index_key_pos(state->key.index.pending_multi_tap_slots[index]);
    }

    if (state->key.index.preview_owner_slot != UINT8_MAX) {
        out->key.preview_owner_slot_present = true;
        out->key.preview_owner_slot         = noah_runtime_debug_slot_index_key_pos(state->key.index.preview_owner_slot);
    }

    if (state->key.index.pending_fallback_slot != UINT8_MAX) {
        out->key.pending_fallback_slot_present = true;
        out->key.pending_fallback_slot         = noah_runtime_debug_slot_index_key_pos(state->key.index.pending_fallback_slot);
    }

    noah_runtime_debug_snapshot_layer_ownership(ctx, &out->layer_ownership);
    noah_runtime_debug_snapshot_held_actions(ctx, &out->held_actions);
    noah_runtime_debug_snapshot_held_repeats(ctx, &out->held_repeats);
    noah_runtime_debug_snapshot_keyboard_mod_ownership(ctx, &out->keyboard_mod_ownership);
    noah_runtime_debug_snapshot_trace(ctx, &out->trace);
}

void noah_runtime_debug_snapshot(noah_runtime_debug_snapshot_t *out) {
    noah_runtime_context_debug_snapshot(noah_runtime_context(), out);
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
    return snapshot ? snapshot->key.active_slot_count : 0;
}

bool noah_runtime_debug_active_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, uint8_t order, keypos_t *out) {
    if (!(snapshot && out) || order >= snapshot->key.active_slot_count) {
        return false;
    }

    *out = snapshot->key.active_slots[order];
    return true;
}

uint8_t noah_runtime_debug_pending_multi_tap_slot_count(const noah_runtime_debug_snapshot_t *snapshot) {
    return snapshot ? snapshot->key.pending_multi_tap_count : 0;
}

bool noah_runtime_debug_pending_multi_tap_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, uint8_t order, keypos_t *out) {
    if (!(snapshot && out) || order >= snapshot->key.pending_multi_tap_count) {
        return false;
    }

    *out = snapshot->key.pending_multi_tap_slots[order];
    return true;
}

bool noah_runtime_debug_preview_owner_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, keypos_t *out) {
    if (!(snapshot && out) || !snapshot->key.preview_owner_slot_present) {
        return false;
    }

    *out = snapshot->key.preview_owner_slot;
    return true;
}

bool noah_runtime_debug_pending_fallback_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, keypos_t *out) {
    if (!(snapshot && out) || !snapshot->key.pending_fallback_slot_present) {
        return false;
    }

    *out = snapshot->key.pending_fallback_slot;
    return true;
}
