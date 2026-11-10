#pragma once
#include <stdbool.h>
#include <stdint.h>
bool noah_profile_macro_start(uint16_t action);
// Whether noah_profile_macro_start would play the macro now; nothing is sent.
bool noah_profile_macro_can_play(uint16_t action);
bool noah_profile_macro_try_play(uint16_t action);
