// ────────────────────────────────────────────────────────────────────────────
// Multi-Tap Engine
// ────────────────────────────────────────────────────────────────────────────
//
// Count-based tap detection: tracks how many times a key has been tapped
// within CUSTOM_MULTI_TAP_TERM and resolves to the configured action.
//
// The normalized key_behavior view maps (keycode, tap_count) pairs to
// behavior steps. A key can have entries at any tap count (1, 2, 3, ...),
// and the state machine handles them all uniformly. A tap_count=1 entry
// overrides the single-tap action (useful for giving MO keys a tap action).
//
//   - On each quick tap, increment the count.
//   - If a higher tap count exists for this key, defer and wait.
//   - If this is the max configured count, fire immediately.
//   - If the timer expires, resolve the exact tap count we reached.
//
// Hold-after-multi-tap:
//
//   When a behavior step has hold.present = true or long_hold.present = true,
//   the state machine enters a "pending hold" state instead of firing
//   immediately. It then distinguishes between:
//     - Quick release -> fires the entry's action (tap)
//     - Held past CUSTOM_TAP_HOLD_TERM -> fires the hold tier, if one exists
//     - Held longer -> fires the long-hold tier, if one exists
//   If more taps exist, a quick release resumes normal deferral so the user
//   can still reach higher tap counts.
//
//   hold.mode controls when the hold fires:
//     PRESS_IMMEDIATELY_UNTIL_RELEASE -> internal runtime mode for implicit
//       single-press pd-mode holds; pending multi-tap hold resolution still
//       waits for threshold.
//     PRESS_AND_HOLD_UNTIL_RELEASE -> fires at threshold via matrix_scan,
//       registered for auto-repeat, can transition to long hold.
//     TAP_AT_HOLD_THRESHOLD -> fires once at threshold via matrix_scan.
//     REPEAT_WHILE_HELD -> fires once at threshold via matrix_scan, then keeps
//       tapping at the authored cadence until release.
//     TAP_ON_RELEASE_AFTER_HOLD -> does not fire at threshold; resolved
//       entirely on release.
//
// This interface is consumed by the split key runtime modules.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "key_behavior.h"

typedef struct {
    uint16_t        keycode;       // the key being tracked (KC_NO = idle)
    keypos_t        key_pos;       // physical key that owns the current tap sequence
    uint16_t        timer;         // when the current pending window started
    uint8_t         count;         // taps counted so far (0 = idle)
    uint16_t        single_action; // resolved tap action for the first tap count
    bool            pending_hold;  // true = waiting to see if final tap is held
    uint16_t        tap_action;    // resolved tap action for the current tap count
    uint8_t         tap_repeat_count;
    bool            has_more_taps;
    uint16_t        tap_hold_term;  // resolved tap-vs-hold threshold for this key
    uint16_t        multi_tap_term; // max gap between consecutive taps
    hold_behavior_t hold;           // hold tier for the current tap count
    hold_behavior_t long_hold;      // long-hold tier paired with hold
    uint8_t         saved_mods;     // real mods active when the tap sequence began resolving
    uint8_t         saved_weak_mods;
    uint8_t         saved_oneshot_mods;
    uint8_t         saved_oneshot_locked_mods;
} multi_tap_t;

void multi_tap_reset(multi_tap_t *mt);
bool multi_tap_active(const multi_tap_t *mt);
bool multi_tap_pending_hold(const multi_tap_t *mt);
bool multi_tap_expired(const multi_tap_t *mt);
bool multi_tap_hold_elapsed(const multi_tap_t *mt);
bool multi_tap_matches(const multi_tap_t *mt, uint16_t keycode, keypos_t key_pos);

void     multi_tap_begin(multi_tap_t *mt, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, uint8_t tap_repeat_count, uint16_t tap_hold_term, uint16_t multi_tap_term, bool has_more_taps);
void     multi_tap_flush(multi_tap_t *mt, void (*dispatch)(uint16_t, const multi_tap_t *));
uint16_t multi_tap_advance(multi_tap_t *mt, uint16_t tap_action, uint8_t tap_repeat_count, bool has_more_taps, bool tap_resolves_on_press, hold_behavior_t hold, hold_behavior_t long_hold);
uint16_t multi_tap_resolve_hold(multi_tap_t *mt, uint8_t *repeat_count);
