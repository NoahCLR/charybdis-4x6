#pragma once

#include "profile_blob_v1.h"
#include "profile_reader.h"
#include "profile_versions.h"

// Domain 0x30 version 3 (D-F14): an eight-byte header with the default window
// and hold threshold, then counted rows. A row is 12 fixed bytes (input count,
// flags, window, allowed layers, output) and four bytes per input, so a
// two-key combo takes 20 bytes and a sixteen-key one 76.
enum {
    NOAH_PROFILE_COMBO_V2_HEADER_SIZE    = 8u,
    NOAH_PROFILE_COMBO_V3_ROW_FIXED_SIZE = 12u,
    NOAH_PROFILE_COMBO_V1_MAX_ROWS       = 128u,
    NOAH_PROFILE_COMBO_V1_MIN_INPUTS     = 2u,
    NOAH_PROFILE_COMBO_V1_MAX_INPUTS     = 16u,
    NOAH_PROFILE_COMBO_V3_MAX_ROW_SIZE   = NOAH_PROFILE_COMBO_V3_ROW_FIXED_SIZE + 4u * NOAH_PROFILE_COMBO_V1_MAX_INPUTS,
    // Inputs are read four at a time, sixteen bytes a step.
    NOAH_PROFILE_COMBO_V3_INPUTS_PER_STEP = 4u,
    NOAH_PROFILE_COMBO_VERSION           = NOAH_PROFILE_DOMAIN_VERSION_COMBOS,
};
// Row flags: bit 0 must hold, 1 must tap (not both), 2 press in order,
// 3 disabled. A disabled combo keeps its inputs, output and window.
enum {
    NOAH_PROFILE_COMBO_V3_FLAG_MUST_HOLD     = 1u << 0,
    NOAH_PROFILE_COMBO_V3_FLAG_MUST_TAP      = 1u << 1,
    NOAH_PROFILE_COMBO_V3_FLAG_PRESS_IN_ORDER = 1u << 2,
    NOAH_PROFILE_COMBO_V3_FLAG_DISABLED      = 1u << 3,
    NOAH_PROFILE_COMBO_V3_KNOWN_FLAGS        = 0x0fu,
};
typedef struct {
    uint8_t  row_count;
    uint16_t default_term_ms;
    uint16_t hold_term_ms;
} noah_profile_combo_v1_header_t;
typedef struct {
    uint16_t                 term_ms; // zero follows the default window
    uint8_t                  input_count;
    uint8_t                  flags;
    uint32_t                 allowed_layers; // one bit per bank layer
    noah_profile_action_v1_t output;
    noah_profile_action_v1_t inputs[NOAH_PROFILE_COMBO_V1_MAX_INPUTS];
} noah_profile_combo_v1_row_t;
typedef struct {
    uint16_t offset;     // the next byte to read, from the payload's start
    uint16_t row_offset; // where the current row starts
    uint8_t  phase, row_index, row_count, inputs_read;
    uint8_t  bytes[NOAH_PROFILE_COMBO_V3_MAX_ROW_SIZE];
} noah_profile_combo_v1_validation_t;

// Header and row decoding consume only caller-supplied bytes; a row's bytes are
// its fixed part and every input.
noah_profile_codec_v1_result_t noah_profile_combo_v1_decode_header(const uint8_t *bytes, uint16_t payload_length, noah_profile_combo_v1_header_t *header);
noah_profile_codec_v1_result_t noah_profile_combo_v1_decode_row(const uint8_t *bytes, const noah_profile_action_v1_limits_t *limits, noah_profile_combo_v1_row_t *row);
// Readers take the domain's validated payload range within the blob at
// blob_base_offset. Rows vary in length, so read_row walks the rows before it;
// a whole-table reader iterates instead.
bool noah_profile_combo_v1_read_header(const noah_profile_reader_t *reader, size_t blob_base_offset, noah_profile_domain_range_t payload, noah_profile_combo_v1_header_t *header);
bool noah_profile_combo_v1_read_row(const noah_profile_reader_t *reader, size_t blob_base_offset, noah_profile_domain_range_t payload, uint8_t index, noah_profile_combo_v1_row_t *row);

typedef enum {
    NOAH_PROFILE_COMBO_V1_ITERATING = 0,
    NOAH_PROFILE_COMBO_V1_HEADER,
    NOAH_PROFILE_COMBO_V1_ROW,
    NOAH_PROFILE_COMBO_V1_COMPLETE,
    NOAH_PROFILE_COMBO_V1_REJECTED,
} noah_profile_combo_v1_iteration_t;
// Zero-initialize. One step reads the header, a row's 12 fixed bytes, or up
// to four of its inputs (16 bytes). HEADER and ROW expose decoded values;
// runtime reference and placement admission remain with the whole-profile
// validator. COMPLETE requires the rows to end exactly at the payload's end.
noah_profile_combo_v1_iteration_t noah_profile_combo_v1_iteration_step(noah_profile_combo_v1_validation_t *state, const noah_profile_reader_t *reader, size_t base, uint16_t length, const noah_profile_action_v1_limits_t *limits, noah_profile_combo_v1_header_t *header, noah_profile_combo_v1_row_t *row, noah_profile_codec_v1_result_t *detail);
