// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Results
// ────────────────────────────────────────────────────────────────────────────
//
// Adapters from the local press/release/scan slot protocols to one shared slot
// result shape consumed by the higher-level transition planner.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_result.h"

#include "key_runtime_slot_press.h"
#include "key_runtime_slot_release.h"
#include "key_runtime_slot_scan.h"

static void key_runtime_slot_result_push(key_runtime_slot_result_t *result, key_runtime_slot_result_effect_t effect) {
    if (!result) {
        return;
    }

    if (result->count < ARRAY_SIZE(result->effects)) {
        result->effects[result->count++] = effect;
        return;
    }

    result->overflowed = true;
}

static bool key_runtime_slot_result_request_has_effect(key_runtime_slot_effect_request_t request) {
    return request.kind != KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE || request.release_owned_state || request.feedback_pulse;
}

static void key_runtime_slot_result_push_request_if_present(key_runtime_slot_result_t *result, keypos_t key_pos, key_runtime_slot_effect_request_t request) {
    if (!key_runtime_slot_result_request_has_effect(request)) {
        return;
    }

    key_runtime_slot_result_push(result, (key_runtime_slot_result_effect_t){
                                           .kind                          = KEY_RUNTIME_SLOT_RESULT_EFFECT_SLOT_EFFECT_REQUEST,
                                           .key_pos                       = key_pos,
                                           .data.slot_effect_request = request,
                                       });
}

static void key_runtime_slot_result_push_dispatch_action(key_runtime_slot_result_t *result, keypos_t key_pos, uint16_t action) {
    if (action == KC_NO) {
        return;
    }

    key_runtime_slot_result_push_request_if_present(result, key_pos, (key_runtime_slot_effect_request_t){
                                                                    .kind   = KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION,
                                                                    .action = action,
                                                                });
}

