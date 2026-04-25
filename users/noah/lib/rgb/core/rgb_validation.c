// ────────────────────────────────────────────────────────────────────────────
// RGB Authored Config Validation
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_validation.h"

#if defined(RGB_MATRIX_ENABLE)

#    ifdef CONSOLE_ENABLE
#        include "print.h"
#    endif

#    include "rgb_helpers.h"

extern const layer_color_config_t     layer_colors[];
extern const layer_led_group_t *const layer_led_groups;
extern const uint8_t                  layer_led_group_count;

#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
extern const automouse_fade_end_config_t automouse_fade_end_config;
#    endif

#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
extern const combo_feedback_color_config_t combo_feedback_colors;
extern const combo_feedback_led_group_t *const combo_feedback_led_groups;
extern const uint8_t                          combo_feedback_led_group_count;
#    endif

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
extern const key_behavior_feedback_color_config_t key_behavior_feedback_colors;
extern const key_behavior_feedback_led_group_t *const key_behavior_feedback_led_groups;
extern const uint8_t                                  key_behavior_feedback_led_group_count;
#    endif

#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
#        include "../../pointing/defs/pd_modes.h"

extern const pd_mode_color_t            pd_mode_colors[];
extern const uint8_t                    pd_mode_color_count;
extern const pd_mode_led_group_t *const pd_mode_led_groups;
extern const uint8_t                    pd_mode_led_group_count;

static bool rgb_validation_pd_mode_known(pd_mode_mask_t mode) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].mode_flag == mode) {
            return true;
        }
    }

    return false;
}

static uint8_t rgb_validation_pd_mode_color_match_count(pd_mode_mask_t mode) {
    uint8_t matches = 0;

    for (uint8_t i = 0; i < pd_mode_color_count; i++) {
        if (pd_mode_colors[i].pointing_mode == mode) {
            matches++;
        }
    }

    return matches;
}
#    endif

static void rgb_validation_log_invalid_layer_led_group_layer(uint8_t group_index, uint8_t layer) {
#    ifdef CONSOLE_ENABLE
    uprintf("Invalid layer_led_groups[%u].layer %u; expected a layer in 0..%u\n", (unsigned int)group_index, (unsigned int)layer, (unsigned int)(LAYER_COUNT - 1u));
#    else
    (void)group_index;
    (void)layer;
#    endif
}

static void rgb_validation_log_invalid_layer_led_index(const char *group_kind, uint8_t group_index, uint8_t led_index, uint8_t led) {
#    ifdef CONSOLE_ENABLE
    uprintf("Invalid %s[%u].led_group.leds[%u] LED index %u; expected a value below RGB_MATRIX_LED_COUNT (%u)\n", group_kind, (unsigned int)group_index, (unsigned int)led_index, (unsigned int)led, (unsigned int)RGB_MATRIX_LED_COUNT);
#    else
    (void)group_kind;
    (void)group_index;
    (void)led_index;
    (void)led;
#    endif
}

static void rgb_validation_log_invalid_layer_color_mode(uint8_t layer, uint8_t mode) {
#    ifdef CONSOLE_ENABLE
    uprintf("Invalid layer_colors[%u].mode %u; expected ALL_KEYS (0) or KEYS_MAPPED_ON_THIS_LAYER_ONLY (1)\n", (unsigned int)layer, (unsigned int)mode);
#    else
    (void)layer;
    (void)mode;
#    endif
}

#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
static void rgb_validation_log_invalid_automouse_fade_end_mode(uint8_t mode) {
#        ifdef CONSOLE_ENABLE
    uprintf("Invalid automouse_fade_end_config.mode %u; expected FOLLOW_REAL_DESTINATION (0), END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW (1), or END_COLOR_ON_ALL_KEYS (2)\n", (unsigned int)mode);
#        else
    (void)mode;
#        endif
}
#    endif

