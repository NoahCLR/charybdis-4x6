// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Results
// ────────────────────────────────────────────────────────────────────────────
//
// Shared slot-result helpers plus the reducer-style slot event seam used by
// higher-level handled-key orchestration.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_step.h"
#include "key_runtime_slot_result_internal.h"

void key_runtime_slot_result_push(key_runtime_slot_result_t *result, key_runtime_slot_result_effect_t effect) {
    if (!result) {
        return;
    }

    if (result->count < ARRAY_SIZE(result->effects)) {
        result->effects[result->count++] = effect;
        return;
    }

    result->overflowed = true;
}

bool key_runtime_slot_result_request_has_effect(key_runtime_slot_effect_request_t request) {
    return request.kind != KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE || request.release_owned_state || request.feedback_pulse;
}

void key_runtime_slot_result_push_request_if_present(key_runtime_slot_result_t *result, keypos_t key_pos, key_runtime_slot_effect_request_t request) {
    if (!key_runtime_slot_result_request_has_effect(request)) {
        return;
    }

    key_runtime_slot_result_push(result, (key_runtime_slot_result_effect_t){
                                           .kind                          = KEY_RUNTIME_SLOT_RESULT_EFFECT_SLOT_EFFECT_REQUEST,
                                           .key_pos                       = key_pos,
                                           .data.slot_effect_request = request,
                                       });
}

void key_runtime_slot_result_push_dispatch_action(key_runtime_slot_result_t *result, keypos_t key_pos, uint16_t action) {
    if (action == KC_NO) {
        return;
    }

    key_runtime_slot_result_push_request_if_present(result, key_pos, (key_runtime_slot_effect_request_t){
                                                                    .kind   = KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION,
                                                                    .action = action,
                                                                });
}

void key_runtime_slot_result_push_delayed_action(key_runtime_slot_result_t *result, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count) {
    if (action == KC_NO || repeat_count == 0) {
        return;
    }

    key_runtime_slot_result_push(result, (key_runtime_slot_result_effect_t){
                                           .kind = KEY_RUNTIME_SLOT_RESULT_EFFECT_DELAYED_ACTION,
                                           .data.delayed_action =
                                               {
                                                   .action       = action,
                                                   .mods         = mods,
                                                   .repeat_count = repeat_count,
                                               },
                                       });
}

void key_runtime_slot_result_push_layer_press(key_runtime_slot_result_t *result, keypos_t key_pos, uint8_t layer) {
    key_runtime_slot_result_push(result, (key_runtime_slot_result_effect_t){
                                           .kind       = KEY_RUNTIME_SLOT_RESULT_EFFECT_LAYER_PRESS,
                                           .key_pos    = key_pos,
                                           .data.layer = layer,
                                       });
}

void key_runtime_slot_result_push_layer_release(key_runtime_slot_result_t *result, keypos_t key_pos) {
    key_runtime_slot_result_push(result, (key_runtime_slot_result_effect_t){
                                           .kind    = KEY_RUNTIME_SLOT_RESULT_EFFECT_LAYER_RELEASE,
                                           .key_pos = key_pos,
                                       });
}

void key_runtime_slot_result_push_pd_mode_lock_tap(key_runtime_slot_result_t *result, pd_mode_mask_t mode) {
    if (!mode) {
        return;
    }

    key_runtime_slot_result_push(result, (key_runtime_slot_result_effect_t){
                                           .kind         = KEY_RUNTIME_SLOT_RESULT_EFFECT_PD_MODE_LOCK_TAP,
                                           .data.pd_mode = mode,
                                       });
}

key_runtime_slot_result_t key_runtime_slot_step(active_key_state_t *slot, key_runtime_slot_event_t event) {
    switch (event.kind) {
        case KEY_RUNTIME_SLOT_EVENT_HANDLED_PRESS:
            return key_runtime_slot_take_handled_press_result(
                slot,
                event.data.handled_press.keycode,
                event.data.handled_press.key_pos,
                event.data.handled_press.key,
                event.data.handled_press.active_held_action_survives_flush);
        case KEY_RUNTIME_SLOT_EVENT_HANDLED_RELEASE:
            return key_runtime_slot_take_handled_release_result(
                slot,
                event.data.handled_release.keycode,
                event.data.handled_release.key_pos,
                event.data.handled_release.behavior);
        case KEY_RUNTIME_SLOT_EVENT_ACTIVE_SCAN:
            return key_runtime_slot_take_active_scan_result(slot);
        case KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_SCAN:
            return key_runtime_slot_take_pending_multi_tap_scan_result(slot);
        case KEY_RUNTIME_SLOT_EVENT_INTERRUPT:
            return key_runtime_slot_take_interrupt_result(slot, event.data.interrupt.other_key_pos);
        case KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_FLUSH:
            return key_runtime_slot_take_pending_multi_tap_flush_result(slot);
        case KEY_RUNTIME_SLOT_EVENT_NONE:
        default:
            return (key_runtime_slot_result_t){0};
    }
}

key_runtime_slot_result_t key_runtime_slot_take_interrupt_result(active_key_state_t *slot, keypos_t other_key_pos) {
    key_runtime_slot_result_t         result  = {0};
    key_runtime_slot_effect_request_t request = key_runtime_slot_interrupt_on_other_press(slot, other_key_pos);

    if (!key_runtime_slot_result_request_has_effect(request)) {
        return result;
    }

    result.handled = true;
    key_runtime_slot_result_push_request_if_present(&result, slot ? slot->key_pos : (keypos_t){0}, request);
    return result;
}

key_runtime_slot_result_t key_runtime_slot_take_pending_multi_tap_flush_result(active_key_state_t *slot) {
    key_runtime_slot_result_t                  result = {0};
    key_runtime_slot_pending_multi_tap_flush_t flush  = key_runtime_slot_take_pending_multi_tap_flush(slot);

    if (!flush.handled) {
        return result;
    }

    result.handled = true;
    key_runtime_slot_result_push_delayed_action(&result, flush.action, flush.mods, flush.repeat_count);
    return result;
}
