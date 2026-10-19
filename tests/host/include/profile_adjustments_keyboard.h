#pragma once
#include "portable_profile/portable_profile_keyboard.h"
#define MOD_MASK_SHIFT 0x22u
#define SNIPING_MODE_TOGGLE (QK_KB_0 + 5u)
void rgb_matrix_enable_noeeprom(void);
void rgb_matrix_disable_noeeprom(void);
void rgb_matrix_toggle_noeeprom(void);
void rgb_matrix_step_noeeprom(void);
void rgb_matrix_step_reverse_noeeprom(void);
void rgb_matrix_increase_hue_noeeprom(void);
void rgb_matrix_decrease_hue_noeeprom(void);
void rgb_matrix_increase_sat_noeeprom(void);
void rgb_matrix_decrease_sat_noeeprom(void);
void rgb_matrix_increase_val_noeeprom(void);
void rgb_matrix_decrease_val_noeeprom(void);
void rgb_matrix_increase_speed_noeeprom(void);
void rgb_matrix_decrease_speed_noeeprom(void);
void rgb_matrix_flags_step_noeeprom(void);
void rgb_matrix_flags_step_reverse_noeeprom(void);
