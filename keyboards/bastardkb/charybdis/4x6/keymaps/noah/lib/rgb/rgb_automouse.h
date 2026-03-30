// ────────────────────────────────────────────────────────────────────────────
// Auto-Mouse RGB Gradient
// ────────────────────────────────────────────────────────────────────────────
//
// Public interface for rendering the auto-mouse timeout gradient.
// Implementation lives in rgb_automouse.c.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

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
