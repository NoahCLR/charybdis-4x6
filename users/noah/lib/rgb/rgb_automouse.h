// ────────────────────────────────────────────────────────────────────────────
// Auto-Mouse RGB Timeout Fade
// ────────────────────────────────────────────────────────────────────────────
//
// Public interface for tracking auto-mouse timeout progress.
// Rendering lives in rgb_runtime.c.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#if __has_include("color.h")
#    include "color.h" // QMK
#endif

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

_Static_assert(AUTO_MOUSE_TIME > 0, "AUTO_MOUSE_TIME must be greater than zero");
_Static_assert(AUTOMOUSE_RGB_DEAD_TIME < AUTO_MOUSE_TIME, "AUTOMOUSE_RGB_DEAD_TIME must be less than AUTO_MOUSE_TIME");
_Static_assert(AUTOMOUSE_RGB_ACTIVE_SPAN > 0, "AUTOMOUSE_RGB_ACTIVE_SPAN must be greater than zero");
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
uint16_t              automouse_rgb_current_progress(void);
bool                  automouse_rgb_should_render(void);
static inline uint8_t automouse_rgb_blend_amount(uint16_t progress) {
    if (progress >= AUTOMOUSE_RGB_ACTIVE_SPAN) {
        return UINT8_MAX;
    }

    return (uint8_t)((uint32_t)progress * UINT8_MAX / AUTOMOUSE_RGB_ACTIVE_SPAN);
}
#else
static inline uint16_t automouse_rgb_current_progress(void) {
    return 0;
}
static inline bool automouse_rgb_should_render(void) {
    return false;
}
static inline uint8_t automouse_rgb_blend_amount(uint16_t progress) {
    (void)progress;
    return 0;
}
#endif // defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)
