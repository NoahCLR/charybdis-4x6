// ────────────────────────────────────────────────────────────────────────────
// RGB Key Feedback Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_key_feedback_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE)

#    include <string.h>

#    include "../core/rgb_helpers.h"
#    include "../../key/runtime/feedback.h"
#    include "../../state/runtime/split_runtime_sync.h"

extern const key_behavior_feedback_color_config_t key_behavior_feedback_colors;

static rgb_t key_behavior_feedback_multi_tap_pending_rgb;
static rgb_t key_behavior_feedback_hold_active_rgb;
static rgb_t key_behavior_feedback_long_hold_active_rgb;

#    if RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS <= 0
#        error "RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS must be greater than zero"
#    endif

void rgb_runtime_key_feedback_stage_post_init(void) {
    key_behavior_feedback_multi_tap_pending_rgb = hsv_to_rgb(key_behavior_feedback_colors.multi_tap_pending_color);
    key_behavior_feedback_hold_active_rgb       = hsv_to_rgb(key_behavior_feedback_colors.hold_active_color);
    key_behavior_feedback_long_hold_active_rgb  = hsv_to_rgb(key_behavior_feedback_colors.long_hold_active_color);
}

static bool rgb_runtime_key_feedback_stage_led_range_intersects(uint8_t from, uint8_t to, uint8_t led_min, uint8_t led_max) {
    return from < led_max && to > led_min;
}

static void rgb_runtime_key_feedback_stage_current_semantic_map(uint8_t *out_map) {
    if (!out_map) {
        return;
    }

    if (is_keyboard_master()) {
        key_feedback_semantic_map(out_map);
        return;
    }

    memcpy(out_map, split_runtime_sync_remote.key_feedback_semantic_map, KEY_FEEDBACK_SEMANTIC_MAP_SIZE);
}

static uint8_t rgb_runtime_key_feedback_stage_current_flash_meta(void) {
    return is_keyboard_master() ? key_feedback_flash_meta() : split_runtime_sync_remote.key_feedback_flash_meta;
}

static bool rgb_runtime_key_feedback_stage_semantic_color(key_feedback_semantic_t semantic, rgb_t *out_color) {
    if (!out_color) {
        return false;
    }

    switch (semantic) {
        case KEY_FEEDBACK_SEMANTIC_MULTI_TAP_PENDING:
            *out_color = key_behavior_feedback_multi_tap_pending_rgb;
            return true;
        case KEY_FEEDBACK_SEMANTIC_HOLD_PENDING:
        case KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_STEADY:
        case KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING:
            *out_color = key_behavior_feedback_hold_active_rgb;
            return true;
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY:
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING:
            *out_color = key_behavior_feedback_long_hold_active_rgb;
            return true;
        case KEY_FEEDBACK_SEMANTIC_NONE:
        default:
            *out_color = (rgb_t){0};
            return false;
    }
}

static bool rgb_runtime_key_feedback_stage_semantic_visible(key_feedback_semantic_t semantic, uint8_t flash_meta) {
    return !key_feedback_semantic_is_flashing(semantic) || key_feedback_flash_meta_phase(flash_meta);
}

static bool rgb_runtime_key_feedback_stage_paint_key(rgb_t color, keypos_t key_pos, uint8_t led_min, uint8_t led_max) {
    uint8_t leds[RGB_MATRIX_LED_COUNT];
    uint8_t led_count = rgb_matrix_map_row_column_to_led(key_pos.row, key_pos.col, leds);
    bool    painted   = false;

    for (uint8_t i = 0; i < led_count; i++) {
        uint8_t led = leds[i];

        if (led >= led_min && led < led_max) {
            rgb_set_led_color(led, led_min, led_max, color);
            painted = true;
        }
    }

    return painted;
}

static bool rgb_runtime_key_feedback_stage_render_key_mode(const uint8_t *semantic_map, uint8_t flash_meta, uint8_t led_min, uint8_t led_max) {
    bool painted = false;

    if (!semantic_map) {
        return false;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            keypos_t                 key_pos   = {.row = row, .col = col};
            key_feedback_semantic_t  semantic  = key_feedback_semantic_map_get(semantic_map, key_pos);
            rgb_t                    color;

            if (!rgb_runtime_key_feedback_stage_semantic_color(semantic, &color) || !rgb_runtime_key_feedback_stage_semantic_visible(semantic, flash_meta)) {
                continue;
            }

            painted |= rgb_runtime_key_feedback_stage_paint_key(color, key_pos, led_min, led_max);
        }
    }

    return painted;
}

