// ────────────────────────────────────────────────────────────────────────────
// Noah's Charybdis 4x6 — Keymap Configuration Overrides
// ────────────────────────────────────────────────────────────────────────────
//
// This file holds the keymap-facing behavior, RGB behavior, timing, and
// pointer-layer policy settings. Split transport, board-level hardware
// assumptions, low-level pointing-device plumbing, and shared QMK behavior
// overrides live in users/noah/config.h.
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

// ─── Layers ────────────────────────────────────────────────────────────────
//
// If you add or remove a layer, update the enum below.
// LAYER_COUNT is derived automatically as the last sentinel value.
#ifndef __ASSEMBLER__
enum charybdis_keymap_layers {
    LAYER_BASE = 0, // Default QWERTY typing layer
    LAYER_NUM,      // Numpad on the right half
    LAYER_SYM,      // Symbols, brackets, and DPI controls
    LAYER_NAV,      // Navigation, media, macros, and mouse buttons (current sniping layer)
    LAYER_POINTER,  // Dedicated pointer layout; default auto-mouse target layer
    LAYER_COUNT,    // sentinel — must be last — used for VIA Dynamic layer count
};
#endif

// ─── Timing ────────────────────────────────────────────────────────────────

// QMK built-in dual-role keys (LT, MT) use TAPPING_TERM.
#define TAPPING_TERM 200

// Max gap between consecutive taps to register as a multi-tap combo.
#define COMBO_TERM 50

// Custom key behavior system (process_record_user / matrix_scan_user).
// These are distinct from QMK's TAPPING_TERM — checked manually at runtime.
//
// Single key timeline:
//   < CUSTOM_TAP_HOLD_TERM                         = tap (send plain key)
//   CUSTOM_TAP_HOLD_TERM – CUSTOM_LONGER_HOLD_TERM = hold (shifted / alt action)
//   > CUSTOM_LONGER_HOLD_TERM                      = longer hold (third-tier action)
//
// Multi-tap: each tap must arrive within CUSTOM_MULTI_TAP_TERM of the previous.
#define KEY_BEHAVIOR_MAX_TAP_COUNT 5 // max tap-count entries per key (single through quintuple)
#define CUSTOM_TAP_HOLD_TERM 150     // tap vs hold boundary (ms)
#define CUSTOM_LONGER_HOLD_TERM 400  // hold vs longer-hold boundary (ms)
#define CUSTOM_MULTI_TAP_TERM 150    // max gap between consecutive taps (ms)

// ─── Pointing device ────────────────────────────────────────────────────────

#ifdef POINTING_DEVICE_ENABLE

// Drag-scroll DPI — how fast dragging the ball scrolls.
// Tuning for the scroll hardware itself lives in users/noah/config.h.
#    define CHARYBDIS_DRAGSCROLL_DPI 100

// Per-mode pointer DPI. Each mode can optionally override the normal pointer
// DPI while it is active. Sniping always takes priority when active.
// Dragscroll and pinch are excluded — Charybdis manages their CPI internally.
// 0 means "keep normal pointer DPI" for that mode.
#    define PD_MODE_VOLUME_DPI 0
#    define PD_MODE_BRIGHTNESS_DPI 0
#    define PD_MODE_ZOOM_DPI 400
#    define PD_MODE_ARROW_DPI 400

// Default pointer DPI ladder: 800, 1000, 1200, …
#    define CHARYBDIS_MINIMUM_DEFAULT_DPI 800
#    define CHARYBDIS_DEFAULT_DPI_CONFIG_STEP 200

// Sniping DPI ladder: 200, 300, 400, 500.
#    define CHARYBDIS_MINIMUM_SNIPING_DPI 200
#    define CHARYBDIS_SNIPING_DPI_CONFIG_STEP 100

// Auto-sniping: engage sniping mode automatically while LAYER_NAV is active.
#    define CHARYBDIS_AUTO_SNIPING_ENABLE
#    define CHARYBDIS_AUTO_SNIPING_LAYER LAYER_NAV

// Auto-mouse: activates LAYER_POINTER on trackball movement; deactivates after timeout.
#    define POINTING_DEVICE_AUTO_MOUSE_ENABLE
#    define AUTO_MOUSE_DEFAULT_LAYER LAYER_POINTER
#    define AUTO_MOUSE_TIME 1200

#endif // POINTING_DEVICE_ENABLE

// ─── RGB Matrix ─────────────────────────────────────────────────────────────

#ifdef RGB_MATRIX_ENABLE

// Base display settings.
// Solid red by default; layer indicators override this when a non-base layer is active.
#    define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_SOLID_COLOR
#    define RGB_MATRIX_DEFAULT_HUE 0
#    define RGB_MATRIX_DEFAULT_SAT 255
// 200/255 ≈ 78% — caps current draw to safe levels.
#    define RGB_MATRIX_MAXIMUM_BRIGHTNESS 200
// RGB_MATRIX_DEFAULT_VAL is defined upstream in QMK core, so undef first.
#    undef RGB_MATRIX_DEFAULT_VAL
#    define RGB_MATRIX_DEFAULT_VAL RGB_MATRIX_MAXIMUM_BRIGHTNESS
// Minimum ms between LED updates — higher reduces CPU load, lowers animation smoothness.
#    define RGB_MATRIX_LED_FLUSH_LIMIT 32
// Turn off LEDs after this many ms of inactivity (requires SPLIT_ACTIVITY_ENABLE).
#    define RGB_MATRIX_TIMEOUT 900000

// Key-behavior feedback overlay.
// Shows multi-tap progress and hold-threshold colors on individual keys.
// Comment out RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE to disable entirely.
#    define RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
// Flash half-period for keys held with PRESS_AND_HOLD_UNTIL_RELEASE or repeat actions.
#    define RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS 200

// Auto-mouse timeout gradient overlay.
// Fades from white to red as the auto-mouse layer times out.
// Comment out RGB_AUTOMOUSE_GRADIENT_ENABLE to disable.
#    define RGB_AUTOMOUSE_GRADIENT_ENABLE
// Dead time before the gradient starts animating (first third of the timeout window).
#    define AUTOMOUSE_RGB_DEAD_TIME (AUTO_MOUSE_TIME / 3)

#endif // RGB_MATRIX_ENABLE
