#pragma once

#include <stdbool.h>
#include <stdint.h>

bool    get_auto_mouse_toggle(void);
int8_t  get_auto_mouse_key_tracker(void);
uint8_t get_auto_mouse_layer(void);
uint16_t auto_mouse_get_time_elapsed(void);
