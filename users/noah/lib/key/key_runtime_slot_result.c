// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Results
// ────────────────────────────────────────────────────────────────────────────
//
// Shared slot-result builders used by the slot-step reducer implementation.
// ────────────────────────────────────────────────────────────────────────────

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
