// ────────────────────────────────────────────────────────────────────────────
// RGB Key Feedback Stage
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_key_feedback_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE)

#    include "rgb_helpers.h"
#    include "../key/key_runtime_feedback.h"
#    include "../state/split_runtime_sync.h"

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

bool rgb_runtime_key_feedback_stage_render(uint8_t led_min, uint8_t led_max) {
    uint8_t fb = is_keyboard_master() ? key_feedback_pack() : split_runtime_sync_remote.key_feedback_flags;

    if (key_feedback_flags_multi_tap_pending(fb)) {
        rgb_set_both_halves(key_behavior_feedback_multi_tap_pending_rgb, led_min, led_max);
        return true;
    }

    if (key_feedback_flags_hold_active(fb)) {
        if (!key_feedback_flags_level_flash(fb) || key_feedback_flags_flash_phase(fb)) {
            if (key_feedback_flags_long_hold_active(fb)) {
                rgb_set_both_halves(key_behavior_feedback_long_hold_active_rgb, led_min, led_max);
            } else {
                rgb_set_both_halves(key_behavior_feedback_hold_active_rgb, led_min, led_max);
            }
            return true;
        }

        return false;
    }

    if (key_feedback_flags_hold_pending(fb)) {
        rgb_set_both_halves(key_behavior_feedback_hold_active_rgb, led_min, led_max);
        return true;
    }

    return false;
}

#endif
