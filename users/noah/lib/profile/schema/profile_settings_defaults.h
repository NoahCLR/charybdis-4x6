#pragma once
#include "profile_settings_v1.h"
// Immutable authored factory settings, independent of mutable QMK owners.
uint32_t noah_profile_settings_default(uint8_t id);
uint16_t noah_profile_settings_defaults_length(void);
uint8_t  noah_profile_settings_defaults_byte(uint16_t offset);
