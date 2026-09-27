#pragma once

#include "profile_blob_v1.h"
#include "profile_reader.h"

// Domain 0x30. Version 1 repeats QMK's one hold threshold on every row and
// stores every combo window explicitly. Version 2 keeps both combo-wide
// values once in an eight-byte header: the default window, which a row with
// window zero follows, and the hold threshold.
enum {
    NOAH_PROFILE_COMBO_V1_HEADER_SIZE = 4u,
    NOAH_PROFILE_COMBO_V2_HEADER_SIZE = 8u,
    NOAH_PROFILE_COMBO_V1_ROW_SIZE    = 28u,
    NOAH_PROFILE_COMBO_V1_MAX_ROWS    = 32u,
    NOAH_PROFILE_COMBO_V1_MAX_INPUTS  = 4u,
    NOAH_PROFILE_COMBO_VERSION        = 2u,
};
#define NOAH_PROFILE_COMBO_VERSION_ACCEPTED(version) ((version) == 1u || (version) == 2u)
#define NOAH_PROFILE_COMBO_HEADER_SIZE(version) ((version) >= 2u ? NOAH_PROFILE_COMBO_V2_HEADER_SIZE : NOAH_PROFILE_COMBO_V1_HEADER_SIZE)
// Payload offset is relative to the enclosing bounded 4,064-byte blob.
typedef struct {
    uint16_t payload_offset;
    uint8_t  row_count;
    uint8_t  version;
} noah_profile_combo_v1_view_t;
typedef struct {
    uint8_t  row_count;
    uint16_t default_term_ms; // version 2; zero in version 1, which has none
    uint16_t hold_term_ms;    // version 2; version 1 carries it on each row
} noah_profile_combo_v1_header_t;
typedef struct {
    uint16_t                 term_ms; // version 2: zero follows the default window
    uint16_t                 hold_term_ms; // version 1 only
    uint8_t                  input_count;
    uint8_t                  flags;
    noah_profile_action_v1_t output;
    noah_profile_action_v1_t inputs[4];
} noah_profile_combo_v1_row_t;
typedef struct {
    uint16_t hold_term_ms;
    uint8_t  phase, row_index, version;
    uint8_t  bytes[28];
} noah_profile_combo_v1_validation_t;

// Header and row decoding consume only caller-supplied bytes. Validation owners
// split reader I/O into 12- and 16-byte steps to preserve the scan read budget.
noah_profile_codec_v1_result_t noah_profile_combo_v1_decode_header(const uint8_t *bytes, uint8_t version, uint16_t payload_length, noah_profile_combo_v1_header_t *header);
noah_profile_codec_v1_result_t noah_profile_combo_v1_decode_row(const uint8_t bytes[28], uint8_t version, const noah_profile_action_v1_limits_t *limits, noah_profile_combo_v1_row_t *row);
bool                           noah_profile_combo_v1_read_header(const noah_profile_reader_t *reader, size_t blob_base_offset, const noah_profile_combo_v1_view_t *view, noah_profile_combo_v1_header_t *header);
bool                           noah_profile_combo_v1_read_row(const noah_profile_reader_t *reader, size_t blob_base_offset, const noah_profile_combo_v1_view_t *view, uint8_t index, noah_profile_combo_v1_row_t *row);
