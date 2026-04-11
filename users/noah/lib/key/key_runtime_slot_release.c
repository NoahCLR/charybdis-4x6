// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Release
// ────────────────────────────────────────────────────────────────────────────
//
// Release-specific slot transition helpers for the handled-key runtime.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_release_internal.h"

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

key_runtime_slot_release_apply_t key_runtime_slot_take_active_release(active_key_state_t *slot, uint16_t keycode, key_behavior_view_t behavior) {
    key_runtime_slot_release_apply_t apply = {0};

    if (!slot || slot->keycode == KC_NO) {
        return apply;
    }

    active_key_state_t released_key = *slot;
    uint16_t           elapsed      = timer_elapsed(released_key.timer);

    apply.handled       = true;
    apply.release_layer = behavior.is_momentary_layer;
    apply.key_pos       = released_key.key_pos;

    key_runtime_slot_reset(slot);

    key_runtime_slot_release_resolution_t resolution = key_runtime_slot_resolve_release(keycode, released_key, behavior, elapsed);
    apply.release_owned_state                        = resolution.release_owned_state;

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_TAP:
            if (behavior.has_multi_tap) {
                key_runtime_slot_begin_pending_multi_tap(slot, keycode, released_key.key_pos, released_key.tap_action, released_key.tap_hold_term, released_key.multi_tap_term);
            } else if (released_key.tap_action != KC_NO) {
                apply.outcome = KEY_RUNTIME_SLOT_RELEASE_APPLY_OUTCOME_DISPATCH_ACTION;
                apply.action  = released_key.tap_action;
            }
            return apply;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_ACTION:
            apply.outcome = KEY_RUNTIME_SLOT_RELEASE_APPLY_OUTCOME_DISPATCH_ACTION;
            apply.action  = resolution.action;
            return apply;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_PD_MODE_LOCK_TAP:
            apply.outcome          = KEY_RUNTIME_SLOT_RELEASE_APPLY_OUTCOME_PD_MODE_LOCK_TAP;
            apply.pd_mode_lock_tap = resolution.pd_mode_lock_tap;
            return apply;
        case KEY_RUNTIME_SLOT_RELEASE_OUTCOME_NONE:
        default:
            return apply;
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

key_runtime_slot_pending_multi_tap_hold_release_t key_runtime_slot_take_pending_multi_tap_hold_release(active_key_state_t *slot, uint16_t keycode, key_behavior_view_t behavior, uint16_t elapsed) {
    key_runtime_slot_pending_multi_tap_hold_release_t release = {0};

    if (!(slot && key_runtime_slot_pending_multi_tap_pending_hold(slot) && key_runtime_slot_pending_multi_tap_matches(slot, keycode, slot->key_pos))) {
        return release;
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

    release.handled                    = true;
    release.release_layer_after_action = behavior.is_momentary_layer;
    release.key_pos                    = slot->key_pos;
    release.action                     = action;
    release.mods                       = cached_mods;
    release.repeat_count               = repeat_count;
    release.outcome = key_runtime_slot_pending_multi_tap_release_uses_held_lifecycle(slot, cached_hold, action, repeat_count, elapsed)
                          ? KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_HOLD_RELEASE_HELD_LIFECYCLE
                          : KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_HOLD_RELEASE_DELAYED_ACTION;

    if (action == KC_NO && repeat_count == 0 && key_runtime_slot_has_pending_multi_tap(slot)) {
        // A quick release can intentionally keep the chain alive so a later
        // tap or timeout still resolves the current tap index.
        key_runtime_slot_clear_active_state(slot);
    } else {
        key_runtime_slot_reset(slot);
    }
    return release;
}

key_runtime_slot_release_event_t key_runtime_slot_take_handled_release(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, key_behavior_view_t behavior) {
    key_runtime_slot_release_event_t event = {0};

    if (slot) {
        uint16_t                                           elapsed = timer_elapsed(slot->timer);
        key_runtime_slot_pending_multi_tap_hold_release_t pending = key_runtime_slot_take_pending_multi_tap_hold_release(slot, keycode, behavior, elapsed);
        if (pending.handled) {
            event.handled                              = true;
            event.kind                                 = KEY_RUNTIME_SLOT_RELEASE_EVENT_PENDING_MULTI_TAP_HOLD_RELEASE;
            event.data.pending_multi_tap_hold_release = pending;
            return event;
        }
    }

    if (key_runtime_slot_matches(slot, keycode, key_pos)) {
        event.handled             = true;
        event.kind                = KEY_RUNTIME_SLOT_RELEASE_EVENT_ACTIVE_RELEASE;
        event.data.active_release = key_runtime_slot_take_active_release(slot, keycode, behavior);
        return event;
    }

    event.handled                   = true;
    event.kind                      = KEY_RUNTIME_SLOT_RELEASE_EVENT_CLEANUP;
    event.data.cleanup.key_pos      = key_pos;
    event.data.cleanup.release_layer = behavior.is_momentary_layer;
    event.data.cleanup.release_owned_state = true;
    return event;
}
