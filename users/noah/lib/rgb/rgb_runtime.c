// ────────────────────────────────────────────────────────────────────────────
// RGB Runtime
// ────────────────────────────────────────────────────────────────────────────

#include "noah_keymap.h"
#include "rgb_runtime.h"
#include "rgb_config.h"
#include "../pointing/pointing_device_modes.h"
#include "rgb_automouse.h"
#include "rgb_helpers.h"

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE)
#    include "../key/key_runtime_feedback.h"
#    include "../state/runtime_shared_state.h"
#endif

#define PD_MODE_COLOR_COUNT (sizeof(pd_mode_colors) / sizeof(pd_mode_colors[0]))
#define LAYER_LED_GROUP_COUNT (sizeof(layer_led_groups) / sizeof(layer_led_groups[0]))
#define PD_MODE_LED_GROUP_COUNT (sizeof(pd_mode_led_groups) / sizeof(pd_mode_led_groups[0]))

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
static inline bool rgb_feedback_flash_phase_on(void) {
    return ((timer_read() / RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS) & 1u) == 0;
}
#endif

#ifdef RGB_MATRIX_ENABLE
static rgb_t layer_rgb[LAYER_COUNT];
static rgb_t pd_mode_rgb[PD_MODE_COUNT];
static rgb_t led_group_rgb[LAYER_LED_GROUP_COUNT];
static rgb_t pd_mode_led_group_rgb[PD_MODE_LED_GROUP_COUNT];
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
static rgb_t feedback_multi_tap_pending_rgb;
static rgb_t feedback_hold_active_rgb;
static rgb_t feedback_long_hold_active_rgb;
#    endif
#endif

void noah_rgb_runtime_post_init(void) {
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

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    feedback_multi_tap_pending_rgb = hsv_to_rgb(feedback_multi_tap_pending_color);
    feedback_hold_active_rgb       = hsv_to_rgb(feedback_hold_active_color);
    feedback_long_hold_active_rgb  = hsv_to_rgb(feedback_long_hold_active_color);
#    endif
#endif
}

#ifdef RGB_MATRIX_ENABLE
bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    bool layer_painted = false;

    for (int8_t i = LAYER_COUNT - 1; i > 0; i--) {
        if (!layer_state_cmp(layer_state, i)) continue;
#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
        if (i == (int8_t)get_auto_mouse_layer()) continue;
#    endif
        if (layer_colors[i].s == 0 && layer_colors[i].v == 0) continue;
        rgb_set_both_halves(layer_rgb[i], led_min, led_max);
        layer_painted = true;
        break;
    }

#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
    if (!layer_painted && layer_state_cmp(layer_state, get_auto_mouse_layer())) {
        automouse_rgb_render(led_min, led_max, automouse_color_start, automouse_color_end);
        layer_painted = true;
    }
#    endif

    for (uint8_t g = 0; g < LAYER_LED_GROUP_COUNT; g++) {
        if (layer_state_cmp(layer_state, layer_led_groups[g].layer)) {
            rgb_set_led_group(layer_led_groups[g].leds, layer_led_groups[g].count, led_min, led_max, led_group_rgb[g]);
        }
    }

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    // ─── Key behavior feedback ──────────────────────────────────────────
    //
    // Paint both halves to reflect key engine state. Rendered before the
    // pd-mode overlay so an active trackball mode still shows on the right.
    // Master reads live state; slave reads synced semantic flags from the
    // split packet. Effects such as flashing compute phase locally.
    // Multi-tap pending > hold feedback > nothing (priority order).
    uint8_t fb = is_keyboard_master() ? key_feedback_pack() : runtime_shared_state_remote.key_feedback_flags;

    if (key_feedback_flags_multi_tap_pending(fb)) {
        rgb_set_both_halves(feedback_multi_tap_pending_rgb, led_min, led_max);
    } else if (key_feedback_flags_hold_active(fb)) {
        if (!key_feedback_flags_level_flash(fb) || rgb_feedback_flash_phase_on()) {
            if (key_feedback_flags_long_hold_active(fb)) {
                rgb_set_both_halves(feedback_long_hold_active_rgb, led_min, led_max);
            } else {
                rgb_set_both_halves(feedback_hold_active_rgb, led_min, led_max);
            }
        }
    } else if (key_feedback_flags_hold_pending(fb)) {
        rgb_set_both_halves(feedback_hold_active_rgb, led_min, led_max);
    }
#    endif

    // ─── Pointing-device mode overlay ───────────────────────────────────
    //
    // Paints after feedback so the right half still shows the active mode
    // color when a trackball mode is running.
    uint8_t active_mode = pd_mode_first_active_index();
    if (active_mode < PD_MODE_COUNT) {
        rgb_set_right_half(pd_mode_rgb[active_mode], led_min, led_max);
    }

    for (uint8_t g = 0; g < PD_MODE_LED_GROUP_COUNT; g++) {
        if (pd_mode_active(pd_mode_led_groups[g].mode_flag)) {
            rgb_set_led_group(pd_mode_led_groups[g].leds, pd_mode_led_groups[g].count, led_min, led_max, pd_mode_led_group_rgb[g]);
        }
    }

    return layer_painted;
}
#else
bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    (void)led_min;
    (void)led_max;
    return true;
}
#endif // RGB_MATRIX_ENABLE
