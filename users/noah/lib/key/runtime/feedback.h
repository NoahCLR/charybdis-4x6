// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Feedback
// ────────────────────────────────────────────────────────────────────────────
//
// Read-only runtime-visible feedback surfaces for RGB and split sync.
//
// This module now exposes three distinct surfaces:
// - preview layer display state
// - combo RGB locality (underlay vs overlay)
// - authored key-behavior feedback truth as packed semantic and tap-branch maps
//
// Exact per-key truth is produced on the master half. Split sync mirrors the
// already-computed combo locality bitmaps plus packed key-feedback maps so the
// slave renders the same scene without direct access to the key runtime.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "feedback_kind.h"
#include "origin_registry.h"

#ifndef KEY_FEEDBACK_PREVIEW_DISPLAY_BRIDGE_MS
#    define KEY_FEEDBACK_PREVIEW_DISPLAY_BRIDGE_MS 20
#endif

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS
#    define KEY_FEEDBACK_FLASH_HALF_PERIOD_MS RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS
#else
#    define KEY_FEEDBACK_FLASH_HALF_PERIOD_MS 200
#endif

#define KEY_FEEDBACK_SEMANTIC_BITS 3u
#define KEY_FEEDBACK_SEMANTIC_MAP_SIZE ((((MATRIX_ROWS * MATRIX_COLS) * KEY_FEEDBACK_SEMANTIC_BITS) + 7u) / 8u)
#define KEY_FEEDBACK_SEMANTIC_MASK ((uint16_t)((1u << KEY_FEEDBACK_SEMANTIC_BITS) - 1u))

#if KEY_BEHAVIOR_MAX_TAP_COUNT == 0u
#    error "KEY_BEHAVIOR_MAX_TAP_COUNT must be greater than zero"
#elif KEY_BEHAVIOR_MAX_TAP_COUNT > 255u
#    error "KEY_BEHAVIOR_MAX_TAP_COUNT must fit in uint8_t"
#elif KEY_BEHAVIOR_MAX_TAP_COUNT <= 1u
#    define KEY_FEEDBACK_TAP_BRANCH_BITS 1u
#elif KEY_BEHAVIOR_MAX_TAP_COUNT <= 3u
#    define KEY_FEEDBACK_TAP_BRANCH_BITS 2u
#elif KEY_BEHAVIOR_MAX_TAP_COUNT <= 7u
#    define KEY_FEEDBACK_TAP_BRANCH_BITS 3u
#elif KEY_BEHAVIOR_MAX_TAP_COUNT <= 15u
#    define KEY_FEEDBACK_TAP_BRANCH_BITS 4u
#elif KEY_BEHAVIOR_MAX_TAP_COUNT <= 31u
#    define KEY_FEEDBACK_TAP_BRANCH_BITS 5u
#elif KEY_BEHAVIOR_MAX_TAP_COUNT <= 63u
#    define KEY_FEEDBACK_TAP_BRANCH_BITS 6u
#elif KEY_BEHAVIOR_MAX_TAP_COUNT <= 127u
#    define KEY_FEEDBACK_TAP_BRANCH_BITS 7u
#else
#    define KEY_FEEDBACK_TAP_BRANCH_BITS 8u
#endif

#define KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE ((((MATRIX_ROWS * MATRIX_COLS) * KEY_FEEDBACK_TAP_BRANCH_BITS) + 7u) / 8u)
#define KEY_FEEDBACK_TAP_BRANCH_MASK ((uint16_t)((1u << KEY_FEEDBACK_TAP_BRANCH_BITS) - 1u))

_Static_assert(KEY_FEEDBACK_TAP_BRANCH_BITS <= 8u, "key feedback tap branch values must fit in one byte");
_Static_assert(KEY_BEHAVIOR_MAX_TAP_COUNT <= KEY_FEEDBACK_TAP_BRANCH_MASK, "key feedback tap branch map must represent every authored tap count");

typedef enum {
    KEY_FEEDBACK_SEMANTIC_NONE = 0,
    KEY_FEEDBACK_SEMANTIC_HOLD_PENDING,
    KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING,
    KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY,
    KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING,
    KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH,
    KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED,
    KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED,
} key_feedback_semantic_t;

_Static_assert(KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED <= KEY_FEEDBACK_SEMANTIC_MASK, "key feedback semantic map must represent every semantic value");

static inline bool key_feedback_semantic_is_flashing(key_feedback_semantic_t semantic) {
    return semantic == KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING || semantic == KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING;
}

static inline void key_feedback_semantic_map_clear(uint8_t *map) {
    if (!map) {
        return;
    }

    for (uint8_t index = 0; index < KEY_FEEDBACK_SEMANTIC_MAP_SIZE; index++) {
        map[index] = 0u;
    }
}

static inline key_feedback_semantic_t key_feedback_semantic_map_get(const uint8_t *map, keypos_t key_pos) {
    uint16_t bit_index;
    uint16_t byte_index;
    uint8_t  shift;
    uint16_t packed;

    if (!(map && key_origin_keypos_valid(key_pos))) {
        return KEY_FEEDBACK_SEMANTIC_NONE;
    }

    bit_index  = (uint16_t)(key_origin_keypos_index(key_pos) * KEY_FEEDBACK_SEMANTIC_BITS);
    byte_index = (uint16_t)(bit_index / 8u);
    shift      = (uint8_t)(bit_index % 8u);
    packed     = map[byte_index];
    if ((byte_index + 1u) < KEY_FEEDBACK_SEMANTIC_MAP_SIZE) {
        packed |= (uint16_t)((uint16_t)map[byte_index + 1u] << 8u);
    }

    return (key_feedback_semantic_t)((packed >> shift) & KEY_FEEDBACK_SEMANTIC_MASK);
}

