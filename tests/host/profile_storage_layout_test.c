#include QMK_KEYBOARD_H

#include <stdio.h>

#include "users/noah/lib/profile/storage/profile_storage_layout.h"

_Static_assert(sizeof(noah_profile_storage_address_t) == 4u, "storage addresses are 32 bits wide");

_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_LOGICAL_LAYERS == 16u, "unexpected layer ceiling");
// The behaviour ceilings follow the one tap depth (D-F14): 128 rows, depth
// steps in a row, rows × depth in all.
_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_BEHAVIOR_ROWS == 128u, "unexpected behavior-row ceiling");
_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_TAP_STEPS_PER_BEHAVIOR == NOAH_PROFILE_TAP_DEPTH, "unexpected tap-step ceiling");
_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_POPULATED_BEHAVIOR_STEPS == 128u * NOAH_PROFILE_TAP_DEPTH, "unexpected populated-step ceiling");
_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_COMBOS == 128u && NOAH_PROFILE_WIRE_V1_MAX_KEYS_PER_COMBO == 16u, "unexpected combo ceilings");
_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_REUSABLE_RGB_GROUPS == 16u && NOAH_PROFILE_WIRE_V1_MAX_RGB_STAGE_GROUP_ROWS == 32u, "unexpected v1 RGB-group ceilings");
_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_CUSTOM_KEYS == 128u && NOAH_PROFILE_WIRE_V1_MAX_HARDCODED_MACRO_BYTES == 1024u, "unexpected custom-key and retired user-macro ceilings");

int main(void) {
    // D-F14: a 36 KiB VIA region, then two 52 KiB slots; slot B ends at the
    // last of 140 KiB of logical EEPROM, past any 16-bit address.
    if (DYNAMIC_KEYMAP_EEPROM_MAX_ADDR != 0x8FFFu || NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR != 0x9000u || NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR != 0x16000u || NOAH_PROFILE_STORAGE_SLOT_B_END_ADDR != 0x22FFFu || NOAH_PROFILE_STORAGE_SLOT_A_SIZE != 53248u || NOAH_PROFILE_STORAGE_SLOT_B_SIZE != 53248u || NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX != 53216u || NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE != 143360u || NOAH_PROFILE_WIRE_V1_LED_BITMAP_SIZE != 8u) {
        return 1;
    }

    puts("profile storage layout host tests passed");
    return 0;
}
