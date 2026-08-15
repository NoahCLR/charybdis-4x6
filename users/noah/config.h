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

// A failed userspace runtime RPC stops the current send pass. Retry probes use
// a short exponential backoff so an unplugged half cannot spend every scan in
// repeated serial timeouts, while reconnect recovery remains prompt.
#    define SPLIT_RUNTIME_SYNC_RETRY_INITIAL_MS 50u
#    define SPLIT_RUNTIME_SYNC_RETRY_MAX_MS 1000u

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

// Register the custom split RPCs for:
// - base runtime-visible state (automouse / pd / preview)
// - combo feedback locality
// - truthful key-feedback semantics and tap branch state
// - durable committed VIA-storage reconciliation
// so both halves render layer-owned RGB from the same runtime and keymap data.
#    define SPLIT_TRANSACTION_IDS_USER PUT_SPLIT_RUNTIME_BASE_SYNC, PUT_SPLIT_COMBO_FEEDBACK_SYNC, PUT_SPLIT_KEY_FEEDBACK_SEMANTIC_SYNC, PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC, PUT_VIA_KEYMAP_SYNC

// Dynamic key-local placement for PD-mode RGB overlays.
// Required only when a pd_mode_colors[] row uses RGB_KEY_HALF or
// RGB_KEYS_ONLY.
// Comment out RGB_PD_MODE_ACTIVE_HALF_ENABLE to keep the split runtime packet
// smaller and limit PD overlays to fixed left/right/both placement modes.
#    ifdef POINTING_DEVICE_ENABLE
#        define RGB_PD_MODE_ACTIVE_HALF_ENABLE
#    endif

#endif // SPLIT_KEYBOARD

// ─── Matrix scan ───────────────────────────────────────────────────────────
//
// QMK's default matrix settle delay is 30 us after every selected column. On
// this ROW2COL 4x6 half that is six waits per scan, so it dominates a fixed
// part of the main loop. RP2040 GPIO settles much faster than the legacy
// default; keep a smaller explicit delay for diode/matrix stability.
#ifdef MATRIX_IO_DELAY
#    undef MATRIX_IO_DELAY
#endif
#define MATRIX_IO_DELAY 10

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

// Poll the sensor whenever the main loop reaches QMK's pointing-device task.
// The full firmware loop is currently slower than 1 kHz, so the 1 ms QMK
// throttle does not limit reports; it only adds timer work to the hot path.
#    undef POINTING_DEVICE_TASK_THROTTLE_MS
#    define POINTING_DEVICE_TASK_THROTTLE_MS 0

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
// otherwise untouched. This exists for the transparent trackball setup here:
// the sensor can occasionally emit a small mouse report even when the board is
// idle. These mini reports are not noticeable during normal use, but they can
// keep RGB activity alive and delay sleep.
//
// Comment out the enable define to compile this path out entirely. Leave it
// off if your sensor is clean and produces no idle noise at all.
//
// IDLE_MS:
//   Short quiet gap required before a tiny report can be treated as noise.
// ARM_IDLE_MS:
//   Longer "really idle" window before the filter is allowed to start
//   suppressing tiny reports.
// ABS_MAX:
//   Maximum total absolute motion still considered idle noise.
#    define NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ENABLE
#    define NOAH_POINTING_IDLE_NOISE_SUPPRESSION_IDLE_MS 1000
#    define NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ARM_IDLE_MS 300000
#    define NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ABS_MAX 2

// Local drag-scroll tuning (DPI for the scroll speed lives in the keymap config).
// The repo-owned NOAH_DRAGSCROLL_* surface controls the local gesture handler.
//   Thresholds     = start moving one axis intentionally before it scrolls
//   Divisors       = counts per emitted scroll unit
//   Start ratio    = dominance needed to begin a one-axis gesture
//   Sustain ratio  = looser dominance needed to keep the gesture on one axis
//   Lock timeout   = pause window that ends the current gesture
//   Buffer expiry  = longer pause that also discards residual motion
#    define NOAH_DRAGSCROLL_REVERSE_Y
#    define NOAH_DRAGSCROLL_THRESHOLD_H 2
#    define NOAH_DRAGSCROLL_THRESHOLD_V 3
#    define NOAH_DRAGSCROLL_DIVISOR_H 6
#    define NOAH_DRAGSCROLL_DIVISOR_V 8
#    define NOAH_DRAGSCROLL_RATE_LIMIT_MS 8
#    define NOAH_DRAGSCROLL_BUFFER_EXPIRE_MS 80
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
#    define DYNAMIC_KEYMAP_MACRO_COUNT 64
// RP2040 wear-leveling exposes half of this backing region as logical EEPROM.
// 32768 bytes backing gives this keymap roughly 15 KB of VIA macro payload
// space after VIA's dynamic layer storage.
#    if defined(MCU_RP)
#        define WEAR_LEVELING_BACKING_SIZE 32768
#    endif
#endif
