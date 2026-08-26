#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/schema/key_behavior_domain_v1.h"

enum {
    TEST_BUFFER_SIZE = NOAH_PROFILE_BLOB_V1_MAX_SIZE + 32u,
};

typedef struct {
    const uint8_t *bytes;
    size_t         length;
    size_t         read_count;
    size_t         total_bytes;
    size_t         max_read;
    size_t         fail_at;
} instrumented_reader_t;

static void expect_result(noah_profile_codec_v1_result_t actual, noah_profile_codec_v1_result_t expected) {
    if (actual != expected) {
        fprintf(stderr, "behavior codec result mismatch: got %u expected %u\n", (unsigned)actual, (unsigned)expected);
        abort();
    }
}

static void expect_error_location(const noah_profile_codec_v1_error_t *error, size_t offset, uint8_t row_index, uint8_t step_index, uint8_t field_id) {
    assert(error->offset == offset);
    assert(error->row_index == row_index);
    assert(error->step_index == step_index);
    assert(error->field_id == field_id);
}

static bool fixture_value(const char *path, const char *key, char *value, size_t capacity) {
    FILE  *file = fopen(path, "r");
    char   line[TEST_BUFFER_SIZE * 2u];
    size_t key_length = strlen(key);

    if (!file) {
        return false;
    }
    while (fgets(line, sizeof(line), file)) {
        size_t length;

        if (strncmp(line, key, key_length) != 0 || line[key_length] != '=') {
            continue;
        }
        length = strcspn(&line[key_length + 1u], "\r\n");
        if (length + 1u > capacity) {
            fclose(file);
            return false;
        }
        memcpy(value, &line[key_length + 1u], length);
        value[length] = '\0';
        fclose(file);
        return true;
    }
    fclose(file);
    return false;
}

static uint8_t hex_nibble(char value) {
    if (value >= '0' && value <= '9') return (uint8_t)(value - '0');
    if (value >= 'a' && value <= 'f') return (uint8_t)(value - 'a' + 10);
    if (value >= 'A' && value <= 'F') return (uint8_t)(value - 'A' + 10);
    abort();
}

static size_t fixture_hex(const char *path, const char *key, uint8_t *output, size_t capacity) {
    char   encoded[TEST_BUFFER_SIZE * 2u];
    size_t length;

    assert(fixture_value(path, key, encoded, sizeof(encoded)));
    length = strlen(encoded);
    assert((length % 2u) == 0u && length / 2u <= capacity);
    for (size_t index = 0u; index < length; index += 2u) {
        output[index / 2u] = (uint8_t)((hex_nibble(encoded[index]) << 4u) | hex_nibble(encoded[index + 1u]));
    }
    return length / 2u;
}

static bool instrumented_read(void *context, size_t offset, uint8_t *target, size_t length) {
    instrumented_reader_t *reader = context;

    reader->read_count++;
    if (length > reader->max_read) reader->max_read = length;
    if ((reader->fail_at != 0u && reader->read_count == reader->fail_at) || length > NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FIXED_SIZE || offset > reader->length || length > reader->length - offset) {
        return false;
    }
    reader->total_bytes += length;
    memcpy(target, &reader->bytes[offset], length);
    return true;
}

static noah_profile_action_v1_t action(uint8_t kind, uint16_t operand) {
    noah_profile_action_v1_t value = {.kind = kind, .flags = 0u, .operand = operand};
    return value;
}

static uint16_t read_u16(const uint8_t *source) {
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8u);
}

static void write_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
}

static size_t encode_representative(uint8_t *output, size_t capacity, noah_profile_codec_v1_error_t *error) {
    static const noah_key_behavior_step_v1_t qmk_steps[] = {
        {
            .tap_index     = 2u,
            .presence_mask = NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP | NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_LONG_HOLD,
            .tap           = {.kind = NOAH_PROFILE_ACTION_V1_VIA_MACRO, .operand = 10u},
            .long_hold     = {.mode = NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_AT_THRESHOLD, .action = {.kind = NOAH_PROFILE_ACTION_V1_LAYER_LOCK, .operand = 3u}},
        },
        {
            .tap_index     = 0u,
            .presence_mask = NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_HOLD,
            .hold          = {.mode = NOAH_KEY_BEHAVIOR_HOLD_V1_REPEAT_WHILE_HELD, .repeat_hz = 25u, .action = {.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = 0x28u}},
        },
    };
    static const noah_key_behavior_step_v1_t pd_steps[] = {
        {.tap_index = 1u, .presence_mask = NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP, .tap = {.kind = NOAH_PROFILE_ACTION_V1_HARDCODED_MACRO, .operand = 2u}},
    };
    static const noah_key_behavior_row_v1_t rows[] = {
        {.target = {.kind = NOAH_PROFILE_ACTION_V1_PD_MODE_MOMENTARY, .operand = 2u}, .steps = pd_steps, .step_count = 1u},
        {
            .target          = {.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = 0x1234u},
            .tap_hold_term   = 150u,
            .longer_hold_term = 400u,
            .multi_tap_term = 175u,
            .flags          = NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FLAG_AUTO_MOUSE,
            .steps          = qmk_steps,
            .step_count     = 2u,
        },
    };
    size_t written;

    expect_result(noah_key_behavior_domain_v1_encode(rows, 2u, NULL, NULL, output, capacity, &written, error), NOAH_PROFILE_CODEC_V1_OK);
    return written;
}

