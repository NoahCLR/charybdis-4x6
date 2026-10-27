// ────────────────────────────────────────────────────────────────────────────
// Live Profile Storage Layout Contract
// ──────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "profile_storage_layout.h"

#include "eeprom.h"
#include "nvm_eeprom_eeconfig_internal.h" // IWYU pragma: keep

// The profile slots have their own flash region (D-F29) and VIA is gone
// (D-F31): QMK's logical EEPROM holds only its eeconfig and datablocks.
_Static_assert(TOTAL_EEPROM_BYTE_COUNT == NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE, "target logical EEPROM no longer matches the accepted 4 KiB eeconfig region");
#ifdef EECONFIG_SIZE
_Static_assert(EECONFIG_SIZE + 256u <= TOTAL_EEPROM_BYTE_COUNT, "logical EEPROM must leave room for a user datablock beyond eeconfig");
#endif

_Static_assert(LAYER_COUNT <= NOAH_PROFILE_WIRE_V1_MAX_LOGICAL_LAYERS, "compiled layer count exceeds Profile Wire capacity");
_Static_assert(KEY_BEHAVIOR_MAX_TAP_COUNT == NOAH_PROFILE_WIRE_V1_MAX_TAP_STEPS_PER_BEHAVIOR, "compiled behavior tap depth no longer matches Profile Wire");

#ifdef RGB_MATRIX_ENABLE
_Static_assert(RGB_MATRIX_LED_COUNT == NOAH_PROFILE_WIRE_V1_MAX_PHYSICAL_LEDS, "compiled LED count no longer matches the first Profile Wire capability set");
#endif