#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
static void rgb_validation_log_invalid_combo_feedback_locality(uint8_t locality) {
#        ifdef CONSOLE_ENABLE
    uprintf("Invalid combo_feedback_colors.locality %u; expected RGB_BOTH_HALVES (0), RGB_LEFT_HALF (1), RGB_RIGHT_HALF (2), RGB_KEY_HALF (3), or RGB_KEYS_ONLY (4)\n", (unsigned int)locality);
#        else
    (void)locality;
#        endif
}
#    endif

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
static void rgb_validation_log_invalid_key_behavior_feedback_locality(uint8_t locality) {
#        ifdef CONSOLE_ENABLE
    uprintf("Invalid key_behavior_feedback_colors.locality %u; expected RGB_BOTH_HALVES (0), RGB_LEFT_HALF (1), RGB_RIGHT_HALF (2), RGB_KEY_HALF (3), or RGB_KEYS_ONLY (4)\n", (unsigned int)locality);
#        else
    (void)locality;
#        endif
}

static void rgb_validation_log_invalid_key_behavior_feedback_tap_commit_mode(uint8_t mode) {
#        ifdef CONSOLE_ENABLE
    uprintf("Invalid key_behavior_feedback_colors.tap_commit_mode %u; expected KEY_FEEDBACK_TAP_COMMIT_OFF (0), KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS (1), or KEY_FEEDBACK_TAP_COMMIT_ALL_TAPS (2)\n", (unsigned int)mode);
#        else
    (void)mode;
#        endif
}

static void rgb_validation_log_invalid_key_behavior_feedback_tap_pending_mode(uint8_t mode) {
#        ifdef CONSOLE_ENABLE
    uprintf("Invalid key_behavior_feedback_colors.tap_pending_mode %u; expected KEY_FEEDBACK_TAP_PENDING_SINGLE_COLOR (0) or KEY_FEEDBACK_TAP_PENDING_BRANCH_COLORS (1)\n", (unsigned int)mode);
#        else
    (void)mode;
#        endif
}

static void rgb_validation_log_invalid_key_behavior_feedback_tap_pending_colors(void) {
#        ifdef CONSOLE_ENABLE
    uprintf("Invalid key_behavior_feedback_colors tap-pending colors; RGB_TAP_PENDING_COLORS(...) must provide at least one color\n");
#        endif
}

static void rgb_validation_log_invalid_key_behavior_feedback_group_semantic(uint8_t group_index, uint8_t semantic) {
#        ifdef CONSOLE_ENABLE
    uprintf("Invalid key_behavior_feedback_led_groups[%u].semantic %u; expected KEY_FEEDBACK_GROUP_MULTI_TAP_PENDING (0), KEY_FEEDBACK_GROUP_TAP_COMMITTED (1), KEY_FEEDBACK_GROUP_HOLD_ACTIVE (2), or KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE (3)\n", (unsigned int)group_index, (unsigned int)semantic);
#        else
    (void)group_index;
    (void)semantic;
#        endif
}
#    endif

#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
static void rgb_validation_log_unknown_pd_mode_color(uint8_t color_index, pd_mode_mask_t mode) {
#        ifdef CONSOLE_ENABLE
    uprintf("Unknown pd_mode_colors[%u].pointing_mode 0x%04X; entry does not match any registered pd mode\n", (unsigned int)color_index, (unsigned int)mode);
#        else
    (void)color_index;
    (void)mode;
#        endif
}

static void rgb_validation_log_invalid_pd_mode_color_locality(uint8_t color_index, uint8_t locality) {
#        ifdef CONSOLE_ENABLE
    uprintf("Invalid pd_mode_colors[%u].locality %u; expected RGB_BOTH_HALVES (0), RGB_LEFT_HALF (1), RGB_RIGHT_HALF (2), RGB_KEY_HALF (3), or RGB_KEYS_ONLY (4)\n", (unsigned int)color_index, (unsigned int)locality);
#        else
    (void)color_index;
    (void)locality;
#        endif
}

#        ifndef RGB_PD_MODE_ACTIVE_HALF_ENABLE
static void rgb_validation_log_pd_mode_key_locality_requires_feature(uint8_t color_index, pd_mode_mask_t mode) {
#            ifdef CONSOLE_ENABLE
    uprintf("pd_mode_colors[%u].locality uses key-local PD RGB placement for pd mode 0x%04X, but RGB_PD_MODE_ACTIVE_HALF_ENABLE is disabled\n", (unsigned int)color_index, (unsigned int)mode);
#            else
    (void)color_index;
    (void)mode;
#            endif
}
#        endif

