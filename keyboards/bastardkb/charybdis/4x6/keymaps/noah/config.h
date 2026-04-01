// ────────────────────────────────────────────────────────────────────────────
// Noah's Charybdis 4x6 — Keymap Configuration Overrides
// ────────────────────────────────────────────────────────────────────────────
//
// This file holds the keymap-facing behavior, RGB behavior, timing, and
// pointer-layer policy settings. Split transport, board-level hardware
// assumptions, low-level pointing-device plumbing, and shared QMK behavior
// overrides live in users/noah/config.h.
//
// This file uses the same #undef-before-#define pattern for every config macro
// it sets. That keeps local overrides explicit and avoids redefinition
// warnings when parent configs already provide a value.
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

// ─── VIA configuration ──────────────────────────────────────────────────────

// VIA is a GUI tool for remapping keys without recompiling firmware.
// We need 5 dynamic layers to match our layer count (Base, Num, Sym, Nav, Pointer).
#ifdef VIA_ENABLE
#    undef DYNAMIC_KEYMAP_LAYER_COUNT

#    define DYNAMIC_KEYMAP_LAYER_COUNT 5
#endif

// ─── QMK dual-role timing ───────────────────────────────────────────────────

// Built-in QMK dual-role keys still use TAPPING_TERM.
// Keep the LT()/MT() keys on the current stock QMK default explicitly.
#ifdef TAPPING_TERM
#    undef TAPPING_TERM
#endif

#define TAPPING_TERM 200

// ─── Custom key behavior timing ─────────────────────────────────────────────

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
#ifdef KEY_BEHAVIOR_MAX_TAP_COUNT
#    undef KEY_BEHAVIOR_MAX_TAP_COUNT
#endif
#ifdef CUSTOM_TAP_HOLD_TERM
#    undef CUSTOM_TAP_HOLD_TERM
#endif
#ifdef CUSTOM_LONGER_HOLD_TERM
#    undef CUSTOM_LONGER_HOLD_TERM
#endif
#ifdef CUSTOM_MULTI_TAP_TERM
#    undef CUSTOM_MULTI_TAP_TERM
#endif

#define KEY_BEHAVIOR_MAX_TAP_COUNT 5 // max tap-count entries per key (single through quintuple)
#define CUSTOM_TAP_HOLD_TERM 150     // tap vs hold boundary
#define CUSTOM_LONGER_HOLD_TERM 400  // hold vs longer-hold boundary
#define CUSTOM_MULTI_TAP_TERM 150    // max gap between consecutive taps

// ─── Combos ─────────────────────────────────────────────────────────────────

#ifdef COMBO_ENABLE
#    ifdef COMBO_TERM
#        undef COMBO_TERM
#    endif

#    define COMBO_TERM 50 // max ms between keys to register as a combo
#endif

// ─── Pointing-device behavior ───────────────────────────────────────────────

#ifdef POINTING_DEVICE_ENABLE
// Auto-mouse: automatically activates the configured target layer when the
// trackball moves, and deactivates it after the timeout expires.
#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
#        undef POINTING_DEVICE_AUTO_MOUSE_ENABLE
#    endif
#    ifdef AUTO_MOUSE_DEFAULT_LAYER
#        undef AUTO_MOUSE_DEFAULT_LAYER
#    endif
#    ifdef AUTO_MOUSE_TIME
#        undef AUTO_MOUSE_TIME
#    endif
#    define POINTING_DEVICE_AUTO_MOUSE_ENABLE
#    define AUTO_MOUSE_DEFAULT_LAYER 4 // QMK expects a numeric layer index here; 4 = LAYER_POINTER in noah_keymap.h.
#    define AUTO_MOUSE_TIME 1200

// Auto-sniping: enable Charybdis sniping automatically while a layer is active.
// The current keymap uses NAV for precise cursor work and pointer-adjacent
// shortcuts, so sniping comes up there by default.
#    ifdef CHARYBDIS_AUTO_SNIPING_ENABLE
#        undef CHARYBDIS_AUTO_SNIPING_ENABLE
#    endif
#    ifdef CHARYBDIS_AUTO_SNIPING_LAYER
#        undef CHARYBDIS_AUTO_SNIPING_LAYER
#    endif
#    define CHARYBDIS_AUTO_SNIPING_ENABLE
#    define CHARYBDIS_AUTO_SNIPING_LAYER LAYER_NAV

// Sniping-mode DPI tuning.
// Sniping uses a separate DPI ladder from the normal pointer DPI.
// With the defaults below, the sniping steps are 200, 300, 400, and 500.
#    ifdef CHARYBDIS_MINIMUM_SNIPING_DPI
#        undef CHARYBDIS_MINIMUM_SNIPING_DPI
#    endif
#    ifdef CHARYBDIS_SNIPING_DPI_CONFIG_STEP
#        undef CHARYBDIS_SNIPING_DPI_CONFIG_STEP
#    endif
#    define CHARYBDIS_MINIMUM_SNIPING_DPI 200
#    define CHARYBDIS_SNIPING_DPI_CONFIG_STEP 100

// Default pointer DPI tuning.
// With the defaults below, the normal pointer steps are 800, 1000, 1200, etc.
#    ifdef CHARYBDIS_MINIMUM_DEFAULT_DPI
#        undef CHARYBDIS_MINIMUM_DEFAULT_DPI
#    endif
#    ifdef CHARYBDIS_DEFAULT_DPI_CONFIG_STEP
#        undef CHARYBDIS_DEFAULT_DPI_CONFIG_STEP
#    endif
#    define CHARYBDIS_MINIMUM_DEFAULT_DPI 800
#    define CHARYBDIS_DEFAULT_DPI_CONFIG_STEP 200

