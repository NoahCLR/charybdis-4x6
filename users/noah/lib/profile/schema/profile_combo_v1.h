#pragma once

#include "profile_blob_v1.h"
#include "profile_reader.h"
#include "profile_versions.h"

// Domain 0x30 version 2 stores the default window and hold threshold once.
enum {
    NOAH_PROFILE_COMBO_V2_HEADER_SIZE = 8u,
    NOAH_PROFILE_COMBO_V1_ROW_SIZE    = 28u,
    NOAH_PROFILE_COMBO_V1_MAX_ROWS    = 32u,
    NOAH_PROFILE_COMBO_V1_MAX_INPUTS  = 4u,
    NOAH_PROFILE_COMBO_VERSION        = NOAH_PROFILE_DOMAIN_VERSION_COMBOS,
};
typedef struct {
    uint8_t  row_count;
    uint16_t default_term_ms;
    uint16_t hold_term_ms;
} noah_profile_combo_v1_header_t;
typedef struct {
    uint16_t                 term_ms; // version 2: zero follows the default window
    uint8_t                  input_count;
    uint8_t                  flags;
    noah_profile_action_v1_t output;
    noah_profile_action_v1_t inputs[4];
} noah_profile_combo_v1_row_t;
typedef struct {
    uint8_t  phase, row_index, row_count;
    uint8_t  bytes[28];
} noah_profile_combo_v1_validation_t;

// Header and row decoding consume only caller-supplied bytes. Validation owners
// split reader I/O into 12- and 16-byte steps to preserve the scan read budget.
noah_profile_codec_v1_result_t noah_profile_combo_v1_decode_header(const uint8_t *bytes, uint16_t payload_length, noah_profile_combo_v1_header_t *header);
noah_profile_codec_v1_result_t noah_profile_combo_v1_decode_row(const uint8_t bytes[28], const noah_profile_action_v1_limits_t *limits, noah_profile_combo_v1_row_t *row);
// Readers take the domain's validated payload range within the blob at
// blob_base_offset; its length fixes the row count.
uint8_t                        noah_profile_combo_v1_row_count(noah_profile_domain_range_t payload);
bool                           noah_profile_combo_v1_read_header(const noah_profile_reader_t *reader, size_t blob_base_offset, noah_profile_domain_range_t payload, noah_profile_combo_v1_header_t *header);
bool                           noah_profile_combo_v1_read_row(const noah_profile_reader_t *reader, size_t blob_base_offset, noah_profile_domain_range_t payload, uint8_t index, noah_profile_combo_v1_row_t *row);

typedef enum {
    NOAH_PROFILE_COMBO_V1_ITERATING = 0,
    NOAH_PROFILE_COMBO_V1_HEADER,
    NOAH_PROFILE_COMBO_V1_ROW,
    NOAH_PROFILE_COMBO_V1_COMPLETE,
    NOAH_PROFILE_COMBO_V1_REJECTED,
} noah_profile_combo_v1_iteration_t;
// Zero-initialize. One step reads the header, 12 row bytes,
// or 16 row bytes. HEADER and ROW expose decoded values; runtime reference
// and placement admission remain with the whole-profile validator.
noah_profile_combo_v1_iteration_t noah_profile_combo_v1_iteration_step(noah_profile_combo_v1_validation_t *state, const noah_profile_reader_t *reader, size_t base, uint16_t length, const noah_profile_action_v1_limits_t *limits, noah_profile_combo_v1_header_t *header, noah_profile_combo_v1_row_t *row, noah_profile_codec_v1_result_t *detail);
