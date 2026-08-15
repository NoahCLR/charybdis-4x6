#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef VIA_EEPROM_LAYOUT_OPTIONS_SIZE
#    define VIA_EEPROM_LAYOUT_OPTIONS_SIZE 1
#endif

enum {
    id_set_keyboard_value              = 0x03,
    id_eeprom_reset                    = 0x0A,
    id_dynamic_keymap_set_keycode      = 0x05,
    id_dynamic_keymap_set_buffer       = 0x13,
    id_dynamic_keymap_reset            = 0x06,
    id_dynamic_keymap_macro_reset      = 0x10,
    id_dynamic_keymap_macro_set_buffer = 0x0F,
    id_dynamic_keymap_set_encoder      = 0x15,
    id_layout_options                  = 0x02,
};

bool     via_eeprom_is_valid(void);
void     via_eeprom_set_valid(bool valid);
uint32_t via_get_layout_options(void);
void     via_set_layout_options(uint32_t value);
void     eeconfig_init_via(void);
