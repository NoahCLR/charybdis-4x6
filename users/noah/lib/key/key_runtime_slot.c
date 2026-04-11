// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slots
// ────────────────────────────────────────────────────────────────────────────
//
// Shared slot storage helpers for the split key runtime modules.
// This module centralizes slot lookup and lifecycle so higher-level code does
// not depend on the raw shared-state layout.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_state.h"
#include "key_behavior_lookup.h"
#include <stddef.h>

#include "../action/action_lifecycle.h"
#include "../action/action_dispatch.h"
#include "../pointing/pd_modes.h"

typedef enum {
    KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_NONE = 0,
    KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_TAP,
    KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_HELD,
    KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_REPEAT,
} key_runtime_slot_hold_threshold_dispatch_t;

static key_runtime_slot_effect_request_t key_runtime_slot_effect_request_none(void) {
    return (key_runtime_slot_effect_request_t){0};
}

static key_runtime_slot_hold_threshold_dispatch_t key_runtime_slot_hold_threshold_dispatch_kind(hold_behavior_t hold) {
    if (!hold.present) {
        return KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_NONE;
    }

    switch (hold.mode) {
        case HOLD_BEHAVIOR_TAP_AT_HOLD_THRESHOLD:
            return KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_TAP;
        case HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE:
            return noah_action_hold_kind(hold.action) == NOAH_ACTION_HOLD_KIND_PRESS_ONLY ? KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_TAP : KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_HELD;
        case HOLD_BEHAVIOR_REPEAT_WHILE_HELD:
            return KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_REPEAT;
        default:
            return KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_NONE;
    }
}

static bool key_runtime_slot_hold_activation_needs_pulse(hold_behavior_t hold, bool pulse_momentary_layer_action) {
    if (action_dispatch_is_layer_lock(hold.action)) {
        return true;
    }

    if (IS_QK_MOMENTARY(hold.action)) {
        return pulse_momentary_layer_action;
    }

    return false;
}

static void key_runtime_slot_clear_owned_hold(active_key_state_t *slot, key_runtime_slot_effect_request_t *request) {
    if (!slot || !request) {
        return;
    }

    if (slot->held_action_keycode != KC_NO || slot->repeat_binding_active) {
        request->release_owned_state = true;
        slot->held_action_keycode    = KC_NO;
        slot->repeat_binding_active  = false;
    }
}

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

bool key_runtime_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

active_key_state_t *key_runtime_primary_slot(void) {
    return &noah_runtime_shared_state.key.active_slots[0];
}

active_key_state_t *key_runtime_slot_at(uint8_t index) {
    if (index >= KEY_RUNTIME_ACTIVE_SLOT_CAPACITY) {
        return NULL;
    }

    return &noah_runtime_shared_state.key.active_slots[index];
}

active_key_state_t *key_runtime_first_active_slot(void) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (key_runtime_slot_active(slot)) {
            return slot;
        }
    }

    return NULL;
}

uint8_t key_runtime_slot_index(const active_key_state_t *slot) {
    if (!slot) {
        return KEY_RUNTIME_ACTIVE_SLOT_CAPACITY;
    }

    const active_key_state_t *base = &noah_runtime_shared_state.key.active_slots[0];

    if (slot < base || slot >= base + KEY_RUNTIME_ACTIVE_SLOT_CAPACITY) {
        return KEY_RUNTIME_ACTIVE_SLOT_CAPACITY;
    }

    return (uint8_t)(slot - base);
}

bool key_runtime_slot_idle(const active_key_state_t *slot) {
    return slot != NULL && slot->keycode == KC_NO && !multi_tap_active(&slot->pending_multi_tap);
}

bool key_runtime_slot_active(const active_key_state_t *slot) {
    return slot != NULL && slot->keycode != KC_NO;
}

bool key_runtime_slot_matches(const active_key_state_t *slot, uint16_t keycode, keypos_t key_pos) {
    return key_runtime_slot_active(slot) && slot->keycode == keycode && key_runtime_keypos_equal(slot->key_pos, key_pos);
}

bool key_runtime_slot_owns_key_position(const active_key_state_t *slot, keypos_t key_pos) {
    return (key_runtime_slot_active(slot) && key_runtime_keypos_equal(slot->key_pos, key_pos)) || (key_runtime_slot_has_pending_multi_tap(slot) && key_runtime_keypos_equal(slot->pending_multi_tap.key_pos, key_pos));
}

