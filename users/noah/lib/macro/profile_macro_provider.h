#pragma once
#include <stdbool.h>
#include <stdint.h>
bool noah_profile_macro_start(uint16_t action);
// The same macro played as a correction's replacement (D-F69): its output
// isolated and the presses meanwhile left to the typing stage.
bool noah_profile_macro_start_replacement(uint16_t action);
// Whether noah_profile_macro_start would play the macro now; nothing is sent.
bool noah_profile_macro_can_play(uint16_t action);
bool noah_profile_macro_try_play(uint16_t action);
