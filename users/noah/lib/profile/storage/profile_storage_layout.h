// ───────────────────────────────────────────────────────────────────────────
// Live Profile Storage Layout
// ──────────────────────────────────────────────────────────────────────────
//
// Frozen Stage 00 bounds only. This contract intentionally provides no EEPROM
// access, slot selection, validation, or commit behavior.
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>

#define NOAH_PROFILE_WIRE_V1_MAX_LOGICAL_LAYERS 8u
#define NOAH_PROFILE_WIRE_V1_MAX_BEHAVIOR_ROWS 64u
#define NOAH_PROFILE_WIRE_V1_MAX_TAP_STEPS_PER_BEHAVIOR 5u
#define NOAH_PROFILE_WIRE_V1_MAX_POPULATED_BEHAVIOR_STEPS 128u
#define NOAH_PROFILE_WIRE_V1_MAX_COMBOS 32u
#define NOAH_PROFILE_WIRE_V1_MAX_KEYS_PER_COMBO 4u
#define NOAH_PROFILE_WIRE_V1_MAX_REUSABLE_RGB_GROUPS 16u
#define NOAH_PROFILE_WIRE_V1_MAX_RGB_STAGE_GROUP_ROWS 32u
#define NOAH_PROFILE_WIRE_V1_MAX_PHYSICAL_LEDS 58u
#define NOAH_PROFILE_WIRE_V1_LED_BITMAP_SIZE ((NOAH_PROFILE_WIRE_V1_MAX_PHYSICAL_LEDS + 7u) / 8u)
#define NOAH_PROFILE_WIRE_V1_MAX_HARDCODED_MACRO_SLOTS 16u
#define NOAH_PROFILE_WIRE_V1_MAX_HARDCODED_MACRO_BYTES 1024u

#define NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE 0x4000u
#define NOAH_PROFILE_STORAGE_DYNAMIC_KEYMAP_START_ADDR 0x0029u
#ifdef NOAH_LEGACY_SNAPSHOT_BRIDGE
#    define NOAH_PROFILE_STORAGE_DYNAMIC_KEYMAP_SIZE 600u
#    define NOAH_PROFILE_STORAGE_VIA_MACRO_START_ADDR 0x0281u
#    define NOAH_PROFILE_STORAGE_VIA_MACRO_SIZE 7551u
#else
#define NOAH_PROFILE_STORAGE_DYNAMIC_KEYMAP_SIZE 960u
#define NOAH_PROFILE_STORAGE_VIA_MACRO_START_ADDR 0x03E9u
#define NOAH_PROFILE_STORAGE_VIA_MACRO_SIZE 7191u
#endif
#define NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE 32u
#define NOAH_PROFILE_STORAGE_COMMIT_MARKER_SIZE 2u

#ifdef VIA_ENABLE
#    ifndef DYNAMIC_KEYMAP_EEPROM_MAX_ADDR
#        error "DYNAMIC_KEYMAP_EEPROM_MAX_ADDR must reserve the live-profile slots"
#    endif
#    ifndef NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR
#        error "live-profile slot A start address is not configured"
#    endif
#    ifndef NOAH_PROFILE_STORAGE_SLOT_A_END_ADDR
#        error "live-profile slot A end address is not configured"
#    endif
#    ifndef NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR
#        error "live-profile slot B start address is not configured"
#    endif
#    ifndef NOAH_PROFILE_STORAGE_SLOT_B_END_ADDR
#        error "live-profile slot B end address is not configured"
#    endif

#    define NOAH_PROFILE_STORAGE_SLOT_A_SIZE (NOAH_PROFILE_STORAGE_SLOT_A_END_ADDR - NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + 1u)
#    define NOAH_PROFILE_STORAGE_SLOT_B_SIZE (NOAH_PROFILE_STORAGE_SLOT_B_END_ADDR - NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + 1u)
#    define NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX (NOAH_PROFILE_STORAGE_SLOT_A_SIZE - NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE)

_Static_assert(DYNAMIC_KEYMAP_EEPROM_MAX_ADDR == 0x1FFFu, "VIA storage must end at the accepted 0x1FFF boundary");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR == 0x2000u && NOAH_PROFILE_STORAGE_SLOT_A_END_ADDR == 0x2FFFu, "live-profile slot A must remain 0x2000-0x2FFF");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR == 0x3000u && NOAH_PROFILE_STORAGE_SLOT_B_END_ADDR == 0x3FFFu, "live-profile slot B must remain 0x3000-0x3FFF");
_Static_assert(DYNAMIC_KEYMAP_EEPROM_MAX_ADDR < NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, "VIA storage overlaps live-profile slot A");
_Static_assert(DYNAMIC_KEYMAP_EEPROM_MAX_ADDR + 1u == NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, "unowned gap between VIA storage and live-profile slot A");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_A_END_ADDR < NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR, "live-profile slots overlap");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_A_END_ADDR + 1u == NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR, "unowned gap between live-profile slots");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_B_END_ADDR + 1u == NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE, "live-profile slot B must end at logical EEPROM boundary");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_A_SIZE == 4096u && NOAH_PROFILE_STORAGE_SLOT_B_SIZE == 4096u, "each live-profile slot must be exactly 4 KiB");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX == 4064u, "32-byte slot header must leave a 4,064-byte payload");
_Static_assert(NOAH_PROFILE_STORAGE_COMMIT_MARKER_SIZE <= NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE, "commit marker must fit inside the canonical slot header");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX <= UINT16_MAX, "profile payload length must fit the wire/storage uint16 field");
_Static_assert(NOAH_PROFILE_WIRE_V1_LED_BITMAP_SIZE == 8u, "58 physical LEDs must use an eight-byte canonical bitmap");
#endif
