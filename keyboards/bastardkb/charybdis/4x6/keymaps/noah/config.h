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
// Every image reserves sixteen logical layers (D-F14). Layers 8..15 start
// transparent, unnamed and inactive.
#define NOAH_LAYER_BANK_COUNT 16
#ifndef __ASSEMBLER__
enum charybdis_keymap_layers {
    LAYER_BASE = 0, // Default QWERTY typing layer
    LAYER_NUM,      // Numpad on the right half
    LAYER_SYM,      // Symbols, brackets, and DPI controls
    LAYER_NAV,      // Navigation, media, macros, and mouse buttons (current sniping layer)
    LAYER_POINTER,  // Dedicated pointer layout; default auto-mouse target layer
    LAYER_EXTRA_1,
    LAYER_EXTRA_2,
    LAYER_EXTRA_3,
    LAYER_EXTRA_4,
    LAYER_EXTRA_5,
    LAYER_EXTRA_6,
    LAYER_EXTRA_7,
    LAYER_EXTRA_8,
    LAYER_EXTRA_9,
    LAYER_EXTRA_10,
    LAYER_EXTRA_11,
    // ─── Add new layers above this line. ───────────────────────────────────
    LAYER_COUNT = NOAH_LAYER_BANK_COUNT, // sentinel — the size of the layer bank
};
#endif

// ─── Timing ────────────────────────────────────────────────────────────────

// QMK's dual-role keys (LT, MT, TT, OSL, OSM) hold at TAPPING_TERM, with or
// without a behaviour row; the key runtime decides all of them (D-F27). It is
// the default for the live profile's dual-role setting, which replaces it once
// a profile's settings are live.
#define TAPPING_TERM 200

// How close together a combo's keys must be pressed. The compiled combos use
// it, and it is the default window of a live profile until one is stored.
#define COMBO_TERM 50

// Custom key behavior system (process_record_user / matrix_scan_user).
// Every behaviour row except a dual-role key's uses these.
//
// Single key timeline:
//   < CUSTOM_TAP_HOLD_TERM                         = tap (send plain key)
//   CUSTOM_TAP_HOLD_TERM – CUSTOM_LONGER_HOLD_TERM = hold (shifted / alt action)
//   > CUSTOM_LONGER_HOLD_TERM                      = long hold (third-tier action)
//
// Multi-tap: each tap must arrive within CUSTOM_MULTI_TAP_TERM of the previous.
#define KEY_BEHAVIOR_MAX_TAP_COUNT 5 // max tap-count entries per key (single through quintuple)
#define CUSTOM_TAP_HOLD_TERM 150     // tap vs hold boundary (ms)
#define CUSTOM_LONGER_HOLD_TERM 400  // hold vs long-hold boundary (ms)
#define CUSTOM_MULTI_TAP_TERM 150    // max gap between consecutive taps (ms)

// ─── Pointing device ────────────────────────────────────────────────────────

#ifdef POINTING_DEVICE_ENABLE

// Drag-scroll DPI — how fast dragging the ball scrolls.
// Tuning for the scroll hardware itself lives in users/noah/config.h.
#    define CHARYBDIS_DRAGSCROLL_DPI 100

// Per-mode pointer DPI. Each mode can optionally override the normal pointer
// DPI while it is active. Sniping always takes priority when active.
// Dragscroll and pinch are excluded — the shared local dragscroll handler
// owns their CPI through the pd-mode lifecycle.
// 0 means "keep normal pointer DPI" for that mode.
#    define PD_MODE_VOLUME_DPI 400
#    define PD_MODE_BRIGHTNESS_DPI 400
#    define PD_MODE_ZOOM_DPI 400
#    define PD_MODE_ARROW_DPI 400

// Default pointer DPI ladder: 800, 1000, 1200, …
#    define CHARYBDIS_MINIMUM_DEFAULT_DPI 400
#    define CHARYBDIS_DEFAULT_DPI_CONFIG_STEP 200

// Sniping DPI ladder: 200, 300, 400, 500.
#    define CHARYBDIS_MINIMUM_SNIPING_DPI 100
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
// Effect speed, 0–255: how fast an animated base effect runs.
#    undef RGB_MATRIX_DEFAULT_SPD
#    define RGB_MATRIX_DEFAULT_SPD 32
// Turn off LEDs after this many ms of inactivity (requires SPLIT_ACTIVITY_ENABLE);
// zero keeps them on. The live profile stores its own once one is saved.
#    define NOAH_RGB_IDLE_TIMEOUT_MS 600000
#    define RGB_MATRIX_TIMEOUT NOAH_RGB_IDLE_TIMEOUT_MS

// PD-mode RGB feedback overlay.
// Comment out RGB_PD_MODE_FEEDBACK_ENABLE to disable the stage entirely.
#    ifdef POINTING_DEVICE_ENABLE
#        define RGB_PD_MODE_FEEDBACK_ENABLE
#    endif

// Combo feedback overlay.
// Comment out RGB_COMBO_FEEDBACK_ENABLE to disable the stage entirely.
#    ifdef COMBO_ENABLE
#        define RGB_COMBO_FEEDBACK_ENABLE
#    endif

// Key-behavior feedback overlay.
// Shows multi-tap progress and hold-threshold colors on individual keys.
// Comment out RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE to disable entirely.
#    define RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
// Flash half-period for keys held with PRESS_AND_HOLD_UNTIL_RELEASE or repeat actions.
#    define RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS 200

// Auto-mouse timeout fade overlay.
// Fades from the authored pointer layer rendering toward its destination
// state as auto-mouse times out.
// Comment out RGB_AUTOMOUSE_GRADIENT_ENABLE to disable.
#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
#        define RGB_AUTOMOUSE_GRADIENT_ENABLE
// Dead time before the timeout fade starts animating (first third of the timeout window).
#        define AUTOMOUSE_RGB_DEAD_TIME (AUTO_MOUSE_TIME / 3)
#    endif

#endif // RGB_MATRIX_ENABLE

// ─── Host and name ──────────────────────────────────────────────────────────

// The computer macros type into: 0 Auto, 1 macOS, 2 Windows, 3 Linux.
#define NOAH_DEFAULT_HOST_OS 1
// The host layout macro text is typed through (an ID from
// docs/spec/host-layouts-v1.md; 0 US, 2 macOS Dutch).
#define NOAH_DEFAULT_HOST_LAYOUT 2
// 1 types characters the layout lacks through the host's Unicode entry.
#define NOAH_DEFAULT_UNICODE_PLAYBACK 0
// 1 when macOS classified the keyboard as ISO.
#define NOAH_DEFAULT_MACOS_ISO 0

// The keyboard's own name, the USB product name computers and Charybdis Ark
// list it under: at most 32 bytes of UTF-8. Empty keeps QMK's built-in name.
#define NOAH_KEYBOARD_NAME "Charybdis - Home"

#if defined(NOAH_PORTABLE_PROFILE_ENABLE) && !defined(__ASSEMBLER__)
#    include <stdint.h>
uint32_t noah_setting(uint8_t id, uint32_t fallback);
#    define AUTO_MOUSE_DELAY noah_setting(25, 200)
#    define AUTO_MOUSE_THRESHOLD noah_setting(26, 10)
#    undef RGB_MATRIX_TIMEOUT
#    define RGB_MATRIX_TIMEOUT 0
#endif
