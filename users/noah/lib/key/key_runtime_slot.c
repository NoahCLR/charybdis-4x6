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

void key_runtime_slot_reset_pending_multi_tap(active_key_state_t *slot) {
    if (!slot) {
        return;
    }

    multi_tap_reset(&slot->pending_multi_tap);
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
