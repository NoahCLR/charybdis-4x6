#include QMK_KEYBOARD_H

#include <stdio.h>

#include "users/noah/lib/profile/storage/profile_storage_layout.h"

_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_LOGICAL_LAYERS == 8u, "unexpected v1 layer ceiling");
_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_BEHAVIOR_ROWS == 64u, "unexpected v1 behavior-row ceiling");
_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_TAP_STEPS_PER_BEHAVIOR == 5u, "unexpected v1 tap-step ceiling");
_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_POPULATED_BEHAVIOR_STEPS == 128u, "unexpected v1 populated-step ceiling");
_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_COMBOS == 32u && NOAH_PROFILE_WIRE_V1_MAX_KEYS_PER_COMBO == 4u, "unexpected v1 combo ceilings");
_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_REUSABLE_RGB_GROUPS == 16u && NOAH_PROFILE_WIRE_V1_MAX_RGB_STAGE_GROUP_ROWS == 32u, "unexpected v1 RGB-group ceilings");
_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_HARDCODED_MACRO_SLOTS == 16u && NOAH_PROFILE_WIRE_V1_MAX_HARDCODED_MACRO_BYTES == 1024u, "unexpected v1 hardcoded-macro ceilings");

int main(void) {
    if (DYNAMIC_KEYMAP_EEPROM_MAX_ADDR != 0x1FFFu || NOAH_PROFILE_STORAGE_SLOT_A_SIZE != 4096u || NOAH_PROFILE_STORAGE_SLOT_B_SIZE != 4096u || NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX != 4064u || NOAH_PROFILE_STORAGE_VIA_MACRO_SIZE != 7551u || NOAH_PROFILE_WIRE_V1_LED_BITMAP_SIZE != 8u) {
        return 1;
    }

    puts("profile storage layout host tests passed");
    return 0;
}
