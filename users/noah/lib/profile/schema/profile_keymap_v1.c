#include "profile_keymap_v1.h"

bool noah_profile_keymap_v1_length_valid(uint32_t length, uint8_t layers, uint8_t positions) {
    return layers != 0u && layers <= NOAH_PROFILE_KEYMAP_V1_MAX_LAYERS && positions != 0u && length == noah_profile_keymap_v1_length(layers, positions);
}