static void rgb_validation_log_duplicate_pd_mode_color(pd_mode_mask_t mode) {
#        ifdef CONSOLE_ENABLE
    uprintf("Duplicate pd_mode_colors entries for pd mode 0x%04X; first-match lookup makes later rows unreachable\n", (unsigned int)mode);
#        else
    (void)mode;
#        endif
}

static void rgb_validation_log_missing_pd_mode_color(pd_mode_mask_t mode, uint16_t keycode) {
#        ifdef CONSOLE_ENABLE
    uprintf("Missing pd_mode_colors entry for pd mode 0x%04X (keycode 0x%04X); the active mode overlay will fall back to black\n", (unsigned int)mode, (unsigned int)keycode);
#        else
    (void)mode;
    (void)keycode;
#        endif
}

static void rgb_validation_log_unknown_pd_mode_led_group(uint8_t group_index, pd_mode_mask_t mode) {
#        ifdef CONSOLE_ENABLE
    uprintf("Unknown pd_mode_led_groups[%u].pointing_mode 0x%04X; entry does not match any registered pd mode\n", (unsigned int)group_index, (unsigned int)mode);
#        else
    (void)group_index;
    (void)mode;
#        endif
}
#    endif

static void rgb_validation_validate_layer_colors(void) {
    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        if (layer_colors[layer].mode > KEYS_MAPPED_ON_THIS_LAYER_ONLY) {
            rgb_validation_log_invalid_layer_color_mode(layer, layer_colors[layer].mode);
        }
    }
}

static void rgb_validation_validate_layer_led_groups(void) {
    for (uint8_t group_index = 0; group_index < layer_led_group_count; group_index++) {
        const layer_led_group_t *group = &layer_led_groups[group_index];

        if (group->layer >= LAYER_COUNT) {
            rgb_validation_log_invalid_layer_led_group_layer(group_index, group->layer);
        }

        const rgb_led_group_t *led_group = &group->led_group;
        for (uint8_t led_index = 0; led_index < led_group->count; led_index++) {
            if (led_group->leds[led_index] >= RGB_MATRIX_LED_COUNT) {
                rgb_validation_log_invalid_layer_led_index("layer_led_groups", group_index, led_index, led_group->leds[led_index]);
            }
        }
    }
}

#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
static void rgb_validation_validate_automouse_fade_end_config(void) {
    if (automouse_fade_end_config.mode > END_COLOR_ON_ALL_KEYS) {
        rgb_validation_log_invalid_automouse_fade_end_mode((uint8_t)automouse_fade_end_config.mode);
    }
}
#    endif

#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
static void rgb_validation_validate_combo_feedback_config(void) {
    if (combo_feedback_colors.locality > RGB_KEYS_ONLY) {
        rgb_validation_log_invalid_combo_feedback_locality((uint8_t)combo_feedback_colors.locality);
    }
}

static void rgb_validation_validate_combo_feedback_led_groups(void) {
    for (uint8_t group_index = 0; group_index < combo_feedback_led_group_count; group_index++) {
        const combo_feedback_led_group_t *group = &combo_feedback_led_groups[group_index];

        const rgb_led_group_t *led_group = &group->led_group;
        for (uint8_t led_index = 0; led_index < led_group->count; led_index++) {
            if (led_group->leds[led_index] >= RGB_MATRIX_LED_COUNT) {
                rgb_validation_log_invalid_layer_led_index("combo_feedback_led_groups", group_index, led_index, led_group->leds[led_index]);
            }
        }
    }
}
#    endif

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
static void rgb_validation_validate_key_behavior_feedback_config(void) {
    if (key_behavior_feedback_colors.locality > RGB_KEYS_ONLY) {
        rgb_validation_log_invalid_key_behavior_feedback_locality((uint8_t)key_behavior_feedback_colors.locality);
    }
    if (key_behavior_feedback_colors.tap_commit_mode > KEY_FEEDBACK_TAP_COMMIT_ALL_TAPS) {
        rgb_validation_log_invalid_key_behavior_feedback_tap_commit_mode((uint8_t)key_behavior_feedback_colors.tap_commit_mode);
    }
    if (key_behavior_feedback_colors.tap_pending_mode > KEY_FEEDBACK_TAP_PENDING_BRANCH_COLORS) {
        rgb_validation_log_invalid_key_behavior_feedback_tap_pending_mode((uint8_t)key_behavior_feedback_colors.tap_pending_mode);
    }
    if (!key_behavior_feedback_colors.tap_pending_colors || key_behavior_feedback_colors.tap_pending_color_count == 0u) {
        rgb_validation_log_invalid_key_behavior_feedback_tap_pending_colors();
    }
}

