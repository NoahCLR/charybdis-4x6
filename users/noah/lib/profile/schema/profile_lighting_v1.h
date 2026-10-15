// ────────────────────────────────────────────────────────────────────────────
// Lighting Rules — RGB domain 0x10 version 5
// ────────────────────────────────────────────────────────────────────────────
//
// The strict validator, the reader-backed view and the version-4 conversion
// of docs/architecture/lighting-rules-v1.md. Errors carry the payload-relative
// offset the reference model (tools/lighting_reference.py) reports, so every
// implementation of the contract rejects the same bytes at the same place.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "profile_lighting_kinds_v1.h"
#include "profile_reader.h"
#include "profile_rgb_v1.h"
#include "profile_versions.h"

enum {
    NOAH_PROFILE_LIGHTING_V1_FORMAT_VERSION   = 5u,
    NOAH_PROFILE_LIGHTING_V1_HEADER_SIZE      = 8u,
    NOAH_PROFILE_LIGHTING_V1_LED_COUNT        = 58u,
    NOAH_PROFILE_LIGHTING_V1_LEFT_LED_COUNT   = 29u,
    NOAH_PROFILE_LIGHTING_V1_PAYLOAD_MAX      = 8192u,
    NOAH_PROFILE_LIGHTING_V1_PALETTE_MAX      = 128u,
    NOAH_PROFILE_LIGHTING_V1_CONDITIONS_MAX   = 4u,
    NOAH_PROFILE_LIGHTING_V1_TIMINGS_MAX      = 4u,
    NOAH_PROFILE_LIGHTING_V1_PARAMETER_MAX    = 16u,
    NOAH_PROFILE_LIGHTING_V1_RULE_MIN_SIZE    = 6u,
    NOAH_PROFILE_LIGHTING_V1_KEY_SET_SIZE     = 8u,
    NOAH_PROFILE_LIGHTING_V1_DENSE_MAP_SIZE   = 59u,
    NOAH_PROFILE_LIGHTING_V1_SPARSE_MAX       = 28u,
    NOAH_PROFILE_LIGHTING_V1_NONE             = 0xffu,
    NOAH_PROFILE_LIGHTING_V1_INHERIT          = 0xfeu,
    NOAH_PROFILE_LIGHTING_V1_ANY              = 0xffu,
    NOAH_PROFILE_LIGHTING_V1_MAP_PAINT        = 0x80u,
    NOAH_PROFILE_LIGHTING_V1_MAP_CLEAR        = 0x80u,
    NOAH_PROFILE_LIGHTING_V1_RULE_DISABLED    = 1u << 0,
    NOAH_PROFILE_LIGHTING_V1_FADE_ON          = 1u << 0,
    // The largest record: place plus hold, long hold, tap and depth − 1 branches.
    NOAH_PROFILE_LIGHTING_V1_RECORD_MAX = 1u + 3u + (NOAH_PROFILE_TAP_DEPTH - 1u),
    // Validation performs at most one reader call per step and reads at most
    // this many bytes: a part header and its largest parameters.
    NOAH_PROFILE_LIGHTING_V1_VALIDATION_READ_MAX = 20u,
};

// Part slots: (slot << 5) | parameter length, then the kind.
enum {
    NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION  = 0u,
    NOAH_PROFILE_LIGHTING_V1_SLOT_NEGATED    = 1u,
    NOAH_PROFILE_LIGHTING_V1_SLOT_TARGET     = 2u,
    NOAH_PROFILE_LIGHTING_V1_SLOT_PAINT      = 3u,
    NOAH_PROFILE_LIGHTING_V1_SLOT_BACKGROUND = 4u,
    NOAH_PROFILE_LIGHTING_V1_SLOT_TIMING     = 5u,
    NOAH_PROFILE_LIGHTING_V1_SLOT_EXTENSION  = 6u,
    NOAH_PROFILE_LIGHTING_V1_SLOT_RESERVED   = 7u,
};

typedef enum {
    NOAH_PROFILE_LIGHTING_V1_OK = 0u,
    NOAH_PROFILE_LIGHTING_V1_INVALID_ARGUMENT,
    NOAH_PROFILE_LIGHTING_V1_READ_ERROR,
    NOAH_PROFILE_LIGHTING_V1_TRUNCATED,
    NOAH_PROFILE_LIGHTING_V1_TRAILING_BYTES,
    NOAH_PROFILE_LIGHTING_V1_CAPACITY_EXCEEDED,
    NOAH_PROFILE_LIGHTING_V1_INVALID_VERSION,
    NOAH_PROFILE_LIGHTING_V1_RESERVED_BITS,
    NOAH_PROFILE_LIGHTING_V1_INCOMPATIBLE_GEOMETRY,
    NOAH_PROFILE_LIGHTING_V1_BRIGHTNESS_EXCEEDED,
    NOAH_PROFILE_LIGHTING_V1_UNSUPPORTED_KIND,
    NOAH_PROFILE_LIGHTING_V1_NONCANONICAL_ORDER,
    NOAH_PROFILE_LIGHTING_V1_NONCANONICAL_ENCODING,
    NOAH_PROFILE_LIGHTING_V1_INVALID_ENUM,
    NOAH_PROFILE_LIGHTING_V1_INVALID_LENGTH,
    NOAH_PROFILE_LIGHTING_V1_INVALID_STRUCTURE,
    NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER,
    NOAH_PROFILE_LIGHTING_V1_INVALID_REFERENCE,
    NOAH_PROFILE_LIGHTING_V1_WORKING_MEMORY_EXCEEDED,
    NOAH_PROFILE_LIGHTING_V1_UNSUPPORTED_WORKING_MODEL,
} noah_profile_lighting_v1_result_t;

