// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Press
// ────────────────────────────────────────────────────────────────────────────
//
// Press-specific slot transition planning helpers.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_press.h"
#include "key_runtime_slot_result_internal.h"

#include "../pointing/pd_modes.h"

static key_runtime_slot_hold_strategy_t key_runtime_slot_hold_strategy_for_handled_key(handled_key_view_t key) {
    if (handled_key_uses_implicit_hold(key)) {
        return KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT;
    }

    if (handled_key_uses_fallback_hold(key)) {
        return KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK;
    }

    return KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT;
}

static key_runtime_slot_phase_t key_runtime_slot_initial_press_phase(hold_behavior_t hold) {
    return hold_registers_on_press(hold) ? KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW : KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW;
}

key_runtime_slot_effect_request_t key_runtime_slot_begin_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, key_runtime_slot_phase_t phase, key_runtime_slot_hold_strategy_t hold_strategy, bool pd_mode_was_locked_on_press) {
    key_runtime_slot_effect_request_t request = {0};

    if (!slot) {
        return request;
    }

    key_runtime_slot_track(slot, keycode, key_pos, tap_action, hold, long_hold, tap_hold_term, longer_hold_term, multi_tap_term, phase, hold_strategy);
    slot->pd_mode_was_locked_on_press = pd_mode_was_locked_on_press;

    if (hold_registers_on_press(hold)) {
        slot->held_action_keycode = hold.action;
        request.kind              = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER;
        request.action            = hold.action;
    }

    return request;
}

static key_runtime_slot_result_t key_runtime_slot_prepare_handled_press_result(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, bool active_held_action_survives_flush, bool has_multi_tap, bool is_momentary_layer, uint8_t layer, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, key_runtime_slot_hold_strategy_t hold_strategy, bool pd_mode_was_locked_on_press) {
    key_runtime_slot_result_t result = {0};

    if (!slot) {
        return result;
    }

    result.handled = true;

    if (has_multi_tap && key_runtime_slot_pending_multi_tap_matches(slot, keycode, key_pos)) {
        key_runtime_slot_result_push_dispatch_action(&result, key_pos, key_runtime_slot_advance_pending_multi_tap(slot, keycode));

        if (is_momentary_layer) {
            key_runtime_slot_result_push_layer_press(&result, key_pos, layer);
        }

        bool pending_hold = key_runtime_slot_pending_multi_tap_pending_hold(slot);
        if (pending_hold || is_momentary_layer) {
            key_runtime_slot_effect_request_t begin_request = key_runtime_slot_begin_press(
                slot, keycode, key_pos, KC_NO, hold_behavior_none(), hold_behavior_none(), tap_hold_term, longer_hold_term, multi_tap_term, pending_hold ? KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW : KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE, KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT, false);
            key_runtime_slot_result_push_request_if_present(&result, key_pos, begin_request);
        }

        return result;
    }

    if (key_runtime_slot_has_pending_multi_tap(slot) && !key_runtime_slot_pending_multi_tap_matches(slot, keycode, key_pos)) {
        key_runtime_slot_pending_multi_tap_flush_t flush = key_runtime_slot_take_pending_multi_tap_flush(slot);
        key_runtime_slot_result_push_delayed_action(&result, flush.action, flush.mods, flush.repeat_count);
    }

    if (is_momentary_layer) {
        key_runtime_slot_result_push_layer_press(&result, key_pos, layer);
    }

    if (key_runtime_slot_active(slot) && !key_runtime_slot_matches(slot, keycode, key_pos)) {
        keypos_t reclaim_key_pos = slot->key_pos;
        key_runtime_slot_effect_request_t reclaim_request = key_runtime_slot_take_flush(slot, active_held_action_survives_flush);
        key_runtime_slot_result_push_request_if_present(&result, reclaim_key_pos, reclaim_request);
    }

    key_runtime_slot_effect_request_t begin_request = key_runtime_slot_begin_press(
        slot, keycode, key_pos, tap_action, hold, long_hold, tap_hold_term, longer_hold_term, multi_tap_term, key_runtime_slot_initial_press_phase(hold), hold_strategy, pd_mode_was_locked_on_press);
    key_runtime_slot_result_push_request_if_present(&result, key_pos, begin_request);
    return result;
}

key_runtime_slot_result_t key_runtime_slot_take_handled_press_result(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_view_t key, bool active_held_action_survives_flush) {
    key_behavior_view_t behavior = key.behavior;
    hold_behavior_t     hold     = handled_key_single_hold(key);
    key_runtime_slot_hold_strategy_t hold_strategy = key_runtime_slot_hold_strategy_for_handled_key(key);
    pd_mode_mask_t      mode     = pd_mode_for_keycode(keycode);

    return key_runtime_slot_prepare_handled_press_result(
        slot, keycode, key_pos, active_held_action_survives_flush, behavior.has_multi_tap, behavior.is_momentary_layer, behavior.is_momentary_layer ? behavior_get_layer(keycode) : 0, handled_key_tap_action(key), hold, behavior.single.long_hold, behavior.tap_hold_term, behavior.longer_hold_term, behavior.multi_tap_term, hold_strategy, mode && pd_mode_locked(mode));
}