static bool errors_equal(const noah_profile_codec_v1_error_t *lhs, const noah_profile_codec_v1_error_t *rhs) {
    return lhs->code == rhs->code && lhs->offset == rhs->offset && lhs->domain_index == rhs->domain_index && lhs->domain_id == rhs->domain_id && lhs->table_id == rhs->table_id && lhs->row_index == rhs->row_index && lhs->step_index == rhs->step_index && lhs->field_id == rhs->field_id;
}

static size_t run_incremental_budget(instrumented_reader_t *state, size_t base_offset, size_t length, noah_key_behavior_domain_v1_t *domain) {
    noah_profile_reader_t reader = {.read = instrumented_read, .context = state, .length = state->length};
    noah_key_behavior_domain_v1_validation_t validation;
    noah_profile_codec_v1_error_t error;
    noah_key_behavior_domain_v1_validation_result_t result;
    size_t event_count = 0u;
    size_t iterations  = 0u;
    size_t reads_before = state->read_count;
    size_t bytes_before = state->total_bytes;

    result = noah_key_behavior_domain_v1_validation_begin(&validation, &reader, base_offset, length, NULL, NULL, &error);
    assert(result == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_IN_PROGRESS);
    assert(state->read_count == reads_before && state->total_bytes == bytes_before);
    assert(noah_key_behavior_domain_v1_validation_view(&validation, domain, &error) == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_IN_PROGRESS);
    assert(!noah_key_behavior_domain_v1_validation_action_event(&validation, &(noah_key_behavior_domain_v1_action_event_t){0}));

    while (result == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_IN_PROGRESS) {
        noah_key_behavior_domain_v1_action_event_t event;
        size_t prior_reads = state->read_count;
        size_t prior_bytes = state->total_bytes;

        result = noah_key_behavior_domain_v1_validation_step(&validation, &error);
        assert(state->read_count - prior_reads <= 1u);
        assert(state->total_bytes - prior_bytes <= NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_READ_MAX);
        if (noah_key_behavior_domain_v1_validation_action_event(&validation, &event)) {
            assert(state->read_count - prior_reads == 1u);
            event_count++;
        }
        assert(++iterations < 2048u);
    }
    assert(result == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_VALID);
    reads_before = state->read_count;
    bytes_before = state->total_bytes;
    assert(noah_key_behavior_domain_v1_validation_view(&validation, domain, &error) == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_VALID);
    assert(state->read_count == reads_before && state->total_bytes == bytes_before);
    assert(noah_key_behavior_domain_v1_validation_step(&validation, &error) == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_VALID);
    assert(!noah_key_behavior_domain_v1_validation_action_event(&validation, &(noah_key_behavior_domain_v1_action_event_t){0}));
    assert(state->read_count == reads_before && state->total_bytes == bytes_before);
    return event_count;
}