typedef struct {
    noah_profile_lighting_v1_result_t code;
    size_t                            offset;
} noah_profile_lighting_v1_error_t;

// What the keyboard supports. One bit per id in each registry word.
typedef struct {
    uint16_t max_payload_size;
    uint8_t  tap_depth;
    uint8_t  logical_layer_count;
    uint8_t  maximum_brightness;
    uint32_t supported_mode_mask;
    uint32_t facts;
    uint32_t targets;
    uint32_t paints;
    uint32_t timings;
    uint32_t rule_extensions;
    uint32_t domain_extensions;
    uint32_t things;
} noah_profile_lighting_v1_limits_t;

// A validated reader-backed view: counts and section offsets only.
typedef struct {
    noah_profile_reader_t             reader;
    size_t                            base_offset;
    uint16_t                          byte_length;
    uint8_t                           palette_count;
    uint8_t                           thing_table_count;
    uint8_t                           key_set_count;
    uint8_t                           map_count;
    uint8_t                           rule_count;
    uint16_t                          things_offset;
    uint16_t                          key_sets_offset;
    uint16_t                          maps_offset;
    uint16_t                          rules_offset;
    uint16_t                          extensions_offset;
    uint8_t                           tap_depth;
} noah_profile_lighting_v1_view_t;

typedef enum {
    NOAH_PROFILE_LIGHTING_V1_VALIDATION_IN_PROGRESS = 0u,
    NOAH_PROFILE_LIGHTING_V1_VALIDATION_VALID,
    NOAH_PROFILE_LIGHTING_V1_VALIDATION_REJECTED,
} noah_profile_lighting_v1_validation_result_t;

// Caller-owned, payload-independent validation state. Treat the fields as
// private. begin() performs no reader I/O; each step() performs at most one
// read of at most NOAH_PROFILE_LIGHTING_V1_VALIDATION_READ_MAX bytes.
typedef struct {
    noah_profile_lighting_v1_view_t          candidate;
    const noah_profile_lighting_v1_limits_t *limits; // the caller's, for the validation's lifetime
    noah_profile_lighting_v1_error_t         rejection;
    uint16_t                         offset;    // next byte to examine
    uint16_t                         item_end;  // end of the current rule
    uint8_t                          phase;
    uint8_t                          remaining; // items left in the section
    uint8_t                          sub;       // records or entries left in the item
    uint8_t                          previous;  // the previous kind, LED or table
    uint8_t                          record_size;
    uint8_t                          rank;
    uint8_t                          conditions;
    uint8_t                          timings;
    uint8_t                          singles;
    uint8_t                          previous_timing;
    uint8_t                          previous_extension;
    uint8_t                          painted;
    uint8_t                          encoding;
    uint8_t                          result;
    uint32_t                         working_bank;
    uint16_t                         working_max;
    uint8_t                          working_count, working_members;
    bool                             working_retained, working_disabled;
} noah_profile_lighting_v1_validation_t;

noah_profile_lighting_v1_limits_t noah_profile_lighting_v1_default_limits(void);

noah_profile_lighting_v1_validation_result_t noah_profile_lighting_v1_validation_begin(noah_profile_lighting_v1_validation_t *validation, const noah_profile_reader_t *reader, size_t base_offset, size_t length, const noah_profile_lighting_v1_limits_t *limits, noah_profile_lighting_v1_error_t *error);
noah_profile_lighting_v1_validation_result_t noah_profile_lighting_v1_validation_step(noah_profile_lighting_v1_validation_t *validation, noah_profile_lighting_v1_error_t *error);
noah_profile_lighting_v1_validation_result_t noah_profile_lighting_v1_validation_view(const noah_profile_lighting_v1_validation_t *validation, noah_profile_lighting_v1_view_t *view, noah_profile_lighting_v1_error_t *error);

