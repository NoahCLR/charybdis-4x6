#pragma once

#include <stdint.h>

uint8_t  dynamic_keymap_macro_get_count(void);
uint16_t dynamic_keymap_macro_get_buffer_size(void);
void     dynamic_keymap_macro_get_buffer(uint16_t offset, uint16_t size, uint8_t *data);
void     dynamic_keymap_macro_set_buffer(uint16_t offset, uint16_t size, uint8_t *data);
