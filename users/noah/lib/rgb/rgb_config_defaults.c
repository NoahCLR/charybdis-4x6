#include "rgb_helpers.h"

#if defined(RGB_MATRIX_ENABLE)

__attribute__((weak)) const layer_led_group_t *const layer_led_groups      = 0;
__attribute__((weak)) const uint8_t                  layer_led_group_count = 0;

#    ifdef POINTING_DEVICE_ENABLE
__attribute__((weak)) const pd_mode_led_group_t *const pd_mode_led_groups      = 0;
__attribute__((weak)) const uint8_t                    pd_mode_led_group_count = 0;
#    endif

#endif // RGB_MATRIX_ENABLE
