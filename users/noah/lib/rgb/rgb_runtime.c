// ────────────────────────────────────────────────────────────────────────────
// RGB Runtime
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_runtime.h"
#include "rgb_automouse.h"
#include "rgb_automouse_stage.h"
#include "rgb_helpers.h"
#include "rgb_layer_stage.h"
#include "rgb_validation.h"

#if defined(POINTING_DEVICE_ENABLE)
#    include "../pointing/pd_modes.h"
#endif
#if defined(RGB_MATRIX_ENABLE) && defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE)
#    include "../key/key_runtime_feedback.h"
#    include "../state/split_runtime_sync.h"
#endif

// ─── Authored keymap data (defined in rgb_config.c) ──────────────────────
#ifdef RGB_MATRIX_ENABLE
extern const layer_led_group_t *const layer_led_groups;
extern const uint8_t                  layer_led_group_count;
#    ifdef POINTING_DEVICE_ENABLE
extern const pd_mode_color_t            pd_mode_colors[];
extern const uint8_t                    pd_mode_color_count;
extern const pd_mode_led_group_t *const pd_mode_led_groups;
extern const uint8_t                    pd_mode_led_group_count;
#    endif
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
#    ifdef POINTING_DEVICE_ENABLE
static rgb_t pd_mode_rgb[PD_MODE_COUNT];
#    endif
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

#    ifdef POINTING_DEVICE_ENABLE
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        for (uint8_t c = 0; c < pd_mode_color_count; c++) {
            if (pd_mode_colors[c].pointing_mode == pd_modes[i].mode_flag) {
                pd_mode_rgb[i] = hsv_to_rgb(pd_mode_colors[c].color);
                break;
            }
        }
    }
#    endif

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    key_behavior_feedback_multi_tap_pending_rgb = hsv_to_rgb(key_behavior_feedback_colors.multi_tap_pending_color);
    key_behavior_feedback_hold_active_rgb       = hsv_to_rgb(key_behavior_feedback_colors.hold_active_color);
    key_behavior_feedback_long_hold_active_rgb  = hsv_to_rgb(key_behavior_feedback_colors.long_hold_active_color);
#    endif
#endif
}

#ifdef RGB_MATRIX_ENABLE
#    ifdef POINTING_DEVICE_ENABLE
static bool rgb_runtime_led_range_intersects(uint8_t led_min, uint8_t led_max, uint8_t from, uint8_t to) {
    return led_min < to && led_max > from;
}
#    endif

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
static uint8_t rgb_runtime_preview_layer(void) {
    return is_keyboard_master() ? key_feedback_preview_layer() : split_runtime_sync_remote.key_preview_layer;
}
#    endif

#    if defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE) || defined(POINTING_DEVICE_ENABLE)
static bool rgb_runtime_led_group_intersects(const uint8_t *leds, uint8_t count, uint8_t led_min, uint8_t led_max) {
    for (uint8_t i = 0; i < count; i++) {
        if (leds[i] >= led_min && leds[i] < led_max) {
            return true;
        }
    }

    return false;
}
#    endif

bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    bool painted = false;

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    uint8_t preview_layer = rgb_runtime_preview_layer();
#    endif

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

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    if (preview_layer < LAYER_COUNT && rgb_runtime_layer_stage_has_solid_color(preview_layer)) {
        painted |= rgb_runtime_layer_stage_paint_layer(preview_layer, led_min, led_max);

        for (uint8_t g = 0; g < layer_led_group_count; g++) {
            if (layer_led_groups[g].layer != preview_layer) {
                continue;
            }

            rgb_t grp_rgb = hsv_to_rgb(layer_led_groups[g].color);
            rgb_set_led_group(layer_led_groups[g].leds, layer_led_groups[g].count, led_min, led_max, grp_rgb);
            painted |= rgb_runtime_led_group_intersects(layer_led_groups[g].leds, layer_led_groups[g].count, led_min, led_max);
        }
    }
#    endif

    // ─── Pointing-device mode overlay ───────────────────────────────────
    //
    // Paint active pointer-mode color and groups, then let interaction
    // feedback paint over that when needed.
#    ifdef POINTING_DEVICE_ENABLE
    uint8_t active_mode = pd_mode_first_active_index();
    if (active_mode < PD_MODE_COUNT) {
        rgb_set_right_half(pd_mode_rgb[active_mode], led_min, led_max);
        painted |= rgb_runtime_led_range_intersects(led_min, led_max, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT);
    }

    for (uint8_t g = 0; g < pd_mode_led_group_count; g++) {
        if (pd_mode_active(pd_mode_led_groups[g].pointing_mode)) {
            rgb_t grp_rgb = hsv_to_rgb(pd_mode_led_groups[g].color);
            rgb_set_led_group(pd_mode_led_groups[g].leds, pd_mode_led_groups[g].count, led_min, led_max, grp_rgb);
            painted |= rgb_runtime_led_group_intersects(pd_mode_led_groups[g].leds, pd_mode_led_groups[g].count, led_min, led_max);
        }
    }
#    endif

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
