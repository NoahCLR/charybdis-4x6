// ────────────────────────────────────────────────────────────────────────────
// Noah Userspace — Hardware / Transport Tuning
// ────────────────────────────────────────────────────────────────────────────
//
// QMK automatically includes users/noah/config.h before the keyboard keymap's
// own config.h. This file now carries split transport settings, hardware-level
// board assumptions, low-level pointing-device plumbing, and shared QMK
// behavior overrides for the current Charybdis setup, while the keymap config
// holds typing, RGB behavior, and pointer policy.
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

// ─── Split keyboard sync ───────────────────────────────────────────────────
//
// The Charybdis 4x6 is a split keyboard — each half has its own MCU.
// By default, only keystrokes are sent between halves. These options
// enable syncing additional state so both halves stay consistent.
//
#ifdef SPLIT_KEYBOARD

// The default split serial timeout is far too long for a keyboard with a
// trackball — a single failed transaction blocks the main loop for the
// full timeout, causing the sensor to accumulate motion into a cursor jump.
#    undef SERIAL_USART_TIMEOUT
#    define SERIAL_USART_TIMEOUT 5

// Sync the active layer set to the slave half so it can show the correct
// RGB layer indicator colors.
#    ifdef SPLIT_LAYER_STATE_ENABLE
#        undef SPLIT_LAYER_STATE_ENABLE
#    endif
#    define SPLIT_LAYER_STATE_ENABLE

// Sync user activity timestamps so the RGB timeout (sleep) triggers at
// the same time on both halves.
#    ifdef SPLIT_ACTIVITY_ENABLE
#        undef SPLIT_ACTIVITY_ENABLE
#    endif
#    define SPLIT_ACTIVITY_ENABLE

// Register the custom split runtime sync RPC for pd-mode flags, auto-mouse
// RGB progress, and key-feedback flags.
#    define SPLIT_TRANSACTION_IDS_USER PUT_SPLIT_RUNTIME_SYNC

#endif // SPLIT_KEYBOARD

// ─── RGB hardware geometry ──────────────────────────────────────────────────

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

#endif // RGB_MATRIX_ENABLE

// ─── Pointing device (trackball) ────────────────────────────────────────────

#ifdef POINTING_DEVICE_ENABLE

// Keep split-pointing polling at the 1 ms cadence QMK already defaults to.
// Higher values ease transport/main-loop pressure, but trade away responsiveness.
#    undef POINTING_DEVICE_TASK_THROTTLE_MS
#    define POINTING_DEVICE_TASK_THROTTLE_MS 1

// Liftoff distance — how high the ball can be lifted before tracking stops.
// Default is 0x02; higher = tracks further from the surface.
#    define PMW33XX_LIFTOFF_DISTANCE 0x03

// Enable 16-bit motion reports for higher precision at high DPI.
#    define MOUSE_EXTENDED_REPORT
#    define WHEEL_EXTENDED_REPORT

// Hi-res scroll: each scroll unit = 1/120th of a notch.
#    define POINTING_DEVICE_HIRES_SCROLL_ENABLE
#    define POINTING_DEVICE_HIRES_SCROLL_MULTIPLIER 120

// Shared idle-noise filter for tiny trackball motion seen while the board is
// otherwise untouched. Comment out the enable define to compile this path out
// entirely; when enabled, matching reports are zeroed before they can refresh
// RGB activity or drift into auto-mouse activation.
#    define NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ENABLE
#    define NOAH_POINTING_IDLE_NOISE_SUPPRESSION_IDLE_MS 1000
#    define NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ARM_IDLE_MS 300000
#    define NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ABS_MAX 2

// Local drag-scroll tuning (DPI for the scroll speed lives in the keymap config).
// The repo-owned NOAH_DRAGSCROLL_* surface controls gesture feel; the older
// CHARYBDIS_* scroll defines remain as compatibility fallbacks only.
//   Thresholds     = start moving one axis intentionally before it scrolls
//   Divisors       = counts per emitted scroll unit
//   Start ratio    = dominance needed to begin a one-axis gesture
//   Sustain ratio  = looser dominance needed to keep the gesture on one axis
//   Lock timeout   = pause window that ends the current gesture
#    define CHARYBDIS_DRAGSCROLL_REVERSE_Y
#    define CHARYBDIS_DRAGSCROLL_BUFFER_SIZE 0
#    define CHARYBDIS_SCROLL_STEP_DIVISOR 8
#    define CHARYBDIS_SCROLL_RATE_LIMIT_MS 8
#    define CHARYBDIS_SCROLL_SNAP_RATIO 3
#    define CHARYBDIS_SCROLL_BUFFER_EXPIRE_MS 80
#    define NOAH_DRAGSCROLL_THRESHOLD_H 2
#    define NOAH_DRAGSCROLL_THRESHOLD_V 3
#    define NOAH_DRAGSCROLL_DIVISOR_H 6
#    define NOAH_DRAGSCROLL_DIVISOR_V 8
#    define NOAH_DRAGSCROLL_LOCK_START_RATIO_NUM 7
#    define NOAH_DRAGSCROLL_LOCK_START_RATIO_DEN 4
#    define NOAH_DRAGSCROLL_LOCK_SUSTAIN_RATIO_NUM 5
#    define NOAH_DRAGSCROLL_LOCK_SUSTAIN_RATIO_DEN 4
#    define NOAH_DRAGSCROLL_LOCK_TIMEOUT_MS 55
#    define NOAH_DRAGSCROLL_CROSS_AXIS_DECAY_DIVISOR 4

#endif // POINTING_DEVICE_ENABLE

// ─── VIA ────────────────────────────────────────────────────────────────────

#ifdef VIA_ENABLE
#    define DYNAMIC_KEYMAP_LAYER_COUNT LAYER_COUNT
#endif
