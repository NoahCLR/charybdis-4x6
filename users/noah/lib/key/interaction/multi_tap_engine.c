// ────────────────────────────────────────────────────────────────────────────
// Multi-Tap Engine
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "multi_tap_engine.h"

static bool multi_tap_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

void multi_tap_reset(multi_tap_t *mt) {
    mt->count                     = 0;
    mt->keycode                   = KC_NO;
    mt->single_action             = KC_NO;
    mt->pending_hold              = false;
    mt->tap_action                = KC_NO;
    mt->tap_repeat_count          = 0;
    mt->has_more_taps             = false;
    mt->tap_hold_term             = CUSTOM_TAP_HOLD_TERM;
    mt->multi_tap_term            = CUSTOM_MULTI_TAP_TERM;
    mt->hold                      = hold_behavior_none();
    mt->long_hold                 = hold_behavior_none();
    mt->saved_mods                = 0;
    mt->saved_weak_mods           = 0;
    mt->saved_oneshot_mods        = 0;
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

bool multi_tap_matches(const multi_tap_t *mt, uint16_t keycode, keypos_t key_pos) {
    return multi_tap_active(mt) && mt->keycode == keycode && multi_tap_keypos_equal(mt->key_pos, key_pos);
}

void multi_tap_begin(multi_tap_t *mt, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, uint8_t tap_repeat_count, uint16_t tap_hold_term, uint16_t multi_tap_term, bool has_more_taps) {
    mt->count                     = 1;
    mt->timer                     = timer_read();
    mt->keycode                   = keycode;
    mt->key_pos                   = key_pos;
    mt->single_action             = tap_action;
    mt->pending_hold              = false;
    mt->tap_action                = tap_action;
    mt->tap_repeat_count          = tap_repeat_count;
    mt->has_more_taps             = has_more_taps;
    mt->tap_hold_term             = tap_hold_term;
    mt->multi_tap_term            = multi_tap_term;
    mt->hold                      = hold_behavior_none();
    mt->long_hold                 = hold_behavior_none();
    mt->saved_mods                = get_mods();
    mt->saved_weak_mods           = get_weak_mods();
    mt->saved_oneshot_mods        = get_oneshot_mods();
    mt->saved_oneshot_locked_mods = get_oneshot_locked_mods();
}

static void multi_tap_dispatch_repeated(uint16_t action, uint8_t count, const multi_tap_t *mt, void (*dispatch)(uint16_t, const multi_tap_t *)) {
    if (action == KC_NO) return;
    for (uint8_t i = 0; i < count; i++)
        dispatch(action, mt);
}

void multi_tap_flush(multi_tap_t *mt, void (*dispatch)(uint16_t, const multi_tap_t *)) {
    uint16_t action       = mt->tap_repeat_count > 0 ? mt->tap_action : mt->single_action;
    uint8_t  repeat_count = mt->tap_repeat_count > 0 ? mt->tap_repeat_count : mt->count;

    multi_tap_dispatch_repeated(action, repeat_count, mt, dispatch);
    multi_tap_reset(mt);
}

uint16_t multi_tap_advance(multi_tap_t *mt, uint16_t tap_action, uint8_t tap_repeat_count, bool has_more_taps, bool tap_resolves_on_press, hold_behavior_t hold, hold_behavior_t long_hold) {
    mt->count++;
    mt->timer            = timer_read();
    mt->tap_action       = tap_action;
    mt->tap_repeat_count = tap_repeat_count;
    mt->has_more_taps    = has_more_taps;
    mt->hold             = hold;
    mt->long_hold        = long_hold;
    mt->pending_hold     = hold.present || long_hold.present;

    if (mt->pending_hold) {
        return KC_NO;
    }

    if (tap_resolves_on_press) {
        uint16_t action = mt->tap_action;
        multi_tap_reset(mt);
        return action;
    }

    return KC_NO;
}

uint16_t multi_tap_resolve_hold(multi_tap_t *mt, uint8_t *repeat_count) {
    if (!mt->pending_hold) return KC_NO;

    uint16_t elapsed              = timer_elapsed(mt->timer);
    uint16_t cached_tap           = mt->tap_action;
    uint16_t cached_hold          = mt->hold.action;
    uint8_t  cached_tap_repeats   = mt->tap_repeat_count;
    uint16_t cached_term          = mt->tap_hold_term;
    bool     cached_has_hold      = mt->hold.present;
    bool     cached_has_more_taps = mt->has_more_taps;

    *repeat_count = 1;

    mt->pending_hold = false;
    mt->hold         = hold_behavior_none();
    mt->long_hold    = hold_behavior_none();

    if (cached_has_hold && elapsed >= cached_term) {
        uint16_t action = cached_hold;
        multi_tap_reset(mt);
        return action;
    }

    if (elapsed < cached_term && cached_has_more_taps) {
        mt->timer     = timer_read();
        *repeat_count = 0;
        return KC_NO;
    }

    uint16_t action = cached_tap_repeats > 0 ? cached_tap : mt->single_action;
    *repeat_count   = cached_tap_repeats > 0 ? cached_tap_repeats : mt->count;
    multi_tap_reset(mt);
    return action;
}
