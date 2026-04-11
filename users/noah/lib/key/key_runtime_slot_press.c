// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Press
// ────────────────────────────────────────────────────────────────────────────
//
// Press-specific slot transition planning helpers.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_press.h"

#include "../pointing/pd_modes.h"

key_runtime_slot_effect_request_t key_runtime_slot_begin_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired, bool implicit_hold, bool fallback_hold_pending, bool pd_mode_was_locked_on_press) {
    key_runtime_slot_effect_request_t request = {0};

    if (!slot) {
        return request;
    }

    key_runtime_slot_track(slot, keycode, key_pos, tap_action, hold, long_hold, tap_hold_term, longer_hold_term, multi_tap_term, hold_fired);
    slot->implicit_hold               = implicit_hold;
    slot->fallback_hold_pending       = fallback_hold_pending;
    slot->pd_mode_was_locked_on_press = pd_mode_was_locked_on_press;

    if (hold_registers_on_press(hold)) {
        slot->held_action_keycode = hold.action;
        request.kind              = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER;
        request.action            = hold.action;
    }

    return request;
}

static key_runtime_slot_press_plan_t key_runtime_slot_prepare_handled_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, bool active_held_action_survives_flush, bool has_multi_tap, bool is_momentary_layer, uint8_t layer, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool implicit_hold, bool fallback_hold_pending, bool pd_mode_was_locked_on_press) {
    key_runtime_slot_press_plan_t plan = {
        .handled       = slot != NULL,
        .begin_key_pos = key_pos,
    };

    if (!slot) {
        return plan;
    }

    if (has_multi_tap && key_runtime_slot_pending_multi_tap_matches(slot, keycode, key_pos)) {
        plan.dispatch_action = key_runtime_slot_advance_pending_multi_tap(slot, keycode);
        plan.layer_press     = is_momentary_layer;
        plan.layer           = layer;

        bool pending_hold = key_runtime_slot_pending_multi_tap_pending_hold(slot);
        if (pending_hold || is_momentary_layer) {
            plan.begin_request = key_runtime_slot_begin_press(slot, keycode, key_pos, KC_NO, hold_behavior_none(), hold_behavior_none(), tap_hold_term, longer_hold_term, multi_tap_term, !pending_hold, false, false, false);
        }

        return plan;
    }

    if (key_runtime_slot_has_pending_multi_tap(slot) && !key_runtime_slot_pending_multi_tap_matches(slot, keycode, key_pos)) {
        plan.pending_multi_tap_flush = key_runtime_slot_take_pending_multi_tap_flush(slot);
    }

    plan.layer_press = is_momentary_layer;
    plan.layer       = layer;

    if (key_runtime_slot_active(slot) && !key_runtime_slot_matches(slot, keycode, key_pos)) {
        plan.reclaim_key_pos = slot->key_pos;
        plan.reclaim_request = key_runtime_slot_take_flush(slot, active_held_action_survives_flush);
    }

    plan.begin_request = key_runtime_slot_begin_press(slot, keycode, key_pos, tap_action, hold, long_hold, tap_hold_term, longer_hold_term, multi_tap_term, false, implicit_hold, fallback_hold_pending, pd_mode_was_locked_on_press);
    return plan;
}

key_runtime_slot_press_plan_t key_runtime_slot_take_handled_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_view_t key, bool active_held_action_survives_flush) {
    key_behavior_view_t behavior = key.behavior;
    hold_behavior_t     hold     = handled_key_single_hold(key);
    bool                implicit = handled_key_uses_implicit_hold(key);
    pd_mode_mask_t      mode     = pd_mode_for_keycode(keycode);

    return key_runtime_slot_prepare_handled_press(
        slot, keycode, key_pos, active_held_action_survives_flush, behavior.has_multi_tap, behavior.is_momentary_layer, behavior.is_momentary_layer ? behavior_get_layer(keycode) : 0, handled_key_tap_action(key), hold, behavior.single.long_hold, behavior.tap_hold_term, behavior.longer_hold_term, behavior.multi_tap_term, implicit, !implicit && handled_key_uses_fallback_hold(key), mode && pd_mode_locked(mode));
}
