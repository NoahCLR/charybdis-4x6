#include "profile_combo_v1.h"
#include <string.h>

static uint16_t u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8u);
}

noah_profile_codec_v1_result_t noah_profile_combo_v1_decode_header(const uint8_t *bytes, uint8_t version, uint16_t payload_length, noah_profile_combo_v1_header_t *header) {
    if (!bytes || !header) return NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT;
    memset(header, 0, sizeof(*header));
    if (!NOAH_PROFILE_COMBO_VERSION_ACCEPTED(version)) return NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN_VERSION;
    uint16_t size = NOAH_PROFILE_COMBO_HEADER_SIZE(version);
    if (payload_length < size) return NOAH_PROFILE_CODEC_V1_TRUNCATED;
    if (bytes[0] > NOAH_PROFILE_COMBO_V1_MAX_ROWS || bytes[1] || bytes[2] || bytes[3]) return NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS;
    if (payload_length != size + (uint16_t)bytes[0] * NOAH_PROFILE_COMBO_V1_ROW_SIZE) return NOAH_PROFILE_CODEC_V1_TRUNCATED;
    header->row_count = bytes[0];
    if (version >= 2u) {
        header->default_term_ms = u16(&bytes[4]);
        header->hold_term_ms    = u16(&bytes[6]);
        // QMK fires a combo only inside its window: a zero default would
        // silently disable every combo that follows it.
        if (header->default_term_ms == 0u) return NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS;
    }
    return NOAH_PROFILE_CODEC_V1_OK;
}

noah_profile_codec_v1_result_t noah_profile_combo_v1_decode_row(const uint8_t bytes[28], uint8_t version, const noah_profile_action_v1_limits_t *limits, noah_profile_combo_v1_row_t *row) {
    if (!bytes || !row) return NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT;
    memset(row, 0, sizeof(*row));
    if (!NOAH_PROFILE_COMBO_VERSION_ACCEPTED(version)) return NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN_VERSION;
    if (bytes[0] < 2u || bytes[0] > 4u || ((bytes[1] & ~7u) || (bytes[1] & 3u) == 3u) || bytes[6] || bytes[7]) return NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS;
    if (version >= 2u && (bytes[4] || bytes[5])) return NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS;
    row->input_count = bytes[0];
    row->flags       = bytes[1];
    row->term_ms     = u16(&bytes[2]);
    if (version == 1u) row->hold_term_ms = u16(&bytes[4]);
    if (noah_profile_action_v1_decode(&bytes[8], 4u, limits, &row->output, NULL) != NOAH_PROFILE_CODEC_V1_OK || row->output.kind == NOAH_PROFILE_ACTION_V1_NONE || (row->output.kind == NOAH_PROFILE_ACTION_V1_QMK_KEYCODE && row->output.operand == 0u)) return NOAH_PROFILE_CODEC_V1_INVALID_ACTION;
    for (uint8_t input = 0; input < 4u; input++) {
        const uint8_t *action = &bytes[12u + 4u * input];
        if (input >= row->input_count) {
            if (action[0] || action[1] || action[2] || action[3]) return NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS;
            continue;
        }
        if (noah_profile_action_v1_decode(action, 4u, limits, &row->inputs[input], NULL) != NOAH_PROFILE_CODEC_V1_OK || row->inputs[input].kind == NOAH_PROFILE_ACTION_V1_NONE || (row->inputs[input].kind == NOAH_PROFILE_ACTION_V1_QMK_KEYCODE && row->inputs[input].operand <= 1u)) return NOAH_PROFILE_CODEC_V1_INVALID_ACTION;
        for (uint8_t prior = 0; prior < input; prior++) {
            if (memcmp(action, &bytes[12u + 4u * prior], 4u) == 0) return NOAH_PROFILE_CODEC_V1_DUPLICATE_TARGET;
        }
    }
    return NOAH_PROFILE_CODEC_V1_OK;
}

bool noah_profile_combo_v1_read_header(const noah_profile_reader_t *reader, size_t blob_base_offset, const noah_profile_combo_v1_view_t *view, noah_profile_combo_v1_header_t *header) {
    uint8_t bytes[NOAH_PROFILE_COMBO_V2_HEADER_SIZE];
    if (!view || !NOAH_PROFILE_COMBO_VERSION_ACCEPTED(view->version)) return false;
    uint16_t size = NOAH_PROFILE_COMBO_HEADER_SIZE(view->version);
    if (!noah_profile_reader_read(reader, blob_base_offset + view->payload_offset, bytes, size)) return false;
    return noah_profile_combo_v1_decode_header(bytes, view->version, (uint16_t)(size + (uint16_t)view->row_count * NOAH_PROFILE_COMBO_V1_ROW_SIZE), header) == NOAH_PROFILE_CODEC_V1_OK && header->row_count == view->row_count;
}

bool noah_profile_combo_v1_read_row(const noah_profile_reader_t *reader, size_t blob_base_offset, const noah_profile_combo_v1_view_t *view, uint8_t index, noah_profile_combo_v1_row_t *row) {
    uint8_t bytes[28];
    if (!view || index >= view->row_count || !NOAH_PROFILE_COMBO_VERSION_ACCEPTED(view->version) || !noah_profile_reader_read(reader, blob_base_offset + view->payload_offset + NOAH_PROFILE_COMBO_HEADER_SIZE(view->version) + (size_t)index * 28u, bytes, sizeof(bytes))) return false;
    noah_profile_action_v1_limits_t limits = noah_profile_action_v1_default_limits();
    return noah_profile_combo_v1_decode_row(bytes, view->version, &limits, row) == NOAH_PROFILE_CODEC_V1_OK;
}
