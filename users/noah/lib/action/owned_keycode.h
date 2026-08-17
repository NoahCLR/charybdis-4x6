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

// Head start given to a freshly registered modifier before a mouse button it
// qualifies goes down. Spans several 1ms USB frames so the two reports cannot
// share one. See owned_keycode_acquire_components().
#ifndef OWNED_KEYCODE_MOD_TO_MOUSE_SETTLE_MS
#    define OWNED_KEYCODE_MOD_TO_MOUSE_SETTLE_MS 8u
#endif

typedef struct {
    bool    active;
    bool    has_basic;
    uint8_t basic;
    uint8_t mods;
} owned_keycode_lease_t;

typedef struct {
    uint8_t  physical_count;
    uint8_t  managed_count;
    uint16_t saturation_count;
    uint16_t underflow_count;
    uint16_t unsupported_count;
    uint16_t idempotent_release_count;
} owned_keycode_debug_snapshot_t;

bool owned_keycode_acquire(uint16_t keycode, owned_keycode_lease_t *lease);
bool owned_keycode_release(owned_keycode_lease_t *lease);
bool owned_keycode_is_supported(uint16_t keycode);
bool owned_keycode_register(uint16_t keycode);
bool owned_keycode_unregister(uint16_t keycode);
bool owned_keycode_tap(uint16_t keycode);
void owned_keycode_track_physical_event(uint16_t keycode, keyrecord_t *record);
bool owned_keycode_should_suppress_default(uint16_t keycode, keyrecord_t *record);
void owned_keycode_debug_snapshot(uint8_t keycode, owned_keycode_debug_snapshot_t *out);
void owned_keycode_reset_for_test(void);
