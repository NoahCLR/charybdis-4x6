// ────────────────────────────────────────────────────────────────────────────
// RGB Combo Feedback Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_combo_feedback_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(COMBO_ENABLE)

#    include "../core/rgb_helpers.h"
#    include "../../key/runtime/feedback.h"
#    include "../../state/runtime/split_runtime_sync.h"

extern const combo_feedback_color_config_t combo_feedback_colors;

static rgb_t combo_feedback_active_rgb;

void rgb_runtime_combo_feedback_stage_post_init(void) {
    combo_feedback_active_rgb = hsv_to_rgb(combo_feedback_colors.color);
}

static bool rgb_runtime_combo_feedback_stage_led_range_intersects(uint8_t from, uint8_t to, uint8_t led_min, uint8_t led_max) {
    return from < led_max && to > led_min;
}

static void rgb_runtime_combo_feedback_stage_current_bitmap(bool underlay, uint8_t *out_bitmap) {
    if (!out_bitmap) {
        return;
    }

    if (is_keyboard_master()) {
        if (underlay) {
            combo_feedback_underlay_bitmap(out_bitmap);
        } else {
            combo_feedback_overlay_bitmap(out_bitmap);
        }
        return;
    }

    key_origin_bitmap_copy(out_bitmap, underlay ? split_runtime_sync_remote.combo_underlay_bitmap : split_runtime_sync_remote.combo_overlay_bitmap);
}

static bool rgb_runtime_combo_feedback_stage_paint_key(keypos_t key_pos, uint8_t led_min, uint8_t led_max) {
    uint8_t leds[RGB_MATRIX_LED_COUNT];
    uint8_t led_count = rgb_matrix_map_row_column_to_led(key_pos.row, key_pos.col, leds);
    bool    painted   = false;

    for (uint8_t index = 0; index < led_count; index++) {
        uint8_t led = leds[index];

        if (led >= led_min && led < led_max) {
            rgb_set_led_color(led, led_min, led_max, combo_feedback_active_rgb);
            painted = true;
        }
    }

    return painted;
}

static bool rgb_runtime_combo_feedback_stage_paint_bitmap_keys(const uint8_t *bitmap, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    if (!bitmap) {
        return false;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            keypos_t key_pos = {.row = row, .col = col};

            if (!key_origin_bitmap_has_keypos(bitmap, key_pos)) {
                continue;
            }

            painted |= rgb_runtime_combo_feedback_stage_paint_key(key_pos, led_min, led_max);
        }
    }

    return painted;
}

static bool rgb_runtime_combo_feedback_stage_render(bool underlay, uint8_t led_min, uint8_t led_max) {
    uint8_t           bitmap[KEY_ORIGIN_BITMAP_SIZE];
    split_side_mask_t sides;

    rgb_runtime_combo_feedback_stage_current_bitmap(underlay, bitmap);
    if (!key_origin_bitmap_has_any(bitmap)) {
        return false;
    }

    switch (combo_feedback_colors.locality) {
        case RGB_KEYS_ONLY:
            return rgb_runtime_combo_feedback_stage_paint_bitmap_keys(bitmap, led_min, led_max);
        case RGB_LEFT_HALF:
            if (!rgb_runtime_combo_feedback_stage_led_range_intersects(0, RGB_LEFT_LED_COUNT, led_min, led_max)) {
                return false;
            }
            rgb_set_left_half(combo_feedback_active_rgb, led_min, led_max);
            return true;
        case RGB_RIGHT_HALF:
            if (!rgb_runtime_combo_feedback_stage_led_range_intersects(RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, led_min, led_max)) {
                return false;
            }
            rgb_set_right_half(combo_feedback_active_rgb, led_min, led_max);
            return true;
        case RGB_KEY_HALF:
            sides = key_origin_bitmap_side_mask(bitmap);
            if (sides == SPLIT_SIDE_MASK_LEFT) {
                if (!rgb_runtime_combo_feedback_stage_led_range_intersects(0, RGB_LEFT_LED_COUNT, led_min, led_max)) {
                    return false;
                }
                rgb_set_left_half(combo_feedback_active_rgb, led_min, led_max);
                return true;
            }

            if (sides == SPLIT_SIDE_MASK_RIGHT) {
                if (!rgb_runtime_combo_feedback_stage_led_range_intersects(RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, led_min, led_max)) {
                    return false;
                }
                rgb_set_right_half(combo_feedback_active_rgb, led_min, led_max);
                return true;
            }
            break;
        case RGB_BOTH_HALVES:
        default:
            break;
    }

    rgb_set_both_halves(combo_feedback_active_rgb, led_min, led_max);
    return led_min < led_max;
}

bool rgb_runtime_combo_feedback_stage_render_underlay(uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_combo_feedback_stage_render(true, led_min, led_max);
}

bool rgb_runtime_combo_feedback_stage_render_overlay(uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_combo_feedback_stage_render(false, led_min, led_max);
}

#endif