static void key_runtime_slot_result_push_delayed_action(key_runtime_slot_result_t *result, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count) {
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

static void key_runtime_slot_result_push_layer_press(key_runtime_slot_result_t *result, keypos_t key_pos, uint8_t layer) {
    key_runtime_slot_result_push(result, (key_runtime_slot_result_effect_t){
                                           .kind       = KEY_RUNTIME_SLOT_RESULT_EFFECT_LAYER_PRESS,
                                           .key_pos    = key_pos,
                                           .data.layer = layer,
                                       });
}

static void key_runtime_slot_result_push_layer_release(key_runtime_slot_result_t *result, keypos_t key_pos) {
    key_runtime_slot_result_push(result, (key_runtime_slot_result_effect_t){
                                           .kind    = KEY_RUNTIME_SLOT_RESULT_EFFECT_LAYER_RELEASE,
                                           .key_pos = key_pos,
                                       });
}

static void key_runtime_slot_result_push_pd_mode_lock_tap(key_runtime_slot_result_t *result, pd_mode_mask_t mode) {
    if (!mode) {
        return;
    }

    key_runtime_slot_result_push(result, (key_runtime_slot_result_effect_t){
                                           .kind         = KEY_RUNTIME_SLOT_RESULT_EFFECT_PD_MODE_LOCK_TAP,
                                           .data.pd_mode = mode,
                                       });
}

key_runtime_slot_result_t key_runtime_slot_take_handled_press_result(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_view_t key, bool active_held_action_survives_flush) {
    key_runtime_slot_result_t    result = {0};
    key_runtime_slot_press_plan_t press  = key_runtime_slot_take_handled_press(slot, keycode, key_pos, key, active_held_action_survives_flush);

    if (!press.handled) {
        return result;
    }

    result.handled = true;

    key_runtime_slot_result_push_delayed_action(&result, press.pending_multi_tap_flush.action, press.pending_multi_tap_flush.mods, press.pending_multi_tap_flush.repeat_count);
    key_runtime_slot_result_push_dispatch_action(&result, press.begin_key_pos, press.dispatch_action);

    if (press.layer_press) {
        key_runtime_slot_result_push_layer_press(&result, press.begin_key_pos, press.layer);
    }

    key_runtime_slot_result_push_request_if_present(&result, press.reclaim_key_pos, press.reclaim_request);
    key_runtime_slot_result_push_request_if_present(&result, press.begin_key_pos, press.begin_request);
    return result;
}

key_runtime_slot_result_t key_runtime_slot_take_handled_release_result(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, key_behavior_view_t behavior) {
    key_runtime_slot_result_t         result  = {0};
    key_runtime_slot_release_event_t release = key_runtime_slot_take_handled_release(slot, keycode, key_pos, behavior);

    if (!release.handled) {
        return result;
    }

    result.handled = true;

    switch (release.kind) {
        case KEY_RUNTIME_SLOT_RELEASE_EVENT_ACTIVE_RELEASE:
            if (release.data.active_release.release_layer) {
                key_runtime_slot_result_push_layer_release(&result, release.data.active_release.key_pos);
            }

            if (release.data.active_release.release_owned_state) {
                key_runtime_slot_result_push_request_if_present(&result, release.data.active_release.key_pos, (key_runtime_slot_effect_request_t){
                                                                                                             .release_owned_state = true,
                                                                                                         });
            }

            switch (release.data.active_release.outcome) {
                case KEY_RUNTIME_SLOT_RELEASE_APPLY_OUTCOME_DISPATCH_ACTION:
                    key_runtime_slot_result_push_dispatch_action(&result, release.data.active_release.key_pos, release.data.active_release.action);
                    return result;
                case KEY_RUNTIME_SLOT_RELEASE_APPLY_OUTCOME_PD_MODE_LOCK_TAP:
                    key_runtime_slot_result_push_pd_mode_lock_tap(&result, release.data.active_release.pd_mode_lock_tap);
                    return result;
                case KEY_RUNTIME_SLOT_RELEASE_APPLY_OUTCOME_NONE:
                default:
                    return result;
            }
        case KEY_RUNTIME_SLOT_RELEASE_EVENT_PENDING_MULTI_TAP_HOLD_RELEASE:
            switch (release.data.pending_multi_tap_hold_release.outcome) {
                case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_HOLD_RELEASE_HELD_LIFECYCLE:
                    key_runtime_slot_result_push_request_if_present(&result, release.data.pending_multi_tap_hold_release.key_pos, (key_runtime_slot_effect_request_t){
                                                                                                                               .kind   = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER,
                                                                                                                               .action = release.data.pending_multi_tap_hold_release.action,
                                                                                                                           });
                    key_runtime_slot_result_push_request_if_present(&result, release.data.pending_multi_tap_hold_release.key_pos, (key_runtime_slot_effect_request_t){
                                                                                                                               .kind   = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_UNREGISTER,
                                                                                                                               .action = release.data.pending_multi_tap_hold_release.action,
                                                                                                                           });
                    break;
                case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_HOLD_RELEASE_DELAYED_ACTION:
                    key_runtime_slot_result_push_delayed_action(&result, release.data.pending_multi_tap_hold_release.action, release.data.pending_multi_tap_hold_release.mods, release.data.pending_multi_tap_hold_release.repeat_count);
                    break;
                case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_HOLD_RELEASE_NONE:
                default:
                    break;
            }

            if (release.data.pending_multi_tap_hold_release.release_layer_after_action) {
                key_runtime_slot_result_push_layer_release(&result, release.data.pending_multi_tap_hold_release.key_pos);
            }
            return result;
        case KEY_RUNTIME_SLOT_RELEASE_EVENT_CLEANUP:
            if (release.data.cleanup.release_layer) {
                key_runtime_slot_result_push_layer_release(&result, release.data.cleanup.key_pos);
            }

            if (release.data.cleanup.release_owned_state) {
                key_runtime_slot_result_push_request_if_present(&result, release.data.cleanup.key_pos, (key_runtime_slot_effect_request_t){
                                                                                                         .release_owned_state = true,
                                                                                                     });
            }
            return result;
        case KEY_RUNTIME_SLOT_RELEASE_EVENT_NONE:
        default:
            return result;
    }
}

key_runtime_slot_result_t key_runtime_slot_take_active_scan_result(active_key_state_t *slot) {
    key_runtime_slot_result_t      result = {0};
    key_runtime_slot_scan_event_t event  = key_runtime_slot_take_active_scan_event(slot);

    if (!event.handled) {
        return result;
    }

    result.handled = true;
    key_runtime_slot_result_push_request_if_present(&result, event.key_pos, event.data.active_effects.immediate_hold_request);
    key_runtime_slot_result_push_request_if_present(&result, event.key_pos, event.data.active_effects.effect_request);
    return result;
}

key_runtime_slot_result_t key_runtime_slot_take_pending_multi_tap_scan_result(active_key_state_t *slot) {
    key_runtime_slot_result_t      result = {0};
    key_runtime_slot_scan_event_t event  = key_runtime_slot_take_pending_multi_tap_scan_event(slot);

    if (!event.handled) {
        return result;
    }

    result.handled = true;

    switch (event.kind) {
        case KEY_RUNTIME_SLOT_SCAN_EVENT_PENDING_MULTI_TAP_EFFECTS:
            if (event.data.pending_multi_tap_effects.release_layer_before_action) {
                key_runtime_slot_result_push_layer_release(&result, event.key_pos);
            }
            key_runtime_slot_result_push_request_if_present(&result, event.key_pos, event.data.pending_multi_tap_effects.effect_request);
            return result;
        case KEY_RUNTIME_SLOT_SCAN_EVENT_PENDING_MULTI_TAP_FLUSH:
            key_runtime_slot_result_push_delayed_action(&result, event.data.pending_multi_tap_flush.action, event.data.pending_multi_tap_flush.mods, event.data.pending_multi_tap_flush.repeat_count);
            return result;
        case KEY_RUNTIME_SLOT_SCAN_EVENT_ACTIVE_EFFECTS:
        case KEY_RUNTIME_SLOT_SCAN_EVENT_NONE:
        default:
            return result;
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