// Drag-scroll and hi-res scroll tuning.
// With hi-res scrolling, each scroll unit = 1/120th of a notch.
//
//   Speed      = DPI / STEP_DIVISOR   (lower = slower, more precise)
//   Smoothness = RATE_LIMIT_MS        (lower = more frequent updates)
//   Snap feel  = SNAP_RATIO           (higher = stricter axis lock)
#    ifdef POINTING_DEVICE_HIRES_SCROLL_ENABLE
#        undef POINTING_DEVICE_HIRES_SCROLL_ENABLE
#    endif
#    ifdef POINTING_DEVICE_HIRES_SCROLL_MULTIPLIER
#        undef POINTING_DEVICE_HIRES_SCROLL_MULTIPLIER
#    endif
#    ifdef CHARYBDIS_DRAGSCROLL_REVERSE_Y
#        undef CHARYBDIS_DRAGSCROLL_REVERSE_Y
#    endif
#    ifdef CHARYBDIS_DRAGSCROLL_DPI
#        undef CHARYBDIS_DRAGSCROLL_DPI
#    endif
#    ifdef CHARYBDIS_DRAGSCROLL_BUFFER_SIZE
#        undef CHARYBDIS_DRAGSCROLL_BUFFER_SIZE
#    endif
#    ifdef CHARYBDIS_SCROLL_STEP_DIVISOR
#        undef CHARYBDIS_SCROLL_STEP_DIVISOR
#    endif
#    ifdef CHARYBDIS_SCROLL_RATE_LIMIT_MS
#        undef CHARYBDIS_SCROLL_RATE_LIMIT_MS
#    endif
#    ifdef CHARYBDIS_SCROLL_SNAP_RATIO
#        undef CHARYBDIS_SCROLL_SNAP_RATIO
#    endif
#    ifdef CHARYBDIS_SCROLL_BUFFER_EXPIRE_MS
#        undef CHARYBDIS_SCROLL_BUFFER_EXPIRE_MS
#    endif
#    define POINTING_DEVICE_HIRES_SCROLL_ENABLE
#    define POINTING_DEVICE_HIRES_SCROLL_MULTIPLIER 120
#    define CHARYBDIS_DRAGSCROLL_REVERSE_Y
#    define CHARYBDIS_DRAGSCROLL_DPI 100
#    define CHARYBDIS_DRAGSCROLL_BUFFER_SIZE 0
#    define CHARYBDIS_SCROLL_STEP_DIVISOR 8
#    define CHARYBDIS_SCROLL_RATE_LIMIT_MS 8
#    define CHARYBDIS_SCROLL_SNAP_RATIO 3
#    define CHARYBDIS_SCROLL_BUFFER_EXPIRE_MS 80
#endif // POINTING_DEVICE_ENABLE

// ─── RGB Matrix configuration ───────────────────────────────────────────────
//
// The Charybdis 4x6 has per-key RGB LEDs driven by QMK's RGB Matrix system.
// These settings control LED count, brightness, default effect, and timing.

#ifdef RGB_MATRIX_ENABLE
#    ifdef RGB_MATRIX_MAXIMUM_BRIGHTNESS
#        undef RGB_MATRIX_MAXIMUM_BRIGHTNESS
#    endif
#    ifdef RGB_MATRIX_DEFAULT_MODE
#        undef RGB_MATRIX_DEFAULT_MODE
#    endif
#    ifdef RGB_MATRIX_DEFAULT_HUE
#        undef RGB_MATRIX_DEFAULT_HUE
#    endif
#    ifdef RGB_MATRIX_DEFAULT_SAT
#        undef RGB_MATRIX_DEFAULT_SAT
#    endif
#    ifdef RGB_MATRIX_DEFAULT_VAL
#        undef RGB_MATRIX_DEFAULT_VAL
#    endif
#    ifdef RGB_MATRIX_LED_FLUSH_LIMIT
#        undef RGB_MATRIX_LED_FLUSH_LIMIT
#    endif
#    ifdef RGB_MATRIX_TIMEOUT
#        undef RGB_MATRIX_TIMEOUT
#    endif

// Cap brightness to protect LEDs from drawing too much current.
// 200/255 ≈ 78% brightness.
#    define RGB_MATRIX_MAXIMUM_BRIGHTNESS 200

// Start with a solid color effect (no animation) — layer indicators
// override this when a non-base layer is active.
#    define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_SOLID_COLOR
#    define RGB_MATRIX_DEFAULT_HUE 0 // Red hue
#    define RGB_MATRIX_DEFAULT_SAT 255 // Full saturation
#    define RGB_MATRIX_DEFAULT_VAL RGB_MATRIX_MAXIMUM_BRIGHTNESS

// Minimum ms between RGB matrix updates. Higher = less CPU load but
// choppier animations.
#    define RGB_MATRIX_LED_FLUSH_LIMIT 32

// Turn off LEDs after this many ms of inactivity.
// Requires SPLIT_ACTIVITY_ENABLE to keep both halves in sync.
#    define RGB_MATRIX_TIMEOUT 900000

#endif // RGB_MATRIX_ENABLE
