// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Contract Compatibility
// ────────────────────────────────────────────────────────────────────────────

#include "qmk_via_storage_contract.h"

#ifdef VIA_ENABLE

#    include "dynamic_keymap.h"
#    include "eeprom.h"
#    include "nvm_eeprom_eeconfig_internal.h" // IWYU pragma: keep
#    include "nvm_eeprom_via_internal.h"
#    include "via.h"
#    ifdef ENCODER_MAP_ENABLE
#        include "encoder.h"
#    endif

#    ifndef DYNAMIC_KEYMAP_EEPROM_MAX_ADDR
#        define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR (TOTAL_EEPROM_BYTE_COUNT - 1)
#    endif

#    ifndef DYNAMIC_KEYMAP_EEPROM_ADDR
#        define DYNAMIC_KEYMAP_EEPROM_ADDR (VIA_EEPROM_CONFIG_END)
#    endif

#    ifndef DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR
#        define DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR (DYNAMIC_KEYMAP_EEPROM_ADDR + (DYNAMIC_KEYMAP_LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2))
#    endif

#    ifdef ENCODER_MAP_ENABLE
#        ifndef DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR
#            define DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR (DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR + (DYNAMIC_KEYMAP_LAYER_COUNT * NUM_ENCODERS * 2 * 2))
#        endif
#    else
#        ifndef DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR
#            define DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR (DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR)
#        endif
#    endif

#    ifndef DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE
#        define DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE (DYNAMIC_KEYMAP_EEPROM_MAX_ADDR - DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR + 1)
#    endif

#    define NOAH_QMK_VIA_KEYMAP_BUFFER_CAPACITY ((uint32_t)DYNAMIC_KEYMAP_LAYER_COUNT * (uint32_t)MATRIX_ROWS * (uint32_t)MATRIX_COLS * 2u)

_Static_assert(NOAH_QMK_VIA_KEYMAP_BUFFER_CAPACITY <= UINT16_MAX, "VIA dynamic-keymap buffer must fit the 16-bit offset contract");

uint16_t noah_qmk_via_keymap_buffer_capacity(void) {
    return (uint16_t)NOAH_QMK_VIA_KEYMAP_BUFFER_CAPACITY;
}

#    undef NOAH_QMK_VIA_KEYMAP_BUFFER_CAPACITY

uint16_t noah_qmk_via_macro_seed_capacity(void) {
    uint16_t capacity = dynamic_keymap_macro_get_buffer_size();

    if (capacity == 0 || capacity > DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE) {
        return 0;
    }

    return capacity;
}

void noah_qmk_via_macro_set_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    dynamic_keymap_macro_set_buffer(offset, size, data);
}

bool noah_qmk_via_should_seed_defaults_post_init(void) {
    return !via_eeprom_is_valid();
}

uint8_t noah_qmk_via_command_effects(uint8_t command_id) {
    switch (command_id) {
#    ifdef VIA_EEPROM_ALLOW_RESET
        case id_eeprom_reset:
            return NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_RGB | NOAH_QMK_VIA_COMMAND_EFFECT_RESEED_MACROS | NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR;
#    endif
        case id_dynamic_keymap_set_keycode:
        case id_dynamic_keymap_set_buffer:
        case id_dynamic_keymap_reset:
            return NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_RGB | NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR;
        case id_dynamic_keymap_set_encoder:
            return NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR;
        case id_dynamic_keymap_macro_reset:
            return NOAH_QMK_VIA_COMMAND_EFFECT_RESEED_MACROS;
        default:
            return NOAH_QMK_VIA_COMMAND_EFFECT_NONE;
    }
}

#endif
