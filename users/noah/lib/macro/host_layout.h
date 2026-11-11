#pragma once

#include <stdbool.h>
#include <stdint.h>

// Host keyboard layouts: for each layout, the key strokes that type each
// character it can type, and what each stroke types. The tables are generated
// from tests/fixtures/host_layouts_v1.json; see docs/spec/host-layouts-v1.md.

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

// The keys a layout is read for: A-Z and 1-0, Space to Slash, and the non-US
// backslash key.
#define HOST_LAYOUT_KEY_SLOTS 50u
// In a key's cell: the stroke is a dead key and types nothing by itself.
#define HOST_LAYOUT_DEAD_KEY 0xFFFFu

// Every character a layout types is in the Basic Multilingual Plane.
typedef struct {
    uint16_t codepoint;
    // A character takes one stroke, or two: a dead key, then Space or a letter.
    uint16_t strokes[2];
} host_layout_char_t;

// A dead key's compositions: `count` of the layout's composes from `first`.
typedef struct {
    uint16_t stroke;
    uint16_t first;
    uint16_t count;
} host_layout_dead_t;

// What a dead key then this stroke types.
typedef struct {
    uint16_t stroke;
    uint16_t codepoint;
} host_layout_compose_t;

typedef struct {
    uint8_t                      id;
    uint8_t                      flags;
    uint16_t                     count;
    const host_layout_char_t    *chars; // Sorted by codepoint.
    // What each key types plain, with Shift, with AltGr and with both: a
    // character, zero for nothing, or HOST_LAYOUT_DEAD_KEY.
    const uint16_t             (*keys)[4];
    uint16_t                     dead_count;
    const host_layout_dead_t    *deads;    // Sorted by stroke.
    const host_layout_compose_t *composes; // Each dead key's, sorted by stroke.
} host_layout_t;

extern const host_layout_t host_layouts[];
extern const uint8_t       host_layout_table_count;
extern const uint16_t      host_layout_table_bytes;

// The layout with this catalogue ID, or NULL for an unknown ID.
const host_layout_t *host_layout_get(uint8_t id);

// The strokes that type this character on this layout; false if it cannot.
bool host_layout_lookup(const host_layout_t *layout, uint32_t codepoint, uint16_t strokes[2]);

typedef enum {
    HOST_LAYOUT_TYPES_NOTHING = 0,
    HOST_LAYOUT_TYPES_CHARACTER,
    HOST_LAYOUT_TYPES_DEAD_KEY,
} host_layout_types_t;

// What this stroke types by itself on this layout.
host_layout_types_t host_layout_stroke_types(const host_layout_t *layout, uint16_t stroke, uint16_t *codepoint);

// What this dead key then this stroke types; false when the layout composes
// nothing from them.
bool host_layout_compose(const host_layout_t *layout, uint16_t dead, uint16_t stroke, uint16_t *codepoint);
