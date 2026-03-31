// ────────────────────────────────────────────────────────────────────────────
// Noah's Charybdis 4x6 — Keymap Configuration Overrides
// ────────────────────────────────────────────────────────────────────────────
//
// This file holds the keymap-facing behavior, RGB, timing, and pointer-layer
// policy settings. Split transport and low-level pointing-device tuning live
// in users/noah/config.h.
//
// The #undef-before-#define pattern is used throughout because QMK may
// have already defined these values in parent config files.  Without
// the #undef, redefining a macro would cause a compiler warning.
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

// VIA is a GUI tool for remapping keys without recompiling firmware.
// We need 5 dynamic layers to match our layer count (Base, Num, Sym, Nav, Pointer).
#ifdef VIA_ENABLE
#    undef DYNAMIC_KEYMAP_LAYER_COUNT
#    define DYNAMIC_KEYMAP_LAYER_COUNT 5
#endif

// Built-in QMK dual-role keys are still used in a few places, most notably
// MT(MOD_LSFT, KC_CAPS). Favor the hold path for that key as soon as another
// key is pressed so Shift chords beat the tap-side Caps Lock more reliably.
#define HOLD_ON_OTHER_KEY_PRESS_PER_KEY

// Used by the custom key behavior system in the userspace runtime.
// These are NOT QMK's built-in TAPPING_TERM — they're checked manually in
// process_record_user() and matrix_scan_user().
//
// Single key timeline:
//   < CUSTOM_TAP_HOLD_TERM                          = tap (send plain key)
//   CUSTOM_TAP_HOLD_TERM – CUSTOM_LONGER_HOLD_TERM  = hold (shifted variant / alt action)
//   > CUSTOM_LONGER_HOLD_TERM                        = longer hold (third-tier action)
//
// Multi-tap timing:
//   Each tap must arrive within CUSTOM_MULTI_TAP_TERM of the previous tap.
//   A double-tap takes up to 2x the term, triple up to 3x, etc.
//   Single taps on multi-tap keys are delayed by one window.
#define KEY_BEHAVIOR_MAX_TAP_COUNT 4 // max tap-count entries per key (single / double / triple / quadruple)
#define CUSTOM_TAP_HOLD_TERM 150     // tap vs hold boundary
#define CUSTOM_LONGER_HOLD_TERM 400  // hold vs longer-hold boundary
#define CUSTOM_MULTI_TAP_TERM 150    // max gap between consecutive taps
#define COMBO_TERM 50                // max ms between keys to register as a combo

// Auto-mouse: automatically activates LAYER_POINTER when the trackball
// moves, and deactivates it after the timeout expires.
#define POINTING_DEVICE_AUTO_MOUSE_ENABLE
#define AUTO_MOUSE_TIME 1200

// Default pointer DPI (base value before DPI_MOD/DPI_RMOD adjustments).
#define CHARYBDIS_MINIMUM_DEFAULT_DPI 800

// ─── RGB Matrix configuration ───────────────────────────────────────────────
//
// The Charybdis 4x6 has per-key RGB LEDs driven by QMK's RGB Matrix system.
// These settings control LED count, brightness, default effect, and timing.

#ifdef RGB_MATRIX_ENABLE

// Total LED count across both halves.
// The right (pointer) half has 2 dummy LED positions, but they still count.
#    ifdef RGB_MATRIX_LED_COUNT
#        undef RGB_MATRIX_LED_COUNT
#    endif
#    define RGB_MATRIX_LED_COUNT 58

// How the LEDs are split between halves: 29 left, 29 right.
#    ifdef RGB_MATRIX_SPLIT
#        undef RGB_MATRIX_SPLIT
#    endif
#    define RGB_MATRIX_SPLIT {29, 29}

// Cap brightness to protect LEDs from drawing too much current.
// 200/255 ≈ 78% brightness.
#    ifdef RGB_MATRIX_MAXIMUM_BRIGHTNESS
#        undef RGB_MATRIX_MAXIMUM_BRIGHTNESS
#    endif
#    define RGB_MATRIX_MAXIMUM_BRIGHTNESS 200

// Start with a solid color effect (no animation) — layer indicators
// override this when a non-base layer is active.
#    ifdef RGB_MATRIX_DEFAULT_MODE
#        undef RGB_MATRIX_DEFAULT_MODE
#    endif
#    define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_SOLID_COLOR

#    ifdef RGB_MATRIX_DEFAULT_HUE
#        undef RGB_MATRIX_DEFAULT_HUE
#    endif
#    define RGB_MATRIX_DEFAULT_HUE 0 // Red hue

#    ifdef RGB_MATRIX_DEFAULT_SAT
#        undef RGB_MATRIX_DEFAULT_SAT
#    endif
#    define RGB_MATRIX_DEFAULT_SAT 255 // Full saturation

#    ifdef RGB_MATRIX_DEFAULT_VAL
#        undef RGB_MATRIX_DEFAULT_VAL
#    endif
#    define RGB_MATRIX_DEFAULT_VAL RGB_MATRIX_MAXIMUM_BRIGHTNESS

// Minimum ms between RGB matrix updates. Higher = less CPU load but
// choppier animations.
#    ifdef RGB_MATRIX_LED_FLUSH_LIMIT
#        undef RGB_MATRIX_LED_FLUSH_LIMIT
#    endif
#    define RGB_MATRIX_LED_FLUSH_LIMIT 32

// Turn off LEDs after this many ms of inactivity.
// Requires SPLIT_ACTIVITY_ENABLE to keep both halves in sync.
#    ifdef RGB_MATRIX_TIMEOUT
#        undef RGB_MATRIX_TIMEOUT
#    endif
#    define RGB_MATRIX_TIMEOUT 900000

#endif // RGB_MATRIX_ENABLE
