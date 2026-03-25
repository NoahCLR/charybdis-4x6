// ────────────────────────────────────────────────────────────────────────────
// Multi-Tap State Machine
// ────────────────────────────────────────────────────────────────────────────
//
// Count-based tap detection: tracks how many times a key has been tapped
// within CUSTOM_MULTI_TAP_TERM and resolves to the configured action.
//
// The tap_actions[] config table (in key_config.h) maps (keycode, tap_count)
// pairs to actions.  A key can have entries at any tap count (2, 3, 4, ...),
// and the state machine handles them all uniformly:
//
//   - On each quick tap, increment the count.
//   - If a higher tap count exists for this key, defer and wait.
//   - If this is the max configured count, fire immediately.
//   - If the timer expires, fire whatever count we've reached.
//
// Hold-after-multi-tap:
//
//   When a tap_action entry has hold_action != KC_NO, the state machine
//   enters a "pending hold" state instead of firing immediately.  It then
//   distinguishes between:
//     - Quick release → fires the entry's action (tap)
//     - Held past CUSTOM_TAP_HOLD_TERM → fires hold_action
//   If more taps exist, a quick release resumes normal deferral so the
//   user can still reach higher tap counts.
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

// ─── State ──────────────────────────────────────────────────────────────────

typedef struct {
    uint16_t keycode;       // the key being tracked (KC_NO = idle)
    uint16_t timer;         // when the current pending window started
    uint8_t  count;         // taps counted so far (0 = idle)
    uint16_t single_action; // what to send if count stays at 1
    bool     pending_hold;  // true = waiting to see if final tap is held
    uint16_t tap_action;    // action to fire on quick release during pending_hold
    uint16_t hold_action;   // action to fire if held past threshold during pending_hold
} multi_tap_t;

// ─── Helpers ────────────────────────────────────────────────────────────────

static inline void multi_tap_reset(multi_tap_t *mt) {
    mt->count         = 0;
    mt->keycode       = KC_NO;
    mt->single_action = KC_NO;
    mt->pending_hold  = false;
    mt->tap_action    = KC_NO;
    mt->hold_action   = KC_NO;
}

static inline bool multi_tap_active(const multi_tap_t *mt) {
    return mt->count > 0;
}

static inline bool multi_tap_pending_hold(const multi_tap_t *mt) {
    return mt->pending_hold;
}

static inline bool multi_tap_expired(const multi_tap_t *mt) {
    return multi_tap_active(mt) && !mt->pending_hold &&
           timer_elapsed(mt->timer) >= CUSTOM_MULTI_TAP_TERM;
}

// Has the hold threshold been reached during pending_hold?
static inline bool multi_tap_hold_elapsed(const multi_tap_t *mt) {
    return mt->pending_hold && timer_elapsed(mt->timer) >= CUSTOM_TAP_HOLD_TERM;
}

// ─── Core API ───────────────────────────────────────────────────────────────

// Start tracking a new multi-tap sequence (call on tap release).
// single_action is what to send if no second tap arrives (KC_NO = nothing).
static inline void multi_tap_begin(multi_tap_t *mt, uint16_t keycode, uint16_t single_action) {
    mt->count         = 1;
    mt->timer         = timer_read();
    mt->keycode       = keycode;
    mt->single_action = single_action;
    mt->pending_hold  = false;
    mt->tap_action    = KC_NO;
    mt->hold_action   = KC_NO;
}

// Commit the pending action and reset (call on timeout or different-key press).
// Walks backwards from the current count to find the highest defined action.
// Falls back to single_action if no multi-tap entry matches.
// tap_action_lookup is provided by keymap.c (depends on config table macros).
// dispatch is called instead of tap_code16 to support custom action types.
static inline void multi_tap_flush(multi_tap_t *mt,
                                   const tap_action_t *(*lookup)(uint16_t, uint8_t),
                                   void (*dispatch)(uint16_t)) {
    // If we're in pending_hold when flushed (e.g. different key pressed),
    // treat it as a tap — fire the tap_action.
    if (mt->pending_hold) {
        if (mt->tap_action != KC_NO) dispatch(mt->tap_action);
        multi_tap_reset(mt);
        return;
    }

    if (mt->count >= 2) {
        // Find the highest defined action at or below our count.
        for (uint8_t c = mt->count; c >= 2; c--) {
            const tap_action_t *entry = lookup(mt->keycode, c);
            if (entry) {
                dispatch(entry->action);
                multi_tap_reset(mt);
                return;
            }
        }
    }
    // count == 1 or no multi-tap entry matched — send single tap.
    if (mt->single_action != KC_NO) dispatch(mt->single_action);
    multi_tap_reset(mt);
}

// Advance the state machine on re-press of the same key.
// Returns the action to fire immediately, or KC_NO if state was advanced.
// tap_action_lookup and has_more_taps are provided by keymap.c.
static inline uint16_t multi_tap_advance(multi_tap_t *mt, uint16_t keycode,
                                         const tap_action_t *(*lookup)(uint16_t, uint8_t),
                                         bool (*has_more)(uint16_t, uint8_t)) {
    mt->count++;
    mt->timer = timer_read();

    const tap_action_t *entry = lookup(keycode, mt->count);

    // If the current count has a hold variant, enter pending-hold state.
    // The caller will resolve on release (quick = tap_action) or hold
    // threshold (matrix_scan = hold_action).  If more taps exist, a quick
    // release resumes normal deferral.
    if (entry && entry->hold_action != KC_NO) {
        mt->pending_hold = true;
        mt->tap_action   = entry->action;
        mt->hold_action  = entry->hold_action;
        return KC_NO;
    }

    if (entry && !has_more(keycode, mt->count)) {
        // Max configured tap count reached, no hold variant — fire immediately.
        uint16_t action = entry->action;
        multi_tap_reset(mt);
        return action;
    }

    // Higher tap count exists — keep waiting.  If we're at a gap
    // (no entry here but one above), flush will walk backwards to
    // find the best match when the timer expires.
    return KC_NO;
}

// Resolve the pending-hold state on key release.
// Returns the action to fire, or KC_NO if resuming multi-tap deferral
// (quick release with more taps available).
static inline uint16_t multi_tap_resolve_hold(multi_tap_t *mt, uint16_t keycode,
                                              bool (*has_more)(uint16_t, uint8_t)) {
    if (!mt->pending_hold) return KC_NO;

    uint16_t elapsed     = timer_elapsed(mt->timer);
    uint16_t cached_tap  = mt->tap_action;
    uint16_t cached_hold = mt->hold_action;

    mt->pending_hold = false;
    mt->tap_action   = KC_NO;
    mt->hold_action  = KC_NO;

    if (elapsed >= CUSTOM_TAP_HOLD_TERM) {
        // Held long enough — fire hold action.
        uint16_t action = cached_hold;
        multi_tap_reset(mt);
        return action;
    }

    // Released quickly — this was a tap, not a hold.
    if (has_more(keycode, mt->count)) {
        // More taps possible — resume normal multi-tap deferral.
        mt->timer = timer_read();
        return KC_NO;
    }

    // No more taps — fire tap action now.
    uint16_t action = cached_tap;
    multi_tap_reset(mt);
    return action;
}
