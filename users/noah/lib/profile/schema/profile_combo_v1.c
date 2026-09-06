#include "profile_combo_v1.h"
#include <string.h>

static uint16_t u16(const uint8_t *p) {return (uint16_t)p[0] | ((uint16_t)p[1] << 8u);}

noah_profile_codec_v1_result_t noah_profile_combo_v1_decode_row(const uint8_t bytes[28], const noah_profile_action_v1_limits_t *limits, noah_profile_combo_v1_row_t *row) {
    if (!bytes || !row) return NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT;
    memset(row, 0, sizeof(*row));
    if (bytes[0] < 2u || bytes[0] > 4u || ((bytes[1] & ~7u) || (bytes[1] & 3u) == 3u) || bytes[6] || bytes[7]) return NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS;
    row->input_count = bytes[0]; row->flags = bytes[1]; row->term_ms = u16(&bytes[2]); row->hold_term_ms = u16(&bytes[4]);
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

bool noah_profile_combo_v1_read_row(const noah_profile_reader_t *reader, size_t blob_base_offset, const noah_profile_combo_v1_view_t *view, uint8_t index, noah_profile_combo_v1_row_t *row) {
    uint8_t bytes[28];
    if (!view || index >= view->row_count || !noah_profile_reader_read(reader, blob_base_offset + view->payload_offset + 4u + (size_t)index * 28u, bytes, sizeof(bytes))) return false;
    noah_profile_action_v1_limits_t limits = noah_profile_action_v1_default_limits();
    return noah_profile_combo_v1_decode_row(bytes, &limits, row) == NOAH_PROFILE_CODEC_V1_OK;
}
