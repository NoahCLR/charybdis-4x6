// PD domain contract (D-F14): 32 slots, stored sparsely, in 128-byte records.
// Version 3 moved the name to the shared counted rule: its length in byte 8,
// its UTF-8 in bytes 96..127. Version 4 links a slot to layers in bytes 11..15
// (D-F62) and retires the mouse button records, bytes 52..69, as reserved zero.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "profile_versions.h"
#include "profile_reader.h"
#include "profile_name_v1.h"

enum {
    NOAH_PROFILE_PD_V1_DOMAIN_ID   = NOAH_PROFILE_DOMAIN_V1_PD,
    NOAH_PROFILE_PD_V1_VERSION     = NOAH_PROFILE_PD_VERSION,
    NOAH_PROFILE_PD_V1_SLOT_COUNT  = NOAH_PROFILE_PD_COUNT,
    NOAH_PROFILE_PD_V1_HEADER_SIZE = 8,
    NOAH_PROFILE_PD_V1_RECORD_SIZE = 128,
    // What the pointing engine reads; the name follows it.
    NOAH_PROFILE_PD_V1_MODE_SIZE   = 96,
    NOAH_PROFILE_PD_V1_NAME_LENGTH = 8,
    NOAH_PROFILE_PD_V1_NAME_OFFSET = NOAH_PROFILE_PD_V1_MODE_SIZE,
    NOAH_PROFILE_PD_V1_NAME_MAX    = NOAH_PROFILE_NAME_MAX,
    NOAH_PROFILE_PD_V1_STEP_READ_MAX = 20,
    // Every slot present: the largest payload the domain can have.
    NOAH_PROFILE_PD_V1_MAX_SIZE = NOAH_PROFILE_PD_V1_HEADER_SIZE + NOAH_PROFILE_PD_V1_SLOT_COUNT * NOAH_PROFILE_PD_V1_RECORD_SIZE,
};

// The values each field holds. Fields are named in noah_pd_config_t below.
// What a slot is (kind).
enum {
    NOAH_PD_KIND_DISABLED = 0,
    NOAH_PD_KIND_DIRECTIONAL,
    NOAH_PD_KIND_SCROLLING,
};
// Whether the pointer layer stays while the mode runs (pointer_layer).
enum {
    NOAH_PD_POINTER_LAYER_KEEP = 0,
    NOAH_PD_POINTER_LAYER_PREFER_TYPING,
};
// Directional axis policies (axis of a directional slot): which directions
// exist. Vertical and horizontal have two, dominant axis the four straight
// ones, eight directions adds the diagonals (zero in every other directional
// slot). One engine runs them all.
enum {
    NOAH_PD_AXIS_VERTICAL = 0,
    NOAH_PD_AXIS_HORIZONTAL,
    NOAH_PD_AXIS_DOMINANT,
    NOAH_PD_AXIS_EIGHT,
};
// Which axes a scrolling mode scrolls (axis of a scrolling slot).
enum {
    NOAH_PD_SCROLL_BOTH = 0,
    NOAH_PD_SCROLL_HORIZONTAL,
    NOAH_PD_SCROLL_VERTICAL,
};
// What a directional mode does with motion toward a direction that has no
// shortcut (empty_direction, every directional slot).
enum {
    NOAH_PD_EMPTY_DIRECTION_NEAREST = 0, // its neighbours take its share
    NOAH_PD_EMPTY_DIRECTION_BOTH,        // both compass neighbours (eight directions; elsewhere as nearest)
    NOAH_PD_EMPTY_DIRECTION_NOTHING,     // a dead zone
};
// How often a directional mode sends (direction_output, every directional
// slot).
enum {
    NOAH_PD_DIRECTION_OUTPUT_REPEAT = 0, // one output per threshold step
    NOAH_PD_DIRECTION_OUTPUT_ONCE,       // one output per movement
};
// A tap's modifier policy (noah_pd_tap_t.policy). Its mask is nonzero only
// for NOAH_PD_TAP_MASK.
enum {
    NOAH_PD_TAP_INHERIT = 0, // the held modifiers apply
    NOAH_PD_TAP_MASK,        // the modifiers in mask are hidden
    NOAH_PD_TAP_EXACT,       // the shortcut alone
};
// What ends a mode its linked layers started (link_ends), until that layer
// goes off: nothing, a layer above the highest linked layer that is on, or any
// layer outside both of the link's sets that is on. The pointer layer and a
// default layer never end it.
enum {
    NOAH_PD_LINK_ENDS_NOTHING = 0,
    NOAH_PD_LINK_ENDS_HIGHER_LAYER,
    NOAH_PD_LINK_ENDS_OTHER_LAYER,
};
// The straight directions, in their order in directions[].
enum {
    NOAH_PD_DIRECTION_LEFT = 0,
    NOAH_PD_DIRECTION_RIGHT,
    NOAH_PD_DIRECTION_UP,
    NOAH_PD_DIRECTION_DOWN,
};
// The diagonals, in their order in diagonals[].
enum {
    NOAH_PD_DIAGONAL_UP_LEFT = 0,
    NOAH_PD_DIAGONAL_UP_RIGHT,
    NOAH_PD_DIAGONAL_DOWN_LEFT,
    NOAH_PD_DIAGONAL_DOWN_RIGHT,
};

