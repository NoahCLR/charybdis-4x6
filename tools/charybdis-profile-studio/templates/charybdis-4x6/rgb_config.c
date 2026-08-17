// {{PROFILE_TITLE}} RGB configuration.

#include "noah_keymap.h"

#if defined(RGB_MATRIX_ENABLE)

#    define RGB_LED_GROUP_LEFT_THUMB RGB_LED_GROUP(26, 27, 28, 25, 24)
#    define RGB_LED_GROUP_RIGHT_THUMB RGB_LED_GROUP(53, 54, 55)
#    define RGB_LED_GROUP_THUMBS RGB_LED_GROUP(26, 27, 28, 25, 24, 53, 54, 55)
#    define RGB_LED_GROUP_TRACKBALL RGB_LED_GROUP(56)

const layer_color_config_t layer_colors[LAYER_COUNT] = {
    [LAYER_BASE] =
        {
            .color = HSV(0, 0, 0),
            .mode  = ALL_KEYS,
        },
    [LAYER_NUM] =
        {
            .color = HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        },
    [LAYER_SYM] =
        {
            .color = HSV(169, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        },
    [LAYER_NAV] =
        {
            .color = HSV(180, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        },
    [LAYER_POINTER] =
        {
            .color = HSV(0, 0, 150),
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        },
};

static const layer_led_group_t layer_led_groups_data[] = RGB_LED_GROUP_TABLE(
    // { .layer = RGB_LAYER_GROUP_ALL, .color = HSV(0, 0, 0), .led_group = RGB_LED_GROUP_THUMBS },
);

#    ifdef RGB_AUTOMOUSE_GRADIENT_ENABLE
const automouse_fade_end_config_t automouse_fade_end_config = {
    .mode      = FOLLOW_REAL_DESTINATION,
    .end_color = HSV(0, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
};
#    endif // RGB_AUTOMOUSE_GRADIENT_ENABLE

#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
const pd_mode_color_t pd_mode_colors[] = {
    {
        .pointing_mode = PD_MODE_DRAGSCROLL,
        .color         = HSV(21, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
        .locality      = RGB_RIGHT_HALF,
    },
    {
        .pointing_mode = PD_MODE_VOLUME,
        .color         = HSV(43, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
        .locality      = RGB_RIGHT_HALF,
    },
    {
        .pointing_mode = PD_MODE_BRIGHTNESS,
        .color         = HSV(213, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
        .locality      = RGB_RIGHT_HALF,
    },
    {
        .pointing_mode = PD_MODE_ARROW,
        .color         = HSV(127, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
        .locality      = RGB_RIGHT_HALF,
    },
    {
        .pointing_mode = PD_MODE_PINCH,
        .color         = HSV(55, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
        .locality      = RGB_RIGHT_HALF,
    },
    {
        .pointing_mode = PD_MODE_ZOOM,
        .color         = HSV(70, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
        .locality      = RGB_RIGHT_HALF,
    },
};

static const pd_mode_led_group_t pd_mode_led_groups_data[] = RGB_LED_GROUP_TABLE(
    // { .pointing_mode = RGB_PD_MODE_GROUP_ALL, .color = HSV(0, 0, 0), .led_group = RGB_LED_GROUP_THUMBS },
);
#    endif // POINTING_DEVICE_ENABLE && RGB_PD_MODE_FEEDBACK_ENABLE

#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
const combo_feedback_color_config_t combo_feedback_colors = {
    .color    = HSV(191, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    .locality = RGB_KEY_HALF,
};

static const combo_feedback_led_group_t combo_feedback_led_groups_data[] = RGB_LED_GROUP_TABLE(
    // { .color = HSV(0, 0, 0), .led_group = RGB_LED_GROUP_THUMBS },
);
#    endif // COMBO_ENABLE && RGB_COMBO_FEEDBACK_ENABLE

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
const key_behavior_feedback_color_config_t key_behavior_feedback_colors = {
    RGB_TAP_BRANCH_COLORS(HSV(200, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), HSV(180, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), HSV(143, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS)),

    .branch_confirm_mode    = KEY_FEEDBACK_BRANCH_CONFIRM_NON_BASE_TAPS,
    .tap_committed_color    = HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    .tap_commit_mode        = KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS,
    .hold_active_color      = HSV(18, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    .long_hold_active_color = HSV(148, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    .locality               = RGB_KEY_HALF,
};

static const key_behavior_feedback_led_group_t key_behavior_feedback_led_groups_data[] = RGB_LED_GROUP_TABLE(
    // { .semantic = KEY_FEEDBACK_GROUP_ALL, .color = HSV(0, 0, 0), .led_group = RGB_LED_GROUP_THUMBS },
);
#    endif // RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE

MATERIALIZE_RGB_CONFIG();

#endif // RGB_MATRIX_ENABLE
