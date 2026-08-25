// ───────────────────────────────────────────────────────────────────────────
// Canonical Key-Behavior Domain — Profile Wire v1.0
// ─────────────────────────────────────────────────────────────────────────────

#include "key_behavior_domain_v1.h"

#include <stdbool.h>
#include <string.h>

#include "../storage/profile_storage_layout.h"

enum {
    KEY_BEHAVIOR_TABLE_ROWS = 1u,
};

typedef struct {
    noah_profile_reader_t           reader;
    size_t                          base_offset;
    size_t                          length;
    noah_key_behavior_limits_v1_t   limits;
    noah_profile_action_v1_limits_t action_limits;
} decode_context_t;

static uint16_t read_u16(const uint8_t *source) {
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8u);
}

static void write_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
}

static void clear_error(noah_profile_codec_v1_error_t *error) {
    if (!error) {
        return;
    }
    memset(error, 0, sizeof(*error));
    error->domain_index = UINT8_MAX;
    error->domain_id    = NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS;
    error->table_id     = KEY_BEHAVIOR_TABLE_ROWS;
    error->row_index    = UINT8_MAX;
    error->step_index   = UINT8_MAX;
    error->field_id     = UINT8_MAX;
}

static noah_profile_codec_v1_result_t fail(noah_profile_codec_v1_error_t *error, noah_profile_codec_v1_result_t code, size_t offset, uint8_t row_index, uint8_t step_index, uint8_t field_id) {
    if (error) {
        error->code       = code;
        error->offset     = offset;
        error->domain_id  = NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS;
        error->table_id   = KEY_BEHAVIOR_TABLE_ROWS;
        error->row_index  = row_index;
        error->step_index = step_index;
        error->field_id   = field_id;
    }
    return code;
}

noah_key_behavior_limits_v1_t noah_key_behavior_domain_v1_default_limits(void) {
    noah_key_behavior_limits_v1_t limits = {
        .max_rows              = NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS,
        .max_populated_steps   = NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_POPULATED_STEPS,
        .max_tap_steps_per_row = NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_TAP_STEPS_PER_ROW,
        .max_repeat_hz         = NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_REPEAT_HZ,
        .max_payload_size      = NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_PAYLOAD_SIZE,
    };
    return limits;
}

static noah_profile_codec_v1_result_t resolve_limits(const noah_key_behavior_limits_v1_t *supplied, const noah_profile_action_v1_limits_t *supplied_action, noah_key_behavior_limits_v1_t *limits, noah_profile_action_v1_limits_t *action_limits, noah_profile_codec_v1_error_t *error) {
    *limits        = supplied ? *supplied : noah_key_behavior_domain_v1_default_limits();
    *action_limits = supplied_action ? *supplied_action : noah_profile_action_v1_default_limits();

    if (limits->max_rows == 0u || limits->max_rows > NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS || limits->max_populated_steps == 0u || limits->max_populated_steps > NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_POPULATED_STEPS || limits->max_tap_steps_per_row == 0u || limits->max_tap_steps_per_row > NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_TAP_STEPS_PER_ROW || limits->max_repeat_hz == 0u || limits->max_repeat_hz > NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_REPEAT_HZ || limits->max_payload_size == 0u || limits->max_payload_size > NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_PAYLOAD_SIZE) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, 0u, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    }
    if (action_limits->max_logical_layers > UINT32_C(0x10000) || action_limits->max_pd_modes > UINT32_C(0x10000) || action_limits->max_via_macro_slots > UINT32_C(0x10000) || action_limits->max_hardcoded_macro_slots > UINT32_C(0x10000)) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, 0u, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    }
    return NOAH_PROFILE_CODEC_V1_OK;
}

static noah_profile_codec_v1_result_t read_bytes(const decode_context_t *context, size_t offset, uint8_t *target, size_t length, uint8_t row_index, uint8_t step_index, uint8_t field_id, noah_profile_codec_v1_error_t *error) {
    if (offset > context->length || length > context->length - offset) {
        return fail(error, NOAH_PROFILE_CODEC_V1_TRUNCATED, offset, row_index, step_index, field_id);
    }
    if (!noah_profile_reader_read(&context->reader, context->base_offset + offset, target, length)) {
        return fail(error, NOAH_PROFILE_CODEC_V1_READ_ERROR, offset, row_index, step_index, field_id);
    }
    return NOAH_PROFILE_CODEC_V1_OK;
}

static noah_profile_codec_v1_result_t map_action_error(const noah_profile_codec_v1_error_t *action_error, size_t base_offset, uint8_t row_index, uint8_t step_index, uint8_t field_id, noah_profile_codec_v1_error_t *error) {
    return fail(error, action_error->code, base_offset + action_error->offset, row_index, step_index, field_id);
}

