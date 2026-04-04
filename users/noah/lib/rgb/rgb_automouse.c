// ────────────────────────────────────────────────────────────────────────────
// Auto-Mouse RGB Gradient
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)

#    include "pointing_device_auto_mouse.h" // QMK (firmware fork)
#    include "../pointing/pd_mode_flags.h"
#    include "../state/runtime_shared_state.h"
#    include "rgb_automouse.h"
#    include "rgb_helpers.h"

static rgb_t automouse_cached_rgb  = {0};
static hsv_t automouse_cached_hsv  = {0};
static bool  automouse_cache_valid = false;

bool automouse_rgb_render(uint8_t led_min, uint8_t led_max, hsv_t start, hsv_t end) {
    uint16_t progress;

    if (is_keyboard_master()) {
        progress = automouse_rgb_progress(auto_mouse_get_time_elapsed());
    } else {
        progress = runtime_shared_state_remote.automouse_progress;
    }

    if (pd_any_mode_locked()) {
        progress = 0;
    }

    if (progress > AUTOMOUSE_RGB_ACTIVE_SPAN) {
        progress = AUTOMOUSE_RGB_ACTIVE_SPAN;
    }

    hsv_t hsv;

    if (progress == 0) {
        hsv = start;
    } else {
        uint32_t t = (uint32_t)progress * 255 / AUTOMOUSE_RGB_ACTIVE_SPAN;
        hsv.h      = start.h + (uint8_t)((int32_t)((int16_t)end.h - (int16_t)start.h) * t / 255);
        hsv.s      = start.s + (uint8_t)((int32_t)((int16_t)end.s - (int16_t)start.s) * t / 255);
        hsv.v      = start.v + (uint8_t)((int32_t)((int16_t)end.v - (int16_t)start.v) * t / 255);
    }

    if (!automouse_cache_valid || hsv.h != automouse_cached_hsv.h || hsv.s != automouse_cached_hsv.s || hsv.v != automouse_cached_hsv.v) {
        automouse_cached_hsv  = hsv;
        automouse_cached_rgb  = hsv_to_rgb(hsv);
        automouse_cache_valid = true;
    }

    rgb_set_both_halves(automouse_cached_rgb, led_min, led_max);
    return true;
}

#endif // defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)