typedef enum {
    NOAH_PROFILE_PD_V1_OK = 0,
    NOAH_PROFILE_PD_V1_INVALID_ARGUMENT,
    NOAH_PROFILE_PD_V1_INVALID_LENGTH,
    NOAH_PROFILE_PD_V1_INVALID_HEADER,
    NOAH_PROFILE_PD_V1_INVALID_ID,
    NOAH_PROFILE_PD_V1_RESERVED,
    NOAH_PROFILE_PD_V1_INVALID_NAME,
    NOAH_PROFILE_PD_V1_INVALID_POLICY,
    NOAH_PROFILE_PD_V1_INVALID_ACTION,
    NOAH_PROFILE_PD_V1_INVALID_PARAMETER,
    // A present record that says nothing: disabled with no name.
    // Such a slot is stored by leaving it out.
    NOAH_PROFILE_PD_V1_NONCANONICAL,
    NOAH_PROFILE_PD_V1_READ_ERROR,
} noah_profile_pd_v1_result_t;

typedef struct {
    noah_profile_pd_v1_result_t code;
    size_t offset;
} noah_profile_pd_v1_error_t;

// One pointing slot's 128-byte record, field for field. The authored defaults
// are written in it, the encoder emits it, the effective cache holds it and the
// pointing engine reads it. profile_pd_v1.c asserts every field's offset against
// the record table in pd-mode-domain-v1.md, its alignment (that of uint16_t)
// and little-endian byte order. Its bytes are the wire record, so no other file
// names a byte position.
typedef struct {
    uint16_t keycode; // zero: no output
    uint8_t  policy;  // NOAH_PD_TAP_*
    uint8_t  mask;
} noah_pd_tap_t;
typedef struct {
    uint8_t  id;
    uint8_t  kind;           // NOAH_PD_KIND_*
    uint8_t  pointer_layer;  // NOAH_PD_POINTER_LAYER_*
    uint8_t  axis;           // NOAH_PD_AXIS_* (directional) or NOAH_PD_SCROLL_* (scrolling)
    uint16_t dpi;            // zero inherits
    uint8_t  held_modifiers; // owned while a scrolling slot is active
    uint8_t  reserved;
    // The encoder writes the name's length here; authored data leaves it zero.
    uint8_t       name_length;
    // The link (D-F62): what ends a mode its linked layers started
    // (NOAH_PD_LINK_ENDS_*); the linked layers, which start the mode; and the
    // layers its key turns on while it runs the mode. In both masks bit n is
    // layer n. Every link byte zero is no link.
    uint8_t       link_head_reserved[2];
    uint8_t       link_ends;
    uint16_t      linked_layers;
    uint16_t      link_turns_on;
    uint8_t       link_reserved[16];
    uint16_t      threshold_x, threshold_y; // directional step sizes, in sensor counts
    noah_pd_tap_t directions[4];            // NOAH_PD_DIRECTION_*
    uint8_t       retired_buttons[18];      // version 3's mouse button records; zero
    union {
        struct { // scrolling slots
            struct {
                uint16_t threshold_h, threshold_v; // sensor counts that start an axis
                uint16_t divisor_h, divisor_v;     // sensor counts per scroll step
                uint16_t interval_ms;              // between outputs; zero is unthrottled
                uint16_t expiry_ms;                // still time that drops banked motion
                uint16_t axis_timeout_ms;          // still time that releases the axis
            } scroll;
            struct {
                uint8_t start_num, start_den;     // the ratio that starts an axis lock
                uint8_t sustain_num, sustain_den; // the ratio that keeps it
                uint8_t decay;                    // cross-axis decay divisor
                uint8_t invert;                   // bit 0 horizontal, bit 1 vertical
            } scroll_policy;
        };
        struct { // directional slots
            noah_pd_tap_t diagonals[4];      // NOAH_PD_DIAGONAL_*; zero unless the axis is NOAH_PD_AXIS_EIGHT
            uint8_t       empty_direction;   // NOAH_PD_EMPTY_DIRECTION_*
            uint8_t       direction_output;  // NOAH_PD_DIRECTION_OUTPUT_*
            uint8_t       diagonal_reserved[2];
        };
    };
    uint8_t tail_reserved[6];
    // At most 32 bytes of UTF-8; the rest zero. A full name has no terminator.
    char name[NOAH_PROFILE_PD_V1_NAME_MAX];
} noah_pd_config_t;
extern const noah_pd_config_t noah_pd_defaults[NOAH_PROFILE_PD_V1_SLOT_COUNT];
// The structure's bytes with name_length filled in from the name.
void noah_profile_pd_v1_encode_record(const noah_pd_config_t *config, uint8_t output[NOAH_PROFILE_PD_V1_RECORD_SIZE]);
// Whether a version-4 domain stores the slot: it is configured, or disabled
// with a name. Its kind and name length decide it.
bool noah_profile_pd_v1_record_present(const uint8_t *record);

