// ────────────────────────────────────────────────────────────────────────────
// Multi-Tap State Machine
// ────────────────────────────────────────────────────────────────────────────
//
// Count-based tap detection: tracks how many times a key has been tapped
// within CUSTOM_MULTI_TAP_TERM and resolves to the configured action.
//
// The normalized key_behavior view maps (keycode, tap_count) pairs to
// behavior steps.  A key can have entries at any tap count (1, 2, 3, ...),
// and the state machine handles them all uniformly.  A tap_count=1 entry
// overrides the single-tap action (useful for giving MO keys a tap action).
//
//   - On each quick tap, increment the count.
//   - If a higher tap count exists for this key, defer and wait.
//   - If this is the max configured count, fire immediately.
//   - If the timer expires, resolve the exact tap count we reached.
//
// Hold-after-multi-tap:
//
//   When a behavior step has hold.present = true, the state machine
//   enters a "pending hold" state instead of firing immediately.  It then
//   distinguishes between:
//     - Quick release → fires the entry's action (tap)
//     - Held past CUSTOM_TAP_HOLD_TERM → fires the hold tier
//   If more taps exist, a quick release resumes normal deferral so the
//   user can still reach higher tap counts.
//
//   hold.mode controls *when* the hold fires:
//     PRESS_AND_HOLD_UNTIL_RELEASE — fires at threshold via matrix_scan,
//       registered for auto-repeat, can transition to long hold.
//     TAP_AT_HOLD_THRESHOLD — fires once at threshold via matrix_scan.
//     TAP_ON_RELEASE_AFTER_HOLD — does NOT fire at threshold; resolved entirely
//       on release.  The caller checks elapsed time and the cached
//       long-hold tier to pick the right tier (tap / hold / long hold).
//
// Usage from keymap.c:
//
//   multi_tap_begin()        — call on tap release to start tracking.
//   multi_tap_advance()      — call on re-press of the same key.
//                              Returns the action to fire, or KC_NO if waiting.
//   multi_tap_flush()        — call when the timer expires or a different key
//                              is pressed.  Commits the pending action.
//   multi_tap_resolve_hold() — call on release during pending_hold.
//                              Returns action to fire, or KC_NO if resuming deferral.
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // QMK

#include "key_behavior.h"

// ─── State ──────────────────────────────────────────────────────────────────

typedef struct {
    uint16_t keycode;        // the key being tracked (KC_NO = idle)
    uint16_t timer;          // when the current pending window started
    uint8_t  count;          // taps counted so far (0 = idle)
    uint16_t single_action;  // what to send if count stays at 1
    bool     pending_hold;   // true = waiting to see if final tap is held
    uint16_t tap_action;     // action to fire on quick release during pending_hold
    uint16_t tap_hold_term;  // resolved tap-vs-hold threshold for this key
    uint16_t multi_tap_term; // max gap between consecutive taps
    hold_behavior_t hold;    // hold tier for the current tap count
    hold_behavior_t long_hold; // long-hold tier paired with hold
} multi_tap_t;

// ─── Helpers ────────────────────────────────────────────────────────────────

static inline void multi_tap_reset(multi_tap_t *mt) {
    mt->count         = 0;
    mt->keycode       = KC_NO;
    mt->single_action = KC_NO;
    mt->pending_hold  = false;
    mt->tap_action    = KC_NO;
    mt->tap_hold_term = CUSTOM_TAP_HOLD_TERM;
    mt->multi_tap_term = CUSTOM_MULTI_TAP_TERM;
    mt->hold          = hold_behavior_none();
    mt->long_hold     = hold_behavior_none();
}

static inline bool multi_tap_active(const multi_tap_t *mt) {
    return mt->count > 0;
}

static inline bool multi_tap_pending_hold(const multi_tap_t *mt) {
    return mt->pending_hold;
}

static inline bool multi_tap_expired(const multi_tap_t *mt) {
    return multi_tap_active(mt) && !mt->pending_hold &&
           timer_elapsed(mt->timer) >= mt->multi_tap_term;
}

// Has the hold threshold been reached during pending_hold?
// Only returns true for threshold-fired holds so matrix_scan handles them.
// TAP_ON_RELEASE_AFTER_HOLD is resolved entirely on release in process_record_user.
static inline bool multi_tap_hold_elapsed(const multi_tap_t *mt) {
    return mt->pending_hold && hold_fires_at_threshold(mt->hold) &&
           timer_elapsed(mt->timer) >= mt->tap_hold_term;
}

// ─── Core API ───────────────────────────────────────────────────────────────

// Start tracking a new multi-tap sequence (call on tap release).
// single_action is the normal single-tap action for this key (KC_NO = nothing).
static inline void multi_tap_begin(multi_tap_t *mt, uint16_t keycode, uint16_t single_action,
                                   uint16_t tap_hold_term, uint16_t multi_tap_term) {
    mt->count          = 1;
    mt->timer          = timer_read();
    mt->keycode        = keycode;
    mt->single_action  = single_action;
    mt->pending_hold   = false;
    mt->tap_action     = KC_NO;
    mt->tap_hold_term  = tap_hold_term;
    mt->multi_tap_term = multi_tap_term;
    mt->hold           = hold_behavior_none();
    mt->long_hold      = hold_behavior_none();
}