bool key_runtime_slot_has_pending_multi_tap(const active_key_state_t *slot) {
    return slot != NULL && multi_tap_active(&slot->pending_multi_tap);
}

bool key_runtime_slot_pending_multi_tap_matches(const active_key_state_t *slot, uint16_t keycode, keypos_t key_pos) {
    return slot != NULL && multi_tap_matches(&slot->pending_multi_tap, keycode, key_pos);
}

bool key_runtime_slot_pending_multi_tap_pending_hold(const active_key_state_t *slot) {
    return slot != NULL && multi_tap_pending_hold(&slot->pending_multi_tap);
}

bool key_runtime_slot_pending_multi_tap_expired(const active_key_state_t *slot) {
    return slot != NULL && multi_tap_expired(&slot->pending_multi_tap);
}

active_key_state_t *key_runtime_find_slot_by_position(keypos_t key_pos) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (key_runtime_slot_active(slot) && key_runtime_keypos_equal(slot->key_pos, key_pos)) {
            return slot;
        }
    }

    return NULL;
}

active_key_state_t *key_runtime_find_slot_with_pending_multi_tap(keypos_t key_pos) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (!key_runtime_slot_active(slot) && key_runtime_slot_has_pending_multi_tap(slot) && key_runtime_keypos_equal(slot->pending_multi_tap.key_pos, key_pos)) {
            return slot;
        }
    }

    return NULL;
}

active_key_state_t *key_runtime_find_free_slot(void) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (key_runtime_slot_idle(slot)) {
            return slot;
        }
    }

    return NULL;
}

active_key_state_t *key_runtime_find_reclaimable_slot(void) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (!key_runtime_slot_active(slot) && key_runtime_slot_has_pending_multi_tap(slot)) {
            return slot;
        }
    }

    return NULL;
}

active_key_state_t *key_runtime_select_slot_for_press(keypos_t key_pos) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (key_runtime_slot_owns_key_position(slot, key_pos)) {
            return slot;
        }
    }

    active_key_state_t *slot = key_runtime_find_free_slot();
    if (slot) {
        return slot;
    }

    slot = key_runtime_find_reclaimable_slot();
    if (slot) {
        return slot;
    }

    return key_runtime_primary_slot();
}

multi_tap_t *key_runtime_primary_multi_tap(void) {
    return &noah_runtime_shared_state.key.active_slots[0].pending_multi_tap;
}

multi_tap_t *key_runtime_multi_tap_slot_at(uint8_t index) {
    active_key_state_t *slot = key_runtime_slot_at(index);

    if (!slot) {
        return NULL;
    }

    return &slot->pending_multi_tap;
}

multi_tap_t *key_runtime_multi_tap_for_slot(const active_key_state_t *slot) {
    if (!slot) {
        return NULL;
    }

    return (multi_tap_t *)&slot->pending_multi_tap;
}

active_key_state_t *key_runtime_slot_for_multi_tap(const multi_tap_t *mt) {
    if (!mt) {
        return NULL;
    }

    const active_key_state_t *base = &noah_runtime_shared_state.key.active_slots[0];
    const multi_tap_t        *min  = &base[0].pending_multi_tap;
    const multi_tap_t        *max  = &base[KEY_RUNTIME_ACTIVE_SLOT_CAPACITY - 1].pending_multi_tap;

    if (mt < min || mt > max) {
        return NULL;
    }

    return (active_key_state_t *)((char *)mt - offsetof(active_key_state_t, pending_multi_tap));
}

bool key_runtime_multi_tap_slot_active(const multi_tap_t *mt) {
    return mt != NULL && multi_tap_active(mt);
}

multi_tap_t *key_runtime_first_active_multi_tap(void) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        multi_tap_t *mt = key_runtime_multi_tap_slot_at(index);

        if (key_runtime_multi_tap_slot_active(mt)) {
            return mt;
        }
    }

    return NULL;
}

