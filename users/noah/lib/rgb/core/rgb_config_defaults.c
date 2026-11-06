#include "rgb_config_helpers.h"

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

// Room for the compiled lighting of empty tables; MATERIALIZE_RGB_CONFIG
// sizes the keymap's own (EXPORT_RGB_COMPILED_CACHE).
static uint8_t                       default_compiled_cache[RGB_COMPILED_CACHE_BYTES(0u, 0u, 0u, 0u, 0u)];
__attribute__((weak)) uint8_t *const noah_rgb_compiled_cache      = default_compiled_cache;
__attribute__((weak)) const uint16_t noah_rgb_compiled_cache_size = (uint16_t)sizeof(default_compiled_cache);

#endif // RGB_MATRIX_ENABLE
