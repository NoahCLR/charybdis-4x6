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
// Usage from keymap.c:
//
//   multi_tap_begin()   — call on tap release to start tracking.
//   multi_tap_advance() — call on re-press of the same key.
//                         Returns the action to fire, or KC_NO if waiting.
//   multi_tap_flush()   — call when the timer expires or a different key
//                         is pressed.  Commits the pending action.
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
} multi_tap_t;

// ─── Helpers ────────────────────────────────────────────────────────────────

static inline void multi_tap_reset(multi_tap_t *mt) {
    mt->count         = 0;
    mt->keycode       = KC_NO;
    mt->single_action = KC_NO;
}

static inline bool multi_tap_active(const multi_tap_t *mt) {
    return mt->count > 0;
}

static inline bool multi_tap_expired(const multi_tap_t *mt) {
    return multi_tap_active(mt) && timer_elapsed(mt->timer) >= CUSTOM_MULTI_TAP_TERM;
}

// ─── Core API ───────────────────────────────────────────────────────────────

// Start tracking a new multi-tap sequence (call on tap release).
// single_action is what to send if no second tap arrives (KC_NO = nothing).
static inline void multi_tap_begin(multi_tap_t *mt, uint16_t keycode, uint16_t single_action) {
    mt->count         = 1;
    mt->timer         = timer_read();
    mt->keycode       = keycode;
    mt->single_action = single_action;
}

// Commit the pending action and reset (call on timeout or different-key press).
// Walks backwards from the current count to find the highest defined action.
// Falls back to single_action if no multi-tap entry matches.
// tap_action_lookup is provided by keymap.c (depends on config table macros).
static inline void multi_tap_flush(multi_tap_t *mt, const tap_action_t *(*lookup)(uint16_t, uint8_t)) {
    if (mt->count >= 2) {
        // Find the highest defined action at or below our count.
        for (uint8_t c = mt->count; c >= 2; c--) {
            const tap_action_t *entry = lookup(mt->keycode, c);
            if (entry) {
                tap_code16(entry->action);
                multi_tap_reset(mt);
                return;
            }
        }
    }
    // count == 1 or no multi-tap entry matched — send single tap.
    if (mt->single_action != KC_NO) tap_code16(mt->single_action);
    multi_tap_reset(mt);
}

// Advance the state machine on re-press of the same key.
// Returns the action to fire immediately, or KC_NO if state was advanced (waiting for more taps).
// tap_action_lookup and has_more_taps are provided by keymap.c.
static inline uint16_t multi_tap_advance(multi_tap_t *mt, uint16_t keycode,
                                         const tap_action_t *(*lookup)(uint16_t, uint8_t),
                                         bool (*has_more)(uint16_t, uint8_t)) {
    mt->count++;
    mt->timer = timer_read();

    const tap_action_t *entry = lookup(keycode, mt->count);

    if (entry && !has_more(keycode, mt->count)) {
        // Max configured tap count reached — fire immediately.
        uint16_t action = entry->action;
        multi_tap_reset(mt);
        return action;
    }

    // Higher tap count exists — keep waiting.  If we're at a gap
    // (no entry here but one above), flush will walk backwards to
    // find the best match when the timer expires.
    return KC_NO;
}