static noah_profile_codec_v1_result_t decode_action_bytes(const uint8_t bytes[NOAH_PROFILE_BLOB_V1_ACTION_SIZE], size_t offset, const noah_profile_action_v1_limits_t *limits, bool target, uint8_t row_index, uint8_t step_index, uint8_t field_id, noah_profile_action_v1_t *action, noah_profile_codec_v1_error_t *error) {
    noah_profile_codec_v1_error_t action_error;
    noah_profile_codec_v1_result_t result = noah_profile_action_v1_decode(bytes, NOAH_PROFILE_BLOB_V1_ACTION_SIZE, limits, action, &action_error);

    if (result != NOAH_PROFILE_CODEC_V1_OK) {
        return map_action_error(&action_error, offset, row_index, step_index, field_id, error);
    }
    if (action->kind == NOAH_PROFILE_ACTION_V1_NONE) {
        return fail(error, target ? NOAH_PROFILE_CODEC_V1_INVALID_TARGET : NOAH_PROFILE_CODEC_V1_INVALID_ACTION, offset, row_index, step_index, field_id);
    }
    return NOAH_PROFILE_CODEC_V1_OK;
}

static noah_profile_codec_v1_result_t read_action(const decode_context_t *context, size_t offset, size_t end, bool target, uint8_t row_index, uint8_t step_index, uint8_t field_id, noah_profile_action_v1_t *action, noah_profile_codec_v1_error_t *error) {
    uint8_t bytes[NOAH_PROFILE_BLOB_V1_ACTION_SIZE];

    if (offset > end || NOAH_PROFILE_BLOB_V1_ACTION_SIZE > end - offset) {
        return fail(error, NOAH_PROFILE_CODEC_V1_TRUNCATED, offset, row_index, step_index, field_id);
    }
    if (read_bytes(context, offset, bytes, sizeof(bytes), row_index, step_index, field_id, error) != NOAH_PROFILE_CODEC_V1_OK) {
        return error ? error->code : NOAH_PROFILE_CODEC_V1_READ_ERROR;
    }
    return decode_action_bytes(bytes, offset, &context->action_limits, target, row_index, step_index, field_id, action, error);
}

static noah_profile_codec_v1_result_t validate_hold_values(const noah_key_behavior_hold_v1_t *hold, const noah_key_behavior_limits_v1_t *limits, const noah_profile_action_v1_limits_t *action_limits, uint8_t row_index, uint8_t step_index, uint8_t mode_field, uint8_t repeat_field, uint8_t action_field, noah_profile_codec_v1_error_t *error) {
    uint8_t                          action_bytes[NOAH_PROFILE_BLOB_V1_ACTION_SIZE];
    noah_profile_codec_v1_error_t    action_error;
    noah_profile_codec_v1_result_t   result;

    if (hold->mode < NOAH_KEY_BEHAVIOR_HOLD_V1_PRESS_AND_HOLD_UNTIL_RELEASE || hold->mode > NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_ON_RELEASE_AFTER_HOLD) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_HOLD_MODE, 0u, row_index, step_index, mode_field);
    }
    if ((hold->mode == NOAH_KEY_BEHAVIOR_HOLD_V1_REPEAT_WHILE_HELD && (hold->repeat_hz == 0u || hold->repeat_hz > limits->max_repeat_hz)) || (hold->mode != NOAH_KEY_BEHAVIOR_HOLD_V1_REPEAT_WHILE_HELD && hold->repeat_hz != 0u)) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_REPEAT_RATE, 0u, row_index, step_index, repeat_field);
    }
    result = noah_profile_action_v1_encode(&hold->action, action_limits, action_bytes, &action_error);
    if (result != NOAH_PROFILE_CODEC_V1_OK) {
        return fail(error, result, action_error.offset, row_index, step_index, action_field);
    }
    if (hold->action.kind == NOAH_PROFILE_ACTION_V1_NONE) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ACTION, 0u, row_index, step_index, action_field);
    }
    return NOAH_PROFILE_CODEC_V1_OK;
}