static key_feedback_semantic_t rgb_runtime_key_feedback_stage_half_semantic(const uint8_t *semantic_map, bool right_half) {
    key_feedback_semantic_t best = KEY_FEEDBACK_SEMANTIC_NONE;

    if (!semantic_map) {
        return KEY_FEEDBACK_SEMANTIC_NONE;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            keypos_t                key_pos   = {.row = row, .col = col};
            key_feedback_semantic_t semantic;
            split_half_t            half;

            half = split_half_from_keypos(key_pos);
            if ((right_half && half != SPLIT_HALF_RIGHT) || (!right_half && half != SPLIT_HALF_LEFT)) {
                continue;
            }

            semantic = key_feedback_semantic_map_get(semantic_map, key_pos);
            if (semantic > best) {
                best = semantic;
            }
        }
    }

    return best;
}

static key_feedback_semantic_t rgb_runtime_key_feedback_stage_global_semantic(const uint8_t *semantic_map) {
    key_feedback_semantic_t best = KEY_FEEDBACK_SEMANTIC_NONE;

    if (!semantic_map) {
        return KEY_FEEDBACK_SEMANTIC_NONE;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            key_feedback_semantic_t semantic = key_feedback_semantic_map_get(semantic_map, (keypos_t){.row = row, .col = col});

            if (semantic > best) {
                best = semantic;
            }
        }
    }

    return best;
}

static bool rgb_runtime_key_feedback_stage_paint_half(bool right_half, key_feedback_semantic_t semantic, uint8_t flash_meta, uint8_t led_min, uint8_t led_max) {
    rgb_t color;

    if (!rgb_runtime_key_feedback_stage_semantic_color(semantic, &color) || !rgb_runtime_key_feedback_stage_semantic_visible(semantic, flash_meta)) {
        return false;
    }

    if (right_half) {
        if (!rgb_runtime_key_feedback_stage_led_range_intersects(RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, led_min, led_max)) {
            return false;
        }

        rgb_set_right_half(color, led_min, led_max);
        return true;
    }

    if (!rgb_runtime_key_feedback_stage_led_range_intersects(0, RGB_LEFT_LED_COUNT, led_min, led_max)) {
        return false;
    }

    rgb_set_left_half(color, led_min, led_max);
    return true;
}

static bool rgb_runtime_key_feedback_stage_render_impl(uint8_t led_min, uint8_t led_max) {
    uint8_t                semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t                flash_meta;
    key_feedback_semantic_t left_semantic;
    key_feedback_semantic_t right_semantic;
    key_feedback_semantic_t global_semantic;
    rgb_t                  color;

    rgb_runtime_key_feedback_stage_current_semantic_map(semantic_map);
    flash_meta = rgb_runtime_key_feedback_stage_current_flash_meta();

    if (key_behavior_feedback_colors.mode == KEY_FEEDBACK_MODE_KEY) {
        return rgb_runtime_key_feedback_stage_render_key_mode(semantic_map, flash_meta, led_min, led_max);
    }

    if (key_behavior_feedback_colors.mode == KEY_FEEDBACK_MODE_KEY_HALF) {
        bool painted = false;

        left_semantic  = rgb_runtime_key_feedback_stage_half_semantic(semantic_map, false);
        right_semantic = rgb_runtime_key_feedback_stage_half_semantic(semantic_map, true);
        painted |= rgb_runtime_key_feedback_stage_paint_half(false, left_semantic, flash_meta, led_min, led_max);
        painted |= rgb_runtime_key_feedback_stage_paint_half(true, right_semantic, flash_meta, led_min, led_max);
        return painted;
    }

    global_semantic = rgb_runtime_key_feedback_stage_global_semantic(semantic_map);
    if (key_behavior_feedback_colors.mode == KEY_FEEDBACK_MODE_LEFT_HALF) {
        return rgb_runtime_key_feedback_stage_paint_half(false, global_semantic, flash_meta, led_min, led_max);
    }

    if (key_behavior_feedback_colors.mode == KEY_FEEDBACK_MODE_RIGHT_HALF) {
        return rgb_runtime_key_feedback_stage_paint_half(true, global_semantic, flash_meta, led_min, led_max);
    }

    if (!rgb_runtime_key_feedback_stage_semantic_color(global_semantic, &color) || !rgb_runtime_key_feedback_stage_semantic_visible(global_semantic, flash_meta)) {
        return false;
    }

    rgb_set_both_halves(color, led_min, led_max);
    return led_min < led_max;
}

bool rgb_runtime_key_feedback_stage_render(uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_key_feedback_stage_render_impl(led_min, led_max);
}

#endif