static void rgb_validation_validate_key_behavior_feedback_led_groups(void) {
    for (uint8_t group_index = 0; group_index < key_behavior_feedback_led_group_count; group_index++) {
        const key_behavior_feedback_led_group_t *group    = &key_behavior_feedback_led_groups[group_index];
        uint8_t                                  semantic = (uint8_t)group->semantic;

        if (semantic > KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE) {
            rgb_validation_log_invalid_key_behavior_feedback_group_semantic(group_index, semantic);
        }

        const rgb_led_group_t *led_group = &group->led_group;
        for (uint8_t led_index = 0; led_index < led_group->count; led_index++) {
            if (led_group->leds[led_index] >= RGB_MATRIX_LED_COUNT) {
                rgb_validation_log_invalid_layer_led_index("key_behavior_feedback_led_groups", group_index, led_index, led_group->leds[led_index]);
            }
        }
    }
}
#    endif

#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
static void rgb_validation_validate_pd_mode_colors(void) {
    for (uint8_t color_index = 0; color_index < pd_mode_color_count; color_index++) {
        pd_mode_mask_t mode     = pd_mode_colors[color_index].pointing_mode;
        uint8_t        locality = (uint8_t)pd_mode_colors[color_index].locality;

        if (!rgb_validation_pd_mode_known(mode)) {
            rgb_validation_log_unknown_pd_mode_color(color_index, mode);
        }

        if (locality > RGB_KEYS_ONLY) {
            rgb_validation_log_invalid_pd_mode_color_locality(color_index, locality);
            continue;
        }

#        ifndef RGB_PD_MODE_ACTIVE_HALF_ENABLE
        if (locality == RGB_KEY_HALF || locality == RGB_KEYS_ONLY) {
            rgb_validation_log_pd_mode_key_locality_requires_feature(color_index, mode);
        }
#        endif
    }

    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        uint8_t matches = rgb_validation_pd_mode_color_match_count(pd_modes[i].mode_flag);

        if (matches == 0) {
            rgb_validation_log_missing_pd_mode_color(pd_modes[i].mode_flag, pd_modes[i].keycode);
        } else if (matches > 1) {
            rgb_validation_log_duplicate_pd_mode_color(pd_modes[i].mode_flag);
        }
    }
}

static void rgb_validation_validate_pd_mode_led_groups(void) {
    for (uint8_t group_index = 0; group_index < pd_mode_led_group_count; group_index++) {
        const pd_mode_led_group_t *group = &pd_mode_led_groups[group_index];

        if (!rgb_validation_pd_mode_known(group->pointing_mode)) {
            rgb_validation_log_unknown_pd_mode_led_group(group_index, group->pointing_mode);
        }

        const rgb_led_group_t *led_group = &group->led_group;
        for (uint8_t led_index = 0; led_index < led_group->count; led_index++) {
            if (led_group->leds[led_index] >= RGB_MATRIX_LED_COUNT) {
                rgb_validation_log_invalid_layer_led_index("pd_mode_led_groups", group_index, led_index, led_group->leds[led_index]);
            }
        }
    }
}
#    endif

void noah_rgb_validate_config(void) {
    rgb_validation_validate_layer_colors();
    rgb_validation_validate_layer_led_groups();

#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
    rgb_validation_validate_automouse_fade_end_config();
#    endif

#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    rgb_validation_validate_combo_feedback_config();
    rgb_validation_validate_combo_feedback_led_groups();
#    endif

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    rgb_validation_validate_key_behavior_feedback_config();
    rgb_validation_validate_key_behavior_feedback_led_groups();
#    endif

#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
    rgb_validation_validate_pd_mode_colors();
    rgb_validation_validate_pd_mode_led_groups();
#    endif
}

#else

void noah_rgb_validate_config(void) {}

#endif // defined(RGB_MATRIX_ENABLE)
