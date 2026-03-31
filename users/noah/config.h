// ────────────────────────────────────────────────────────────────────────────
// Noah Userspace — Hardware / Transport Tuning
// ────────────────────────────────────────────────────────────────────────────
//
// QMK automatically includes users/noah/config.h before the keyboard keymap's
// own config.h. This file now carries the split transport, low-level
// pointing-device tuning, and shared QMK behavior overrides for the current
// Charybdis setup, while the keymap config holds typing, RGB, and
// pointer-layer behavior.
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

// Built-in QMK dual-role keys are still used in a few places, most notably
// MT(MOD_LSFT, KC_CAPS). Favor the hold path for that key as soon as another
// key is pressed so Shift chords beat the tap-side Caps Lock more reliably.
#define HOLD_ON_OTHER_KEY_PRESS_PER_KEY

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

#endif // SPLIT_KEYBOARD

// ─── Pointing device (trackball) ────────────────────────────────────────────

#ifdef POINTING_DEVICE_ENABLE

// Register a custom split RPC transaction for syncing pointing device
// state (auto-mouse RGB progress + mode flags) from master to slave.
#    ifdef SPLIT_KEYBOARD
#        define SPLIT_TRANSACTION_IDS_USER PUT_PD_SYNC
#    endif

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

#    define POINTING_DEVICE_HIRES_SCROLL_ENABLE
#    define POINTING_DEVICE_HIRES_SCROLL_MULTIPLIER 120

// Drag-scroll converts trackball motion into scroll events.
// With hi-res scrolling, each scroll unit = 1/120th of a notch.
//
//   Speed      = DPI / STEP_DIVISOR   (lower = slower, more precise)
//   Smoothness = RATE_LIMIT_MS        (lower = more frequent updates)
//   Snap feel  = SNAP_RATIO           (higher = stricter axis lock)
#    define CHARYBDIS_DRAGSCROLL_REVERSE_Y
#    define CHARYBDIS_DRAGSCROLL_DPI 100
#    define CHARYBDIS_DRAGSCROLL_BUFFER_SIZE 0
#    define CHARYBDIS_SCROLL_STEP_DIVISOR 8
#    define CHARYBDIS_SCROLL_RATE_LIMIT_MS 8
#    define CHARYBDIS_SCROLL_SNAP_RATIO 3
#    define CHARYBDIS_SCROLL_BUFFER_EXPIRE_MS 80

#endif // POINTING_DEVICE_ENABLE
