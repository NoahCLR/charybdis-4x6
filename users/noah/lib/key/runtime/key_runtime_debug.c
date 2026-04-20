// ────────────────────────────────────────────────────────────────────────────
// Runtime Debug
// ────────────────────────────────────────────────────────────────────────────

#include "../../state/runtime/runtime_debug.h"

#include "../../runtime_v2/runtime_v2.h"

static bool noah_runtime_debug_key_pos_valid(keypos_t key_pos) {
    return key_pos.row < MATRIX_ROWS && key_pos.col < MATRIX_COLS;
}

bool noah_runtime_debug_feedback_active(void) {
    runtime_v2_state_t *state = runtime_v2_state();
    return state ? state->feedback_pulse_active : false;
}

uint16_t noah_runtime_debug_slot_owner_keycode(keypos_t key_pos) {
    return noah_runtime_debug_key_pos_valid(key_pos) ? runtime_v2_owner_keycode_at(key_pos) : KC_NO;
}

uint16_t noah_runtime_debug_slot_tap_action(keypos_t key_pos) {
    return noah_runtime_debug_key_pos_valid(key_pos) ? runtime_v2_tap_action_at(key_pos) : KC_NO;
}

uint16_t noah_runtime_debug_slot_held_action_keycode(keypos_t key_pos) {
    return noah_runtime_debug_key_pos_valid(key_pos) ? runtime_v2_held_action_keycode_at(key_pos) : KC_NO;
}

key_runtime_slot_phase_t noah_runtime_debug_slot_phase(keypos_t key_pos) {
    return noah_runtime_debug_key_pos_valid(key_pos) ? runtime_v2_slot_phase_at(key_pos) : KEY_RUNTIME_SLOT_PHASE_IDLE;
}

bool noah_runtime_debug_slot_momentary_tap_interrupted(keypos_t key_pos) {
    return noah_runtime_debug_key_pos_valid(key_pos) && runtime_v2_momentary_layer_tap_interrupted_at(key_pos);
}

uint8_t noah_runtime_debug_slot_pending_multi_tap_count(keypos_t key_pos) {
    return noah_runtime_debug_key_pos_valid(key_pos) ? runtime_v2_pending_multi_tap_tap_count_at(key_pos) : 0u;
}

bool noah_runtime_debug_slot_pending_multi_tap_holding(keypos_t key_pos) {
    return noah_runtime_debug_key_pos_valid(key_pos) && runtime_v2_pending_multi_tap_holding_at(key_pos);
}

bool noah_runtime_debug_slot_has_pending_multi_tap(keypos_t key_pos) {
    return noah_runtime_debug_key_pos_valid(key_pos) && runtime_v2_has_pending_multi_tap_at(key_pos);
}

bool noah_runtime_debug_slot_hold_is_complete(keypos_t key_pos) {
    return noah_runtime_debug_key_pos_valid(key_pos) && runtime_v2_hold_is_complete_at(key_pos);
}

uint8_t noah_runtime_debug_active_slot_count(void) {
    return runtime_v2_active_press_token_count();
}

bool noah_runtime_debug_active_slot_key_pos(uint8_t order, keypos_t *out) {
    return runtime_v2_active_press_token_key_pos(order, out);
}

uint8_t noah_runtime_debug_pending_multi_tap_slot_count(void) {
    return runtime_v2_pending_multi_tap_count();
}

bool noah_runtime_debug_pending_multi_tap_slot_key_pos(uint8_t order, keypos_t *out) {
    return runtime_v2_pending_multi_tap_key_pos(order, out);
}

uint8_t noah_runtime_debug_deferred_release_count(void) {
    return runtime_v2_pending_release_count();
}

bool noah_runtime_debug_deferred_release_key_pos(uint8_t order, keypos_t *out) {
    pending_release_t pending;

    if (!(out && runtime_v2_pending_release_at_order(order, &pending))) {
        return false;
    }

    *out = pending.key_pos;
    return true;
}

uint16_t noah_runtime_debug_deferred_release_action(uint8_t order) {
    pending_release_t pending;

    return runtime_v2_pending_release_at_order(order, &pending) ? pending.action : KC_NO;
}

uint8_t noah_runtime_debug_deferred_release_blocker_count(void) {
    return runtime_v2_deferred_release_blocker_count();
}

uint8_t noah_runtime_debug_deferred_release_timed_blocker_count(void) {
    return runtime_v2_deferred_release_timed_blocker_count();
}

bool noah_runtime_debug_preview_owner_slot_key_pos(keypos_t *out) {
    return runtime_v2_preview_owner_key_pos(out);
}

bool noah_runtime_debug_pending_fallback_slot_key_pos(keypos_t *out) {
    return runtime_v2_pending_fallback_key_pos(out);
}
