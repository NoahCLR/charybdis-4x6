// ────────────────────────────────────────────────────────────────────────────
// RGB Combo Feedback Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_combo_feedback_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)

#    include "../../key/runtime/feedback.h"

void rgb_runtime_combo_feedback_stage_post_init(void) {
    // Retained as a pipeline hook; effective colors are resolved per frame.
}

static bool rgb_runtime_combo_feedback_stage_paint_range(rgb_runtime_frame_t *frame, rgb_t color, uint8_t from, uint8_t to, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    if (!frame || from >= led_max || to <= led_min) {
        return false;
    }
    if (from < led_min) {
        from = led_min;
    }
    if (to > led_max) {
        to = led_max;
    }
    for (uint8_t led = from; led < to; led++) {
        frame->colors[led]  = color;
        frame->painted[led] = true;
        painted             = true;
    }
    return painted;
}

static bool rgb_runtime_combo_feedback_stage_paint_key(rgb_runtime_frame_t *frame, rgb_t color, keypos_t key_pos, uint8_t led_min, uint8_t led_max) {
    uint8_t leds[RGB_MATRIX_LED_COUNT];
    uint8_t led_count = rgb_matrix_map_row_column_to_led(key_pos.row, key_pos.col, leds);
    bool    painted   = false;

    for (uint8_t index = 0u; index < led_count; index++) {
        uint8_t led = leds[index];

        if (frame && led >= led_min && led < led_max) {
            frame->colors[led]  = color;
            frame->painted[led] = true;
            painted             = true;
        }
    }
    return painted;
}

static bool rgb_runtime_combo_feedback_stage_paint_bitmap_keys(rgb_runtime_frame_t *frame, rgb_t color, const uint8_t *bitmap, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    if (!bitmap) {
        return false;
    }
    for (uint8_t row = 0u; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0u; col < MATRIX_COLS; col++) {
            keypos_t key_pos = {.row = row, .col = col};

            if (key_origin_bitmap_has_keypos(bitmap, key_pos)) {
                painted |= rgb_runtime_combo_feedback_stage_paint_key(frame, color, key_pos, led_min, led_max);
            }
        }
    }
    return painted;
}

static bool rgb_runtime_combo_feedback_stage_paint_locality(rgb_runtime_frame_t *frame, rgb_t color, rgb_locality_t locality, const uint8_t *bitmap, uint8_t led_min, uint8_t led_max) {
    split_side_mask_t sides;

    switch (locality) {
        case RGB_KEYS_ONLY:
            return rgb_runtime_combo_feedback_stage_paint_bitmap_keys(frame, color, bitmap, led_min, led_max);
        case RGB_LEFT_HALF:
            return rgb_runtime_combo_feedback_stage_paint_range(frame, color, 0u, RGB_LEFT_LED_COUNT, led_min, led_max);
        case RGB_RIGHT_HALF:
            return rgb_runtime_combo_feedback_stage_paint_range(frame, color, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, led_min, led_max);
        case RGB_KEY_HALF:
            sides = key_origin_bitmap_side_mask(bitmap);
            if (sides == SPLIT_SIDE_MASK_LEFT) {
                return rgb_runtime_combo_feedback_stage_paint_range(frame, color, 0u, RGB_LEFT_LED_COUNT, led_min, led_max);
            }
            if (sides == SPLIT_SIDE_MASK_RIGHT) {
                return rgb_runtime_combo_feedback_stage_paint_range(frame, color, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, led_min, led_max);
            }
            break;
        case RGB_BOTH_HALVES:
        default:
            break;
    }
    return rgb_runtime_combo_feedback_stage_paint_range(frame, color, 0u, RGB_MATRIX_LED_COUNT, led_min, led_max);
}

static bool rgb_runtime_combo_feedback_stage_paint_group(rgb_runtime_frame_t *frame, const rgb_effective_combo_group_t *group, rgb_t color, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    if (!(frame && group)) {
        return false;
    }
    for (uint8_t led = led_min; led < led_max; led++) {
        if (led >= NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT || (group->bitmap[led / 8u] & (uint8_t)(1u << (led % 8u))) == 0u) {
            continue;
        }
        frame->colors[led]  = color;
        frame->painted[led] = true;
        painted             = true;
    }
    return painted;
}

bool rgb_runtime_combo_feedback_stage_render_effective_frame(rgb_runtime_frame_t *frame, const noah_effective_rgb_frame_t *profile_frame, const uint8_t *bitmap, uint8_t led_min, uint8_t led_max) {
    combo_feedback_color_config_t feedback_config;
    rgb_effective_combo_group_t   group_config;
    rgb_t                         active_rgb;
    bool                          painted = false;
    uint8_t                       group_count;

    if (!frame) {
        return false;
    }
    rgb_runtime_frame_clear(frame, led_min, led_max);
    if (!bitmap || !key_origin_bitmap_has_any(bitmap) || !rgb_effective_config_combo_stage_enabled(profile_frame) || !rgb_effective_config_combo_feedback(profile_frame, &feedback_config)) {
        return false;
    }
    active_rgb = hsv_to_rgb(feedback_config.color);
    painted |= rgb_runtime_combo_feedback_stage_paint_locality(frame, active_rgb, feedback_config.locality, bitmap, led_min, led_max);

    group_count = rgb_effective_config_combo_group_count(profile_frame);
    for (uint8_t group = 0u; group < group_count; group++) {
        if (!rgb_effective_config_combo_group_at(profile_frame, group, &group_config)) {
            rgb_runtime_frame_clear(frame, led_min, led_max);
            return false;
        }

        rgb_t group_rgb = rgb_hsv_is_inherit_color(group_config.color) ? active_rgb : hsv_to_rgb(group_config.color);
        painted |= rgb_runtime_combo_feedback_stage_paint_group(frame, &group_config, group_rgb, led_min, led_max);
    }

    if (!rgb_effective_config_frame_current(profile_frame)) {
        rgb_runtime_frame_clear(frame, led_min, led_max);
        return false;
    }
    return painted;
}

#endif
