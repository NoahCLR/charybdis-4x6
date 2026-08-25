// ────────────────────────────────────────────────────────────────────────────
// Live Profile Storage Layout Contract
// ──────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#ifdef VIA_ENABLE

#    include "profile_storage_layout.h"

#    include "eeprom.h"
#    include "nvm_eeprom_eeconfig_internal.h" // IWYU pragma: keep
#    include "nvm_eeprom_via_internal.h"

#    ifdef ENCODER_MAP_ENABLE
#        include "encoder.h"
#        define NOAH_PROFILE_STORAGE_ENCODER_SIZE ((uint32_t)DYNAMIC_KEYMAP_LAYER_COUNT * (uint32_t)NUM_ENCODERS * 2u * 2u)
#    else
#        define NOAH_PROFILE_STORAGE_ENCODER_SIZE 0u
#    endif

#    define NOAH_PROFILE_STORAGE_COMPILED_KEYMAP_SIZE ((uint32_t)DYNAMIC_KEYMAP_LAYER_COUNT * (uint32_t)MATRIX_ROWS * (uint32_t)MATRIX_COLS * 2u)
#    define NOAH_PROFILE_STORAGE_COMPILED_VIA_MACRO_START ((uint32_t)VIA_EEPROM_CONFIG_END + NOAH_PROFILE_STORAGE_COMPILED_KEYMAP_SIZE + NOAH_PROFILE_STORAGE_ENCODER_SIZE)
#    define NOAH_PROFILE_STORAGE_COMPILED_VIA_MACRO_SIZE ((uint32_t)DYNAMIC_KEYMAP_EEPROM_MAX_ADDR - NOAH_PROFILE_STORAGE_COMPILED_VIA_MACRO_START + 1u)

_Static_assert(TOTAL_EEPROM_BYTE_COUNT == NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE, "target logical EEPROM no longer matches the accepted 16 KiB profile partition");
_Static_assert(VIA_EEPROM_CONFIG_END == NOAH_PROFILE_STORAGE_DYNAMIC_KEYMAP_START_ADDR, "VIA config end moved the accepted dynamic-keymap start");
_Static_assert(NOAH_PROFILE_STORAGE_COMPILED_KEYMAP_SIZE == NOAH_PROFILE_STORAGE_DYNAMIC_KEYMAP_SIZE, "compiled dynamic keymap no longer occupies the accepted 600-byte region");
_Static_assert(NOAH_PROFILE_STORAGE_COMPILED_VIA_MACRO_START == NOAH_PROFILE_STORAGE_VIA_MACRO_START_ADDR, "compiled VIA macro start no longer matches the accepted address map");
_Static_assert(NOAH_PROFILE_STORAGE_COMPILED_VIA_MACRO_SIZE == NOAH_PROFILE_STORAGE_VIA_MACRO_SIZE, "compiled VIA macro capacity no longer matches the accepted 7,551-byte budget");
_Static_assert(DYNAMIC_KEYMAP_LAYER_COUNT <= NOAH_PROFILE_WIRE_V1_MAX_LOGICAL_LAYERS, "compiled layer count exceeds Profile Wire v1 capacity");
_Static_assert(KEY_BEHAVIOR_MAX_TAP_COUNT == NOAH_PROFILE_WIRE_V1_MAX_TAP_STEPS_PER_BEHAVIOR, "compiled behavior tap depth no longer matches Profile Wire v1");

#    ifdef RGB_MATRIX_ENABLE
_Static_assert(RGB_MATRIX_LED_COUNT == NOAH_PROFILE_WIRE_V1_MAX_PHYSICAL_LEDS, "compiled LED count no longer matches the first Profile Wire v1 capability set");
#    endif

#    undef NOAH_PROFILE_STORAGE_COMPILED_VIA_MACRO_SIZE
#    undef NOAH_PROFILE_STORAGE_COMPILED_VIA_MACRO_START
#    undef NOAH_PROFILE_STORAGE_COMPILED_KEYMAP_SIZE
#    undef NOAH_PROFILE_STORAGE_ENCODER_SIZE

#endif
