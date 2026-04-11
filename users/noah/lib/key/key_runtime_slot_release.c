// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Release
// ────────────────────────────────────────────────────────────────────────────
//
// Release-specific slot transition helpers for the handled-key runtime.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_release.h"
#include "key_runtime_slot_result_internal.h"

#include "../action/action_lifecycle.h"
#include "../pointing/pd_modes.h"

static void key_runtime_slot_clear_active_state(active_key_state_t *slot) {
    if (!slot) {
        return;
    }

    multi_tap_t pending_multi_tap = slot->pending_multi_tap;
    *slot                         = (active_key_state_t)ACTIVE_KEY_STATE_INIT;
    slot->pending_multi_tap       = pending_multi_tap;
}

static uint16_t key_runtime_slot_select_release_hold_action(uint16_t elapsed, uint16_t hold_action, hold_behavior_t long_hold, uint16_t longer_hold_term) {
    if (hold_sends_on_release(long_hold) && elapsed >= longer_hold_term) {
        return long_hold.action;
    }

    return hold_action;
}

static bool key_runtime_slot_release_is_interrupted_layer_tap(active_key_state_t released_key, key_behavior_view_t behavior) {
    return behavior.is_momentary_layer && released_key.layer_interrupted;
}

static bool key_runtime_slot_release_is_buffered_base_tap(active_key_state_t released_key) {
    return key_runtime_slot_uses_fallback_hold(&released_key) && released_key.tap_action == KC_NO && released_key.held_action_keycode == KC_NO;
}

static bool key_runtime_slot_release_is_quick_tap(active_key_state_t released_key, key_behavior_view_t behavior, uint16_t elapsed) {
    return elapsed < released_key.tap_hold_term && !key_runtime_slot_release_is_interrupted_layer_tap(released_key, behavior);
}

static pd_mode_mask_t key_runtime_slot_locked_pd_mode_tap_mode(uint16_t keycode, active_key_state_t released_key, uint16_t elapsed, key_behavior_view_t behavior) {
    pd_mode_mask_t mode = pd_mode_for_keycode(keycode);
    if (!mode) {
        return 0;
    }

    if (!released_key.pd_mode_was_locked_on_press || elapsed >= released_key.tap_hold_term) {
        return 0;
    }

    if (key_runtime_slot_release_is_interrupted_layer_tap(released_key, behavior)) {
        return 0;
    }

    return mode;
}

key_runtime_slot_release_resolution_t key_runtime_slot_resolve_release(uint16_t keycode, active_key_state_t released_key, key_behavior_view_t behavior, uint16_t elapsed) {
    bool                                  quick_tap            = key_runtime_slot_release_is_quick_tap(released_key, behavior, elapsed);
    bool                                  quick_immediate_hold = hold_registers_on_press(released_key.hold) && key_runtime_slot_allows_tap_release(&released_key) && quick_tap;
    pd_mode_mask_t                        lock_tap_mode        = key_runtime_slot_locked_pd_mode_tap_mode(keycode, released_key, elapsed, behavior);
    key_runtime_slot_release_resolution_t resolution           = {
        .release_owned_state = released_key.held_action_keycode != KC_NO || released_key.repeat_binding_active,
    };

    if (key_runtime_slot_has_active_hold_tier(&released_key) || key_runtime_slot_hold_is_complete(&released_key) || released_key.held_action_keycode != KC_NO || released_key.repeat_binding_active) {
        if (lock_tap_mode) {
            resolution.outcome          = KEY_RUNTIME_SLOT_RELEASE_OUTCOME_PD_MODE_LOCK_TAP;
            resolution.pd_mode_lock_tap = lock_tap_mode;
            return resolution;
        }

        if (quick_immediate_hold) {
            resolution.outcome = KEY_RUNTIME_SLOT_RELEASE_OUTCOME_TAP;
            return resolution;
        }

        if (hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term) {
            resolution.outcome = KEY_RUNTIME_SLOT_RELEASE_OUTCOME_ACTION;
            resolution.action  = released_key.long_hold.action;
        }
        return resolution;
    }

    if (key_runtime_slot_release_is_buffered_base_tap(released_key)) {
        resolution.outcome = KEY_RUNTIME_SLOT_RELEASE_OUTCOME_TAP;
        return resolution;
    }

    if (quick_tap) {
        if (lock_tap_mode) {
            resolution.outcome          = KEY_RUNTIME_SLOT_RELEASE_OUTCOME_PD_MODE_LOCK_TAP;
            resolution.pd_mode_lock_tap = lock_tap_mode;
            return resolution;
        }

        resolution.outcome = KEY_RUNTIME_SLOT_RELEASE_OUTCOME_TAP;
        return resolution;
    }

    if (key_runtime_slot_uses_fallback_hold(&released_key)) {
        return resolution;
    }

    if (key_runtime_slot_has_pending_release_hold(&released_key) || hold_sends_on_release(released_key.hold)) {
        resolution.outcome = KEY_RUNTIME_SLOT_RELEASE_OUTCOME_ACTION;
        resolution.action  = hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term ? released_key.long_hold.action : released_key.hold.action;
        return resolution;
    }

    if (hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term) {
        resolution.outcome = KEY_RUNTIME_SLOT_RELEASE_OUTCOME_ACTION;
        resolution.action  = released_key.long_hold.action;
        return resolution;
    }

    if (key_runtime_slot_allows_tap_release(&released_key) && !behavior.is_momentary_layer && released_key.tap_action != KC_NO) {
        resolution.outcome = KEY_RUNTIME_SLOT_RELEASE_OUTCOME_TAP;
    }

    return resolution;
}

