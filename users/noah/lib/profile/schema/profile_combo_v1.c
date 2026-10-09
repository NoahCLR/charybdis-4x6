#include "profile_combo_v1.h"
#include <string.h>

static uint16_t u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8u);
}

static uint32_t u32(const uint8_t *p) {
    return (uint32_t)u16(p) | ((uint32_t)u16(&p[2]) << 16u);
}

noah_profile_codec_v1_result_t noah_profile_combo_v1_decode_header(const uint8_t *bytes, uint16_t payload_length, noah_profile_combo_v1_header_t *header) {
    if (!bytes || !header) return NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT;
    memset(header, 0, sizeof(*header));
    if (payload_length < NOAH_PROFILE_COMBO_V2_HEADER_SIZE) return NOAH_PROFILE_CODEC_V1_TRUNCATED;
    if (bytes[1] || bytes[2] || bytes[3]) return NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS;
    if (bytes[0] > NOAH_PROFILE_COMBO_V1_MAX_ROWS) return NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED;
    // Every row has at least its fixed part and two inputs.
    if ((uint32_t)payload_length < NOAH_PROFILE_COMBO_V2_HEADER_SIZE + (uint32_t)bytes[0] * (NOAH_PROFILE_COMBO_V3_ROW_FIXED_SIZE + 4u * NOAH_PROFILE_COMBO_V1_MIN_INPUTS)) return NOAH_PROFILE_CODEC_V1_TRUNCATED;
    header->row_count       = bytes[0];
    header->default_term_ms = u16(&bytes[4]);
    header->hold_term_ms    = u16(&bytes[6]);
    // QMK fires a combo only inside its window: a zero default would
    // silently disable every combo that follows it.
    if (header->default_term_ms == 0u) return NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS;
    return NOAH_PROFILE_CODEC_V1_OK;
}

static bool input_count_valid(uint8_t count) {
    return count >= NOAH_PROFILE_COMBO_V1_MIN_INPUTS && count <= NOAH_PROFILE_COMBO_V1_MAX_INPUTS;
}

noah_profile_codec_v1_result_t noah_profile_combo_v1_decode_row(const uint8_t *bytes, const noah_profile_action_v1_limits_t *limits, noah_profile_combo_v1_row_t *row) {
    if (!bytes || !row || !limits) return NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT;
    memset(row, 0, sizeof(*row));
    if (!input_count_valid(bytes[0])) return NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED;
    if ((bytes[1] & (uint8_t)~NOAH_PROFILE_COMBO_V3_KNOWN_FLAGS) || (bytes[1] & 3u) == 3u) return NOAH_PROFILE_CODEC_V1_RESERVED_FLAGS;
    row->input_count    = bytes[0];
    row->flags          = bytes[1];
    row->term_ms        = u16(&bytes[2]);
    row->allowed_layers = u32(&bytes[4]);
    // The allowed layers name only bank layers.
    if (limits->max_logical_layers < 32u && (row->allowed_layers >> limits->max_logical_layers) != 0u) return NOAH_PROFILE_CODEC_V1_INVALID_OPERAND;
    if (noah_profile_action_v1_decode(&bytes[8], 4u, limits, &row->output, NULL) != NOAH_PROFILE_CODEC_V1_OK || row->output.kind == NOAH_PROFILE_ACTION_V1_NONE || (row->output.kind == NOAH_PROFILE_ACTION_V1_QMK_KEYCODE && row->output.operand == 0u)) return NOAH_PROFILE_CODEC_V1_INVALID_ACTION;
    for (uint8_t input = 0; input < row->input_count; input++) {
        const uint8_t *action = &bytes[NOAH_PROFILE_COMBO_V3_ROW_FIXED_SIZE + 4u * input];
        if (noah_profile_action_v1_decode(action, 4u, limits, &row->inputs[input], NULL) != NOAH_PROFILE_CODEC_V1_OK || row->inputs[input].kind == NOAH_PROFILE_ACTION_V1_NONE || (row->inputs[input].kind == NOAH_PROFILE_ACTION_V1_QMK_KEYCODE && row->inputs[input].operand <= 1u)) return NOAH_PROFILE_CODEC_V1_INVALID_ACTION;
        for (uint8_t prior = 0; prior < input; prior++) {
            if (memcmp(action, &bytes[NOAH_PROFILE_COMBO_V3_ROW_FIXED_SIZE + 4u * prior], 4u) == 0) return NOAH_PROFILE_CODEC_V1_DUPLICATE_TARGET;
        }
    }
    return NOAH_PROFILE_CODEC_V1_OK;
}

