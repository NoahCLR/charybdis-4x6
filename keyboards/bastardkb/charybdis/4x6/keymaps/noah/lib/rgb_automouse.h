// ────────────────────────────────────────────────────────────────────────────
// Auto-Mouse RGB Gradient
// ────────────────────────────────────────────────────────────────────────────
//
// When LAYER_POINTER is active (auto-mouse triggered by trackball
// movement), all LEDs show an animated gradient that represents the
// remaining time before the layer deactivates:
//
//   Start (just triggered): white
//   End   (about to expire): red
//
// The first third of the timeout is "dead time" — the color stays
// white so you don't get distracting flicker during active use.
// The gradient only animates during the final two-thirds.
//
// HSV → RGB conversion is cached to avoid expensive math every frame.
//
// The elapsed time used for the gradient comes from two sources:
//   - Master half: reads auto_mouse_get_time_elapsed() directly
//   - Slave half:  reads pd_sync_remote.elapsed (synced via split_sync.h)
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)

#    include "pointing_device_auto_mouse.h" // QMK (firmware fork)
#    include "rgb_helpers.h"
#    include "split_sync.h"

// How long the color stays at the start value before the gradient begins.
// Default: first 1/3 of AUTO_MOUSE_TIME.
#    ifndef AUTOMOUSE_RGB_DEAD_TIME
#        define AUTOMOUSE_RGB_DEAD_TIME (AUTO_MOUSE_TIME / 3)
#    endif

// The remaining time over which the gradient animates.
#    define AUTOMOUSE_RGB_ACTIVE_SPAN (AUTO_MOUSE_TIME - AUTOMOUSE_RGB_DEAD_TIME)

// ─── Auto-mouse gradient rendering ─────────────────────────────────────────

// Cache the last computed HSV → RGB conversion to avoid redundant math.
// Only recomputed when the interpolated HSV actually changes.
// Persists across layer activations intentionally — the gradient always
// restarts from prog=0 (start color), so stale cache values are harmless.
static rgb_t automouse_cached_rgb  = {0};
static hsv_t automouse_cached_hsv  = {0};
static bool  automouse_cache_valid = false;

// Render the auto-mouse countdown gradient across all LEDs.
//
// Parameters:
//   led_min/max: the LED chunk being rendered (split-safe)
//   start/end:  the HSV colors for the gradient endpoints
//
// The gradient interpolates linearly from `start` to `end` based on how
// much of the auto-mouse timeout has elapsed.
static bool automouse_rgb_render(uint8_t led_min, uint8_t led_max, hsv_t start, hsv_t end) {
    // Read elapsed once and reuse for both sync and rendering.
    // Master reads the timer directly; slave uses the synced value.
    uint16_t elapsed;
    if (is_keyboard_master()) {
        elapsed = auto_mouse_get_time_elapsed();
        pd_state_sync_elapsed(elapsed); // broadcast to slave using same value
    } else {
        elapsed = pd_sync_remote.elapsed;
    }

    if (elapsed > AUTO_MOUSE_TIME) {
        elapsed = AUTO_MOUSE_TIME;
    }

    // During dead time (first 1/3), progress stays at 0 → color stays at `start`.
    uint16_t prog = (elapsed <= AUTOMOUSE_RGB_DEAD_TIME) ? 0 : (elapsed - AUTOMOUSE_RGB_DEAD_TIME);

    // Interpolate HSV channels linearly from start → end.
    hsv_t hsv;

    if (prog == 0) {
        hsv = start;
    } else {
        // t ranges from 0 to 255 as prog goes from 0 to ACTIVE_SPAN.
        uint32_t t = (uint32_t)prog * 255 / AUTOMOUSE_RGB_ACTIVE_SPAN;
        // int32_t cast prevents overflow: diff can be up to 255, t up to 255,
        // so the product can reach 65025 which exceeds int16_t range.
        hsv.h = start.h + (uint8_t)((int32_t)((int16_t)end.h - (int16_t)start.h) * t / 255);
        hsv.s = start.s + (uint8_t)((int32_t)((int16_t)end.s - (int16_t)start.s) * t / 255);
        hsv.v = start.v + (uint8_t)((int32_t)((int16_t)end.v - (int16_t)start.v) * t / 255);
    }

    // Only call hsv_to_rgb() when the color actually changed.
    if (!automouse_cache_valid || hsv.h != automouse_cached_hsv.h || hsv.s != automouse_cached_hsv.s || hsv.v != automouse_cached_hsv.v) {
        automouse_cached_hsv  = hsv;
        automouse_cached_rgb  = hsv_to_rgb(hsv);
        automouse_cache_valid = true;
    }

    rgb_set_both_halves(automouse_cached_rgb, led_min, led_max);

    return true;
}

#else  // Required features not all enabled — provide empty stubs.
static inline bool automouse_rgb_render(uint8_t led_min, uint8_t led_max, hsv_t start, hsv_t end) {
    (void)led_min;
    (void)led_max;
    (void)start;
    (void)end;
    return false;
}
#endif // defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)
