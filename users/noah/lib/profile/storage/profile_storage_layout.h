// ───────────────────────────────────────────────────────────────────────────
// Live Profile Storage Layout
// ──────────────────────────────────────────────────────────────────────────
//
// Storage geometry (D-F29, replacing D-F14's EEPROM slots). This contract
// provides no flash access, slot selection, validation or commit behavior.
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

// Storage addresses are offsets into the profile slot region (D-F29), and
// payload offsets and lengths inside a slot are 32 bits wide (D-F28).
typedef uint32_t noah_profile_storage_address_t;

// The profile slots have a flash region of their own (D-F29): two 128 KiB
// slots, read in place through XIP and never through QMK's emulated EEPROM.
// The region starts 1 MiB into flash, between the end of the firmware image
// (checked at link time, qmk_profile_flash.ld) and the wear-leveling backing
// at the top of the 2 MiB the build assumes.
#define NOAH_PROFILE_STORAGE_FLASH_SIZE 0x200000u
#define NOAH_PROFILE_STORAGE_FLASH_OFFSET 0x100000u
#define NOAH_PROFILE_STORAGE_SECTOR_SIZE 4096u
#define NOAH_PROFILE_STORAGE_PAGE_SIZE 256u
#define NOAH_PROFILE_STORAGE_SLOT_SIZE 0x20000u
#define NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR 0x00000u
#define NOAH_PROFILE_STORAGE_SLOT_A_END_ADDR 0x1FFFFu
#define NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR 0x20000u
#define NOAH_PROFILE_STORAGE_SLOT_B_END_ADDR 0x3FFFFu
#define NOAH_PROFILE_STORAGE_REGION_SIZE 0x40000u
#define NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE 36u
#define NOAH_PROFILE_STORAGE_COMMIT_MARKER_SIZE 1u
#define NOAH_PROFILE_STORAGE_SLOT_A_SIZE (NOAH_PROFILE_STORAGE_SLOT_A_END_ADDR - NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + 1u)
#define NOAH_PROFILE_STORAGE_SLOT_B_SIZE (NOAH_PROFILE_STORAGE_SLOT_B_END_ADDR - NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + 1u)

// Storage addresses are the store's view of a slot: its 36-byte header, the
// commit marker as the header's last byte, then the payload. On flash no
// 256-byte page is programmed twice between erases, so each part a save
// programs on its own has a page of its own in the slot's first sector:
// page 0 the header, pages 1 to 3 the marker's three cells (the marker reads
// as their bitwise AND, each step programs the next cell), and the payload
// from page 4 (profile_flash_store.h).
#define NOAH_PROFILE_STORAGE_SLOT_MARKER_CELLS 3u
#define NOAH_PROFILE_STORAGE_SLOT_MARKER_CELL_FLASH_OFFSET 0x100u
#define NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_FLASH_OFFSET 0x400u
#define NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX (NOAH_PROFILE_STORAGE_SLOT_SIZE - NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_FLASH_OFFSET)

// QMK's logical EEPROM is the VIA region alone: 41 bytes of QMK and VIA
// configuration, the dynamic keymap and the VIA macro bank.
#define NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE 0x9000u
#define NOAH_PROFILE_STORAGE_VIA_REGION_SIZE 0x9000u
#define NOAH_PROFILE_STORAGE_DYNAMIC_KEYMAP_START_ADDR 0x0029u
#define NOAH_PROFILE_STORAGE_DYNAMIC_KEYMAP_SIZE 1920u
#define NOAH_PROFILE_STORAGE_VIA_MACRO_START_ADDR 0x07A9u
#define NOAH_PROFILE_STORAGE_VIA_MACRO_SIZE 34903u

_Static_assert(NOAH_PROFILE_STORAGE_SLOT_A_SIZE == NOAH_PROFILE_STORAGE_SLOT_SIZE && NOAH_PROFILE_STORAGE_SLOT_B_SIZE == NOAH_PROFILE_STORAGE_SLOT_SIZE, "both slots have the one slot size");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_A_END_ADDR + 1u == NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR, "slot B follows slot A with no gap");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_B_END_ADDR + 1u == NOAH_PROFILE_STORAGE_REGION_SIZE, "the region is exactly the two slots");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_SIZE % NOAH_PROFILE_STORAGE_SECTOR_SIZE == 0u && NOAH_PROFILE_STORAGE_FLASH_OFFSET % NOAH_PROFILE_STORAGE_SECTOR_SIZE == 0u, "slots start and end on erase sectors");
_Static_assert(NOAH_PROFILE_STORAGE_SECTOR_SIZE % NOAH_PROFILE_STORAGE_PAGE_SIZE == 0u, "a sector is whole program pages");
_Static_assert(NOAH_PROFILE_STORAGE_FLASH_OFFSET + NOAH_PROFILE_STORAGE_REGION_SIZE <= NOAH_PROFILE_STORAGE_FLASH_SIZE, "the slot region lies inside the assumed flash");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE <= NOAH_PROFILE_STORAGE_PAGE_SIZE, "a slot header fits its own page");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_MARKER_CELL_FLASH_OFFSET == NOAH_PROFILE_STORAGE_PAGE_SIZE && NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_FLASH_OFFSET == (1u + NOAH_PROFILE_STORAGE_SLOT_MARKER_CELLS) * NOAH_PROFILE_STORAGE_PAGE_SIZE, "header, marker cells and payload each start a page of their own");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_FLASH_OFFSET <= NOAH_PROFILE_STORAGE_SECTOR_SIZE, "erasing a slot's first sector clears its header and every marker cell");
_Static_assert(NOAH_PROFILE_STORAGE_COMMIT_MARKER_SIZE <= NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE, "commit marker must fit inside the canonical slot header");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX == NOAH_PROFILE_PAYLOAD_MAX, "the schema's payload ceiling is one slot's payload");
_Static_assert(NOAH_PROFILE_WIRE_V1_LED_BITMAP_SIZE == 8u, "58 physical LEDs must use an eight-byte canonical bitmap");

#ifdef VIA_ENABLE
#    ifndef DYNAMIC_KEYMAP_EEPROM_MAX_ADDR
#        error "DYNAMIC_KEYMAP_EEPROM_MAX_ADDR must end VIA storage at the logical EEPROM boundary"
#    endif
_Static_assert(DYNAMIC_KEYMAP_EEPROM_MAX_ADDR + 1u == NOAH_PROFILE_STORAGE_VIA_REGION_SIZE, "VIA storage must end at the accepted 36 KiB boundary");
_Static_assert(NOAH_PROFILE_STORAGE_VIA_REGION_SIZE == NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE, "logical EEPROM is the VIA region alone");
#endif
