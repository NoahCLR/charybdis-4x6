#include "rgb_helpers.h"

#if defined(RGB_MATRIX_ENABLE)

__attribute__((weak)) const layer_led_group_t *const layer_led_groups      = 0;
__attribute__((weak)) const uint8_t                  layer_led_group_count = 0;

#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
__attribute__((weak)) const pd_mode_led_group_t *const pd_mode_led_groups      = 0;
__attribute__((weak)) const uint8_t                    pd_mode_led_group_count = 0;
#    endif

#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
__attribute__((weak)) const combo_feedback_led_group_t *const combo_feedback_led_groups      = 0;
__attribute__((weak)) const uint8_t                           combo_feedback_led_group_count = 0;
#    endif

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
__attribute__((weak)) const key_behavior_feedback_led_group_t *const key_behavior_feedback_led_groups      = 0;
__attribute__((weak)) const uint8_t                                  key_behavior_feedback_led_group_count = 0;
#    endif

#endif // RGB_MATRIX_ENABLE