static noah_profile_codec_v1_result_t decode_hold(const decode_context_t *context, size_t offset, size_t end, uint8_t row_index, uint8_t step_index, bool long_hold, noah_key_behavior_hold_v1_t *hold, noah_profile_codec_v1_error_t *error) {
    uint8_t fields[2];
    uint8_t mode_field   = long_hold ? NOAH_KEY_BEHAVIOR_FIELD_V1_LONG_HOLD_MODE : NOAH_KEY_BEHAVIOR_FIELD_V1_HOLD_MODE;
    uint8_t repeat_field = long_hold ? NOAH_KEY_BEHAVIOR_FIELD_V1_LONG_HOLD_REPEAT : NOAH_KEY_BEHAVIOR_FIELD_V1_HOLD_REPEAT;
    uint8_t action_field = long_hold ? NOAH_KEY_BEHAVIOR_FIELD_V1_LONG_HOLD_ACTION : NOAH_KEY_BEHAVIOR_FIELD_V1_HOLD_ACTION;

    if (offset > end || NOAH_KEY_BEHAVIOR_DOMAIN_V1_HOLD_SIZE > end - offset) {
        return fail(error, NOAH_PROFILE_CODEC_V1_TRUNCATED, offset, row_index, step_index, mode_field);
    }
    if (read_bytes(context, offset, fields, sizeof(fields), row_index, step_index, mode_field, error) != NOAH_PROFILE_CODEC_V1_OK) {
        return error ? error->code : NOAH_PROFILE_CODEC_V1_READ_ERROR;
    }
    hold->mode      = fields[0];
    hold->repeat_hz = fields[1];
    if (hold->mode < NOAH_KEY_BEHAVIOR_HOLD_V1_PRESS_AND_HOLD_UNTIL_RELEASE || hold->mode > NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_ON_RELEASE_AFTER_HOLD) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_HOLD_MODE, offset, row_index, step_index, mode_field);
    }
    if ((hold->mode == NOAH_KEY_BEHAVIOR_HOLD_V1_REPEAT_WHILE_HELD && (hold->repeat_hz == 0u || hold->repeat_hz > context->limits.max_repeat_hz)) || (hold->mode != NOAH_KEY_BEHAVIOR_HOLD_V1_REPEAT_WHILE_HELD && hold->repeat_hz != 0u)) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_REPEAT_RATE, offset + 1u, row_index, step_index, repeat_field);
    }
    return read_action(context, offset + 2u, end, false, row_index, step_index, action_field, &hold->action, error);
}

static noah_profile_codec_v1_result_t decode_step(const decode_context_t *context, size_t offset, size_t row_end, uint8_t row_index, uint8_t step_index, noah_key_behavior_step_v1_t *step, size_t *next_offset, noah_profile_codec_v1_error_t *error) {
    uint8_t header[NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HEADER_SIZE];
    noah_profile_codec_v1_result_t result;

    memset(step, 0, sizeof(*step));
    if (offset > row_end || NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HEADER_SIZE > row_end - offset) {
        return fail(error, NOAH_PROFILE_CODEC_V1_TRUNCATED, offset, row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_INDEX);
    }
    result = read_bytes(context, offset, header, sizeof(header), row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_INDEX, error);
    if (result != NOAH_PROFILE_CODEC_V1_OK) {
        return result;
    }
    step->tap_index     = header[0];
    step->presence_mask = header[1];
    if (step->tap_index >= context->limits.max_tap_steps_per_row) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_TAP_INDEX, offset, row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_INDEX);
    }
    if (step->presence_mask == 0u) {
        return fail(error, NOAH_PROFILE_CODEC_V1_EMPTY_STEP, offset + 1u, row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_PRESENCE_MASK);
    }
    if ((step->presence_mask & (uint8_t)~NOAH_KEY_BEHAVIOR_DOMAIN_V1_KNOWN_STEP_MASK) != 0u) {
        return fail(error, NOAH_PROFILE_CODEC_V1_RESERVED_FLAGS, offset + 1u, row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_PRESENCE_MASK);
    }

    offset += NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HEADER_SIZE;
    if ((step->presence_mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP) != 0u) {
        result = read_action(context, offset, row_end, false, row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_ACTION, &step->tap, error);
        if (result != NOAH_PROFILE_CODEC_V1_OK) {
            return result;
        }
        offset += NOAH_PROFILE_BLOB_V1_ACTION_SIZE;
    }
    if ((step->presence_mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_HOLD) != 0u) {
        result = decode_hold(context, offset, row_end, row_index, step_index, false, &step->hold, error);
        if (result != NOAH_PROFILE_CODEC_V1_OK) {
            return result;
        }
        offset += NOAH_KEY_BEHAVIOR_DOMAIN_V1_HOLD_SIZE;
    }
    if ((step->presence_mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_LONG_HOLD) != 0u) {
        result = decode_hold(context, offset, row_end, row_index, step_index, true, &step->long_hold, error);
        if (result != NOAH_PROFILE_CODEC_V1_OK) {
            return result;
        }
        offset += NOAH_KEY_BEHAVIOR_DOMAIN_V1_HOLD_SIZE;
    }
    *next_offset = offset;
    return NOAH_PROFILE_CODEC_V1_OK;
}