static void test_incremental_validation(const char *fixture_path) {
    static const size_t expected_offsets[] = {6u, 22u, 28u, 34u, 40u, 54u};
    static const uint8_t expected_rows[] = {0u, 0u, 0u, 0u, 1u, 1u};
    static const uint8_t expected_steps[] = {UINT8_MAX, 0u, 1u, 1u, UINT8_MAX, 0u};
    static const uint8_t expected_fields[] = {
        NOAH_KEY_BEHAVIOR_FIELD_V1_TARGET,
        NOAH_KEY_BEHAVIOR_FIELD_V1_HOLD_ACTION,
        NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_ACTION,
        NOAH_KEY_BEHAVIOR_FIELD_V1_LONG_HOLD_ACTION,
        NOAH_KEY_BEHAVIOR_FIELD_V1_TARGET,
        NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_ACTION,
    };
    static const uint8_t expected_kinds[] = {
        NOAH_PROFILE_ACTION_V1_QMK_KEYCODE,
        NOAH_PROFILE_ACTION_V1_QMK_KEYCODE,
        NOAH_PROFILE_ACTION_V1_VIA_MACRO,
        NOAH_PROFILE_ACTION_V1_LAYER_LOCK,
        NOAH_PROFILE_ACTION_V1_PD_MODE_MOMENTARY,
        NOAH_PROFILE_ACTION_V1_HARDCODED_MACRO,
    };
    static const uint16_t expected_operands[] = {0x1234u, 0x28u, 10u, 3u, 2u, 2u};
    uint8_t payload[TEST_BUFFER_SIZE];
    size_t length = fixture_hex(fixture_path, "payload.representative.hex", payload, sizeof(payload));
    instrumented_reader_t state = {.bytes = payload, .length = length};
    noah_profile_reader_t reader = {.read = instrumented_read, .context = &state, .length = length};
    noah_key_behavior_domain_v1_validation_t validation;
    noah_key_behavior_domain_v1_action_event_t event;
    noah_key_behavior_domain_v1_t domain;
    noah_profile_codec_v1_error_t error;
    noah_key_behavior_domain_v1_validation_result_t result;
    size_t event_index = 0u;

    result = noah_key_behavior_domain_v1_validation_begin(&validation, &reader, 0u, length, NULL, NULL, &error);
    assert(result == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_IN_PROGRESS && state.read_count == 0u);
    while (result == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_IN_PROGRESS) {
        size_t prior_reads = state.read_count;
        size_t prior_bytes = state.total_bytes;

        result = noah_key_behavior_domain_v1_validation_step(&validation, &error);
        assert(state.read_count - prior_reads <= 1u);
        assert(state.total_bytes - prior_bytes <= NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_READ_MAX);
        if (noah_key_behavior_domain_v1_validation_action_event(&validation, &event)) {
            assert(event_index < sizeof(expected_offsets) / sizeof(expected_offsets[0]));
            assert(event.offset == expected_offsets[event_index]);
            assert(event.row_index == expected_rows[event_index]);
            assert(event.step_index == expected_steps[event_index]);
            assert(event.field_id == expected_fields[event_index]);
            assert(event.action.kind == expected_kinds[event_index]);
            assert(event.action.flags == 0u && event.action.operand == expected_operands[event_index]);
            event_index++;
        }
    }
    assert(result == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_VALID);
    assert(event_index == sizeof(expected_offsets) / sizeof(expected_offsets[0]));
    assert(state.max_read == NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FIXED_SIZE);
    assert(state.read_count == 14u && state.total_bytes == length);
    assert(noah_key_behavior_domain_v1_validation_view(&validation, &domain, &error) == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_VALID);
    assert(domain.row_count == 2u && domain.populated_step_count == 3u);

    size_t successful_reads = state.read_count;
    for (size_t fail_at = 1u; fail_at <= successful_reads; fail_at++) {
        noah_profile_codec_v1_error_t first_error;

        state.read_count = 0u;
        state.total_bytes = 0u;
        state.max_read = 0u;
        state.fail_at = fail_at;
        result = noah_key_behavior_domain_v1_validation_begin(&validation, &reader, 0u, length, NULL, NULL, &error);
        while (result == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_IN_PROGRESS) {
            size_t prior_reads = state.read_count;
            size_t prior_bytes = state.total_bytes;

            result = noah_key_behavior_domain_v1_validation_step(&validation, &error);
            assert(state.read_count - prior_reads <= 1u);
            assert(state.total_bytes - prior_bytes <= NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_READ_MAX);
        }
        assert(result == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_REJECTED);
        assert(error.code == NOAH_PROFILE_CODEC_V1_READ_ERROR);
        first_error = error;
        assert(noah_key_behavior_domain_v1_validation_step(&validation, &error) == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_REJECTED);
        assert(errors_equal(&first_error, &error));
        assert(noah_key_behavior_domain_v1_validation_view(&validation, &domain, &error) == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_REJECTED);
        assert(errors_equal(&first_error, &error));
    }
    state.fail_at = 0u;
    assert(noah_key_behavior_domain_v1_validation_begin(NULL, &reader, 0u, length, NULL, NULL, &error) == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_REJECTED);
    assert(error.code == NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);
    assert(noah_key_behavior_domain_v1_validation_step(NULL, &error) == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_REJECTED);
    assert(noah_key_behavior_domain_v1_validation_view(NULL, &domain, &error) == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_REJECTED);
}

