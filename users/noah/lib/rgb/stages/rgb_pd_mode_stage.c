// ────────────────────────────────────────────────────────────────────────────
// RGB PD-Mode Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_pd_mode_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_ENABLE)

#    include "../core/rgb_helpers.h"
#    include "../../pointing/defs/pd_modes.h"

extern const pd_mode_color_t            pd_mode_colors[];
extern const uint8_t                    pd_mode_color_count;
extern const pd_mode_led_group_t *const pd_mode_led_groups;
extern const uint8_t                    pd_mode_led_group_count;

static rgb_t pd_mode_rgb[PD_MODE_COUNT];
static uint8_t pd_mode_render_mode[PD_MODE_COUNT];

static bool rgb_runtime_pd_mode_stage_led_range_intersects(uint8_t led_min, uint8_t led_max, uint8_t from, uint8_t to) {
    return led_min < to && led_max > from;
}

static bool rgb_runtime_pd_mode_stage_led_group_intersects(const uint8_t *leds, uint8_t count, uint8_t led_min, uint8_t led_max) {
    for (uint8_t i = 0; i < count; i++) {
        if (leds[i] >= led_min && leds[i] < led_max) {
            return true;
        }
    }

    return false;
}

void rgb_runtime_pd_mode_stage_post_init(void) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        pd_mode_rgb[i]         = (rgb_t){0};
        pd_mode_render_mode[i] = PD_COLOR_MODE_RIGHT_HALF;

        for (uint8_t c = 0; c < pd_mode_color_count; c++) {
            if (pd_mode_colors[c].pointing_mode == pd_modes[i].mode_flag) {
                pd_mode_rgb[i]         = hsv_to_rgb(pd_mode_colors[c].color);
                pd_mode_render_mode[i] = pd_mode_colors[c].mode;
                break;
            }
        }
    }
}

static split_half_t rgb_runtime_pd_mode_stage_resolve_trigger_half(pd_mode_snapshot_t snapshot) {
    split_half_t owner_half = snapshot.display.owner_half;

    if (owner_half == SPLIT_HALF_LEFT || owner_half == SPLIT_HALF_RIGHT) {
        return owner_half;
    }

    return SPLIT_HALF_RIGHT;
}

static bool rgb_runtime_pd_mode_stage_paint_mode(rgb_t color, uint8_t mode, pd_mode_snapshot_t snapshot, uint8_t led_min, uint8_t led_max) {
    split_half_t half = SPLIT_HALF_NONE;

    switch (mode) {
        case PD_COLOR_MODE_LEFT_HALF:
            if (!rgb_runtime_pd_mode_stage_led_range_intersects(led_min, led_max, 0, RGB_LEFT_LED_COUNT)) {
                return false;
            }
            rgb_set_left_half(color, led_min, led_max);
            return true;
        case PD_COLOR_MODE_BOTH_HALVES:
            rgb_set_both_halves(color, led_min, led_max);
            return led_min < led_max;
        case PD_COLOR_MODE_TRIGGER_HALF:
            half = rgb_runtime_pd_mode_stage_resolve_trigger_half(snapshot);
            break;
        case PD_COLOR_MODE_RIGHT_HALF:
        default:
            half = SPLIT_HALF_RIGHT;
            break;
    }

    if (half == SPLIT_HALF_LEFT) {
        if (!rgb_runtime_pd_mode_stage_led_range_intersects(led_min, led_max, 0, RGB_LEFT_LED_COUNT)) {
            return false;
        }
        rgb_set_left_half(color, led_min, led_max);
        return true;
    }

    if (!rgb_runtime_pd_mode_stage_led_range_intersects(led_min, led_max, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT)) {
        return false;
    }

    rgb_set_right_half(color, led_min, led_max);
    return true;
}

bool rgb_runtime_pd_mode_stage_render(uint8_t led_min, uint8_t led_max) {
    pd_mode_snapshot_t snapshot    = pd_mode_snapshot();
    bool               painted     = false;
    uint8_t            active_mode = snapshot.display.active_index;

    if (active_mode < PD_MODE_COUNT) {
        painted |= rgb_runtime_pd_mode_stage_paint_mode(pd_mode_rgb[active_mode], pd_mode_render_mode[active_mode], snapshot, led_min, led_max);
    }

    for (uint8_t group = 0; group < pd_mode_led_group_count; group++) {
        if (snapshot.display.active_mode != pd_mode_led_groups[group].pointing_mode) {
            continue;
        }

        rgb_t group_rgb = hsv_to_rgb(pd_mode_led_groups[group].color);
        rgb_set_led_group(pd_mode_led_groups[group].leds, pd_mode_led_groups[group].count, led_min, led_max, group_rgb);
        painted |= rgb_runtime_pd_mode_stage_led_group_intersects(pd_mode_led_groups[group].leds, pd_mode_led_groups[group].count, led_min, led_max);
    }

    return painted;
}

#endif
