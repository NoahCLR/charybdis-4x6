#pragma once

#include "profile_blob_v1.h"
#include "profile_reader.h"

enum {NOAH_PROFILE_COMBO_V1_HEADER_SIZE = 4u, NOAH_PROFILE_COMBO_V1_ROW_SIZE = 28u, NOAH_PROFILE_COMBO_V1_MAX_ROWS = 32u, NOAH_PROFILE_COMBO_V1_MAX_INPUTS = 4u};
// Payload offset is relative to the enclosing bounded 4,064-byte blob.
typedef struct {uint16_t payload_offset; uint8_t row_count;} noah_profile_combo_v1_view_t;
typedef struct {
    uint16_t term_ms;
    uint16_t hold_term_ms;
    uint8_t input_count;
    uint8_t flags;
    noah_profile_action_v1_t output;
    noah_profile_action_v1_t inputs[4];
} noah_profile_combo_v1_row_t;
typedef struct {uint16_t hold_term_ms; uint8_t phase, row_index; uint8_t bytes[28];} noah_profile_combo_v1_validation_t;

// Row decoding consumes only caller-supplied bytes. Validation owners split
// reader I/O into 12- and 16-byte steps to preserve the scan read budget.
noah_profile_codec_v1_result_t noah_profile_combo_v1_decode_row(const uint8_t bytes[28], const noah_profile_action_v1_limits_t *limits, noah_profile_combo_v1_row_t *row);
bool noah_profile_combo_v1_read_row(const noah_profile_reader_t *reader, size_t blob_base_offset, const noah_profile_combo_v1_view_t *view, uint8_t index, noah_profile_combo_v1_row_t *row);
