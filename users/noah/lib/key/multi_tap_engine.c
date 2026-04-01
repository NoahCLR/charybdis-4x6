// ────────────────────────────────────────────────────────────────────────────
// Multi-Tap Engine
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#include "multi_tap_engine.h"

void multi_tap_reset(multi_tap_t *mt) {
    mt->count           = 0;
    mt->keycode         = KC_NO;
    mt->single_action   = KC_NO;
    mt->pending_hold    = false;
    mt->tap_action      = KC_NO;
    mt->tap_hold_term   = CUSTOM_TAP_HOLD_TERM;
    mt->multi_tap_term  = CUSTOM_MULTI_TAP_TERM;
    mt->hold            = hold_behavior_none();
    mt->long_hold       = hold_behavior_none();
    mt->saved_mods      = 0;
    mt->saved_weak_mods = 0;
    mt->saved_oneshot_mods = 0;
    mt->saved_oneshot_locked_mods = 0;
}

bool multi_tap_active(const multi_tap_t *mt) {
    return mt->count > 0;
}

bool multi_tap_pending_hold(const multi_tap_t *mt) {
    return mt->pending_hold;
}

bool multi_tap_expired(const multi_tap_t *mt) {
    return multi_tap_active(mt) && !mt->pending_hold && timer_elapsed(mt->timer) >= mt->multi_tap_term;
}

bool multi_tap_hold_elapsed(const multi_tap_t *mt) {
    return mt->pending_hold && hold_fires_at_threshold(mt->hold) && timer_elapsed(mt->timer) >= mt->tap_hold_term;
}

void multi_tap_begin(multi_tap_t *mt, uint16_t keycode, uint16_t single_action, uint16_t tap_hold_term, uint16_t multi_tap_term) {
    mt->count           = 1;
    mt->timer           = timer_read();
    mt->keycode         = keycode;
    mt->single_action   = single_action;
    mt->pending_hold    = false;
    mt->tap_action      = KC_NO;
    mt->tap_hold_term   = tap_hold_term;
    mt->multi_tap_term  = multi_tap_term;
    mt->hold            = hold_behavior_none();
    mt->long_hold       = hold_behavior_none();
    mt->saved_mods      = get_mods();
    mt->saved_weak_mods = get_weak_mods();
    mt->saved_oneshot_mods = get_oneshot_mods();
    mt->saved_oneshot_locked_mods = get_oneshot_locked_mods();
}

static void multi_tap_dispatch_repeated(uint16_t action, uint8_t count, const multi_tap_t *mt, void (*dispatch)(uint16_t, const multi_tap_t *)) {
    if (action == KC_NO) return;
    for (uint8_t i = 0; i < count; i++)
        dispatch(action, mt);
}

void multi_tap_flush(multi_tap_t *mt, key_behavior_step_t (*lookup)(uint16_t, uint8_t), void (*dispatch)(uint16_t, const multi_tap_t *)) {
    if (mt->pending_hold) {
        if (mt->tap_action != KC_NO) {
            dispatch(mt->tap_action, mt);
        } else {
            multi_tap_dispatch_repeated(mt->single_action, mt->count, mt, dispatch);
        }
        multi_tap_reset(mt);
        return;
    }

    if (mt->count >= 2) {
        key_behavior_step_t step = lookup(mt->keycode, mt->count);
        if (step.tap.present && step.tap.action != KC_NO) {
            dispatch(step.tap.action, mt);
            multi_tap_reset(mt);
            return;
        }
    }

    multi_tap_dispatch_repeated(mt->single_action, mt->count, mt, dispatch);
    multi_tap_reset(mt);
}

uint16_t multi_tap_advance(multi_tap_t *mt, uint16_t keycode, key_behavior_step_t (*lookup)(uint16_t, uint8_t), bool (*has_more)(uint16_t, uint8_t)) {
    mt->count++;
    mt->timer = timer_read();

    key_behavior_step_t step = lookup(keycode, mt->count);

    if (key_behavior_step_present(step) && step.hold.present) {
        mt->pending_hold = true;
        mt->tap_action   = step.tap.action;
        mt->hold         = step.hold;
        mt->long_hold    = step.long_hold;
        return KC_NO;
    }

    if (key_behavior_step_present(step) && !has_more(keycode, mt->count)) {
        uint16_t action = step.tap.action;
        multi_tap_reset(mt);
        return action;
    }

    return KC_NO;
}

uint16_t multi_tap_resolve_hold(multi_tap_t *mt, uint16_t keycode, bool (*has_more)(uint16_t, uint8_t), uint8_t *repeat_count) {
    if (!mt->pending_hold) return KC_NO;

    uint16_t elapsed       = timer_elapsed(mt->timer);
    uint16_t cached_tap    = mt->tap_action;
    uint16_t cached_hold   = mt->hold.action;
    uint16_t cached_single = mt->single_action;
    uint8_t  cached_count  = mt->count;
    uint16_t cached_term   = mt->tap_hold_term;

    *repeat_count = 1;

    mt->pending_hold = false;
    mt->tap_action   = KC_NO;
    mt->hold         = hold_behavior_none();
    mt->long_hold    = hold_behavior_none();

    if (elapsed >= cached_term) {
        uint16_t action = cached_hold;
        multi_tap_reset(mt);
        return action;
    }

    if (has_more(keycode, mt->count)) {
        mt->timer     = timer_read();
        *repeat_count = 0;
        return KC_NO;
    }

    uint16_t action = cached_tap;
    if (action == KC_NO) {
        action        = cached_single;
        *repeat_count = cached_count;
    }
    multi_tap_reset(mt);
    return action;
}
