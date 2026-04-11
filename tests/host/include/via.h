#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    id_eeprom_reset               = 1,
    id_dynamic_keymap_set_keycode = 2,
    id_dynamic_keymap_set_buffer  = 3,
    id_dynamic_keymap_reset       = 4,
    id_dynamic_keymap_macro_reset = 5,
};

bool via_eeprom_is_valid(void);