// Drives the incremental validator to completion.
noah_profile_lighting_v1_result_t noah_profile_lighting_v1_decode_reader(const noah_profile_reader_t *reader, size_t base_offset, size_t length, const noah_profile_lighting_v1_limits_t *limits, noah_profile_lighting_v1_view_t *view, noah_profile_lighting_v1_error_t *error);
noah_profile_lighting_v1_result_t noah_profile_lighting_v1_decode(const uint8_t *bytes, size_t length, const noah_profile_lighting_v1_limits_t *limits, noah_profile_lighting_v1_view_t *view, noah_profile_lighting_v1_error_t *error);

// Registry facts the validator and the runtime share.
bool    noah_profile_lighting_v1_place_capable(const noah_profile_lighting_v1_limits_t *limits, uint8_t target);
uint8_t noah_profile_lighting_v1_colour_slots(uint8_t tap_depth, uint8_t thing);
uint8_t noah_profile_lighting_v1_thing_count(const noah_profile_lighting_v1_limits_t *limits, uint8_t thing);

// The lighting capability read (GET 0x0D): page 0 is the format's bounds,
// page 1 + 2r + h registry r's ids 128h..128h+127. Writes at most 25 bytes and
// returns their count. Page15 declares shared working-memory admission;
// page128+fact-id declares that module's subject/member bound. Unsupported
// pages return0; sparse extension pages do not close the registry.
enum {
    NOAH_PROFILE_LIGHTING_V1_CAPABILITY_LAYOUT     = 1u,
    NOAH_PROFILE_LIGHTING_V1_CAPABILITY_REGISTRIES = 7u,
    NOAH_PROFILE_LIGHTING_V1_CAPABILITY_PAGES      = 2u + 2u * NOAH_PROFILE_LIGHTING_V1_CAPABILITY_REGISTRIES,
};
uint8_t noah_profile_lighting_v1_capability_page(const noah_profile_lighting_v1_limits_t *limits, uint8_t page, uint8_t *payload);

// The version-4 → version-5 conversion (Standard lighting). Reads a decoded
// version-4 view and writes the canonical version-5 payload for a keyboard
// with these limits. Rules, tables and extensions whose kinds the limits do
// not support are omitted, as the spec defines.
noah_profile_lighting_v1_result_t noah_profile_lighting_v1_convert_rgb_v4(const noah_profile_rgb_v1_view_t *v4, const noah_profile_lighting_v1_limits_t *limits, uint8_t *out, size_t capacity, size_t *length);

// ─── The decoded program ────────────────────────────────────────────────────
//
// On publication the whole validated payload is copied into RAM and indexed,
// so rendering never reads the profile. Rule i spans [rules[i], rules[i + 1]);
// rules[rule_count] is where the extensions begin.
typedef struct {
    uint8_t  bytes[NOAH_PROFILE_LIGHTING_V1_PAYLOAD_MAX];
    uint16_t length;
    uint8_t  palette_count;
    uint8_t  key_set_count;
    uint8_t  map_count;
    uint8_t  rule_count;
    uint8_t  tap_depth;
    uint8_t  thing_records[NOAH_LIGHTING_THING_KIND_LIMIT];
    uint8_t  thing_record_size[NOAH_LIGHTING_THING_KIND_LIMIT];
    uint16_t thing_table[NOAH_LIGHTING_THING_KIND_LIMIT]; // the default record; 0 without a table
    uint16_t key_sets;
    uint16_t maps[UINT8_MAX];
    uint16_t rules[UINT8_MAX + 1u];
    uint8_t  fade[4];
    bool     fade_present;
    bool     tap_commit_flash;
    bool     valid;
} noah_profile_lighting_v1_program_t;

// Copies and indexes a validated view. False if the reader fails.
bool noah_profile_lighting_v1_program_load(const noah_profile_lighting_v1_view_t *view, noah_profile_lighting_v1_program_t *program);

// A thing's record field (0 place, 1.. colour slots), inheriting from the
// table's default record. False without a table for the kind.
bool noah_profile_lighting_v1_program_thing_field(const noah_profile_lighting_v1_program_t *program, uint8_t kind, uint8_t thing, uint8_t field, uint8_t *value);

// Structural v5 decoding remains separate from target working-memory admission.
// Model1 is direct reservation: two complete banks and one replacement slot.
enum { NOAH_LIGHTING_WORKING_MODEL = 1u, NOAH_LIGHTING_WORKING_CAPACITY = 8220u, NOAH_LIGHTING_WORKING_DESCRIPTOR = 12u, NOAH_LIGHTING_WORKING_HEADER = 12u };
typedef struct {
    uint32_t bank_bytes, required;
    uint16_t max_slot;
    uint8_t rules, contributor;
} noah_lighting_working_cost_t;
noah_profile_lighting_v1_result_t noah_profile_lighting_v1_working_cost(const noah_profile_lighting_v1_view_t *view, noah_lighting_working_cost_t *cost);
noah_profile_lighting_v1_result_t noah_profile_lighting_v1_working_admit(const noah_profile_lighting_v1_view_t *view, uint32_t capacity, noah_lighting_working_cost_t *cost);
