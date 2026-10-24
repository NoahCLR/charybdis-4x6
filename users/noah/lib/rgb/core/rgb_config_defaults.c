#include "rgb_helpers.h"

#if defined(RGB_MATRIX_ENABLE)

// Weak defaults for the keymap's RGB tables, apart from every reader: GCC
// folds a weak const object's initializer into the file that defines it
// (tests/host/weak_const_default_test.py).
__attribute__((weak)) const rgb_led_group_t *const saved_led_groups      = 0;
__attribute__((weak)) const uint8_t                saved_led_group_count = 0;

__attribute__((weak)) const layer_led_group_t *const layer_led_groups      = 0;
__attribute__((weak)) const uint8_t                  layer_led_group_count = 0;

#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_POINTING_MODE_FEEDBACK_ENABLE)
__attribute__((weak)) const pointing_mode_led_group_t *const pointing_mode_led_groups      = 0;
__attribute__((weak)) const uint8_t                          pointing_mode_led_group_count = 0;
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