static void test_shared_vectors_and_reader(const char *fixture_path) {
    uint8_t                         expected[TEST_BUFFER_SIZE];
    uint8_t                         encoded[TEST_BUFFER_SIZE];
    uint8_t                         prefixed[TEST_BUFFER_SIZE];
    size_t                          expected_length;
    size_t                          written;
    noah_profile_codec_v1_error_t   error;
    noah_key_behavior_domain_v1_t   domain;
    noah_key_behavior_row_v1_view_t row;
    noah_key_behavior_row_v1_view_t found_row;
    noah_key_behavior_step_v1_t     step;
    bool                            found;

    expected_length = fixture_hex(fixture_path, "payload.empty.hex", expected, sizeof(expected));
    expect_result(noah_key_behavior_domain_v1_encode(NULL, 0u, NULL, NULL, encoded, sizeof(encoded), &written, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(written == expected_length && memcmp(encoded, expected, written) == 0);

    expected_length = fixture_hex(fixture_path, "payload.representative.hex", expected, sizeof(expected));
    written         = encode_representative(encoded, sizeof(encoded), &error);
    assert(written == expected_length && memcmp(encoded, expected, written) == 0);

    memset(prefixed, 0xA5, sizeof(prefixed));
    memcpy(&prefixed[7], encoded, written);
    instrumented_reader_t state = {.bytes = prefixed, .length = written + 14u};
    noah_profile_reader_t reader = {.read = instrumented_read, .context = &state, .length = state.length};
    expect_result(noah_key_behavior_domain_v1_decode_reader(&reader, 7u, written, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(domain.row_count == 2u && domain.populated_step_count == 3u && domain.byte_length == 58u);
    assert(sizeof(domain) <= 128u);
    assert(state.max_read <= NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FIXED_SIZE);

    expect_result(noah_key_behavior_domain_v1_row_at(&domain, 0u, &row, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(row.target.kind == NOAH_PROFILE_ACTION_V1_QMK_KEYCODE && row.target.operand == 0x1234u);
    assert(row.tap_hold_term == 150u && row.longer_hold_term == 400u && row.multi_tap_term == 175u);
    assert(row.flags == NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FLAG_AUTO_MOUSE && row.step_count == 2u);
    noah_profile_action_v1_t target = {.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = 0x1234u};
    expect_result(noah_key_behavior_domain_v1_find_target(&domain, &target, &found_row, &found, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(found && found_row.row_index == 0u && found_row.row_offset == row.row_offset);
    expect_result(noah_key_behavior_domain_v1_step_in_row(&domain, &found_row, 0u, &step, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(step.tap_index == 0u && step.hold.action.operand == 0x28u);
    state.fail_at = state.read_count + 1u;
    expect_result(noah_key_behavior_domain_v1_step_in_row(&domain, &found_row, 0u, &step, &error), NOAH_PROFILE_CODEC_V1_READ_ERROR);
    state.fail_at = 0u;
    noah_key_behavior_row_v1_view_t forged_row = found_row;
    forged_row.row_offset++;
    expect_result(noah_key_behavior_domain_v1_step_in_row(&domain, &forged_row, 0u, &step, &error), NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);
    expect_result(noah_key_behavior_domain_v1_step_at(&domain, 0u, 0u, &step, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(step.tap_index == 0u && step.presence_mask == NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_HOLD);
    assert(step.hold.mode == NOAH_KEY_BEHAVIOR_HOLD_V1_REPEAT_WHILE_HELD && step.hold.repeat_hz == 25u && step.hold.action.operand == 0x28u);
    expect_result(noah_key_behavior_domain_v1_step_at(&domain, 0u, 1u, &step, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(step.tap_index == 2u && step.tap.kind == NOAH_PROFILE_ACTION_V1_VIA_MACRO && step.tap.operand == 10u);
    assert(step.long_hold.mode == NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_AT_THRESHOLD && step.long_hold.action.kind == NOAH_PROFILE_ACTION_V1_LAYER_LOCK);
    expect_result(noah_key_behavior_domain_v1_row_at(&domain, 1u, &row, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(row.target.kind == NOAH_PROFILE_ACTION_V1_PD_MODE_MOMENTARY && row.target.operand == 2u && row.step_count == 1u);
    target = (noah_profile_action_v1_t){.kind = NOAH_PROFILE_ACTION_V1_PD_MODE_MOMENTARY, .operand = 2u};
    expect_result(noah_key_behavior_domain_v1_find_target(&domain, &target, &found_row, &found, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(found && found_row.row_index == 1u);
    target = (noah_profile_action_v1_t){.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = 0x4321u};
    expect_result(noah_key_behavior_domain_v1_find_target(&domain, &target, &found_row, &found, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(!found);
    target = (noah_profile_action_v1_t){.kind = NOAH_PROFILE_ACTION_V1_NONE};
    expect_result(noah_key_behavior_domain_v1_find_target(&domain, &target, &found_row, &found, &error), NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);
    expect_result(noah_key_behavior_domain_v1_step_at(&domain, 1u, 0u, &step, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(step.tap.kind == NOAH_PROFILE_ACTION_V1_HARDCODED_MACRO && step.tap.operand == 2u);
    expect_result(noah_key_behavior_domain_v1_row_at(&domain, 2u, &row, &error), NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);
    expect_result(noah_key_behavior_domain_v1_step_at(&domain, 0u, 2u, &step, &error), NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);
    assert(state.max_read <= 12u);
    instrumented_reader_t incremental_state = {.bytes = prefixed, .length = written + 14u};
    assert(run_incremental_budget(&incremental_state, 7u, written, &domain) == 6u);
    assert(incremental_state.total_bytes == written);

    noah_profile_domain_v1_t envelope_domain = {.id = NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS, .version = 1u, .payload = encoded, .payload_length = written};
    size_t envelope_length;
    expect_result(noah_profile_domain_v1_encode(&envelope_domain, prefixed, sizeof(prefixed), &envelope_length, &error), NOAH_PROFILE_CODEC_V1_OK);
    expected_length = fixture_hex(fixture_path, "envelope.representative.hex", expected, sizeof(expected));
    assert(envelope_length == expected_length && memcmp(prefixed, expected, expected_length) == 0);
    noah_profile_domain_v1_t blob_domain = envelope_domain;
    expect_result(noah_profile_blob_v1_encode(&blob_domain, 1u, prefixed, sizeof(prefixed), &written, &error), NOAH_PROFILE_CODEC_V1_OK);
    expected_length = fixture_hex(fixture_path, "blob.representative.hex", expected, sizeof(expected));
    assert(written == expected_length && memcmp(prefixed, expected, expected_length) == 0);
}

static void test_reader_failures(const char *fixture_path) {
    uint8_t                       payload[TEST_BUFFER_SIZE];
    size_t                        length = fixture_hex(fixture_path, "payload.representative.hex", payload, sizeof(payload));
    noah_key_behavior_domain_v1_t domain;
    noah_profile_codec_v1_error_t error;
    instrumented_reader_t         state = {.bytes = payload, .length = length};
    noah_profile_reader_t         reader = {.read = instrumented_read, .context = &state, .length = length};

    expect_result(noah_key_behavior_domain_v1_decode_reader(&reader, 0u, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_OK);
    size_t successful_reads = state.read_count;
    assert(successful_reads != 0u && state.max_read <= 12u);
    for (size_t fail_at = 1u; fail_at <= successful_reads; fail_at++) {
        state.read_count = 0u;
        state.max_read   = 0u;
        state.fail_at    = fail_at;
        expect_result(noah_key_behavior_domain_v1_decode_reader(&reader, 0u, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_READ_ERROR);
    }
    state.fail_at = 0u;
    expect_result(noah_key_behavior_domain_v1_decode_reader(&reader, length + 1u, 0u, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);
    expect_result(noah_key_behavior_domain_v1_decode_reader(NULL, 0u, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);
    assert(!noah_profile_reader_read(&reader, length, payload, 1u));
    assert(noah_profile_reader_read(&reader, length, NULL, 0u));
}

static void test_decode_rejections(const char *fixture_path) {
    uint8_t                       valid[TEST_BUFFER_SIZE];
    uint8_t                       bytes[TEST_BUFFER_SIZE];
    size_t                        length = fixture_hex(fixture_path, "payload.representative.hex", valid, sizeof(valid));
    noah_key_behavior_domain_v1_t domain;
    noah_profile_codec_v1_error_t error;

    for (size_t prefix = 0u; prefix < length; prefix++) {
        assert(noah_key_behavior_domain_v1_decode(valid, prefix, NULL, NULL, &domain, &error) != NOAH_PROFILE_CODEC_V1_OK);
    }
    memcpy(bytes, valid, length);
    bytes[2] = 1u;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS);
    expect_error_location(&error, 2u, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    memcpy(bytes, valid, length);
    bytes[1]--;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_COUNT_MISMATCH);
    expect_error_location(&error, 1u, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    memcpy(bytes, valid, length);
    bytes[16] = 0x80u;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_RESERVED_FLAGS);
    memcpy(bytes, valid, length);
    bytes[19] = 0u;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_EMPTY_STEP);
    memcpy(bytes, valid, length);
    bytes[19] = 0x80u;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_RESERVED_FLAGS);
    memcpy(bytes, valid, length);
    bytes[26] = 0u;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_DUPLICATE_STEP);
    expect_error_location(&error, 26u, 0u, 1u, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_INDEX);
    memcpy(bytes, valid, length);
    bytes[18] = 3u;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_STEP_ORDER);
    expect_error_location(&error, 26u, 0u, 1u, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_INDEX);
    memcpy(bytes, valid, length);
    bytes[18] = 5u;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_INVALID_TAP_INDEX);
    memcpy(bytes, valid, length);
    bytes[20] = 0u;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_INVALID_HOLD_MODE);
    memcpy(bytes, valid, length);
    bytes[20] = 5u;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_INVALID_HOLD_MODE);
    memcpy(bytes, valid, length);
    bytes[21] = 0u;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_INVALID_REPEAT_RATE);
    expect_error_location(&error, 21u, 0u, 0u, NOAH_KEY_BEHAVIOR_FIELD_V1_HOLD_REPEAT);
    memcpy(bytes, valid, length);
    bytes[21] = 101u;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_INVALID_REPEAT_RATE);
    memcpy(bytes, valid, length);
    bytes[20] = NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_AT_THRESHOLD;
    bytes[21] = 1u;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_INVALID_REPEAT_RATE);

    memcpy(bytes, valid, length);
    memset(&bytes[6], 0, 4u);
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_INVALID_TARGET);
    memcpy(bytes, valid, length);
    memset(&bytes[22], 0, 4u);
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_INVALID_ACTION);
    memcpy(bytes, valid, length);
    memset(&bytes[28], 0, 4u);
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_INVALID_ACTION);
    memcpy(bytes, valid, length);
    memset(&bytes[34], 0, 4u);
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_INVALID_ACTION);
    expect_error_location(&error, 34u, 0u, 1u, NOAH_KEY_BEHAVIOR_FIELD_V1_LONG_HOLD_ACTION);

    uint16_t first_size = (uint16_t)(read_u16(&valid[4]) + 2u);
    memcpy(bytes, valid, 4u);
    memcpy(&bytes[4], &valid[4u + first_size], length - 4u - first_size);
    memcpy(&bytes[length - first_size], &valid[4], first_size);
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_ROW_ORDER);
    memcpy(bytes, valid, length);
    memcpy(&bytes[4u + first_size + 2u], &valid[6], 4u);
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_DUPLICATE_TARGET);
    expect_error_location(&error, 40u, 1u, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_TARGET);

    memcpy(bytes, valid, length);
    bytes[length] = 0u;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length + 1u, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_TRAILING_BYTES);
    expect_error_location(&error, length, UINT8_MAX, UINT8_MAX, NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER);
    memcpy(bytes, valid, length);
    write_u16(&bytes[4], (uint16_t)(read_u16(&bytes[4]) - 1u));
    assert(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error) == NOAH_PROFILE_CODEC_V1_TRUNCATED || error.code == NOAH_PROFILE_CODEC_V1_ROW_LENGTH);
    memcpy(bytes, valid, length);
    bytes[0] = 65u;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED);
    memcpy(bytes, valid, length);
    bytes[1] = 129u;
    expect_result(noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED);

    noah_key_behavior_limits_v1_t limits = noah_key_behavior_domain_v1_default_limits();
    limits.max_rows++;
    expect_result(noah_key_behavior_domain_v1_decode(valid, length, &limits, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);
    limits = noah_key_behavior_domain_v1_default_limits();
    limits.max_payload_size = (uint16_t)(length - 1u);
    expect_result(noah_key_behavior_domain_v1_decode(valid, length, &limits, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED);
    noah_profile_action_v1_limits_t action_limits = noah_profile_action_v1_default_limits();
    action_limits.max_pd_modes = 2u;
    expect_result(noah_key_behavior_domain_v1_decode(valid, length, NULL, &action_limits, &domain, &error), NOAH_PROFILE_CODEC_V1_INVALID_OPERAND);
}

static void test_encode_rejections(void) {
    uint8_t                         output[TEST_BUFFER_SIZE];
    size_t                          written;
    noah_profile_codec_v1_error_t   error;
    noah_key_behavior_step_v1_t     step = {.tap_index = 0u, .presence_mask = NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP, .tap = {.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = 2u}};
    noah_key_behavior_row_v1_t      row  = {.target = {.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = 1u}, .steps = &step, .step_count = 1u};

    row.target = action(NOAH_PROFILE_ACTION_V1_NONE, 0u);
    expect_result(noah_key_behavior_domain_v1_encode(&row, 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_INVALID_TARGET);
    row.target = action(NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, 1u);
    step.presence_mask = 0u;
    expect_result(noah_key_behavior_domain_v1_encode(&row, 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_EMPTY_STEP);
    step.presence_mask = 0x80u;
    expect_result(noah_key_behavior_domain_v1_encode(&row, 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_RESERVED_FLAGS);
    step.presence_mask = NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP;
    step.tap = action(NOAH_PROFILE_ACTION_V1_NONE, 0u);
    expect_result(noah_key_behavior_domain_v1_encode(&row, 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_INVALID_ACTION);
    step.tap = action(NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, 2u);
    row.flags = 0x80u;
    expect_result(noah_key_behavior_domain_v1_encode(&row, 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_RESERVED_FLAGS);
    row.flags = 0u;
    expect_result(noah_key_behavior_domain_v1_encode(&row, 1u, NULL, NULL, output, 3u, &written, &error), NOAH_PROFILE_CODEC_V1_OUTPUT_TOO_SMALL);

    noah_key_behavior_row_v1_t duplicate_rows[2] = {row, row};
    expect_result(noah_key_behavior_domain_v1_encode(duplicate_rows, 2u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_DUPLICATE_TARGET);
    noah_key_behavior_step_v1_t duplicate_steps[2] = {step, step};
    row.steps = duplicate_steps;
    row.step_count = 2u;
    expect_result(noah_key_behavior_domain_v1_encode(&row, 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_DUPLICATE_STEP);

    noah_key_behavior_limits_v1_t limits = noah_key_behavior_domain_v1_default_limits();
    limits.max_repeat_hz++;
    expect_result(noah_key_behavior_domain_v1_encode(NULL, 0u, &limits, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);
    limits = noah_key_behavior_domain_v1_default_limits();
    limits.max_payload_size = 3u;
    expect_result(noah_key_behavior_domain_v1_encode(NULL, 0u, &limits, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED);
}

static void test_hold_modes_and_timing(void) {
    uint8_t                       output[128];
    size_t                        written;
    noah_profile_codec_v1_error_t error;
    noah_key_behavior_domain_v1_t domain;
    noah_key_behavior_row_v1_view_t decoded_row;
    noah_key_behavior_step_v1_t  step = {
        .tap_index     = 0u,
        .presence_mask = NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_HOLD | NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_LONG_HOLD,
        .hold          = {.action = {.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = 2u}},
        .long_hold     = {.mode = NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_ON_RELEASE_AFTER_HOLD, .action = {.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = 3u}},
    };
    noah_key_behavior_row_v1_t row = {
        .target           = {.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = 1u},
        .tap_hold_term    = UINT16_MAX,
        .longer_hold_term = UINT16_MAX,
        .multi_tap_term   = UINT16_MAX,
        .steps            = &step,
        .step_count       = 1u,
    };

    for (uint8_t mode = NOAH_KEY_BEHAVIOR_HOLD_V1_PRESS_AND_HOLD_UNTIL_RELEASE; mode <= NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_ON_RELEASE_AFTER_HOLD; mode++) {
        step.hold.mode      = mode;
        step.hold.repeat_hz = mode == NOAH_KEY_BEHAVIOR_HOLD_V1_REPEAT_WHILE_HELD ? 100u : 0u;
        expect_result(noah_key_behavior_domain_v1_encode(&row, 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_OK);
        expect_result(noah_key_behavior_domain_v1_decode(output, written, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_OK);
        expect_result(noah_key_behavior_domain_v1_row_at(&domain, 0u, &decoded_row, &error), NOAH_PROFILE_CODEC_V1_OK);
        assert(decoded_row.tap_hold_term == UINT16_MAX && decoded_row.longer_hold_term == UINT16_MAX && decoded_row.multi_tap_term == UINT16_MAX);
    }
    step.hold.mode = 0u;
    expect_result(noah_key_behavior_domain_v1_encode(&row, 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_INVALID_HOLD_MODE);
    step.hold.mode = 5u;
    expect_result(noah_key_behavior_domain_v1_encode(&row, 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_INVALID_HOLD_MODE);
    step.hold.mode = NOAH_KEY_BEHAVIOR_HOLD_V1_REPEAT_WHILE_HELD;
    step.hold.repeat_hz = 0u;
    expect_result(noah_key_behavior_domain_v1_encode(&row, 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_INVALID_REPEAT_RATE);
    step.hold.repeat_hz = 101u;
    expect_result(noah_key_behavior_domain_v1_encode(&row, 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_INVALID_REPEAT_RATE);
    step.hold.mode = NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_AT_THRESHOLD;
    step.hold.repeat_hz = 1u;
    expect_result(noah_key_behavior_domain_v1_encode(&row, 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_INVALID_REPEAT_RATE);
    step.hold.repeat_hz = 0u;
    step.hold.action = action(NOAH_PROFILE_ACTION_V1_NONE, 0u);
    expect_result(noah_key_behavior_domain_v1_encode(&row, 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_INVALID_ACTION);
}

static void test_maximum_counts(void) {
    static noah_key_behavior_row_v1_t  rows[NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS];
    static noah_key_behavior_step_v1_t steps[NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS][3];
    static uint8_t                     output[NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_PAYLOAD_SIZE];
    size_t                             written;
    noah_profile_codec_v1_error_t      error;
    noah_key_behavior_domain_v1_t      domain;

    for (size_t row_index = 0u; row_index < NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS; row_index++) {
        rows[row_index].target = action(NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, (uint16_t)row_index);
        rows[row_index].steps  = steps[row_index];
        rows[row_index].step_count = 2u;
        for (size_t step_index = 0u; step_index < 3u; step_index++) {
            steps[row_index][step_index].tap_index     = (uint8_t)step_index;
            steps[row_index][step_index].presence_mask = NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP;
            steps[row_index][step_index].tap           = action(NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, (uint16_t)(100u + step_index));
        }
    }
    expect_result(noah_key_behavior_domain_v1_encode(rows, NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_OK);
    expect_result(noah_key_behavior_domain_v1_decode(output, written, NULL, NULL, &domain, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(domain.row_count == 64u && domain.populated_step_count == 128u);
    instrumented_reader_t state = {.bytes = output, .length = written};
    assert(run_incremental_budget(&state, 0u, written, &domain) == 192u);
    assert(domain.row_count == 64u && domain.populated_step_count == 128u);
    assert(state.max_read == NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FIXED_SIZE);
    assert(state.total_bytes == written);
    rows[0].step_count = 3u;
    expect_result(noah_key_behavior_domain_v1_encode(rows, NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED);
    rows[0].step_count = 2u;
    expect_result(noah_key_behavior_domain_v1_encode(rows, NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS + 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED);

    noah_key_behavior_step_v1_t five_steps[6];
    noah_key_behavior_row_v1_t one_row = {.target = {.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = 1u}, .steps = five_steps, .step_count = 5u};
    for (size_t index = 0u; index < 6u; index++) {
        five_steps[index] = (noah_key_behavior_step_v1_t){.tap_index = (uint8_t)index, .presence_mask = NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP, .tap = {.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = 2u}};
    }
    expect_result(noah_key_behavior_domain_v1_encode(&one_row, 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_OK);
    one_row.step_count = 6u;
    expect_result(noah_key_behavior_domain_v1_encode(&one_row, 1u, NULL, NULL, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED);
}

static void test_malformed_corpus(void) {
    uint8_t                       bytes[128];
    uint32_t                      state = UINT32_C(0xBEEFBEEF);
    noah_key_behavior_domain_v1_t domain;
    noah_profile_codec_v1_error_t error;

    for (size_t case_index = 0u; case_index < 2048u; case_index++) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        size_t length = state % sizeof(bytes);
        for (size_t index = 0u; index < length; index++) {
            state = state * UINT32_C(1664525) + UINT32_C(1013904223);
            bytes[index] = (uint8_t)(state >> 24u);
        }
        (void)noah_key_behavior_domain_v1_decode(bytes, length, NULL, NULL, &domain, &error);
    }
}

int main(int argc, char **argv) {
    assert(argc == 2);
    test_shared_vectors_and_reader(argv[1]);
    test_incremental_validation(argv[1]);
    test_reader_failures(argv[1]);
    test_decode_rejections(argv[1]);
    test_encode_rejections();
    test_hold_modes_and_timing();
    test_maximum_counts();
    test_malformed_corpus();
    puts("key behavior domain v1 tests passed");
    return 0;
}