static noah_profile_codec_v1_result_t decode_row(const decode_context_t *context, size_t offset, uint8_t row_index, noah_key_behavior_row_v1_view_t *row, size_t *next_offset, noah_profile_codec_v1_error_t *error) {
    uint8_t                         length_bytes[2];
    uint8_t                         fixed[NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FIXED_SIZE];
    uint16_t                        body_length;
    size_t                          body_start;
    size_t                          row_end;
    size_t                          step_offset;
    int16_t                         previous_tap_index = -1;
    noah_profile_codec_v1_result_t  result;

    memset(row, 0, sizeof(*row));
    if (offset > context->length || NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_LENGTH_SIZE > context->length - offset) {
        return fail(error, NOAH_PROFILE_CODEC_V1_TRUNCATED, offset, row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_ROW_LENGTH);
    }
    result = read_bytes(context, offset, length_bytes, sizeof(length_bytes), row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_ROW_LENGTH, error);
    if (result != NOAH_PROFILE_CODEC_V1_OK) {
        return result;
    }
    body_length = read_u16(length_bytes);
    body_start  = offset + NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_LENGTH_SIZE;
    if (body_length < NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FIXED_SIZE || body_length > context->length - body_start) {
        return fail(error, NOAH_PROFILE_CODEC_V1_TRUNCATED, offset, row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_ROW_LENGTH);
    }
    row_end = body_start + body_length;
    result  = read_bytes(context, body_start, fixed, sizeof(fixed), row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_TARGET, error);
    if (result != NOAH_PROFILE_CODEC_V1_OK) {
        return result;
    }
    result = decode_action_bytes(fixed, body_start, &context->action_limits, true, row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_TARGET, &row->target, error);
    if (result != NOAH_PROFILE_CODEC_V1_OK) {
        return result;
    }
    memcpy(row->target_bytes, fixed, sizeof(row->target_bytes));
    row->row_index        = row_index;
    row->tap_hold_term     = read_u16(&fixed[4]);
    row->longer_hold_term  = read_u16(&fixed[6]);
    row->multi_tap_term    = read_u16(&fixed[8]);
    row->flags             = fixed[10];
    row->step_count        = fixed[11];
    row->row_offset        = offset;
    row->steps_offset      = body_start + NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FIXED_SIZE;
    row->row_end           = row_end;
    if ((row->flags & (uint8_t)~NOAH_KEY_BEHAVIOR_DOMAIN_V1_KNOWN_ROW_FLAGS) != 0u) {
        return fail(error, NOAH_PROFILE_CODEC_V1_RESERVED_FLAGS, body_start + 10u, row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_ROW_FLAGS);
    }
    if (row->step_count > context->limits.max_tap_steps_per_row) {
        return fail(error, NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED, body_start + 11u, row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_ROW_STEP_COUNT);
    }

    step_offset = row->steps_offset;
    for (uint8_t step_index = 0u; step_index < row->step_count; step_index++) {
        noah_key_behavior_step_v1_t step;
        size_t                      next_step;

        result = decode_step(context, step_offset, row_end, row_index, step_index, &step, &next_step, error);
        if (result != NOAH_PROFILE_CODEC_V1_OK) {
            return result;
        }
        if ((int16_t)step.tap_index == previous_tap_index) {
            return fail(error, NOAH_PROFILE_CODEC_V1_DUPLICATE_STEP, step_offset, row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_INDEX);
        }
        if ((int16_t)step.tap_index < previous_tap_index) {
            return fail(error, NOAH_PROFILE_CODEC_V1_STEP_ORDER, step_offset, row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_INDEX);
        }
        previous_tap_index = step.tap_index;
        step_offset        = next_step;
    }
    if (step_offset != row_end) {
        return fail(error, NOAH_PROFILE_CODEC_V1_ROW_LENGTH, step_offset, row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_ROW_LENGTH);
    }
    *next_offset = row_end;
    return NOAH_PROFILE_CODEC_V1_OK;
}