static inline void multi_tap_dispatch_repeated(uint16_t action, uint8_t count,
                                               void (*dispatch)(uint16_t)) {
    if (action == KC_NO) return;
    for (uint8_t i = 0; i < count; i++) dispatch(action);
}

// Commit the pending action and reset (call on timeout or different-key press).
// Uses the exact tap count reached. If that tap count has no explicit tap
// action, replay the key's normal single tap count times.
// lookup is provided by keymap.c and returns the normalized behavior step.
// dispatch is called instead of tap_code16 to support custom action types.
static inline void multi_tap_flush(multi_tap_t *mt,
                                   key_behavior_step_t (*lookup)(uint16_t, uint8_t),
                                   void (*dispatch)(uint16_t)) {
    // If we're in pending_hold when flushed (e.g. different key pressed),
    // treat it as a tap at the current count.
    if (mt->pending_hold) {
        if (mt->tap_action != KC_NO) {
            dispatch(mt->tap_action);
        } else {
            multi_tap_dispatch_repeated(mt->single_action, mt->count, dispatch);
        }
        multi_tap_reset(mt);
        return;
    }

    if (mt->count >= 2) {
        key_behavior_step_t step = lookup(mt->keycode, mt->count);
        if (step.tap.present && step.tap.action != KC_NO) {
            dispatch(step.tap.action);
            multi_tap_reset(mt);
            return;
        }
    }

    // count == 1 or no explicit tap action matched — replay the actual taps.
    multi_tap_dispatch_repeated(mt->single_action, mt->count, dispatch);
    multi_tap_reset(mt);
}

// Advance the state machine on re-press of the same key.
// Returns the action to fire immediately, or KC_NO if state was advanced.
// lookup and has_more_taps are provided by keymap.c.
static inline uint16_t multi_tap_advance(multi_tap_t *mt, uint16_t keycode,
                                         key_behavior_step_t (*lookup)(uint16_t, uint8_t),
                                         bool (*has_more)(uint16_t, uint8_t)) {
    mt->count++;
    mt->timer = timer_read();

    key_behavior_step_t step = lookup(keycode, mt->count);

    // If the current count has a hold variant, enter pending-hold state.
    // The caller will resolve on release (quick = tap_action) or hold
    // threshold (matrix_scan = hold tier).  If more taps exist, a quick
    // release resumes normal deferral.
    if (key_behavior_step_present(step) && step.hold.present) {
        mt->pending_hold = true;
        mt->tap_action   = step.tap.action;
        mt->hold         = step.hold;
        mt->long_hold    = step.long_hold;
        return KC_NO;
    }

    if (key_behavior_step_present(step) && !has_more(keycode, mt->count)) {
        // Max configured tap count reached, no hold variant — fire immediately.
        uint16_t action = step.tap.action;
        multi_tap_reset(mt);
        return action;
    }

    // Higher tap count exists — keep waiting. If we're at a gap
    // (no entry here but one above), timeout will fall back to the
    // key's normal single tap repeated count times.
    return KC_NO;
}

// Resolve the pending-hold state on key release.
// Returns the action to fire, and writes how many times to fire it into
// repeat_count. Returns KC_NO if resuming multi-tap deferral
// (quick release with more taps available).
static inline uint16_t multi_tap_resolve_hold(multi_tap_t *mt, uint16_t keycode,
                                              bool (*has_more)(uint16_t, uint8_t),
                                              uint8_t *repeat_count) {
    if (!mt->pending_hold) return KC_NO;

    uint16_t elapsed          = timer_elapsed(mt->timer);
    uint16_t cached_tap       = mt->tap_action;
    uint16_t cached_hold      = mt->hold.action;
    uint16_t cached_single    = mt->single_action;
    uint8_t  cached_count     = mt->count;
    uint16_t cached_term      = mt->tap_hold_term;

    *repeat_count = 1;

    mt->pending_hold = false;
    mt->tap_action   = KC_NO;
    mt->hold         = hold_behavior_none();
    mt->long_hold    = hold_behavior_none();

    if (elapsed >= cached_term) {
        // Held long enough — fire hold action.
        uint16_t action = cached_hold;
        multi_tap_reset(mt);
        return action;
    }

    // Released quickly — this was a tap, not a hold.
    if (has_more(keycode, mt->count)) {
        // More taps possible — resume normal multi-tap deferral.
        mt->timer = timer_read();
        *repeat_count = 0;
        return KC_NO;
    }

    // No more taps — fire the exact tap-count result now.
    uint16_t action = cached_tap;
    if (action == KC_NO) {
        action        = cached_single;
        *repeat_count = cached_count;
    }
    multi_tap_reset(mt);
    return action;
}
