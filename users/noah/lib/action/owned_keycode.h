// ────────────────────────────────────────────────────────────────────────────
// Owned Keycode Dispatch
// ────────────────────────────────────────────────────────────────────────────
//
// Shared literal-keycode helpers for userspace-owned synthetic dispatch.
// This surface intentionally covers only:
//   - plain 8-bit keycodes
//   - plain modifier keycodes
//   - QK_MODS keycodes such as S(KC_1) or G(KC_C)
//
// Higher QMK behavior keycodes stay with their existing dedicated handlers.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "custom_key_shortcut.h"

// Head start given to a freshly registered modifier before a mouse button it
// qualifies goes down. Spans several 1ms USB frames so the two reports cannot
// share one. See owned_keycode_acquire_usages().
#ifndef OWNED_KEYCODE_MOD_TO_MOUSE_SETTLE_MS
#    define OWNED_KEYCODE_MOD_TO_MOUSE_SETTLE_MS 8u
#endif

typedef struct {
    bool    active;
    bool    has_basic;
    uint8_t basic;
    uint8_t mods;
} owned_keycode_lease_t;

// The most keys one chord holds: as many as a custom key's shortcut holds
// (D-F26), the chord lease's one user, whose header owns the number. A chord's
// keys take the form of a shortcut's key list: in press order, KC_NO after the
// last (noah_custom_key_shortcut_key_count()).
#define OWNED_KEYCODE_CHORD_KEYS NOAH_CUSTOM_KEY_SHORTCUT_KEY_COUNT

typedef struct {
    bool    active;
    uint8_t mods;
    uint8_t keys[OWNED_KEYCODE_CHORD_KEYS]; // in press order, KC_NO after the last
} owned_keycode_chord_lease_t;

typedef struct {
    uint8_t  physical_count;
    uint8_t  managed_count;
    uint16_t saturation_count;
    uint16_t underflow_count;
    uint16_t unsupported_count;
    uint16_t idempotent_release_count;
} owned_keycode_debug_snapshot_t;

bool     owned_keycode_acquire(uint16_t keycode, owned_keycode_lease_t *lease);
bool     owned_keycode_release(owned_keycode_lease_t *lease);
// One lease for a chord no single keycode can name: `mods` is an 8-bit report
// modifier mask, left and right mixable, and `keys` up to
// OWNED_KEYCODE_CHORD_KEYS report usages in press order, KC_NO after the last.
// The modifiers go down first and, ahead of the first mouse button, get its
// head start; then each key in order, all held together.
// owned_keycode_release_chord() lets go of the keys in reverse, then the
// modifiers. False, with nothing held and no report sent, when the ledger
// cannot take all of it, the chord is empty or a key is a modifier or no
// report usage.
bool     owned_keycode_acquire_chord(uint8_t mods, const uint8_t keys[OWNED_KEYCODE_CHORD_KEYS], owned_keycode_chord_lease_t *lease);
bool     owned_keycode_tap_chord(uint8_t mods, const uint8_t keys[OWNED_KEYCODE_CHORD_KEYS]);
bool     owned_keycode_release_chord(owned_keycode_chord_lease_t *lease);
bool     owned_keycode_is_supported(uint16_t keycode);
bool     owned_keycode_register(uint16_t keycode);
bool     owned_keycode_unregister(uint16_t keycode);
bool     owned_keycode_tap(uint16_t keycode);
// Every literal tap goes through here, so it can never release a usage another
// owner (a physical key, a behavior hold, a macro) still holds: QMK's own
// tap_code16() ends with an unregister that clears the usage whoever holds it.
// A usage already held stays held and gets no new edge; its tap changes nothing
// in the report, as a second physical key on the same usage would not either.
// A keycode the ledger cannot represent keeps QMK's own tap.
void     owned_keycode_tap_literal(uint16_t keycode);
void     owned_keycode_track_physical_event(uint16_t keycode, keyrecord_t *record);
// Decide before tracking this event: a press is not yet counted, while a
// release still includes its own physical owner. Suppression removes only a
// duplicate native edge; the native event must still settle its ownership.
bool     owned_keycode_should_suppress_default(uint16_t keycode, keyrecord_t *record);
uint16_t owned_keycode_managed_usage_count(void);
void     owned_keycode_debug_snapshot(uint8_t keycode, owned_keycode_debug_snapshot_t *out);
void     owned_keycode_reset_for_test(void);
