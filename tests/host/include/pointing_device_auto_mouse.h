#pragma once

#include <stdbool.h>
#include <stdint.h>

bool    get_auto_mouse_toggle(void);
int8_t  get_auto_mouse_key_tracker(void);
uint8_t get_auto_mouse_layer(void);
uint16_t auto_mouse_get_time_elapsed(void);
bool     is_auto_mouse_active(void);
void     set_auto_mouse_enable(bool enable);
void     set_auto_mouse_layer(uint8_t layer);
void     auto_mouse_layer_off(void);
void     auto_mouse_toggle(void);
void     auto_mouse_keyevent(bool pressed);
