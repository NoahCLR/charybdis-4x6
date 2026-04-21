// ────────────────────────────────────────────────────────────────────────────
// RGB Key Feedback Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_key_feedback_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE)

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

static uint8_t rgb_runtime_key_feedback_stage_current_side(void) {
    return is_keyboard_master() ? key_feedback_side() : split_runtime_sync_remote.key_feedback_side;
}

static bool rgb_runtime_key_feedback_stage_paint(rgb_t color, uint8_t led_min, uint8_t led_max) {
    if (key_behavior_feedback_colors.mode == KEY_FEEDBACK_MODE_KEY_HALF) {
        uint8_t side = rgb_runtime_key_feedback_stage_current_side();

        if (side == KEY_FEEDBACK_SIDE_LEFT) {
            if (!rgb_runtime_key_feedback_stage_led_range_intersects(0, RGB_LEFT_LED_COUNT, led_min, led_max)) {
                return false;
            }

            rgb_set_left_half(color, led_min, led_max);
            return true;
        }

        if (side == KEY_FEEDBACK_SIDE_RIGHT) {
            if (!rgb_runtime_key_feedback_stage_led_range_intersects(RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, led_min, led_max)) {
                return false;
            }

            rgb_set_right_half(color, led_min, led_max);
            return true;
        }
    }

    rgb_set_both_halves(color, led_min, led_max);
    return led_min < led_max;
}

bool rgb_runtime_key_feedback_stage_render(uint8_t led_min, uint8_t led_max) {
    uint8_t fb = is_keyboard_master() ? key_feedback_pack() : split_runtime_sync_remote.key_feedback_flags;

    if (key_feedback_flags_multi_tap_pending(fb)) {
        return rgb_runtime_key_feedback_stage_paint(key_behavior_feedback_multi_tap_pending_rgb, led_min, led_max);
    }

    if (key_feedback_flags_hold_active(fb)) {
        if (!key_feedback_flags_level_flash(fb) || key_feedback_flags_flash_phase(fb)) {
            if (key_feedback_flags_long_hold_active(fb)) {
                return rgb_runtime_key_feedback_stage_paint(key_behavior_feedback_long_hold_active_rgb, led_min, led_max);
            } else {
                return rgb_runtime_key_feedback_stage_paint(key_behavior_feedback_hold_active_rgb, led_min, led_max);
            }
        }

        return false;
    }

    if (key_feedback_flags_hold_pending(fb)) {
        return rgb_runtime_key_feedback_stage_paint(key_behavior_feedback_hold_active_rgb, led_min, led_max);
    }

    return false;
}

#endif
