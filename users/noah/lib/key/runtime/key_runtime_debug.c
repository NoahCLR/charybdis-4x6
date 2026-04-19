// ────────────────────────────────────────────────────────────────────────────
// Runtime Debug
// ────────────────────────────────────────────────────────────────────────────

#include "../../state/runtime/runtime_debug.h"

#include "key_runtime_index_internal.h"
#include "key_runtime_internal.h"

static bool noah_runtime_debug_key_pos_valid(keypos_t key_pos) {
    return key_pos.row < MATRIX_ROWS && key_pos.col < MATRIX_COLS;
}

static active_key_state_t *noah_runtime_debug_slot(keypos_t key_pos) {
    if (!noah_runtime_debug_key_pos_valid(key_pos)) {
        return NULL;
    }

    return key_runtime_slot_for_position(key_pos);
}

static bool noah_runtime_debug_slot_key_pos(const active_key_state_t *slot, keypos_t *out) {
    if (!(slot && out)) {
        return false;
    }

    *out = slot->owner.key_pos;
    return true;
}

static bool noah_runtime_debug_pending_multi_tap_key_pos(const active_key_state_t *slot, keypos_t *out) {
    if (!(slot && out) || !key_runtime_slot_has_pending_multi_tap(slot)) {
        return false;
    }

    *out = slot->pending_multi_tap.key_pos;
    return true;
}

bool noah_runtime_debug_feedback_active(void) {
    return key_runtime_shared_state()->feedback.active;
}

uint16_t noah_runtime_debug_slot_owner_keycode(keypos_t key_pos) {
    const active_key_state_t *slot = noah_runtime_debug_slot(key_pos);
    return slot ? slot->owner.keycode : KC_NO;
}

uint16_t noah_runtime_debug_slot_tap_action(keypos_t key_pos) {
    const active_key_state_t *slot = noah_runtime_debug_slot(key_pos);
    return slot ? slot->interaction.binding.tap_action : KC_NO;
}

uint16_t noah_runtime_debug_slot_held_action_keycode(keypos_t key_pos) {
    const active_key_state_t *slot = noah_runtime_debug_slot(key_pos);
    return slot ? slot->lifecycle.held_action_keycode : KC_NO;
}

key_runtime_slot_phase_t noah_runtime_debug_slot_phase(keypos_t key_pos) {
    const active_key_state_t *slot = noah_runtime_debug_slot(key_pos);
    return key_runtime_slot_phase(slot);
}

bool noah_runtime_debug_slot_momentary_tap_interrupted(keypos_t key_pos) {
    const active_key_state_t *slot = noah_runtime_debug_slot(key_pos);
    return slot ? slot->lifecycle.momentary_layer_tap_interrupted : false;
}

uint8_t noah_runtime_debug_slot_pending_multi_tap_count(keypos_t key_pos) {
    const active_key_state_t *slot = noah_runtime_debug_slot(key_pos);
    return slot ? slot->pending_multi_tap.count : 0;
}

bool noah_runtime_debug_slot_pending_multi_tap_holding(keypos_t key_pos) {
    const active_key_state_t *slot = noah_runtime_debug_slot(key_pos);
    return key_runtime_slot_pending_multi_tap_pending_hold(slot);
}

bool noah_runtime_debug_slot_has_pending_multi_tap(keypos_t key_pos) {
    const active_key_state_t *slot = noah_runtime_debug_slot(key_pos);
    return key_runtime_slot_has_pending_multi_tap(slot);
}

bool noah_runtime_debug_slot_hold_is_complete(keypos_t key_pos) {
    const active_key_state_t *slot = noah_runtime_debug_slot(key_pos);
    return key_runtime_slot_hold_is_complete(slot);
}

uint8_t noah_runtime_debug_active_slot_count(void) {
    return key_runtime_active_slot_count();
}

bool noah_runtime_debug_active_slot_key_pos(uint8_t order, keypos_t *out) {
    return noah_runtime_debug_slot_key_pos(key_runtime_active_slot_by_order(order), out);
}

uint8_t noah_runtime_debug_pending_multi_tap_slot_count(void) {
    return key_runtime_pending_multi_tap_slot_count();
}

bool noah_runtime_debug_pending_multi_tap_slot_key_pos(uint8_t order, keypos_t *out) {
    return noah_runtime_debug_pending_multi_tap_key_pos(key_runtime_pending_multi_tap_slot_by_order(order), out);
}

uint8_t noah_runtime_debug_deferred_release_count(void) {
    return key_runtime_shared_state()->deferred_release_dispatch.count;
}

bool noah_runtime_debug_deferred_release_key_pos(uint8_t order, keypos_t *out) {
    key_runtime_deferred_release_dispatch_queue_t *queue = &key_runtime_shared_state()->deferred_release_dispatch;

    if (!(out && order < queue->count)) {
        return false;
    }

    *out = queue->items[order].key_pos;
    return true;
}

uint16_t noah_runtime_debug_deferred_release_action(uint8_t order) {
    key_runtime_deferred_release_dispatch_queue_t *queue = &key_runtime_shared_state()->deferred_release_dispatch;

    if (order >= queue->count) {
        return KC_NO;
    }

    return queue->items[order].action;
}

uint8_t noah_runtime_debug_deferred_release_blocker_count(void) {
    return key_runtime_deferred_release_blocker_count();
}

uint8_t noah_runtime_debug_deferred_release_timed_blocker_count(void) {
    return key_runtime_shared_state()->index.deferred_release_blocker_timed_count;
}

bool noah_runtime_debug_preview_owner_slot_key_pos(keypos_t *out) {
    return noah_runtime_debug_slot_key_pos(key_runtime_preview_owner_slot(), out);
}

bool noah_runtime_debug_pending_fallback_slot_key_pos(keypos_t *out) {
    return noah_runtime_debug_slot_key_pos(key_runtime_pending_fallback_slot(), out);
}
