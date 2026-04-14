// ────────────────────────────────────────────────────────────────────────────
// Runtime Debug Snapshot
// ────────────────────────────────────────────────────────────────────────────

#include "runtime_debug.h"

static keypos_t noah_runtime_debug_slot_index_key_pos(uint8_t slot_index) {
    return (keypos_t){
        .row = (uint8_t)(slot_index / MATRIX_COLS),
        .col = (uint8_t)(slot_index % MATRIX_COLS),
    };
}

void noah_runtime_debug_snapshot(noah_runtime_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    out->core = noah_runtime_shared_state;
    layer_ownership_debug_snapshot(&out->layer_ownership);
    held_action_debug_snapshot(&out->held_actions);
    held_repeat_debug_snapshot(&out->held_repeats);
    keyboard_mod_ownership_debug_snapshot(&out->keyboard_mod_ownership);
    noah_runtime_trace_snapshot(&out->trace);
}

uint8_t noah_runtime_debug_active_slot_count(const noah_runtime_debug_snapshot_t *snapshot) {
    return snapshot ? snapshot->core.key.index.active_slot_count : 0;
}

bool noah_runtime_debug_active_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, uint8_t order, keypos_t *out) {
    if (!(snapshot && out) || order >= snapshot->core.key.index.active_slot_count) {
        return false;
    }

    *out = noah_runtime_debug_slot_index_key_pos(snapshot->core.key.index.active_slots[order]);
    return true;
}

uint8_t noah_runtime_debug_pending_multi_tap_slot_count(const noah_runtime_debug_snapshot_t *snapshot) {
    return snapshot ? snapshot->core.key.index.pending_multi_tap_count : 0;
}

bool noah_runtime_debug_pending_multi_tap_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, uint8_t order, keypos_t *out) {
    if (!(snapshot && out) || order >= snapshot->core.key.index.pending_multi_tap_count) {
        return false;
    }

    *out = noah_runtime_debug_slot_index_key_pos(snapshot->core.key.index.pending_multi_tap_slots[order]);
    return true;
}

bool noah_runtime_debug_preview_owner_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, keypos_t *out) {
    if (!(snapshot && out) || snapshot->core.key.index.preview_owner_slot == UINT8_MAX) {
        return false;
    }

    *out = noah_runtime_debug_slot_index_key_pos(snapshot->core.key.index.preview_owner_slot);
    return true;
}

bool noah_runtime_debug_pending_fallback_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, keypos_t *out) {
    if (!(snapshot && out) || snapshot->core.key.index.pending_fallback_slot == UINT8_MAX) {
        return false;
    }

    *out = noah_runtime_debug_slot_index_key_pos(snapshot->core.key.index.pending_fallback_slot);
    return true;
}

void noah_runtime_reset_for_test(void) {
    runtime_shared_state_reset(&noah_runtime_shared_state);
    layer_ownership_reset_for_test();
    held_action_reset_for_test();
    held_repeat_reset_for_test();
    keyboard_mod_ownership_reset_for_test();
    noah_runtime_trace_reset();

    layer_state = 0;
    clear_mods();
    clear_weak_mods();
    clear_oneshot_mods();
    clear_oneshot_locked_mods();
    send_keyboard_report();
}