noah_profile_codec_v1_result_t noah_key_behavior_domain_v1_decode_reader(const noah_profile_reader_t *reader, size_t base_offset, size_t length, const noah_key_behavior_limits_v1_t *supplied_limits, const noah_profile_action_v1_limits_t *supplied_action_limits, noah_key_behavior_domain_v1_t *domain, noah_profile_codec_v1_error_t *error) {
    noah_key_behavior_domain_v1_t candidate;
    decode_context_t              context;
    uint8_t                       header[NOAH_KEY_BEHAVIOR_DOMAIN_V1_HEADER_SIZE];
    size_t                        offset;
    size_t                        actual_steps = 0u;
    uint8_t                       previous_target[NOAH_PROFILE_BLOB_V1_ACTION_SIZE];
    bool                          have_previous = false;
    noah_profile_codec_v1_result_t result;

    clear_error(error);
    if (!reader || !reader->read || !domain || base_offset > reader->length || length > reader->length - base_offset) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, 0u, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    }
    memset(&candidate, 0, sizeof(candidate));
    memset(domain, 0, sizeof(*domain));
    result = resolve_limits(supplied_limits, supplied_action_limits, &candidate.limits, &candidate.action_limits, error);
    if (result != NOAH_PROFILE_CODEC_V1_OK) {
        return result;
    }
    if (length < NOAH_KEY_BEHAVIOR_DOMAIN_V1_HEADER_SIZE) {
        return fail(error, NOAH_PROFILE_CODEC_V1_TRUNCATED, length, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    }
    if (length > candidate.limits.max_payload_size) {
        return fail(error, NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED, length, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    }

    candidate.reader      = *reader;
    candidate.base_offset = base_offset;
    candidate.byte_length = length;
    context.reader        = *reader;
    context.base_offset   = base_offset;
    context.length        = length;
    context.limits        = candidate.limits;
    context.action_limits = candidate.action_limits;
    result                = read_bytes(&context, 0u, header, sizeof(header), UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER, error);
    if (result != NOAH_PROFILE_CODEC_V1_OK) {
        return result;
    }
    candidate.row_count            = header[0];
    candidate.populated_step_count = header[1];
    if (header[2] != 0u || header[3] != 0u) {
        return fail(error, NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS, header[2] != 0u ? 2u : 3u, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    }
    if (candidate.row_count > candidate.limits.max_rows || candidate.populated_step_count > candidate.limits.max_populated_steps) {
        return fail(error, NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED, 0u, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    }

    offset = NOAH_KEY_BEHAVIOR_DOMAIN_V1_HEADER_SIZE;
    for (uint8_t row_index = 0u; row_index < candidate.row_count; row_index++) {
        noah_key_behavior_row_v1_view_t row;
        size_t                          next_row;

        result = decode_row(&context, offset, row_index, &row, &next_row, error);
        if (result != NOAH_PROFILE_CODEC_V1_OK) {
            return result;
        }
        if (have_previous) {
            int order = memcmp(previous_target, row.target_bytes, sizeof(previous_target));
            if (order == 0) {
                return fail(error, NOAH_PROFILE_CODEC_V1_DUPLICATE_TARGET, offset + NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_LENGTH_SIZE, row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_TARGET);
            }
            if (order > 0) {
                return fail(error, NOAH_PROFILE_CODEC_V1_ROW_ORDER, offset + NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_LENGTH_SIZE, row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_TARGET);
            }
        }
        memcpy(previous_target, row.target_bytes, sizeof(previous_target));
        have_previous = true;
        actual_steps += row.step_count;
        if (actual_steps > candidate.limits.max_populated_steps) {
            return fail(error, NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED, offset, row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_ROW_STEP_COUNT);
        }
        offset = next_row;
    }
    if (offset != length) {
        return fail(error, NOAH_PROFILE_CODEC_V1_TRAILING_BYTES, offset, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    }
    if (actual_steps != candidate.populated_step_count) {
        return fail(error, NOAH_PROFILE_CODEC_V1_COUNT_MISMATCH, 1u, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    }
    *domain = candidate;
    return NOAH_PROFILE_CODEC_V1_OK;
}

noah_profile_codec_v1_result_t noah_key_behavior_domain_v1_decode(const uint8_t *bytes, size_t length, const noah_key_behavior_limits_v1_t *limits, const noah_profile_action_v1_limits_t *action_limits, noah_key_behavior_domain_v1_t *domain, noah_profile_codec_v1_error_t *error) {
    noah_profile_reader_t reader = noah_profile_reader_from_memory(bytes, length);
    return noah_key_behavior_domain_v1_decode_reader(&reader, 0u, length, limits, action_limits, domain, error);
}

static noah_profile_codec_v1_result_t context_from_domain(const noah_key_behavior_domain_v1_t *domain, decode_context_t *context, noah_profile_codec_v1_error_t *error) {
    if (!domain || !domain->reader.read || domain->base_offset > domain->reader.length || domain->byte_length > domain->reader.length - domain->base_offset) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, 0u, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    }
    context->reader        = domain->reader;
    context->base_offset   = domain->base_offset;
    context->length        = domain->byte_length;
    context->limits        = domain->limits;
    context->action_limits = domain->action_limits;
    return NOAH_PROFILE_CODEC_V1_OK;
}

noah_profile_codec_v1_result_t noah_key_behavior_domain_v1_row_at(const noah_key_behavior_domain_v1_t *domain, uint8_t row_index, noah_key_behavior_row_v1_view_t *row, noah_profile_codec_v1_error_t *error) {
    decode_context_t context;
    size_t           offset = NOAH_KEY_BEHAVIOR_DOMAIN_V1_HEADER_SIZE;

    clear_error(error);
    if (!row || context_from_domain(domain, &context, error) != NOAH_PROFILE_CODEC_V1_OK || row_index >= domain->row_count) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, 0u, row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_ROW_LENGTH);
    }
    for (uint8_t index = 0u; index <= row_index; index++) {
        size_t next_offset;
        noah_profile_codec_v1_result_t result = decode_row(&context, offset, index, row, &next_offset, error);

        if (result != NOAH_PROFILE_CODEC_V1_OK) {
            return result;
        }
        offset = next_offset;
    }
    return NOAH_PROFILE_CODEC_V1_OK;
}

noah_profile_codec_v1_result_t noah_key_behavior_domain_v1_step_at(const noah_key_behavior_domain_v1_t *domain, uint8_t row_index, uint8_t step_index, noah_key_behavior_step_v1_t *step, noah_profile_codec_v1_error_t *error) {
    decode_context_t context;
    noah_key_behavior_row_v1_view_t row;
    size_t           offset;

    clear_error(error);
    if (!step || context_from_domain(domain, &context, error) != NOAH_PROFILE_CODEC_V1_OK || row_index >= domain->row_count) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, 0u, row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_INDEX);
    }
    if (noah_key_behavior_domain_v1_row_at(domain, row_index, &row, error) != NOAH_PROFILE_CODEC_V1_OK) {
        return error ? error->code : NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT;
    }
    if (step_index >= row.step_count) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, row.steps_offset, row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_INDEX);
    }
    offset = row.steps_offset;
    for (uint8_t index = 0u; index <= step_index; index++) {
        size_t next_offset;
        noah_profile_codec_v1_result_t result = decode_step(&context, offset, row.row_end, row.row_index, index, step, &next_offset, error);

        if (result != NOAH_PROFILE_CODEC_V1_OK) {
            return result;
        }
        offset = next_offset;
    }
    return NOAH_PROFILE_CODEC_V1_OK;
}

