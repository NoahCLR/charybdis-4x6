#include "profile_combo_v1.h"
#include <string.h>

static uint16_t u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8u);
}

noah_profile_codec_v1_result_t noah_profile_combo_v1_decode_header(const uint8_t *bytes, uint16_t payload_length, noah_profile_combo_v1_header_t *header) {
    if (!bytes || !header) return NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT;
    memset(header, 0, sizeof(*header));
    if (payload_length < NOAH_PROFILE_COMBO_V2_HEADER_SIZE) return NOAH_PROFILE_CODEC_V1_TRUNCATED;
    if (bytes[0] > NOAH_PROFILE_COMBO_V1_MAX_ROWS || bytes[1] || bytes[2] || bytes[3]) return NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS;
    if (payload_length != NOAH_PROFILE_COMBO_V2_HEADER_SIZE + (uint16_t)bytes[0] * NOAH_PROFILE_COMBO_V1_ROW_SIZE) return NOAH_PROFILE_CODEC_V1_TRUNCATED;
    header->row_count = bytes[0];
    header->default_term_ms = u16(&bytes[4]);
    header->hold_term_ms    = u16(&bytes[6]);
    // QMK fires a combo only inside its window: a zero default would
    // silently disable every combo that follows it.
    if (header->default_term_ms == 0u) return NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS;
    return NOAH_PROFILE_CODEC_V1_OK;
}

noah_profile_codec_v1_result_t noah_profile_combo_v1_decode_row(const uint8_t bytes[28], const noah_profile_action_v1_limits_t *limits, noah_profile_combo_v1_row_t *row) {
    if (!bytes || !row) return NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT;
    memset(row, 0, sizeof(*row));
    if (bytes[0] < 2u || bytes[0] > 4u || ((bytes[1] & ~7u) || (bytes[1] & 3u) == 3u) || bytes[6] || bytes[7]) return NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS;
    if (bytes[4] || bytes[5]) return NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS;
    row->input_count = bytes[0];
    row->flags       = bytes[1];
    row->term_ms     = u16(&bytes[2]);
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

uint8_t noah_profile_combo_v1_row_count(noah_profile_domain_range_t payload) {
    return payload.length < NOAH_PROFILE_COMBO_V2_HEADER_SIZE ? 0u : (uint8_t)((payload.length - NOAH_PROFILE_COMBO_V2_HEADER_SIZE) / NOAH_PROFILE_COMBO_V1_ROW_SIZE);
}

bool noah_profile_combo_v1_read_header(const noah_profile_reader_t *reader, size_t blob_base_offset, noah_profile_domain_range_t payload, noah_profile_combo_v1_header_t *header) {
    uint8_t bytes[NOAH_PROFILE_COMBO_V2_HEADER_SIZE];
    if (payload.length < NOAH_PROFILE_COMBO_V2_HEADER_SIZE || blob_base_offset > SIZE_MAX - payload.offset || !noah_profile_reader_read(reader, blob_base_offset + payload.offset, bytes, sizeof(bytes))) return false;
    return noah_profile_combo_v1_decode_header(bytes, payload.length, header) == NOAH_PROFILE_CODEC_V1_OK;
}

noah_profile_combo_v1_iteration_t noah_profile_combo_v1_iteration_step(noah_profile_combo_v1_validation_t *state, const noah_profile_reader_t *reader, size_t base, uint16_t length, const noah_profile_action_v1_limits_t *limits, noah_profile_combo_v1_header_t *header, noah_profile_combo_v1_row_t *row, noah_profile_codec_v1_result_t *detail) {
    noah_profile_codec_v1_result_t result = NOAH_PROFILE_CODEC_V1_READ_ERROR;
    if (!state || !reader || !header || !row || base > SIZE_MAX - length) {
        result = NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT;
    } else if (state->phase == 0u) {
        if (length < NOAH_PROFILE_COMBO_V2_HEADER_SIZE) result = NOAH_PROFILE_CODEC_V1_TRUNCATED;
        else if (noah_profile_reader_read(reader, base, state->bytes, NOAH_PROFILE_COMBO_V2_HEADER_SIZE)) {
            result = noah_profile_combo_v1_decode_header(state->bytes, length, header);
            if (result == NOAH_PROFILE_CODEC_V1_OK) {
                state->row_count = header->row_count;
                state->phase = 1u;
                return NOAH_PROFILE_COMBO_V1_HEADER;
            }
        }
    } else if (state->row_index == state->row_count) {
        return NOAH_PROFILE_COMBO_V1_COMPLETE;
    } else {
        size_t offset = NOAH_PROFILE_COMBO_V2_HEADER_SIZE + (size_t)state->row_index * NOAH_PROFILE_COMBO_V1_ROW_SIZE;
        if (state->phase == 1u) {
            if (noah_profile_reader_read(reader, base + offset, state->bytes, 12u)) {
                state->phase = 2u;
                return NOAH_PROFILE_COMBO_V1_ITERATING;
            }
        } else if (noah_profile_reader_read(reader, base + offset + 12u, state->bytes + 12, 16u)) {
            result = noah_profile_combo_v1_decode_row(state->bytes, limits, row);
            if (result == NOAH_PROFILE_CODEC_V1_OK) {
                state->row_index++;
                state->phase = 1u;
                return NOAH_PROFILE_COMBO_V1_ROW;
            }
        }
    }
    if (detail) *detail = result;
    return NOAH_PROFILE_COMBO_V1_REJECTED;
}

bool noah_profile_combo_v1_read_row(const noah_profile_reader_t *reader, size_t blob_base_offset, noah_profile_domain_range_t payload, uint8_t index, noah_profile_combo_v1_row_t *row) {
    uint8_t row_count = noah_profile_combo_v1_row_count(payload);
    if (index >= row_count || blob_base_offset > SIZE_MAX - payload.offset) return false;
    noah_profile_combo_v1_validation_t state = {.phase = 1u, .row_index = index, .row_count = row_count};
    noah_profile_combo_v1_header_t header;
    noah_profile_action_v1_limits_t limits = noah_profile_action_v1_default_limits();
    size_t base = blob_base_offset + payload.offset;
    if (noah_profile_combo_v1_iteration_step(&state, reader, base, payload.length, &limits, &header, row, NULL) != NOAH_PROFILE_COMBO_V1_ITERATING) return false;
    return noah_profile_combo_v1_iteration_step(&state, reader, base, payload.length, &limits, &header, row, NULL) == NOAH_PROFILE_COMBO_V1_ROW;
}