static key_runtime_slot_result_t key_runtime_slot_take_active_release_result(active_key_state_t *slot, uint16_t keycode, key_behavior_view_t behavior) {
    key_runtime_slot_result_t result = {0};

    if (!slot || slot->keycode == KC_NO) {
        return result;
    }

    active_key_state_t released_key = *slot;
    uint16_t           elapsed      = timer_elapsed(released_key.timer);

    result.handled = true;

    if (behavior.is_momentary_layer) {
        key_runtime_slot_result_push_layer_release(&result, released_key.key_pos);
    }

    key_runtime_slot_reset(slot);

    key_runtime_slot_release_resolution_t resolution = key_runtime_slot_resolve_release(keycode, released_key, behavior, elapsed);
    if (resolution.release_owned_state) {
        key_runtime_slot_result_push_request_if_present(&result, released_key.key_pos, (key_runtime_slot_effect_request_t){
                                                                                    .release_owned_state = true,
                                                                                });
    }

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_TAP:
            if (behavior.has_multi_tap) {
                key_runtime_slot_begin_pending_multi_tap(slot, keycode, released_key.key_pos, released_key.tap_action, released_key.tap_hold_term, released_key.multi_tap_term);
            } else if (released_key.tap_action != KC_NO) {
                key_runtime_slot_result_push_dispatch_action(&result, released_key.key_pos, released_key.tap_action);
            }
            return result;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_ACTION:
            key_runtime_slot_result_push_dispatch_action(&result, released_key.key_pos, resolution.action);
            return result;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_PD_MODE_LOCK_TAP:
            key_runtime_slot_result_push_pd_mode_lock_tap(&result, resolution.pd_mode_lock_tap);
            return result;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_NONE:
        default:
            return result;
    }
}

static bool key_runtime_slot_pending_multi_tap_release_uses_held_lifecycle(const active_key_state_t *slot, hold_behavior_t hold, uint16_t action, uint8_t repeat_count, uint16_t elapsed) {
    if (!slot || !hold.present || repeat_count != 1 || elapsed < slot->tap_hold_term) {
        return false;
    }

    if (hold.mode != HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE || action != hold.action) {
        return false;
    }

    return noah_action_hold_kind(hold.action) != NOAH_ACTION_HOLD_KIND_PRESS_ONLY;
}

static key_runtime_slot_result_t key_runtime_slot_take_pending_multi_tap_hold_release_result(active_key_state_t *slot, uint16_t keycode, key_behavior_view_t behavior, uint16_t elapsed) {
    key_runtime_slot_result_t result = {0};

    if (!(slot && key_runtime_slot_pending_multi_tap_pending_hold(slot) && key_runtime_slot_pending_multi_tap_matches(slot, keycode, slot->key_pos))) {
        return result;
    }

    multi_tap_t          *slot_multi_tap = key_runtime_multi_tap_for_slot(slot);
    delayed_action_mods_t cached_mods    = delayed_action_mods_from_multi_tap(slot_multi_tap);
    bool                  was_release_hold = hold_sends_on_release(slot_multi_tap->hold);
    hold_behavior_t       cached_hold      = slot_multi_tap->hold;
    hold_behavior_t       cached_long_hold = slot_multi_tap->long_hold;
    uint8_t               repeat_count     = 0;
    uint16_t              action           = key_runtime_slot_resolve_pending_multi_tap_hold(slot, keycode, &repeat_count);

    if (!cached_hold.present && hold_sends_on_release(cached_long_hold) && elapsed >= slot->longer_hold_term) {
        action = cached_long_hold.action;
    } else if (was_release_hold && cached_hold.present && repeat_count == 1 && action == cached_hold.action) {
        action = key_runtime_slot_select_release_hold_action(elapsed, cached_hold.action, cached_long_hold, slot->longer_hold_term);
    }

    result.handled = true;

    if (key_runtime_slot_pending_multi_tap_release_uses_held_lifecycle(slot, cached_hold, action, repeat_count, elapsed)) {
        key_runtime_slot_result_push_request_if_present(&result, slot->key_pos, (key_runtime_slot_effect_request_t){
                                                                                .kind   = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER,
                                                                                .action = action,
                                                                            });
        key_runtime_slot_result_push_request_if_present(&result, slot->key_pos, (key_runtime_slot_effect_request_t){
                                                                                .kind   = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_UNREGISTER,
                                                                                .action = action,
                                                                            });
    } else {
        key_runtime_slot_result_push_delayed_action(&result, action, cached_mods, repeat_count);
    }

    if (behavior.is_momentary_layer) {
        key_runtime_slot_result_push_layer_release(&result, slot->key_pos);
    }

    if (action == KC_NO && repeat_count == 0 && key_runtime_slot_has_pending_multi_tap(slot)) {
        // A quick release can intentionally keep the chain alive so a later
        // tap or timeout still resolves the current tap index.
        key_runtime_slot_clear_active_state(slot);
    } else {
        key_runtime_slot_reset(slot);
    }
    return result;
}

key_runtime_slot_result_t key_runtime_slot_take_handled_release_result(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, key_behavior_view_t behavior) {
    key_runtime_slot_result_t result = {0};

    if (slot) {
        uint16_t elapsed = timer_elapsed(slot->timer);
        result = key_runtime_slot_take_pending_multi_tap_hold_release_result(slot, keycode, behavior, elapsed);
        if (result.handled) {
            return result;
        }
    }

    if (key_runtime_slot_matches(slot, keycode, key_pos)) {
        return key_runtime_slot_take_active_release_result(slot, keycode, behavior);
    }

    result.handled = true;
    if (behavior.is_momentary_layer) {
        key_runtime_slot_result_push_layer_release(&result, key_pos);
    }
    key_runtime_slot_result_push_request_if_present(&result, key_pos, (key_runtime_slot_effect_request_t){
                                                                       .release_owned_state = true,
                                                                   });
    return result;
}
