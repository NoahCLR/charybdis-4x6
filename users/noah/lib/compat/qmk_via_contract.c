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
            return NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_RGB | NOAH_QMK_VIA_COMMAND_EFFECT_RESEED_MACROS | NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR | NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_MACROS;
#    endif
        case id_dynamic_keymap_set_keycode:
        case id_dynamic_keymap_set_buffer:
        case id_dynamic_keymap_reset:
            return NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_RGB | NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR;
        case id_dynamic_keymap_set_encoder:
            return NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR;
        case id_dynamic_keymap_macro_reset:
            return NOAH_QMK_VIA_COMMAND_EFFECT_RESEED_MACROS | NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR | NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_MACROS;
        case id_dynamic_keymap_macro_set_buffer:
            return NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR | NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_MACROS;
        default:
            return NOAH_QMK_VIA_COMMAND_EFFECT_NONE;
    }
}

bool noah_qmk_via_classify_mutation(const uint8_t *data, uint8_t length, uint8_t *out_effects) {
    size_t capacity;
    size_t offset;
    size_t payload_size;

    if (out_effects) {
        *out_effects = NOAH_QMK_VIA_COMMAND_EFFECT_NONE;
    }
    if (!data || !out_effects || length == 0u) {
        return false;
    }

    switch (data[0]) {
        case id_dynamic_keymap_set_keycode:
            if (length < 6u || data[1] >= DYNAMIC_KEYMAP_LAYER_COUNT || data[2] >= MATRIX_ROWS || data[3] >= MATRIX_COLS) {
                return false;
            }
            break;
        case id_dynamic_keymap_set_buffer:
        case id_dynamic_keymap_macro_set_buffer:
            if (length < 4u) {
                return false;
            }
            offset       = ((size_t)data[1] << 8u) | data[2];
            payload_size = data[3];
            capacity     = data[0] == id_dynamic_keymap_set_buffer ? noah_qmk_via_keymap_buffer_capacity() : noah_qmk_via_macro_seed_capacity();
            // Upstream applies these buffer writes byte by byte, keeping every
            // byte whose offset lands inside the region and silently dropping
            // the rest. An over-long or partly out-of-range write therefore
            // still mutates storage, and classifying it as a non-mutation left
            // the digest advertising pre-write content while the halves
            // diverged. Only a write starting past the region touches nothing.
            (void)payload_size;
            if (offset >= capacity) {
                return false;
            }
            break;
        case id_dynamic_keymap_reset:
        case id_dynamic_keymap_macro_reset:
#    ifdef VIA_EEPROM_ALLOW_RESET
        case id_eeprom_reset:
#    endif
            break;
#    ifdef ENCODER_MAP_ENABLE
        case id_dynamic_keymap_set_encoder:
            if (length < 6u || data[1] >= DYNAMIC_KEYMAP_LAYER_COUNT || data[2] >= NUM_ENCODERS) {
                return false;
            }
            break;
#    endif
        case id_set_keyboard_value:
            if (length < 6u || data[1] != id_layout_options) {
                return false;
            }
            *out_effects = NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_RGB | NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR;
            return true;
        default:
            return false;
    }

    *out_effects = noah_qmk_via_command_effects(data[0]);
    return *out_effects != NOAH_QMK_VIA_COMMAND_EFFECT_NONE;
}

#endif
