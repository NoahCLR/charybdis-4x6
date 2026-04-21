#pragma once

#include <stdbool.h>
#include <stdint.h>

void     dynamic_keymap_set_keycode(uint8_t layer, uint8_t row, uint8_t column, uint16_t keycode);
void     dynamic_keymap_set_buffer(uint16_t offset, uint16_t size, uint8_t *data);
void     dynamic_keymap_reset(void);
#ifdef ENCODER_MAP_ENABLE
void     dynamic_keymap_set_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise, uint16_t keycode);
#endif
uint8_t  dynamic_keymap_macro_get_count(void);
uint16_t dynamic_keymap_macro_get_buffer_size(void);
void     dynamic_keymap_macro_get_buffer(uint16_t offset, uint16_t size, uint8_t *data);
void     dynamic_keymap_macro_set_buffer(uint16_t offset, uint16_t size, uint8_t *data);
void     dynamic_keymap_macro_reset(void);
