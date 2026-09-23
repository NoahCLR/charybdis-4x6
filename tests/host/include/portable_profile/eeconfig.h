#pragma once
// Host shadow of QMK's eeconfig.h for the portable-profile readback test.
#include <stdint.h>
uint32_t eeconfig_read_user(void);
void     eeconfig_update_default_layer(uint8_t layers);