multi_tap_t *key_runtime_find_multi_tap_by_position(keypos_t key_pos) {
    for (uint8_t index = 0; index < KEY_RUNTIME_ACTIVE_SLOT_CAPACITY; index++) {
        active_key_state_t *slot = key_runtime_slot_at(index);

        if (key_runtime_slot_has_pending_multi_tap(slot) && key_runtime_keypos_equal(slot->pending_multi_tap.key_pos, key_pos)) {
            return &slot->pending_multi_tap;
        }
    }

    return NULL;
}

void key_runtime_slot_begin_pending_multi_tap(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t single_action, uint16_t tap_hold_term, uint16_t multi_tap_term) {
    if (!slot) {
        return;
    }

    multi_tap_begin(&slot->pending_multi_tap, keycode, key_pos, single_action, tap_hold_term, multi_tap_term);
}

uint16_t key_runtime_slot_advance_pending_multi_tap(active_key_state_t *slot, uint16_t keycode) {
    if (!slot) {
        return KC_NO;
    }

    return multi_tap_advance(&slot->pending_multi_tap, keycode, key_behavior_step_lookup, key_behavior_has_more_taps);
}

uint16_t key_runtime_slot_resolve_pending_multi_tap_hold(active_key_state_t *slot, uint16_t keycode, uint8_t *repeat_count) {
    if (!slot) {
        if (repeat_count) {
            *repeat_count = 0;
        }
        return KC_NO;
    }

    return multi_tap_resolve_hold(&slot->pending_multi_tap, keycode, key_behavior_has_more_taps, repeat_count);
}

key_runtime_slot_pending_multi_tap_flush_t key_runtime_slot_take_pending_multi_tap_flush(active_key_state_t *slot) {
    key_runtime_slot_pending_multi_tap_flush_t flush = {0};
    multi_tap_t                               *mt    = key_runtime_multi_tap_for_slot(slot);

    if (!key_runtime_slot_has_pending_multi_tap(slot) || !mt) {
        return flush;
    }

    flush.handled = true;
    flush.mods    = delayed_action_mods_from_multi_tap(mt);

    if (mt->pending_hold) {
        if (mt->tap_action != KC_NO) {
            flush.action       = mt->tap_action;
            flush.repeat_count = 1;
        } else {
            flush.action       = mt->single_action;
            flush.repeat_count = mt->count;
        }
        key_runtime_slot_reset_pending_multi_tap(slot);
        return flush;
    }

    if (mt->count >= 2) {
        key_behavior_step_t step = key_behavior_step_lookup(mt->keycode, mt->count);
        if (step.tap.present && step.tap.action != KC_NO) {
            flush.action       = step.tap.action;
            flush.repeat_count = 1;
            key_runtime_slot_reset_pending_multi_tap(slot);
            return flush;
        }
    }

    flush.action       = mt->single_action;
    flush.repeat_count = mt->count;
    key_runtime_slot_reset_pending_multi_tap(slot);
    return flush;
}

static bool key_runtime_slot_release_is_interrupted_layer_tap(active_key_state_t released_key, key_behavior_view_t behavior) {
    return behavior.is_momentary_layer && released_key.layer_interrupted;
}