// Vocabulary is the audited QMK 0.0.8 native ABI, not arbitrary uint16 actions.
bool noah_profile_pd_v1_tap_key_valid(uint16_t keycode);

// Cold byte validators; they allocate no profile/cache state. They take any
// buffer, aligned or not, and read each byte by its noah_pd_config_t field.
// The header and entry APIs let a bounded reader validate the domain
// one record at a time without holding the whole payload.
noah_profile_pd_v1_result_t noah_profile_pd_v1_validate_record(const uint8_t *record, size_t length, uint8_t slot, noah_profile_pd_v1_error_t *error);
// The 8-byte header against the whole payload length. On success
// *record_count is the number of records that follow.
noah_profile_pd_v1_result_t noah_profile_pd_v1_validate_header(const uint8_t header[8], size_t payload_length, uint8_t *record_count, noah_profile_pd_v1_error_t *error);
// One present record, whose slot ID must be at least
// minimum_slot (zero for the first record, the previous ID plus one after) and
// below the slot count, and which must not be a disabled record without a name.
noah_profile_pd_v1_result_t noah_profile_pd_v1_validate_entry(const uint8_t *record, size_t length, uint8_t minimum_slot, noah_profile_pd_v1_error_t *error);
// A whole payload.
noah_profile_pd_v1_result_t noah_profile_pd_v1_validate(const uint8_t *bytes, size_t length, noah_profile_pd_v1_error_t *error);

// Shared sparse-record traversal. The cursor also accepts encoded authored
// records, so compiled output follows the same ordering/presence contract.
// starting_layers is every layer an earlier record's link starts its mode
// from, so no layer starts two modes.
typedef struct { uint8_t count, index, minimum; uint16_t starting_layers; } noah_profile_pd_v1_cursor_t;
noah_profile_pd_v1_result_t noah_profile_pd_v1_cursor_begin(noah_profile_pd_v1_cursor_t *cursor, const uint8_t header[8], size_t length, noah_profile_pd_v1_error_t *error);
noah_profile_pd_v1_result_t noah_profile_pd_v1_cursor_next(noah_profile_pd_v1_cursor_t *cursor, const uint8_t record[NOAH_PROFILE_PD_V1_RECORD_SIZE], noah_profile_pd_v1_error_t *error);
uint8_t noah_profile_pd_v1_default_record_count(const noah_pd_config_t defaults[NOAH_PROFILE_PD_V1_SLOT_COUNT]);

typedef struct {
    uint8_t bytes[NOAH_PROFILE_PD_V1_RECORD_SIZE];
    noah_profile_pd_v1_cursor_t cursor;
    uint8_t used;
    bool header;
} noah_profile_pd_v1_iterator_t;

typedef enum {
    NOAH_PROFILE_PD_V1_ITERATING = 0,
    NOAH_PROFILE_PD_V1_RECORD,
    NOAH_PROFILE_PD_V1_COMPLETE,
    NOAH_PROFILE_PD_V1_REJECTED,
} noah_profile_pd_v1_iteration_t;

// Zero-initialize before use. Each step does at most one read, at most 20
// bytes. RECORD exposes one validated record in bytes (including the last);
// complete reports that no further record remains. Errors are payload-relative.
bool noah_profile_pd_v1_iterator_complete(const noah_profile_pd_v1_iterator_t *iterator);
noah_profile_pd_v1_iteration_t noah_profile_pd_v1_iterator_step(noah_profile_pd_v1_iterator_t *iterator, const noah_profile_reader_t *reader, size_t base, size_t length, noah_profile_pd_v1_error_t *error);
