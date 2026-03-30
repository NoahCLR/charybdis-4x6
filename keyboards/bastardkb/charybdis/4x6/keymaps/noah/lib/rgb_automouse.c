// ────────────────────────────────────────────────────────────────────────────
// Auto-Mouse RGB Gradient
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)

#    include "pointing_device_auto_mouse.h" // QMK (firmware fork)
#    include "pointing_device_modes.h"
#    include "rgb_automouse.h"
#    include "rgb_helpers.h"
#    include "split_sync.h"

#    ifndef AUTOMOUSE_RGB_DEAD_TIME
#        define AUTOMOUSE_RGB_DEAD_TIME (AUTO_MOUSE_TIME / 3)
#    endif

#    define AUTOMOUSE_RGB_ACTIVE_SPAN (AUTO_MOUSE_TIME - AUTOMOUSE_RGB_DEAD_TIME)

static rgb_t automouse_cached_rgb  = {0};
static hsv_t automouse_cached_hsv  = {0};
static bool  automouse_cache_valid = false;

bool automouse_rgb_render(uint8_t led_min, uint8_t led_max, hsv_t start, hsv_t end) {
    uint16_t elapsed;

    if (is_keyboard_master()) {
        elapsed = auto_mouse_get_time_elapsed();
        pd_state_sync_elapsed(elapsed);
    } else {
        elapsed = pd_sync_remote.elapsed;
    }

    if (pd_any_mode_locked()) {
        elapsed = 0;
    }

    if (elapsed > AUTO_MOUSE_TIME) {
        elapsed = AUTO_MOUSE_TIME;
    }

    uint16_t prog = (elapsed <= AUTOMOUSE_RGB_DEAD_TIME) ? 0 : (elapsed - AUTOMOUSE_RGB_DEAD_TIME);
    hsv_t     hsv;

    if (prog == 0) {
        hsv = start;
    } else {
        uint32_t t = (uint32_t)prog * 255 / AUTOMOUSE_RGB_ACTIVE_SPAN;
        hsv.h = start.h + (uint8_t)((int32_t)((int16_t)end.h - (int16_t)start.h) * t / 255);
        hsv.s = start.s + (uint8_t)((int32_t)((int16_t)end.s - (int16_t)start.s) * t / 255);
        hsv.v = start.v + (uint8_t)((int32_t)((int16_t)end.v - (int16_t)start.v) * t / 255);
    }

    if (!automouse_cache_valid ||
        hsv.h != automouse_cached_hsv.h ||
        hsv.s != automouse_cached_hsv.s ||
        hsv.v != automouse_cached_hsv.v) {
        automouse_cached_hsv  = hsv;
        automouse_cached_rgb  = hsv_to_rgb(hsv);
        automouse_cache_valid = true;
    }

    rgb_set_both_halves(automouse_cached_rgb, led_min, led_max);
    return true;
}

#endif // defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)
