// ────────────────────────────────────────────────────────────────────────────
// Noah's Charybdis 4x6 — Configuration Overrides
// ────────────────────────────────────────────────────────────────────────────
//
// This file overrides default QMK and Charybdis firmware settings.
// Values here take precedence over the keyboard-level config.h.
//
// The #undef-before-#define pattern is used throughout because QMK may
// have already defined these values in parent config files.  Without
// the #undef, redefining a macro would cause a compiler warning.
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

// ─── Core / feature toggles ─────────────────────────────────────────────────

// VIA is a GUI tool for remapping keys without recompiling firmware.
// We need 5 dynamic layers to match our layer count (Base, Num, Lower, Raise, Pointer).
#ifdef VIA_ENABLE
#    undef DYNAMIC_KEYMAP_LAYER_COUNT
#    define DYNAMIC_KEYMAP_LAYER_COUNT 5
#endif

// ─── Split keyboard sync ───────────────────────────────────────────────────
//
// The Charybdis 4x6 is a split keyboard — each half has its own MCU.
// By default, only keystrokes are sent between halves.  These options
// enable syncing additional state so both halves stay consistent.
//
#ifdef SPLIT_KEYBOARD

// ─── Split role override ────────────────────────────────────────────────────
// Uncomment ONE of these to force this firmware to always be master or slave.
// Useful when both halves have USB connected.  Leave both commented for
// default USB-detect behavior.  Not to be confused with handedness
// (left/right), which is set by MASTER_RIGHT in the keyboard-level config in the qmk repo.
// #define FORCE_MASTER
// #define FORCE_SLAVE

// The default split serial timeout (20ms) is far too long for a keyboard
// with a trackball — a single failed transaction blocks the main loop for
// 20ms, causing the sensor to accumulate motion into a huge cursor jump.
// At 230400 baud a full transaction completes in <1ms; 5ms is generous
// headroom while keeping worst-case stalls well below perceptible.
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

// Minimum ms between RGB matrix updates.  Higher = less CPU load but
// choppier animations.  32ms ≈ 30 FPS.
#    ifdef RGB_MATRIX_LED_FLUSH_LIMIT
#        undef RGB_MATRIX_LED_FLUSH_LIMIT
#    endif
#    define RGB_MATRIX_LED_FLUSH_LIMIT 32

// Turn off LEDs after 15 minutes (900,000ms) of inactivity.
// Requires SPLIT_ACTIVITY_ENABLE to keep both halves in sync.
#    ifdef RGB_MATRIX_TIMEOUT
#        undef RGB_MATRIX_TIMEOUT
#    endif
#    define RGB_MATRIX_TIMEOUT 900000

#endif // RGB_MATRIX_ENABLE

// ─── Pointing device (trackball) ────────────────────────────────────────────
//
// The Charybdis has a trackball on the right half.
// These settings configure auto-mouse behavior, DPI, and scroll tuning.

#ifdef POINTING_DEVICE_ENABLE

// Auto-mouse: automatically activates LAYER_POINTER when the trackball
// moves, and deactivates it after the timeout expires.
#    define POINTING_DEVICE_AUTO_MOUSE_ENABLE
#    define AUTO_MOUSE_TIME 1200 // ms of no trackball movement before the pointer layer deactivates

// Register a custom split RPC transaction for syncing pointing device
// state (auto-mouse elapsed time + mode flags) from master to slave.
#    ifdef SPLIT_KEYBOARD
#        define SPLIT_TRANSACTION_IDS_USER PUT_PD_SYNC
#    endif

// Default pointer DPI (base value before DPI_MOD/DPI_RMOD adjustments).
#    define CHARYBDIS_MINIMUM_DEFAULT_DPI 400

// Liftoff distance — how high the ball can be lifted before tracking stops.
// Default is 0x02; higher = tracks further from the surface.
#    define PMW33XX_LIFTOFF_DISTANCE 0x04

// Enable 16-bit motion reports for higher precision at high DPI.
#    define MOUSE_EXTENDED_REPORT
#    define WHEEL_EXTENDED_REPORT

#    define POINTING_DEVICE_HIRES_SCROLL_ENABLE
#    define POINTING_DEVICE_HIRES_SCROLL_MULTIPLIER 120

// ─── Drag-scroll tuning ────────────────────────────────────────────────────
// Drag-scroll converts trackball motion into scroll events.
// With hi-res scrolling, each scroll unit = 1/120th of a notch.
//
//   Speed     = DPI / STEP_DIVISOR   (lower = slower, more precise)
//   Smoothness = RATE_LIMIT_MS       (lower = more frequent updates)
//   Snap feel  = SNAP_RATIO          (higher = stricter axis lock)

#    define CHARYBDIS_DRAGSCROLL_REVERSE_Y       // "push forward = scroll up"
#    define CHARYBDIS_DRAGSCROLL_DPI 100         // Sensor CPI while scrolling (lower = finer control)
#    define CHARYBDIS_DRAGSCROLL_BUFFER_SIZE 0   // Dead zone in sensor counts (0 = immediate response)
#    define CHARYBDIS_SCROLL_STEP_DIVISOR 8      // Sensor counts per hi-res scroll unit (higher = slower)
#    define CHARYBDIS_SCROLL_RATE_LIMIT_MS 8     // Min ms between scroll events (8 ≈ 125 Hz)
#    define CHARYBDIS_SCROLL_SNAP_RATIO 3        // Axis lock: dominant axis must be Nx the other to snap
#    define CHARYBDIS_SCROLL_BUFFER_EXPIRE_MS 80 // Discard buffered motion after this many ms idle

#endif // POINTING_DEVICE_ENABLE

// ─── Custom tap/hold timing ────────────────────────────────────────────────
// Used by the custom three-tier tap/hold system in keymap.c.
// These are NOT QMK's built-in TAPPING_TERM — they're checked manually
// in process_record_user().
#define CUSTOM_TAP_HOLD_TERM 150 // < 150ms = tap (send plain key)
// 150–400ms = hold (shifted variant)
#define CUSTOM_LONGER_HOLD_TERM 400 // > 400ms = longer hold (third action)
