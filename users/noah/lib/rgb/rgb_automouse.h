// ────────────────────────────────────────────────────────────────────────────
// Auto-Mouse RGB Gradient
// ────────────────────────────────────────────────────────────────────────────
//
// Public interface for rendering the auto-mouse timeout gradient.
// Implementation lives in rgb_automouse.c.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // QMK

#include "rgb_config.h"

#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)

#    ifndef AUTOMOUSE_RGB_DEAD_TIME
#        define AUTOMOUSE_RGB_DEAD_TIME (AUTO_MOUSE_TIME / 3)
#    endif

#    define AUTOMOUSE_RGB_ACTIVE_SPAN (AUTO_MOUSE_TIME - AUTOMOUSE_RGB_DEAD_TIME)

#    ifndef AUTOMOUSE_RGB_SYNC_STEP
#        ifdef RGB_MATRIX_LED_FLUSH_LIMIT
#            define AUTOMOUSE_RGB_SYNC_STEP RGB_MATRIX_LED_FLUSH_LIMIT
#        else
#            define AUTOMOUSE_RGB_SYNC_STEP 50
#        endif
#    endif

_Static_assert(AUTOMOUSE_RGB_SYNC_STEP > 0, "AUTOMOUSE_RGB_SYNC_STEP must be greater than zero");

static inline uint16_t automouse_rgb_progress(uint16_t raw_elapsed) {
    if (raw_elapsed > AUTO_MOUSE_TIME) {
        raw_elapsed = AUTO_MOUSE_TIME;
    }

    if (raw_elapsed <= AUTOMOUSE_RGB_DEAD_TIME) {
        return 0;
    }

    return raw_elapsed - AUTOMOUSE_RGB_DEAD_TIME;
}

static inline uint16_t automouse_rgb_quantize_progress(uint16_t raw_elapsed) {
    uint16_t progress = automouse_rgb_progress(raw_elapsed);
    return progress / AUTOMOUSE_RGB_SYNC_STEP * AUTOMOUSE_RGB_SYNC_STEP;
}

#endif // POINTING_DEVICE_AUTO_MOUSE_ENABLE

#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)
bool automouse_rgb_render(uint8_t led_min, uint8_t led_max, hsv_t start, hsv_t end);
#else
static inline bool automouse_rgb_render(uint8_t led_min, uint8_t led_max, hsv_t start, hsv_t end) {
    (void)led_min;
    (void)led_max;
    (void)start;
    (void)end;
    return false;
}
#endif // defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)