static size_t step_encoded_size(const noah_key_behavior_step_v1_t *step) {
    size_t size = NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HEADER_SIZE;

    if ((step->presence_mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP) != 0u) {
        size += NOAH_PROFILE_BLOB_V1_ACTION_SIZE;
    }
    if ((step->presence_mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_HOLD) != 0u) {
        size += NOAH_KEY_BEHAVIOR_DOMAIN_V1_HOLD_SIZE;
    }
    if ((step->presence_mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_LONG_HOLD) != 0u) {
        size += NOAH_KEY_BEHAVIOR_DOMAIN_V1_HOLD_SIZE;
    }
    return size;
}

static noah_profile_codec_v1_result_t validate_action_value(const noah_profile_action_v1_t *action, const noah_profile_action_v1_limits_t *limits, bool target, uint8_t row_index, uint8_t step_index, uint8_t field_id, noah_profile_codec_v1_error_t *error) {
    uint8_t                       bytes[NOAH_PROFILE_BLOB_V1_ACTION_SIZE];
    noah_profile_codec_v1_error_t action_error;
    noah_profile_codec_v1_result_t result = noah_profile_action_v1_encode(action, limits, bytes, &action_error);

    if (result != NOAH_PROFILE_CODEC_V1_OK) {
        return fail(error, result, action_error.offset, row_index, step_index, field_id);
    }
    if (action->kind == NOAH_PROFILE_ACTION_V1_NONE) {
        return fail(error, target ? NOAH_PROFILE_CODEC_V1_INVALID_TARGET : NOAH_PROFILE_CODEC_V1_INVALID_ACTION, 0u, row_index, step_index, field_id);
    }
    return NOAH_PROFILE_CODEC_V1_OK;
}

static noah_profile_codec_v1_result_t validate_step_value(const noah_key_behavior_step_v1_t *step, const noah_key_behavior_limits_v1_t *limits, const noah_profile_action_v1_limits_t *action_limits, uint8_t row_index, uint8_t step_index, noah_profile_codec_v1_error_t *error) {
    noah_profile_codec_v1_result_t result;

    if (step->tap_index >= limits->max_tap_steps_per_row) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_TAP_INDEX, 0u, row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_INDEX);
    }
    if (step->presence_mask == 0u) {
        return fail(error, NOAH_PROFILE_CODEC_V1_EMPTY_STEP, 0u, row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_PRESENCE_MASK);
    }
    if ((step->presence_mask & (uint8_t)~NOAH_KEY_BEHAVIOR_DOMAIN_V1_KNOWN_STEP_MASK) != 0u) {
        return fail(error, NOAH_PROFILE_CODEC_V1_RESERVED_FLAGS, 0u, row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_PRESENCE_MASK);
    }
    if ((step->presence_mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP) != 0u) {
        result = validate_action_value(&step->tap, action_limits, false, row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_ACTION, error);
        if (result != NOAH_PROFILE_CODEC_V1_OK) {
            return result;
        }
    }
    if ((step->presence_mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_HOLD) != 0u) {
        result = validate_hold_values(&step->hold, limits, action_limits, row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_HOLD_MODE, NOAH_KEY_BEHAVIOR_FIELD_V1_HOLD_REPEAT, NOAH_KEY_BEHAVIOR_FIELD_V1_HOLD_ACTION, error);
        if (result != NOAH_PROFILE_CODEC_V1_OK) {
            return result;
        }
    }
    if ((step->presence_mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_LONG_HOLD) != 0u) {
        result = validate_hold_values(&step->long_hold, limits, action_limits, row_index, step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_LONG_HOLD_MODE, NOAH_KEY_BEHAVIOR_FIELD_V1_LONG_HOLD_REPEAT, NOAH_KEY_BEHAVIOR_FIELD_V1_LONG_HOLD_ACTION, error);
        if (result != NOAH_PROFILE_CODEC_V1_OK) {
            return result;
        }
    }
    return NOAH_PROFILE_CODEC_V1_OK;
}

