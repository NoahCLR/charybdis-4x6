#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "profile_versions.h"

// Domain 0x60 version 1: the keymap, one little-endian 16-bit QMK keycode per
// matrix position, layer by layer, each layer row by row (position = row ×
// columns + column). No header: the length is exactly layers × positions × 2,
// 1,920 bytes for this keyboard's 16 layers of 60 positions. A keycode means
// what it means in QMK; which ones a key may hold is the firmware's to say, so
// a candidate's keycodes go to the runtime's placement check (KEYMAP).
enum {
    NOAH_PROFILE_KEYMAP_V1_VERSION     = NOAH_PROFILE_DOMAIN_VERSION_KEYMAP,
    NOAH_PROFILE_KEYMAP_V1_MAX_LAYERS  = 16u,
    NOAH_PROFILE_KEYMAP_V1_KEYCODE_SIZE = 2u,
    // Ten keycodes a step, inside the validator's 20-byte read.
    NOAH_PROFILE_KEYMAP_V1_STEP_BYTES  = 20u,
};

typedef struct {
    uint32_t offset;
} noah_profile_keymap_v1_validation_t;

static inline uint32_t noah_profile_keymap_v1_length(uint8_t layers, uint8_t positions) {
    return (uint32_t)layers * positions * NOAH_PROFILE_KEYMAP_V1_KEYCODE_SIZE;
}

// The byte offset of one keycode inside the domain.
static inline uint32_t noah_profile_keymap_v1_offset(uint8_t layer, uint8_t position, uint8_t positions) {
    return ((uint32_t)layer * positions + position) * NOAH_PROFILE_KEYMAP_V1_KEYCODE_SIZE;
}

bool noah_profile_keymap_v1_length_valid(uint32_t length, uint8_t layers, uint8_t positions);
