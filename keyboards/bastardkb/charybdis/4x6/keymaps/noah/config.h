#pragma once

/* ────────────────────────────────
 * Core / feature toggles
 * ──────────────────────────────── */
#ifdef VIA_ENABLE
#    undef DYNAMIC_KEYMAP_LAYER_COUNT
#    define DYNAMIC_KEYMAP_LAYER_COUNT 4
#endif

#ifndef __arm__
/* Save space on non-ARM builds. */
#    define NO_ACTION_ONESHOT
#endif

/* ────────────────────────────────
 * Sync-specific features
 * ──────────────────────────────── */

/* Keep layer state in sync across halves */
#ifdef SPLIT_LAYER_STATE_ENABLE
#    undef SPLIT_LAYER_STATE_ENABLE
#endif
#define SPLIT_LAYER_STATE_ENABLE

/* Keep activity state in sync for rgb matrix timeout */
#ifdef SPLIT_ACTIVITY_ENABLE
#    undef SPLIT_ACTIVITY_ENABLE
#endif
#define SPLIT_ACTIVITY_ENABLE

/* ────────────────────────────────
 * RGB Matrix configuration
 * ──────────────────────────────── */
#ifdef RGB_MATRIX_LED_COUNT
#    undef RGB_MATRIX_LED_COUNT
#endif
#define RGB_MATRIX_LED_COUNT 58 // Total LEDs (2 dummy LEDs on pointer side)

#ifdef RGB_MATRIX_SPLIT
#    undef RGB_MATRIX_SPLIT
#endif
#define RGB_MATRIX_SPLIT {29, 29} // Left, Right

/* Brightness Cap to protect the LEDS from drawing too much power. */
#ifdef RGB_MATRIX_MAXIMUM_BRIGHTNESS
#    undef RGB_MATRIX_MAXIMUM_BRIGHTNESS
#endif
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 150

/* Default solid color mode. */
#ifdef RGB_MATRIX_DEFAULT_MODE
#    undef RGB_MATRIX_DEFAULT_MODE
#endif
#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_SOLID_COLOR

// H
#ifdef RGB_MATRIX_DEFAULT_HUE
#    undef RGB_MATRIX_DEFAULT_HUE
#endif
#define RGB_MATRIX_DEFAULT_HUE 0 // Red

// S
#ifdef RGB_MATRIX_DEFAULT_SAT
#    undef RGB_MATRIX_DEFAULT_SAT
#endif
#define RGB_MATRIX_DEFAULT_SAT 255 // Full saturation

// V
#ifdef RGB_MATRIX_DEFAULT_VAL
#    undef RGB_MATRIX_DEFAULT_VAL
#endif
#define RGB_MATRIX_DEFAULT_VAL RGB_MATRIX_MAXIMUM_BRIGHTNESS

/* RGB Matrix timeout/sleep */
#ifdef RGB_MATRIX_TIMEOUT
#    undef RGB_MATRIX_TIMEOUT
#endif
#define RGB_MATRIX_TIMEOUT 900000 // ms before auto-off, requires SPLIT_ACTIVITY_ENABLE to stay in sync across halves

/* ────────────────────────────────
 * Pointing device / auto-mouse
 * ──────────────────────────────── */
#ifdef POINTING_DEVICE_ENABLE
/* Auto pointer layer on movement + 16-bit motion reports. */
#    define POINTING_DEVICE_AUTO_MOUSE_ENABLE
#    define MOUSE_EXTENDED_REPORT
#    define SPLIT_TRANSACTION_IDS_USER
#endif

#ifdef AUTO_MOUSE_TIME
#    undef AUTO_MOUSE_TIME
#endif
#define AUTO_MOUSE_TIME 1200 // ms to switch back after movement or mouse key activity

/* ────────────────────────────────
 * Scroll configuration
 * ──────────────────────────────── */
#define CHARYBDIS_DRAGSCROLL_REVERSE_Y
#define POINTING_DEVICE_HIRES_SCROLL_ENABLE
#define POINTING_DEVICE_HIRES_SCROLL_MULTIPLIER 120
// #define POINTING_DEVICE_HIRES_SCROLL_EXPONENT 1
#define WHEEL_EXTENDED_REPORT true

#define CHARYBDIS_DRAGSCROLL_DPI 100
#define CHARYBDIS_DRAGSCROLL_BUFFER_SIZE 0
#define CHARYBDIS_SCROLL_RATE_LIMIT_MS 8 // 125 Hz-ish
#define CHARYBDIS_SCROLL_SNAP_RATIO 3
#define CHARYBDIS_SCROLL_STEP_DIVISOR 10
#define CHARYBDIS_SCROLL_MAX_STEP 1

/* ────────────────────────────────
 * Tap/Hold timing
 * ──────────────────────────────── */
#define CUSTOM_TAP_HOLD_TERM 180