static int action_canonical_compare(const noah_profile_action_v1_t *lhs, const noah_profile_action_v1_t *rhs) {
    uint8_t lhs_bytes[NOAH_PROFILE_BLOB_V1_ACTION_SIZE] = {lhs->kind, lhs->flags, (uint8_t)lhs->operand, (uint8_t)(lhs->operand >> 8u)};
    uint8_t rhs_bytes[NOAH_PROFILE_BLOB_V1_ACTION_SIZE] = {rhs->kind, rhs->flags, (uint8_t)rhs->operand, (uint8_t)(rhs->operand >> 8u)};
    return memcmp(lhs_bytes, rhs_bytes, sizeof(lhs_bytes));
}

static void write_action(uint8_t *output, const noah_profile_action_v1_t *action) {
    output[0] = action->kind;
    output[1] = action->flags;
    write_u16(&output[2], action->operand);
}

static void write_hold(uint8_t *output, const noah_key_behavior_hold_v1_t *hold) {
    output[0] = hold->mode;
    output[1] = hold->repeat_hz;
    write_action(&output[2], &hold->action);
}

noah_profile_codec_v1_result_t noah_key_behavior_domain_v1_encode(const noah_key_behavior_row_v1_t *rows, size_t row_count, const noah_key_behavior_limits_v1_t *supplied_limits, const noah_profile_action_v1_limits_t *supplied_action_limits, uint8_t *output, size_t output_capacity, size_t *written, noah_profile_codec_v1_error_t *error) {
    noah_key_behavior_limits_v1_t   limits;
    noah_profile_action_v1_limits_t action_limits;
    uint8_t                         row_order[NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS];
    uint16_t                        row_body_lengths[NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS];
    size_t                          total_length = NOAH_KEY_BEHAVIOR_DOMAIN_V1_HEADER_SIZE;
    size_t                          total_steps  = 0u;
    noah_profile_codec_v1_result_t  result;

    clear_error(error);
    if ((!rows && row_count != 0u) || !output || !written) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, 0u, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    }
    *written = 0u;
    result   = resolve_limits(supplied_limits, supplied_action_limits, &limits, &action_limits, error);
    if (result != NOAH_PROFILE_CODEC_V1_OK) {
        return result;
    }
    if (row_count > limits.max_rows || row_count > UINT8_MAX) {
        return fail(error, NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED, 0u, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    }
    if (total_length > limits.max_payload_size) {
        return fail(error, NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED, total_length, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    }

    for (size_t row_index = 0u; row_index < row_count; row_index++) {
        const noah_key_behavior_row_v1_t *row = &rows[row_index];
        size_t                             body_length = NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FIXED_SIZE;

        row_order[row_index] = (uint8_t)row_index;
        if ((!row->steps && row->step_count != 0u) || row->step_count > limits.max_tap_steps_per_row) {
            return fail(error, row->step_count > limits.max_tap_steps_per_row ? NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED : NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, 0u, (uint8_t)row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_ROW_STEP_COUNT);
        }
        if ((row->flags & (uint8_t)~NOAH_KEY_BEHAVIOR_DOMAIN_V1_KNOWN_ROW_FLAGS) != 0u) {
            return fail(error, NOAH_PROFILE_CODEC_V1_RESERVED_FLAGS, 0u, (uint8_t)row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_ROW_FLAGS);
        }
        result = validate_action_value(&row->target, &action_limits, true, (uint8_t)row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_TARGET, error);
        if (result != NOAH_PROFILE_CODEC_V1_OK) {
            return result;
        }
        for (size_t step_index = 0u; step_index < row->step_count; step_index++) {
            result = validate_step_value(&row->steps[step_index], &limits, &action_limits, (uint8_t)row_index, (uint8_t)step_index, error);
            if (result != NOAH_PROFILE_CODEC_V1_OK) {
                return result;
            }
            for (size_t prior = 0u; prior < step_index; prior++) {
                if (row->steps[prior].tap_index == row->steps[step_index].tap_index) {
                    return fail(error, NOAH_PROFILE_CODEC_V1_DUPLICATE_STEP, 0u, (uint8_t)row_index, (uint8_t)step_index, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_INDEX);
                }
            }
            body_length += step_encoded_size(&row->steps[step_index]);
        }
        row_body_lengths[row_index] = (uint16_t)body_length;
        total_steps += row->step_count;
        if (total_steps > limits.max_populated_steps || total_steps > UINT8_MAX) {
            return fail(error, NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED, 0u, (uint8_t)row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_ROW_STEP_COUNT);
        }
        if (NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_LENGTH_SIZE > limits.max_payload_size - total_length || body_length > limits.max_payload_size - total_length - NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_LENGTH_SIZE) {
            return fail(error, NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED, total_length, (uint8_t)row_index, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_ROW_LENGTH);
        }
        total_length += NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_LENGTH_SIZE + body_length;
    }

    for (size_t index = 1u; index < row_count; index++) {
        uint8_t current = row_order[index];
        size_t  position = index;

        while (position != 0u && action_canonical_compare(&rows[row_order[position - 1u]].target, &rows[current].target) > 0) {
            row_order[position] = row_order[position - 1u];
            position--;
        }
        row_order[position] = current;
    }
    for (size_t index = 1u; index < row_count; index++) {
        if (action_canonical_compare(&rows[row_order[index - 1u]].target, &rows[row_order[index]].target) == 0) {
            return fail(error, NOAH_PROFILE_CODEC_V1_DUPLICATE_TARGET, 0u, row_order[index], UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_TARGET);
        }
    }
    if (total_length > output_capacity) {
        return fail(error, NOAH_PROFILE_CODEC_V1_OUTPUT_TOO_SMALL, 0u, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    }

    output[0] = (uint8_t)row_count;
    output[1] = (uint8_t)total_steps;
    output[2] = 0u;
    output[3] = 0u;
    size_t offset = NOAH_KEY_BEHAVIOR_DOMAIN_V1_HEADER_SIZE;
    for (size_t order_index = 0u; order_index < row_count; order_index++) {
        uint8_t                           row_index = row_order[order_index];
        const noah_key_behavior_row_v1_t *row      = &rows[row_index];
        uint8_t                           step_order[NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_TAP_STEPS_PER_ROW];

        write_u16(&output[offset], row_body_lengths[row_index]);
        offset += NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_LENGTH_SIZE;
        write_action(&output[offset], &row->target);
        write_u16(&output[offset + 4u], row->tap_hold_term);
        write_u16(&output[offset + 6u], row->longer_hold_term);
        write_u16(&output[offset + 8u], row->multi_tap_term);
        output[offset + 10u] = row->flags;
        output[offset + 11u] = (uint8_t)row->step_count;
        offset += NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FIXED_SIZE;

        for (size_t step_index = 0u; step_index < row->step_count; step_index++) {
            step_order[step_index] = (uint8_t)step_index;
        }
        for (size_t index = 1u; index < row->step_count; index++) {
            uint8_t current = step_order[index];
            size_t  position = index;

            while (position != 0u && row->steps[step_order[position - 1u]].tap_index > row->steps[current].tap_index) {
                step_order[position] = step_order[position - 1u];
                position--;
            }
            step_order[position] = current;
        }
        for (size_t order_step = 0u; order_step < row->step_count; order_step++) {
            const noah_key_behavior_step_v1_t *step = &row->steps[step_order[order_step]];

            output[offset++] = step->tap_index;
            output[offset++] = step->presence_mask;
            if ((step->presence_mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP) != 0u) {
                write_action(&output[offset], &step->tap);
                offset += NOAH_PROFILE_BLOB_V1_ACTION_SIZE;
            }
            if ((step->presence_mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_HOLD) != 0u) {
                write_hold(&output[offset], &step->hold);
                offset += NOAH_KEY_BEHAVIOR_DOMAIN_V1_HOLD_SIZE;
            }
            if ((step->presence_mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_LONG_HOLD) != 0u) {
                write_hold(&output[offset], &step->long_hold);
                offset += NOAH_KEY_BEHAVIOR_DOMAIN_V1_HOLD_SIZE;
            }
        }
    }
    *written = offset;
    return NOAH_PROFILE_CODEC_V1_OK;
}

_Static_assert(NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS == NOAH_PROFILE_WIRE_V1_MAX_BEHAVIOR_ROWS, "behavior row ceilings drifted");
_Static_assert(NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_POPULATED_STEPS == NOAH_PROFILE_WIRE_V1_MAX_POPULATED_BEHAVIOR_STEPS, "behavior aggregate step ceilings drifted");
_Static_assert(NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_TAP_STEPS_PER_ROW == NOAH_PROFILE_WIRE_V1_MAX_TAP_STEPS_PER_BEHAVIOR, "behavior tap-depth ceilings drifted");
_Static_assert(NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_PAYLOAD_SIZE + NOAH_PROFILE_BLOB_V1_HEADER_SIZE + NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE == NOAH_PROFILE_BLOB_V1_MAX_SIZE, "behavior payload and blob ceilings drifted");
_Static_assert(sizeof(noah_key_behavior_domain_v1_t) <= 128u, "validated behavior domain handle must remain bounded and payload-independent");