static inline void key_feedback_semantic_map_set(uint8_t *map, keypos_t key_pos, key_feedback_semantic_t semantic) {
    uint16_t bit_index;
    uint16_t byte_index;
    uint8_t  shift;
    uint16_t packed;

    if (!(map && key_origin_keypos_valid(key_pos))) {
        return;
    }

    bit_index  = (uint16_t)(key_origin_keypos_index(key_pos) * KEY_FEEDBACK_SEMANTIC_BITS);
    byte_index = (uint16_t)(bit_index / 8u);
    shift      = (uint8_t)(bit_index % 8u);
    packed     = map[byte_index];
    if ((byte_index + 1u) < KEY_FEEDBACK_SEMANTIC_MAP_SIZE) {
        packed |= (uint16_t)((uint16_t)map[byte_index + 1u] << 8u);
    }

    packed &= (uint16_t)~((uint16_t)KEY_FEEDBACK_SEMANTIC_MASK << shift);
    packed |= (uint16_t)(((uint16_t)semantic & KEY_FEEDBACK_SEMANTIC_MASK) << shift);

    map[byte_index] = (uint8_t)(packed & 0xFFu);
    if ((byte_index + 1u) < KEY_FEEDBACK_SEMANTIC_MAP_SIZE) {
        map[byte_index + 1u] = (uint8_t)(packed >> 8u);
    }
}

static inline void key_feedback_tap_branch_map_clear(uint8_t *map) {
    if (!map) {
        return;
    }

    for (uint8_t index = 0; index < KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE; index++) {
        map[index] = 0u;
    }
}

static inline uint8_t key_feedback_tap_branch_map_get(const uint8_t *map, keypos_t key_pos) {
    uint16_t bit_index;
    uint16_t byte_index;
    uint8_t  shift;
    uint16_t packed;

    if (!(map && key_origin_keypos_valid(key_pos))) {
        return 0u;
    }

    bit_index  = (uint16_t)(key_origin_keypos_index(key_pos) * KEY_FEEDBACK_TAP_BRANCH_BITS);
    byte_index = (uint16_t)(bit_index / 8u);
    shift      = (uint8_t)(bit_index % 8u);
    packed     = map[byte_index];
    if ((byte_index + 1u) < KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE) {
        packed |= (uint16_t)((uint16_t)map[byte_index + 1u] << 8u);
    }

    return (uint8_t)((packed >> shift) & KEY_FEEDBACK_TAP_BRANCH_MASK);
}

static inline void key_feedback_tap_branch_map_set(uint8_t *map, keypos_t key_pos, uint8_t tap_branch) {
    uint16_t bit_index;
    uint16_t byte_index;
    uint8_t  shift;
    uint16_t packed;

    if (!(map && key_origin_keypos_valid(key_pos))) {
        return;
    }

    if (tap_branch > KEY_BEHAVIOR_MAX_TAP_COUNT) {
        tap_branch = KEY_BEHAVIOR_MAX_TAP_COUNT;
    }

    bit_index  = (uint16_t)(key_origin_keypos_index(key_pos) * KEY_FEEDBACK_TAP_BRANCH_BITS);
    byte_index = (uint16_t)(bit_index / 8u);
    shift      = (uint8_t)(bit_index % 8u);
    packed     = map[byte_index];
    if ((byte_index + 1u) < KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE) {
        packed |= (uint16_t)((uint16_t)map[byte_index + 1u] << 8u);
    }

    packed &= (uint16_t)~((uint16_t)KEY_FEEDBACK_TAP_BRANCH_MASK << shift);
    packed |= (uint16_t)(((uint16_t)tap_branch & KEY_FEEDBACK_TAP_BRANCH_MASK) << shift);

    map[byte_index] = (uint8_t)(packed & 0xFFu);
    if ((byte_index + 1u) < KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE) {
        map[byte_index + 1u] = (uint8_t)(packed >> 8u);
    }
}

static inline bool key_feedback_semantic_map_has_any(const uint8_t *map) {
    if (!map) {
        return false;
    }

    for (uint8_t index = 0; index < KEY_FEEDBACK_SEMANTIC_MAP_SIZE; index++) {
        if (map[index] != 0u) {
            return true;
        }
    }

    return false;
}

static inline bool key_feedback_semantic_map_has_flashing(const uint8_t *map) {
    if (!key_feedback_semantic_map_has_any(map)) {
        return false;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            if (key_feedback_semantic_is_flashing(key_feedback_semantic_map_get(map, (keypos_t){.row = row, .col = col}))) {
                return true;
            }
        }
    }

    return false;
}

void    key_feedback_semantic_map(uint8_t *out_map);
void    key_feedback_tap_branch_map(uint8_t *out_map);
void    key_feedback_flash_visibility_bitmap_for_semantic_map(const uint8_t *semantic_map, uint8_t *out_bitmap);
void    key_feedback_flash_visibility_bitmap(uint8_t *out_bitmap);
uint8_t key_feedback_preview_layer(void);
void    combo_feedback_underlay_bitmap(uint8_t *out_bitmap);
void    combo_feedback_overlay_bitmap(uint8_t *out_bitmap);
void    key_feedback_pulse_arm(key_feedback_pulse_kind_t kind);
