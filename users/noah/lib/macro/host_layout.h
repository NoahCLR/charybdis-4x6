#pragma once

#include <stdbool.h>
#include <stdint.h>

// Host keyboard layouts: for each layout, the key strokes that type each
// character it can type. The tables are generated from
// tests/fixtures/host_layouts_v1.json; see docs/architecture/host-layouts-v1.md.

// A stroke is one basic keycode tapped with optional Shift and AltGr. AltGr is
// Right Alt, which is Option on macOS. Zero means no stroke.
#define HOST_LAYOUT_STROKE_SHIFT 0x0100u
#define HOST_LAYOUT_STROKE_ALTGR 0x0200u
#define HOST_LAYOUT_STROKE_KEYCODE(stroke) ((uint8_t)((stroke) & 0xFFu))

// On this layout, Option enters hexadecimal Unicode; it types no characters.
#define HOST_LAYOUT_FLAG_UNICODE_HEX_INPUT 0x01u
// A macOS layout: an ISO-classified keyboard exchanges KC_GRV and KC_NUBS.
#define HOST_LAYOUT_FLAG_MACOS 0x02u

#define HOST_LAYOUT_US 0u

typedef struct {
    uint32_t codepoint;
    // A character takes one stroke, or two: a dead key, then Space or a letter.
    uint16_t strokes[2];
} host_layout_char_t;

typedef struct {
    uint8_t                   id;
    uint8_t                   flags;
    uint16_t                  count;
    const host_layout_char_t *chars; // Sorted by codepoint.
} host_layout_t;

extern const host_layout_t host_layouts[];
extern const uint8_t       host_layout_table_count;

// The layout with this catalogue ID, or NULL for an unknown ID.
const host_layout_t *host_layout_get(uint8_t id);

// The strokes that type this character on this layout; false if it cannot.
bool host_layout_lookup(const host_layout_t *layout, uint32_t codepoint, uint16_t strokes[2]);
