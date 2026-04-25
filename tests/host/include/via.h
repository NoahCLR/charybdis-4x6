#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    id_eeprom_reset               = 0x0A,
    id_dynamic_keymap_set_keycode = 0x05,
    id_dynamic_keymap_set_buffer  = 0x13,
    id_dynamic_keymap_reset       = 0x06,
    id_dynamic_keymap_macro_reset = 0x10,
    id_dynamic_keymap_set_encoder = 0x15,
};

bool via_eeprom_is_valid(void);
void via_eeprom_set_valid(bool valid);
void eeconfig_init_via(void);