static bool key_runtime_slot_release_is_buffered_base_tap(active_key_state_t released_key) {
    return released_key.fallback_hold_pending && released_key.tap_action == KC_NO && released_key.held_action_keycode == KC_NO;
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
    bool                                quick_tap            = key_runtime_slot_release_is_quick_tap(released_key, behavior, elapsed);
    bool                                quick_immediate_hold = hold_registers_on_press(released_key.hold) && quick_tap;
    pd_mode_mask_t                      lock_tap_mode        = key_runtime_slot_locked_pd_mode_tap_mode(keycode, released_key, elapsed, behavior);
    key_runtime_slot_release_resolution_t resolution         = {
        .release_owned_state = released_key.held_action_keycode != KC_NO || released_key.repeat_binding_active,
    };

    if (released_key.hold_fired || released_key.held_action_keycode != KC_NO || released_key.repeat_binding_active) {
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

    if (released_key.fallback_hold_pending) {
        return resolution;
    }

    if (hold_sends_on_release(released_key.hold)) {
        resolution.outcome = KEY_RUNTIME_SLOT_RELEASE_OUTCOME_ACTION;
        resolution.action  = hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term ? released_key.long_hold.action : released_key.hold.action;
        return resolution;
    }

    if (hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term) {
        resolution.outcome = KEY_RUNTIME_SLOT_RELEASE_OUTCOME_ACTION;
        resolution.action  = released_key.long_hold.action;
        return resolution;
    }

    if (!released_key.hold_one_shot_fired && !behavior.is_momentary_layer && released_key.tap_action != KC_NO) {
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

key_runtime_slot_scan_resolution_t key_runtime_slot_resolve_scan(active_key_state_t active_key_state, uint16_t elapsed) {
    key_runtime_slot_scan_resolution_t resolution = {0};

    if (hold_registers_on_press(active_key_state.hold) && !active_key_state.hold_one_shot_fired && elapsed >= active_key_state.tap_hold_term) {
        resolution.commit_immediate_hold         = true;
        resolution.immediate_hold_needs_feedback = !active_key_state.implicit_hold;
        resolution.immediate_hold_completes_hold = !active_key_state.long_hold.present;
    }

    if (active_key_state.fallback_hold_pending && active_key_state.held_action_keycode == KC_NO && elapsed >= active_key_state.tap_hold_term) {
        resolution.activate_fallback_hold = true;
        return resolution;
    }

    if (hold_fires_at_threshold(active_key_state.long_hold) && elapsed >= active_key_state.longer_hold_term) {
        resolution.outcome   = KEY_RUNTIME_SLOT_SCAN_OUTCOME_PROMOTE_LONG_HOLD;
        resolution.long_hold = active_key_state.long_hold;
        return resolution;
    }

    if (hold_fires_at_threshold(active_key_state.hold) && elapsed >= active_key_state.tap_hold_term) {
        resolution.outcome   = KEY_RUNTIME_SLOT_SCAN_OUTCOME_FIRE_HOLD;
        resolution.hold      = active_key_state.hold;
        resolution.long_hold = active_key_state.long_hold;
    }

    return resolution;
}

key_runtime_slot_scan_apply_t key_runtime_slot_apply_scan_resolution(active_key_state_t *slot, key_runtime_slot_scan_resolution_t resolution) {
    key_runtime_slot_scan_apply_t apply = {0};

    if (!slot) {
        return apply;
    }

    if (resolution.commit_immediate_hold) {
        apply.immediate_hold_request = key_runtime_slot_commit_immediate_hold(slot, resolution.immediate_hold_needs_feedback, resolution.immediate_hold_completes_hold);
    }

    if (resolution.activate_fallback_hold) {
        apply.outcome_request = key_runtime_slot_activate_pending_fallback_hold_request(slot);
        return apply;
    }

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_SCAN_OUTCOME_FIRE_HOLD:
            apply.outcome_request = key_runtime_slot_fire_hold_at_threshold(slot, resolution.hold, resolution.long_hold, false);
            return apply;
        case KEY_RUNTIME_SLOT_SCAN_OUTCOME_PROMOTE_LONG_HOLD:
            apply.outcome_request = key_runtime_slot_promote_to_long_hold(slot, resolution.long_hold, false);
            return apply;
        case KEY_RUNTIME_SLOT_SCAN_OUTCOME_NONE:
        default:
            return apply;
    }
}

static bool key_runtime_slot_pending_multi_tap_hold_elapsed(const multi_tap_t *multi_tap_state, uint16_t elapsed) {
    return multi_tap_state->pending_hold && hold_fires_at_threshold(multi_tap_state->hold) && elapsed >= multi_tap_state->tap_hold_term;
}

key_runtime_slot_pending_multi_tap_scan_resolution_t key_runtime_slot_resolve_pending_multi_tap_scan(active_key_state_t active_key_state, multi_tap_t multi_tap_state, uint16_t elapsed) {
    key_runtime_slot_pending_multi_tap_scan_resolution_t resolution = {0};

    if (!multi_tap_state.pending_hold || active_key_state.keycode == KC_NO) {
        return resolution;
    }

    if (hold_fires_at_threshold(multi_tap_state.long_hold) && elapsed >= active_key_state.longer_hold_term) {
        resolution.outcome                     = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_PROMOTE_LONG_HOLD;
        resolution.long_hold                   = multi_tap_state.long_hold;
        resolution.release_layer_before_action = is_layer_key(active_key_state.keycode) && action_dispatch_is_layer_lock(multi_tap_state.long_hold.action);
        return resolution;
    }

    if (key_runtime_slot_pending_multi_tap_hold_elapsed(&multi_tap_state, elapsed)) {
        resolution.outcome                     = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_FIRE_HOLD;
        resolution.hold                        = multi_tap_state.hold;
        resolution.long_hold                   = multi_tap_state.long_hold;
        resolution.release_layer_before_action = is_layer_key(active_key_state.keycode) && action_dispatch_is_layer_lock(multi_tap_state.hold.action);
    }

    return resolution;
}

key_runtime_slot_effect_request_t key_runtime_slot_activate_pending_fallback_hold_request(active_key_state_t *slot) {
    key_runtime_slot_effect_request_t request = key_runtime_slot_effect_request_none();

    if (!slot || !slot->fallback_hold_pending || slot->held_action_keycode != KC_NO || slot->keycode == KC_NO) {
        return request;
    }

    slot->held_action_keycode = slot->keycode;
    slot->hold_fired          = true;
    request.kind              = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER;
    request.action            = slot->keycode;
    return request;
}

key_runtime_slot_effect_request_t key_runtime_slot_begin_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired, bool implicit_hold, bool fallback_hold_pending, bool pd_mode_was_locked_on_press) {
    key_runtime_slot_effect_request_t request = key_runtime_slot_effect_request_none();

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

key_runtime_slot_press_plan_t key_runtime_slot_prepare_handled_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, bool active_held_action_survives_flush, bool has_multi_tap, bool is_momentary_layer, uint8_t layer, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool implicit_hold, bool fallback_hold_pending, bool pd_mode_was_locked_on_press) {
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

key_runtime_slot_effect_request_t key_runtime_slot_interrupt_on_other_press(active_key_state_t *slot, keypos_t other_key_pos) {
    if (!key_runtime_slot_active(slot) || key_runtime_keypos_equal(slot->key_pos, other_key_pos)) {
        return key_runtime_slot_effect_request_none();
    }

    key_runtime_slot_effect_request_t request = key_runtime_slot_activate_pending_fallback_hold_request(slot);

    if (is_layer_key(slot->keycode)) {
        slot->layer_interrupted = true;
    }

    return request;
}

key_runtime_slot_effect_request_t key_runtime_slot_commit_immediate_hold(active_key_state_t *slot, bool needs_feedback, bool completes_hold) {
    key_runtime_slot_effect_request_t request = key_runtime_slot_effect_request_none();

    if (!slot) {
        return request;
    }

    slot->hold_one_shot_fired = true;
    if (completes_hold) {
        slot->hold_fired = true;
    }

    request.feedback_pulse           = needs_feedback;
    request.feedback_long_hold_level = false;
    return request;
}

key_runtime_slot_effect_request_t key_runtime_slot_take_flush(active_key_state_t *slot, bool active_held_action_survives_flush) {
    key_runtime_slot_effect_request_t request = key_runtime_slot_effect_request_none();

    if (!key_runtime_slot_active(slot)) {
        return request;
    }

    if (slot->hold_fired || slot->held_action_keycode != KC_NO || slot->repeat_binding_active) {
        if (slot->held_action_keycode != KC_NO && !active_held_action_survives_flush) {
            request.kind   = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_UNREGISTER;
            request.action = slot->held_action_keycode;
        }
    } else if (!is_layer_key(slot->keycode) && slot->tap_action != KC_NO) {
        request.kind   = KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION;
        request.action = slot->tap_action;
    }

    key_runtime_slot_reset(slot);
    return request;
}

key_runtime_slot_effect_request_t key_runtime_slot_fire_hold_at_threshold(active_key_state_t *slot, hold_behavior_t hold, hold_behavior_t long_hold, bool pulse_momentary_layer_action) {
    key_runtime_slot_effect_request_t request = key_runtime_slot_effect_request_none();

    if (!slot) {
        return request;
    }

    switch (key_runtime_slot_hold_threshold_dispatch_kind(hold)) {
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_TAP:
            key_runtime_slot_clear_owned_hold(slot, &request);
            request.kind                 = KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION;
            request.action               = hold.action;
            request.feedback_pulse       = true;
            request.feedback_long_hold_level = false;
            if (long_hold.present) {
                slot->hold_fired          = false;
                slot->hold_one_shot_fired = true;
            } else {
                slot->hold_fired          = true;
                slot->hold_one_shot_fired = false;
            }
            return request;
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_HELD:
            slot->held_action_keycode = hold.action;
            slot->hold_fired          = !long_hold.present;
            slot->hold_one_shot_fired = false;
            request.kind              = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER;
            request.action            = hold.action;
            request.feedback_pulse    = key_runtime_slot_hold_activation_needs_pulse(hold, pulse_momentary_layer_action);
            request.feedback_long_hold_level = false;
            return request;
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_REPEAT:
            key_runtime_slot_clear_owned_hold(slot, &request);
            slot->repeat_binding_active = true;
            slot->hold_fired            = !long_hold.present;
            slot->hold_one_shot_fired   = false;
            request.kind                = KEY_RUNTIME_SLOT_EFFECT_REQUEST_REPEAT_START;
            request.action              = hold.action;
            request.repeat_hz           = hold.repeat_hz;
            request.feedback_pulse      = true;
            request.feedback_long_hold_level = false;
            return request;
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_NONE:
        default:
            return request;
    }
}

key_runtime_slot_effect_request_t key_runtime_slot_promote_to_long_hold(active_key_state_t *slot, hold_behavior_t long_hold, bool pulse_momentary_layer_action) {
    key_runtime_slot_effect_request_t request = key_runtime_slot_effect_request_none();

    if (!slot) {
        return request;
    }

    key_runtime_slot_clear_owned_hold(slot, &request);

    switch (key_runtime_slot_hold_threshold_dispatch_kind(long_hold)) {
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_TAP:
            slot->hold_fired               = true;
            request.kind                   = KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION;
            request.action                 = long_hold.action;
            request.feedback_pulse         = true;
            request.feedback_long_hold_level = true;
            return request;
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_HELD:
            slot->held_action_keycode      = long_hold.action;
            slot->hold_fired               = true;
            request.kind                   = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER;
            request.action                 = long_hold.action;
            request.feedback_pulse         = key_runtime_slot_hold_activation_needs_pulse(long_hold, pulse_momentary_layer_action);
            request.feedback_long_hold_level = true;
            return request;
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_REPEAT:
            slot->repeat_binding_active    = true;
            slot->hold_fired               = true;
            request.kind                   = KEY_RUNTIME_SLOT_EFFECT_REQUEST_REPEAT_START;
            request.action                 = long_hold.action;
            request.repeat_hz              = long_hold.repeat_hz;
            request.feedback_pulse         = true;
            request.feedback_long_hold_level = true;
            return request;
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_NONE:
        default:
            return request;
    }
}

key_runtime_slot_pending_multi_tap_scan_apply_t key_runtime_slot_apply_pending_multi_tap_scan_resolution(active_key_state_t *slot, key_runtime_slot_pending_multi_tap_scan_resolution_t resolution) {
    key_runtime_slot_pending_multi_tap_scan_apply_t apply = {0};

    if (!slot) {
        return apply;
    }

    apply.release_layer_before_action = resolution.release_layer_before_action;

    switch (resolution.outcome) {
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_PROMOTE_LONG_HOLD:
            slot->long_hold       = resolution.long_hold;
            apply.effect_request  = key_runtime_slot_promote_to_long_hold(slot, slot->long_hold, true);
            key_runtime_slot_reset_pending_multi_tap(slot);
            return apply;
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_FIRE_HOLD:
            slot->long_hold       = resolution.long_hold;
            apply.effect_request  = key_runtime_slot_fire_hold_at_threshold(slot, resolution.hold, slot->long_hold, true);
            key_runtime_slot_reset_pending_multi_tap(slot);
            return apply;
        case KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_NONE:
        default:
            return apply;
    }
}

key_runtime_slot_pending_multi_tap_plan_t key_runtime_slot_take_pending_multi_tap_plan(active_key_state_t *slot) {
    key_runtime_slot_pending_multi_tap_plan_t plan = {0};

    if (!slot || !key_runtime_slot_has_pending_multi_tap(slot)) {
        return plan;
    }

    plan.key_pos = key_runtime_slot_active(slot) ? slot->key_pos : slot->pending_multi_tap.key_pos;

    if (key_runtime_slot_pending_multi_tap_pending_hold(slot) && key_runtime_slot_active(slot)) {
        multi_tap_t *slot_multi_tap = key_runtime_multi_tap_for_slot(slot);
        if (!slot_multi_tap) {
            return plan;
        }

        uint16_t elapsed = timer_elapsed(slot_multi_tap->timer);
        key_runtime_slot_pending_multi_tap_scan_resolution_t resolution = key_runtime_slot_resolve_pending_multi_tap_scan(*slot, *slot_multi_tap, elapsed);
        key_runtime_slot_pending_multi_tap_scan_apply_t apply = key_runtime_slot_apply_pending_multi_tap_scan_resolution(slot, resolution);

        if (apply.release_layer_before_action || apply.effect_request.kind != KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE || apply.effect_request.release_owned_state || apply.effect_request.feedback_pulse) {
            plan.handled                     = true;
            plan.kind                        = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_PLAN_EFFECT_REQUEST;
            plan.release_layer_before_action = apply.release_layer_before_action;
            plan.effect_request              = apply.effect_request;
        }
        return plan;
    }

    if (key_runtime_slot_pending_multi_tap_expired(slot)) {
        plan.handled = true;
        plan.kind    = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_PLAN_FLUSH;
        plan.flush   = key_runtime_slot_take_pending_multi_tap_flush(slot);
    }

    return plan;
}

static bool key_runtime_slot_pending_multi_tap_release_uses_held_lifecycle(const active_key_state_t *slot, hold_behavior_t hold, uint16_t action, uint8_t repeat_count, uint16_t elapsed) {
    if (!slot || repeat_count != 1 || elapsed < slot->tap_hold_term) {
        return false;
    }

    if (hold.mode != HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE || action != hold.action) {
        return false;
    }

    return key_runtime_slot_hold_threshold_dispatch_kind(hold) == KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_HELD;
}

key_runtime_slot_pending_multi_tap_hold_release_t key_runtime_slot_take_pending_multi_tap_hold_release(active_key_state_t *slot, uint16_t keycode, key_behavior_view_t behavior, uint16_t elapsed) {
    key_runtime_slot_pending_multi_tap_hold_release_t release = {0};

    if (!(slot && key_runtime_slot_pending_multi_tap_pending_hold(slot) && key_runtime_slot_pending_multi_tap_matches(slot, keycode, slot->key_pos))) {
        return release;
    }

    multi_tap_t        *slot_multi_tap = key_runtime_multi_tap_for_slot(slot);
    delayed_action_mods_t cached_mods  = delayed_action_mods_from_multi_tap(slot_multi_tap);
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

void key_runtime_slot_reset_pending_multi_tap(active_key_state_t *slot) {
    if (!slot) {
        return;
    }

    multi_tap_reset(&slot->pending_multi_tap);
}

bool active_key_matches(uint16_t keycode, keypos_t key_pos) {
    return key_runtime_slot_matches(key_runtime_find_slot_by_position(key_pos), keycode, key_pos);
}

void key_runtime_slot_reset(active_key_state_t *slot) {
    if (!slot) {
        return;
    }

    *slot = (active_key_state_t)ACTIVE_KEY_STATE_INIT;
}

void active_key_reset(void) {
    key_runtime_slot_reset(key_runtime_primary_slot());
}

void key_runtime_slot_track(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired) {
    if (!slot) {
        return;
    }

    multi_tap_t pending_multi_tap = slot->pending_multi_tap;

    *slot = (active_key_state_t){
        .timer               = timer_read(),
        .keycode             = keycode,
        .key_pos             = key_pos,
        .hold_fired          = hold_fired,
        .held_action_keycode = KC_NO,
        .tap_action          = tap_action,
        .tap_hold_term       = tap_hold_term,
        .longer_hold_term    = longer_hold_term,
        .multi_tap_term      = multi_tap_term,
        .hold                = hold,
        .long_hold           = long_hold,
        .pending_multi_tap   = pending_multi_tap,
    };
}

void active_key_track(uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired) {
    key_runtime_slot_track(key_runtime_primary_slot(), keycode, key_pos, tap_action, hold, long_hold, tap_hold_term, longer_hold_term, multi_tap_term, hold_fired);
}
