// ────────────────────────────────────────────────────────────────────────────
// RGB Runtime
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_runtime.h"
#include "rgb_automouse.h"
#include "rgb_automouse_stage.h"
#include "rgb_helpers.h"
#include "rgb_layer_stage.h"
#include "rgb_pd_mode_stage.h"
#include "rgb_preview_stage.h"
#include "rgb_validation.h"

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE)
#    include "../key/key_runtime_feedback.h"
#    include "../state/split_runtime_sync.h"
#endif

// ─── Authored keymap data (defined in rgb_config.c) ──────────────────────
#ifdef RGB_MATRIX_ENABLE
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
extern const key_behavior_feedback_color_config_t key_behavior_feedback_colors;
#    endif
#endif

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
#    if RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS <= 0
#        error "RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS must be greater than zero"
#    endif
#endif

#ifdef RGB_MATRIX_ENABLE
static rgb_runtime_frame_t rgb_runtime_frame_primary;
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
static rgb_t key_behavior_feedback_multi_tap_pending_rgb;
static rgb_t key_behavior_feedback_hold_active_rgb;
static rgb_t key_behavior_feedback_long_hold_active_rgb;
#    endif
#endif

void noah_rgb_runtime_invalidate_layer_maps(void) {
#ifdef RGB_MATRIX_ENABLE
    rgb_runtime_layer_stage_invalidate_maps();
#endif
}

void noah_rgb_runtime_post_init(void) {
#ifdef RGB_MATRIX_ENABLE
    noah_rgb_validate_config();
    rgb_runtime_layer_stage_post_init();
    rgb_runtime_automouse_stage_post_init();
    rgb_runtime_pd_mode_stage_post_init();

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    key_behavior_feedback_multi_tap_pending_rgb = hsv_to_rgb(key_behavior_feedback_colors.multi_tap_pending_color);
    key_behavior_feedback_hold_active_rgb       = hsv_to_rgb(key_behavior_feedback_colors.hold_active_color);
    key_behavior_feedback_long_hold_active_rgb  = hsv_to_rgb(key_behavior_feedback_colors.long_hold_active_color);
#    endif
#endif
}

#ifdef RGB_MATRIX_ENABLE
bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    bool painted = false;

#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
    // The synthetic automouse destination is not a persistent board state.
    // Once this branch stops running, the next frame falls back to ordinary
    // layer rendering below.
    if (rgb_runtime_automouse_stage_should_render(layer_state)) {
        painted |= rgb_runtime_automouse_stage_render(layer_state, led_min, led_max);
    } else
#    endif
    {
        rgb_runtime_layer_stage_render_frame(&rgb_runtime_frame_primary, layer_state, led_min, led_max);
        painted |= rgb_runtime_layer_stage_apply_frame(&rgb_runtime_frame_primary, led_min, led_max);
    }

    painted |= rgb_runtime_preview_stage_render(led_min, led_max);

    // ─── Pointing-device mode overlay ───────────────────────────────────
    //
    // Paint active pointer-mode color and groups, then let interaction
    // feedback paint over that when needed.
    painted |= rgb_runtime_pd_mode_stage_render(led_min, led_max);

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    // ─── Key behavior feedback ──────────────────────────────────────────
    //
    // Paint last so threshold/flash feedback remains visible even on the
    // trackball half while a pd mode is active.
    // Master reads live state; slave reads the synced feedback flags from the
    // split packet, including the flash-phase bit used to keep both halves in
    // lockstep.
    // Multi-tap pending > hold feedback > nothing (priority order).
    uint8_t fb = is_keyboard_master() ? key_feedback_pack() : split_runtime_sync_remote.key_feedback_flags;

    if (key_feedback_flags_multi_tap_pending(fb)) {
        rgb_set_both_halves(key_behavior_feedback_multi_tap_pending_rgb, led_min, led_max);
        painted = true;
    } else if (key_feedback_flags_hold_active(fb)) {
        if (!key_feedback_flags_level_flash(fb) || key_feedback_flags_flash_phase(fb)) {
            if (key_feedback_flags_long_hold_active(fb)) {
                rgb_set_both_halves(key_behavior_feedback_long_hold_active_rgb, led_min, led_max);
            } else {
                rgb_set_both_halves(key_behavior_feedback_hold_active_rgb, led_min, led_max);
            }
            painted = true;
        }
    } else if (key_feedback_flags_hold_pending(fb)) {
        rgb_set_both_halves(key_behavior_feedback_hold_active_rgb, led_min, led_max);
        painted = true;
    }
#    endif

    return painted;
}
#else
bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    (void)led_min;
    (void)led_max;
    return true;
}
#endif // RGB_MATRIX_ENABLE
