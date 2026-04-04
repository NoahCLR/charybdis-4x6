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

// Register a custom split RPC transaction for syncing shared runtime state
// such as pd-mode flags, auto-mouse RGB progress, and key-feedback flags.
#    define SPLIT_TRANSACTION_IDS_USER PUT_RUNTIME_SHARED_SYNC

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

#endif // POINTING_DEVICE_ENABLE