bool noah_profile_combo_v1_read_header(const noah_profile_reader_t *reader, size_t blob_base_offset, noah_profile_domain_range_t payload, noah_profile_combo_v1_header_t *header) {
    uint8_t bytes[NOAH_PROFILE_COMBO_V2_HEADER_SIZE];
    if (payload.length < NOAH_PROFILE_COMBO_V2_HEADER_SIZE || blob_base_offset > SIZE_MAX - payload.offset || !noah_profile_reader_read(reader, blob_base_offset + payload.offset, bytes, sizeof(bytes))) return false;
    return noah_profile_combo_v1_decode_header(bytes, payload.length, header) == NOAH_PROFILE_CODEC_V1_OK;
}

static bool read_at(const noah_profile_reader_t *reader, size_t base, uint16_t length, uint16_t offset, uint8_t *target, uint8_t count) {
    return offset <= length && count <= length - offset && noah_profile_reader_read(reader, base + offset, target, count);
}

noah_profile_combo_v1_iteration_t noah_profile_combo_v1_iteration_step(noah_profile_combo_v1_validation_t *state, const noah_profile_reader_t *reader, size_t base, uint16_t length, const noah_profile_action_v1_limits_t *limits, noah_profile_combo_v1_header_t *header, noah_profile_combo_v1_row_t *row, noah_profile_codec_v1_result_t *detail) {
    noah_profile_codec_v1_result_t result = NOAH_PROFILE_CODEC_V1_READ_ERROR;
    if (!state || !reader || !limits || !header || !row || base > SIZE_MAX - length) {
        result = NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT;
    } else if (state->phase == 0u) {
        if (length < NOAH_PROFILE_COMBO_V2_HEADER_SIZE) {
            result = NOAH_PROFILE_CODEC_V1_TRUNCATED;
        } else if (read_at(reader, base, length, 0u, state->bytes, NOAH_PROFILE_COMBO_V2_HEADER_SIZE)) {
            result = noah_profile_combo_v1_decode_header(state->bytes, length, header);
            if (result == NOAH_PROFILE_CODEC_V1_OK) {
                state->row_count  = header->row_count;
                state->offset     = NOAH_PROFILE_COMBO_V2_HEADER_SIZE;
                state->row_offset = state->offset;
                state->phase      = 1u;
                return NOAH_PROFILE_COMBO_V1_HEADER;
            }
        }
    } else if (state->row_index == state->row_count) {
        if (state->offset == length) return NOAH_PROFILE_COMBO_V1_COMPLETE;
        result = NOAH_PROFILE_CODEC_V1_TRAILING_BYTES;
    } else if (state->phase == 1u) {
        state->row_offset = state->offset;
        if (length - state->offset < NOAH_PROFILE_COMBO_V3_ROW_FIXED_SIZE) {
            result = NOAH_PROFILE_CODEC_V1_TRUNCATED;
        } else if (read_at(reader, base, length, state->offset, state->bytes, NOAH_PROFILE_COMBO_V3_ROW_FIXED_SIZE)) {
            if (!input_count_valid(state->bytes[0])) {
                result = NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED;
            } else {
                state->offset      = (uint16_t)(state->offset + NOAH_PROFILE_COMBO_V3_ROW_FIXED_SIZE);
                state->inputs_read = 0u;
                state->phase       = 2u;
                return NOAH_PROFILE_COMBO_V1_ITERATING;
            }
        }
    } else {
        uint8_t count = (uint8_t)(state->bytes[0] - state->inputs_read);
        if (count > NOAH_PROFILE_COMBO_V3_INPUTS_PER_STEP) count = NOAH_PROFILE_COMBO_V3_INPUTS_PER_STEP;
        if (length - state->offset < 4u * count) {
            result = NOAH_PROFILE_CODEC_V1_TRUNCATED;
        } else if (read_at(reader, base, length, state->offset, &state->bytes[NOAH_PROFILE_COMBO_V3_ROW_FIXED_SIZE + 4u * state->inputs_read], (uint8_t)(4u * count))) {
            state->offset      = (uint16_t)(state->offset + 4u * count);
            state->inputs_read = (uint8_t)(state->inputs_read + count);
            if (state->inputs_read < state->bytes[0]) return NOAH_PROFILE_COMBO_V1_ITERATING;
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
    noah_profile_combo_v1_validation_t state  = {0};
    noah_profile_combo_v1_header_t     header;
    noah_profile_action_v1_limits_t    limits = noah_profile_action_v1_default_limits();
    if (!row || blob_base_offset > SIZE_MAX - payload.offset) return false;
    for (;;) {
        noah_profile_combo_v1_iteration_t result = noah_profile_combo_v1_iteration_step(&state, reader, blob_base_offset + payload.offset, payload.length, &limits, &header, row, NULL);
        if (result == NOAH_PROFILE_COMBO_V1_ROW && state.row_index == (uint16_t)index + 1u) return true;
        if (result == NOAH_PROFILE_COMBO_V1_COMPLETE || result == NOAH_PROFILE_COMBO_V1_REJECTED) return false;
    }
}
