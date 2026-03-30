// ────────────────────────────────────────────────────────────────────────────
// RGB Runtime
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#include "../../keymap_defs.h"
#include "../../rgb_config.h"
#include "../pointing/pointing_device_modes.h"
#include "rgb_automouse.h"
#include "rgb_helpers.h"

#define PD_MODE_COLOR_COUNT (sizeof(pd_mode_colors) / sizeof(pd_mode_colors[0]))
#define LAYER_LED_GROUP_COUNT (sizeof(layer_led_groups) / sizeof(layer_led_groups[0]))
#define PD_MODE_LED_GROUP_COUNT (sizeof(pd_mode_led_groups) / sizeof(pd_mode_led_groups[0]))

#ifdef RGB_MATRIX_ENABLE
static rgb_t layer_rgb[LAYER_COUNT];
static rgb_t pd_mode_rgb[PD_MODE_COUNT];
static rgb_t led_group_rgb[LAYER_LED_GROUP_COUNT];
static rgb_t pd_mode_led_group_rgb[PD_MODE_LED_GROUP_COUNT];
#endif

void keyboard_post_init_user(void) {
#ifdef RGB_MATRIX_ENABLE
    for (uint8_t i = 0; i < LAYER_COUNT; i++) {
        layer_rgb[i] = hsv_to_rgb(layer_colors[i]);
    }

    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        for (uint8_t c = 0; c < PD_MODE_COLOR_COUNT; c++) {
            if (pd_mode_colors[c].mode_flag == pd_modes[i].mode_flag) {
                pd_mode_rgb[i] = hsv_to_rgb(pd_mode_colors[c].color);
                break;
            }
        }
    }

    for (uint8_t i = 0; i < LAYER_LED_GROUP_COUNT; i++) {
        led_group_rgb[i] = hsv_to_rgb(layer_led_groups[i].color);
    }

    for (uint8_t i = 0; i < PD_MODE_LED_GROUP_COUNT; i++) {
        pd_mode_led_group_rgb[i] = hsv_to_rgb(pd_mode_led_groups[i].color);
    }
#endif
}

#ifdef RGB_MATRIX_ENABLE
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    bool layer_painted = false;

    for (int8_t i = LAYER_COUNT - 1; i > 0; i--) {
        if (!layer_state_cmp(layer_state, i)) continue;
        if (layer_colors[i].s == 0 && layer_colors[i].v == 0) continue;
        rgb_set_both_halves(layer_rgb[i], led_min, led_max);
        layer_painted = true;
        break;
    }

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    if (!layer_painted && layer_state_cmp(layer_state, get_auto_mouse_layer())) {
        automouse_rgb_render(led_min, led_max, automouse_color_start, automouse_color_end);
        layer_painted = true;
    }
#    endif

    for (uint8_t g = 0; g < LAYER_LED_GROUP_COUNT; g++) {
        if (layer_led_groups[g].layers & layer_state) {
            rgb_set_led_group(layer_led_groups[g].leds, layer_led_groups[g].count, led_min, led_max, led_group_rgb[g]);
        }
    }

    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_mode_active(pd_modes[i].mode_flag)) {
            rgb_set_right_half(pd_mode_rgb[i], led_min, led_max);
            break;
        }
    }

    for (uint8_t g = 0; g < PD_MODE_LED_GROUP_COUNT; g++) {
        if (pd_mode_active(pd_mode_led_groups[g].mode_flag)) {
            rgb_set_led_group(pd_mode_led_groups[g].leds, pd_mode_led_groups[g].count, led_min, led_max, pd_mode_led_group_rgb[g]);
        }
    }

    return layer_painted;
}
#endif // RGB_MATRIX_ENABLE
