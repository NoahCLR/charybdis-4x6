// ───────────────────────────────────────────────────────────────────────────
// Live Profile Storage Layout
// ──────────────────────────────────────────────────────────────────────────
//
// Storage geometry (D-F14). This contract intentionally provides no EEPROM
// access, slot selection, validation, or commit behavior.
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>

#include "../schema/profile_versions.h"

#define NOAH_PROFILE_WIRE_V1_MAX_LOGICAL_LAYERS 16u
#define NOAH_PROFILE_WIRE_V1_MAX_BEHAVIOR_ROWS NOAH_PROFILE_BEHAVIOR_ROWS
#define NOAH_PROFILE_WIRE_V1_MAX_TAP_STEPS_PER_BEHAVIOR NOAH_PROFILE_TAP_DEPTH
#define NOAH_PROFILE_WIRE_V1_MAX_POPULATED_BEHAVIOR_STEPS (NOAH_PROFILE_BEHAVIOR_ROWS * NOAH_PROFILE_TAP_DEPTH)
#define NOAH_PROFILE_WIRE_V1_MAX_COMBOS 128u
#define NOAH_PROFILE_WIRE_V1_MAX_KEYS_PER_COMBO 16u
#define NOAH_PROFILE_WIRE_V1_MAX_REUSABLE_RGB_GROUPS 16u
#define NOAH_PROFILE_WIRE_V1_MAX_RGB_STAGE_GROUP_ROWS 32u
#define NOAH_PROFILE_WIRE_V1_MAX_PHYSICAL_LEDS 58u
#define NOAH_PROFILE_WIRE_V1_LED_BITMAP_SIZE ((NOAH_PROFILE_WIRE_V1_MAX_PHYSICAL_LEDS + 7u) / 8u)
#define NOAH_PROFILE_WIRE_V1_MAX_CUSTOM_KEYS 128u
#define NOAH_PROFILE_WIRE_V1_MAX_HARDCODED_MACRO_BYTES 1024u
// Every stored name: a length byte, then at most this many UTF-8 bytes.
#define NOAH_PROFILE_WIRE_V1_MAX_NAME_BYTES 32u
// Wire layer masks are 32 bits; bits at or above the bank must be zero.
#define NOAH_PROFILE_WIRE_V1_LAYER_MASK_BITS 32u

// Logical EEPROM addresses reach 0x22FFF, so storage addresses are 32 bits
// wide; payload offsets and lengths inside one slot still fit 16 bits.
typedef uint32_t noah_profile_storage_address_t;

#define NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE 0x23000u
#define NOAH_PROFILE_STORAGE_VIA_REGION_SIZE 0x9000u
#define NOAH_PROFILE_STORAGE_EXPECTED_SLOT_SIZE 0xD000u
#define NOAH_PROFILE_STORAGE_DYNAMIC_KEYMAP_START_ADDR 0x0029u
#define NOAH_PROFILE_STORAGE_DYNAMIC_KEYMAP_SIZE 1920u
#define NOAH_PROFILE_STORAGE_VIA_MACRO_START_ADDR 0x07A9u
#define NOAH_PROFILE_STORAGE_VIA_MACRO_SIZE 34903u
#define NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE 32u
#define NOAH_PROFILE_STORAGE_COMMIT_MARKER_SIZE 1u

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

_Static_assert(DYNAMIC_KEYMAP_EEPROM_MAX_ADDR + 1u == NOAH_PROFILE_STORAGE_VIA_REGION_SIZE, "VIA storage must end at the accepted 36 KiB boundary");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR == NOAH_PROFILE_STORAGE_VIA_REGION_SIZE && NOAH_PROFILE_STORAGE_SLOT_A_END_ADDR == NOAH_PROFILE_STORAGE_VIA_REGION_SIZE + NOAH_PROFILE_STORAGE_EXPECTED_SLOT_SIZE - 1u, "live-profile slot A size does not match schema geometry");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR == NOAH_PROFILE_STORAGE_VIA_REGION_SIZE + NOAH_PROFILE_STORAGE_EXPECTED_SLOT_SIZE && NOAH_PROFILE_STORAGE_SLOT_B_END_ADDR == NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE - 1u, "live-profile slot B size does not match schema geometry");
_Static_assert(DYNAMIC_KEYMAP_EEPROM_MAX_ADDR < NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, "VIA storage overlaps live-profile slot A");
_Static_assert(DYNAMIC_KEYMAP_EEPROM_MAX_ADDR + 1u == NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, "unowned gap between VIA storage and live-profile slot A");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_A_END_ADDR < NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR, "live-profile slots overlap");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_A_END_ADDR + 1u == NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR, "unowned gap between live-profile slots");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_B_END_ADDR + 1u == NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE, "live-profile slot B must end at logical EEPROM boundary");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_A_SIZE == NOAH_PROFILE_STORAGE_EXPECTED_SLOT_SIZE && NOAH_PROFILE_STORAGE_SLOT_B_SIZE == NOAH_PROFILE_STORAGE_EXPECTED_SLOT_SIZE, "each live-profile slot must match the selected schema geometry");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX == NOAH_PROFILE_STORAGE_EXPECTED_SLOT_SIZE - 32u, "32-byte slot header must leave the selected payload capacity");
_Static_assert(NOAH_PROFILE_STORAGE_COMMIT_MARKER_SIZE <= NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE, "commit marker must fit inside the canonical slot header");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX <= UINT16_MAX, "profile payload length must fit the wire/storage uint16 field");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_B_END_ADDR <= UINT32_MAX, "storage addresses fit noah_profile_storage_address_t");
_Static_assert(NOAH_PROFILE_WIRE_V1_LED_BITMAP_SIZE == 8u, "58 physical LEDs must use an eight-byte canonical bitmap");
#endif
