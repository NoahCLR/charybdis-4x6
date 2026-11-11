#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/schema/profile_rgb_v1.h"

enum {
    TEST_BUFFER_SIZE = NOAH_PROFILE_RGB_V1_MAX_PAYLOAD_SIZE + 8u,
};

typedef struct {
    const uint8_t *bytes;
    size_t         length;
    size_t         calls;
    size_t         bytes_read;
    size_t         max_read;
    size_t         fail_call;
} instrumented_reader_t;

static void expect_result(noah_profile_rgb_v1_result_t actual, noah_profile_rgb_v1_result_t expected) {
    if (actual != expected) {
        fprintf(stderr, "RGB result mismatch: got %u expected %u\n", (unsigned)actual, (unsigned)expected);
        abort();
    }
}

static bool fixture_value(const char *path, const char *key, char *value, size_t capacity) {
    FILE  *file = fopen(path, "r");
    char   line[TEST_BUFFER_SIZE * 2u + 64u];
    size_t key_length = strlen(key);

    if (!file) return false;
    while (fgets(line, sizeof(line), file)) {
        size_t length;
        if (strncmp(line, key, key_length) != 0 || line[key_length] != '=') continue;
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
    char   encoded[TEST_BUFFER_SIZE * 2u + 1u];
    size_t length;

    assert(fixture_value(path, key, encoded, sizeof(encoded)));
    length = strlen(encoded);
    assert((length % 2u) == 0u && length / 2u <= capacity);
    for (size_t index = 0u; index < length; index += 2u) {
        output[index / 2u] = (uint8_t)((hex_nibble(encoded[index]) << 4u) | hex_nibble(encoded[index + 1u]));
    }
    return length / 2u;
}

static unsigned fixture_uint(const char *path, const char *key) {
    char          encoded[32];
    char         *end;
    unsigned long value;
    assert(fixture_value(path, key, encoded, sizeof(encoded)));
    value = strtoul(encoded, &end, 10);
    assert(*encoded != '\0' && *end == '\0' && value <= UINT16_MAX);
    return (unsigned)value;
}

static noah_profile_rgb_v1_limits_t fixture_limits(const char *path) {
    noah_profile_rgb_v1_limits_t limits = noah_profile_rgb_v1_default_limits();
    limits.compiled_stage_mask          = (uint16_t)fixture_uint(path, "codec.compiled_stage_mask");
    limits.logical_layer_count          = (uint8_t)fixture_uint(path, "codec.logical_layer_count");
    limits.maximum_brightness           = (uint8_t)fixture_uint(path, "codec.maximum_brightness");
    limits.tap_branch_color_count       = (uint8_t)fixture_uint(path, "codec.tap_branch_color_count");
    limits.supported_pointing_mode_mask = (uint8_t)fixture_uint(path, "codec.supported_pd_mode_mask");
    return limits;
}

static bool instrumented_read(void *context_value, size_t offset, uint8_t *target, size_t length) {
    instrumented_reader_t *context = (instrumented_reader_t *)context_value;
    context->calls++;
    context->bytes_read += length;
    if (length > context->max_read) context->max_read = length;
    if (context->fail_call != 0u && context->calls == context->fail_call) return false;
    assert(offset <= context->length && length <= context->length - offset);
    if (length != 0u) memcpy(target, &context->bytes[offset], length);
    return true;
}

static noah_profile_rgb_v1_validation_result_t drive_incremental(noah_profile_rgb_v1_validation_t *validation, instrumented_reader_t *context, noah_profile_rgb_v1_error_t *error, size_t *steps) {
    noah_profile_rgb_v1_validation_result_t progress = NOAH_PROFILE_RGB_V1_VALIDATION_IN_PROGRESS;

    while (progress == NOAH_PROFILE_RGB_V1_VALIDATION_IN_PROGRESS) {
        size_t calls_before = context->calls;
        size_t bytes_before = context->bytes_read;
        progress            = noah_profile_rgb_v1_validation_step(validation, error);
        assert(context->calls - calls_before <= 1u);
        assert(context->bytes_read - bytes_before <= NOAH_PROFILE_RGB_V1_VALIDATION_READ_MAX);
        (*steps)++;
        // A runaway guard above the largest domain's steps: one per record,
        // at most 1 + 128 + 16 + 640 + 32 + 4 + 3, and its framing.
        assert(*steps < 1024u);
    }
    return progress;
}

static void expect_decode(const uint8_t *bytes, size_t length, const noah_profile_rgb_v1_limits_t *limits, noah_profile_rgb_v1_result_t expected) {
    noah_profile_rgb_v1_view_t  view;
    noah_profile_rgb_v1_error_t error;
    expect_result(noah_profile_rgb_v1_decode(bytes, length, limits, &view, &error), expected);
    if (expected != NOAH_PROFILE_RGB_V1_OK) assert(error.code == expected);
}

static void test_shared_golden_and_accessors(const char *fixture_path) {
    uint8_t                               payload[TEST_BUFFER_SIZE];
    size_t                                length = fixture_hex(fixture_path, "payload.hex", payload, sizeof(payload));
    noah_profile_rgb_v1_limits_t          limits = fixture_limits(fixture_path);
    noah_profile_rgb_v1_view_t            view;
    noah_profile_rgb_v1_error_t           error;
    noah_profile_rgb_v1_group_t           group;
    noah_profile_rgb_v1_layer_color_t     layer;
    noah_profile_rgb_v1_group_row_t       group_row;
    noah_profile_rgb_v1_automouse_t       automouse;
    noah_profile_rgb_v1_pd_color_t        pd;
    noah_profile_rgb_v1_feedback_t        combo;
    noah_profile_rgb_v1_combo_group_row_t combo_row;
    noah_profile_rgb_v1_hsv_t             color;
    noah_profile_rgb_v1_key_feedback_t    key;

    assert(length == 163u);
    expect_result(noah_profile_rgb_v1_decode(payload, length, &limits, &view, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(view.stage_enable_mask == 31u && view.group_count == 3u && view.layer_color_count == 3u);
    assert(view.layer_group_count == 2u && view.pd_color_count == 6u && view.pd_group_count == 2u);
    assert(view.combo_group_count == 1u && view.tap_branch_color_count == 4u && view.key_group_count == 2u);
    assert(sizeof(view) <= 64u);

    expect_result(noah_profile_rgb_v1_group_at(&view, 0u, &group, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(group.id == 0u && memcmp(group.bitmap, (const uint8_t[8]){0}, 8u) == 0);
    expect_result(noah_profile_rgb_v1_group_at(&view, 1u, &group, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(group.bitmap[7] == 2u);
    expect_result(noah_profile_rgb_v1_group_at(&view, 2u, &group, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(group.bitmap[0] == 3u);
    expect_result(noah_profile_rgb_v1_layer_color_at(&view, 2u, &layer, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(layer.layer_id == 2u && layer.color.h == 40u && layer.color.s == 50u && layer.color.v == 60u && layer.mode == 1u);
    expect_result(noah_profile_rgb_v1_layer_group_at(&view, 1u, &group_row, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(group_row.selector == 2u && group_row.color.h == 5u && group_row.group_id == 1u);
    expect_result(noah_profile_rgb_v1_automouse(&view, &automouse, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(automouse.mode == 2u && automouse.end_color.h == 8u && automouse.end_color.v == 10u);
    expect_result(noah_profile_rgb_v1_pd_color_at(&view, 5u, &pd, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(pd.pointing_mode_id == 5u && pd.color.h == 61u && pd.color.v == 63u && pd.locality == 2u);
    expect_result(noah_profile_rgb_v1_pd_group_at(&view, 1u, &group_row, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(group_row.selector == 1u && group_row.color.s == 71u && group_row.group_id == 1u);
    expect_result(noah_profile_rgb_v1_combo_feedback(&view, &combo, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(combo.color.h == 80u && combo.color.v == 82u && combo.locality == 3u);
    expect_result(noah_profile_rgb_v1_combo_group_at(&view, 0u, &combo_row, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(combo_row.color.h == 0u && combo_row.group_id == 2u);
    expect_result(noah_profile_rgb_v1_tap_branch_color_at(&view, 3u, &color, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(color.h == 120u && color.s == 121u && color.v == 122u);
    expect_result(noah_profile_rgb_v1_key_feedback(&view, &key, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(key.tap_committed_color.v == 130u && key.hold_active_color.h == 18u && key.long_hold_active_color.h == 148u && key.tap_commit_mode == 1u && key.locality == 3u);
    expect_result(noah_profile_rgb_v1_key_group_at(&view, 1u, &group_row, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(group_row.selector == 2u && group_row.color.h == 1u && group_row.group_id == 1u);

    expect_result(noah_profile_rgb_v1_group_at(&view, view.group_count, &group, &error), NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT);
    expect_result(noah_profile_rgb_v1_key_feedback(&view, NULL, &error), NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT);
}

static void test_reader_is_bounded(const char *fixture_path) {
    uint8_t                            padded[TEST_BUFFER_SIZE + 14u];
    size_t                             length  = fixture_hex(fixture_path, "payload.hex", &padded[7], TEST_BUFFER_SIZE);
    noah_profile_rgb_v1_limits_t       limits  = fixture_limits(fixture_path);
    instrumented_reader_t              context = {.bytes = padded, .length = length + 14u};
    noah_profile_reader_t              reader  = {.read = instrumented_read, .context = &context, .length = length + 14u};
    noah_profile_rgb_v1_view_t         view;
    noah_profile_rgb_v1_error_t        error;
    noah_profile_rgb_v1_key_feedback_t key;

    expect_result(noah_profile_rgb_v1_decode_reader(&reader, 7u, length, &limits, &view, &error), NOAH_PROFILE_RGB_V1_OK);
    expect_result(noah_profile_rgb_v1_key_feedback(&view, &key, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(context.calls > 1u && context.max_read == 16u);
    // The only read larger than a record is the one-time 16-byte header. All
    // retained-view materialization reads are fixed records of at most 11.
    context.max_read = 0u;
    expect_result(noah_profile_rgb_v1_key_feedback(&view, &key, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(context.max_read == 11u);

    context.calls     = 0u;
    context.fail_call = 3u;
    expect_result(noah_profile_rgb_v1_decode_reader(&reader, 7u, length, &limits, &view, &error), NOAH_PROFILE_RGB_V1_READ_ERROR);
    context.fail_call = 0u;
    expect_result(noah_profile_rgb_v1_decode_reader(&reader, context.length, 1u, &limits, &view, &error), NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT);
}

static void test_incremental_reader_budget_and_failures(const char *fixture_path) {
    uint8_t                                 padded[TEST_BUFFER_SIZE + 14u];
    size_t                                  length  = fixture_hex(fixture_path, "payload.hex", &padded[7], TEST_BUFFER_SIZE);
    noah_profile_rgb_v1_limits_t            limits  = fixture_limits(fixture_path);
    instrumented_reader_t                   context = {.bytes = padded, .length = length + 14u};
    noah_profile_reader_t                   reader  = {.read = instrumented_read, .context = &context, .length = length + 14u};
    noah_profile_rgb_v1_validation_t        validation;
    noah_profile_rgb_v1_validation_result_t progress;
    noah_profile_rgb_v1_view_t              view;
    noah_profile_rgb_v1_error_t             error;
    size_t                                  steps = 0u;
    size_t                                  successful_calls;

    assert(sizeof(validation) <= 128u);
    progress = noah_profile_rgb_v1_validation_begin(&validation, &reader, 7u, length, &limits, &error);
    assert(progress == NOAH_PROFILE_RGB_V1_VALIDATION_IN_PROGRESS);
    assert(context.calls == 0u && context.bytes_read == 0u);
    memset(&view, 0xa5, sizeof(view));
    assert(noah_profile_rgb_v1_validation_view(&validation, &view, &error) == NOAH_PROFILE_RGB_V1_VALIDATION_IN_PROGRESS);
    assert(view.reader.read == NULL && view.byte_length == 0u);

    progress = drive_incremental(&validation, &context, &error, &steps);
    assert(progress == NOAH_PROFILE_RGB_V1_VALIDATION_VALID);
    assert(context.max_read == NOAH_PROFILE_RGB_V1_HEADER_SIZE);
    assert(context.bytes_read == length);
    // One read a record; the replacement group table, empty here, takes a
    // step without one.
    assert(context.calls == 28u && steps == 29u);
    assert(noah_profile_rgb_v1_validation_step(&validation, &error) == NOAH_PROFILE_RGB_V1_VALIDATION_VALID);
    assert(context.calls == 28u && context.bytes_read == length);
    assert(noah_profile_rgb_v1_validation_view(&validation, &view, &error) == NOAH_PROFILE_RGB_V1_VALIDATION_VALID);
    assert(view.byte_length == length && view.base_offset == 7u);
    successful_calls = context.calls;

    for (size_t fail_call = 1u; fail_call <= successful_calls; fail_call++) {
        noah_profile_rgb_v1_error_t retained;

        context.calls      = 0u;
        context.bytes_read = 0u;
        context.max_read   = 0u;
        context.fail_call  = fail_call;
        steps              = 0u;
        assert(noah_profile_rgb_v1_validation_begin(&validation, &reader, 7u, length, &limits, &error) == NOAH_PROFILE_RGB_V1_VALIDATION_IN_PROGRESS);
        progress = drive_incremental(&validation, &context, &error, &steps);
        assert(progress == NOAH_PROFILE_RGB_V1_VALIDATION_REJECTED);
        assert(error.code == NOAH_PROFILE_RGB_V1_READ_ERROR);
        retained = error;
        assert(context.calls == fail_call);
        assert(context.max_read <= NOAH_PROFILE_RGB_V1_VALIDATION_READ_MAX);
        assert(noah_profile_rgb_v1_validation_step(&validation, &error) == NOAH_PROFILE_RGB_V1_VALIDATION_REJECTED);
        assert(memcmp(&error, &retained, sizeof(error)) == 0);
        assert(noah_profile_rgb_v1_validation_view(&validation, &view, &error) == NOAH_PROFILE_RGB_V1_VALIDATION_REJECTED);
        assert(memcmp(&error, &retained, sizeof(error)) == 0);
        assert(context.calls == fail_call);
    }
    context.fail_call = 0u;

    memset(&validation, 0, sizeof(validation));
    assert(noah_profile_rgb_v1_validation_step(&validation, &error) == NOAH_PROFILE_RGB_V1_VALIDATION_REJECTED);
    assert(error.code == NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT);
    context.calls = 0u;
    assert(noah_profile_rgb_v1_validation_begin(&validation, &reader, reader.length, 1u, &limits, &error) == NOAH_PROFILE_RGB_V1_VALIDATION_REJECTED);
    assert(error.code == NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT && context.calls == 0u);
}

static void test_incremental_error_locations(const char *fixture_path) {
    uint8_t                      valid[TEST_BUFFER_SIZE];
    uint8_t                      mutated[TEST_BUFFER_SIZE];
    size_t                       length = fixture_hex(fixture_path, "payload.hex", valid, sizeof(valid));
    noah_profile_rgb_v1_limits_t limits = fixture_limits(fixture_path);
    noah_profile_rgb_v1_view_t   view;
    noah_profile_rgb_v1_error_t  error;

    memcpy(mutated, valid, length);
    mutated[0] = 2u;
    expect_result(noah_profile_rgb_v1_decode(mutated, length, &limits, &view, &error), NOAH_PROFILE_RGB_V1_INVALID_VERSION);
    assert(error.offset == 0u && error.table == NOAH_PROFILE_RGB_V1_TABLE_HEADER && error.row == UINT8_MAX && error.field == NOAH_PROFILE_RGB_V1_FIELD_FORMAT_VERSION);

    memcpy(mutated, valid, length);
    memcpy(&mutated[26], &valid[17], 8u);
    expect_result(noah_profile_rgb_v1_decode(mutated, length, &limits, &view, &error), NOAH_PROFILE_RGB_V1_DUPLICATE_BITMAP);
    assert(error.offset == 26u && error.table == NOAH_PROFILE_RGB_V1_TABLE_GROUPS && error.row == 1u && error.field == NOAH_PROFILE_RGB_V1_FIELD_BITMAP);

    memcpy(mutated, valid, length);
    mutated[46] = 201u;
    expect_result(noah_profile_rgb_v1_decode(mutated, length, &limits, &view, &error), NOAH_PROFILE_RGB_V1_BRIGHTNESS_EXCEEDED);
    assert(error.offset == 46u && error.table == NOAH_PROFILE_RGB_V1_TABLE_LAYER_COLORS && error.row == 0u && error.field == NOAH_PROFILE_RGB_V1_FIELD_COLOR);

    memcpy(mutated, valid, length);
    mutated[62] = 3u;
    expect_result(noah_profile_rgb_v1_decode(mutated, length, &limits, &view, &error), NOAH_PROFILE_RGB_V1_INVALID_REFERENCE);
    assert(error.offset == 62u && error.table == NOAH_PROFILE_RGB_V1_TABLE_LAYER_GROUPS && error.row == 0u && error.field == NOAH_PROFILE_RGB_V1_FIELD_GROUP_ID);

    memcpy(mutated, valid, length);
    mutated[72] = 6u;
    expect_result(noah_profile_rgb_v1_decode(mutated, length, &limits, &view, &error), NOAH_PROFILE_RGB_V1_INVALID_ID);
    assert(error.offset == 72u && error.table == NOAH_PROFILE_RGB_V1_TABLE_PD_COLORS && error.row == 0u && error.field == NOAH_PROFILE_RGB_V1_FIELD_ID);
}

static void test_headers_lengths_and_dictionary(const char *fixture_path) {
    uint8_t                      valid[TEST_BUFFER_SIZE];
    uint8_t                      mutated[TEST_BUFFER_SIZE];
    uint8_t                      swap[8];
    size_t                       length = fixture_hex(fixture_path, "payload.hex", valid, sizeof(valid));
    noah_profile_rgb_v1_limits_t limits = fixture_limits(fixture_path);

    for (size_t truncated = 0u; truncated < length; truncated++)
        expect_decode(valid, truncated, &limits, NOAH_PROFILE_RGB_V1_TRUNCATED);
    memcpy(mutated, valid, length);
    mutated[length] = 0u;
    expect_decode(mutated, length + 1u, &limits, NOAH_PROFILE_RGB_V1_TRAILING_BYTES);
    memcpy(mutated, valid, length);
    mutated[0] = 2u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_VERSION);
    // Bytes 1 and 15 are reserved; byte 14 counts the replacement group rows.
    for (size_t offset = 1u; offset <= 15u; offset += 14u) {
        memcpy(mutated, valid, length);
        mutated[offset] = 1u;
        expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_RESERVED_BITS);
    }
    memcpy(mutated, valid, length);
    mutated[2] |= 0x40u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_RESERVED_BITS);
    memcpy(mutated, valid, length);
    mutated[12] = 57u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_INCOMPATIBLE_GEOMETRY);
    memcpy(mutated, valid, length);
    mutated[13] = 7u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_INCOMPATIBLE_GEOMETRY);
    memcpy(mutated, valid, length);
    mutated[4] = NOAH_PROFILE_RGB_V1_MAX_GROUPS + 1u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_CAPACITY_EXCEEDED);
    memcpy(mutated, valid, length);
    mutated[5] = NOAH_PROFILE_RGB_V1_MAX_LOGICAL_LAYERS + 1u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_CAPACITY_EXCEEDED);
    memcpy(mutated, valid, length);
    mutated[7] = NOAH_PROFILE_RGB_V1_MAX_POINTING_MODES + 1u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_CAPACITY_EXCEEDED);
    memcpy(mutated, valid, length);
    mutated[10] = NOAH_PROFILE_RGB_V1_MAX_TAP_BRANCH_COLORS + 1u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_CAPACITY_EXCEEDED);
    memcpy(mutated, valid, length);
    // 641 rows in all, each table within its byte (D-F63).
    mutated[6]  = NOAH_PROFILE_RGB_V1_MAX_TABLE_GROUP_ROWS;
    mutated[8]  = NOAH_PROFILE_RGB_V1_MAX_TABLE_GROUP_ROWS;
    mutated[9]  = (uint8_t)(NOAH_PROFILE_RGB_V1_MAX_STAGE_GROUP_ROWS + 1u - 2u * NOAH_PROFILE_RGB_V1_MAX_TABLE_GROUP_ROWS);
    mutated[11] = 0u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_CAPACITY_EXCEEDED);

    memcpy(mutated, valid, length);
    mutated[16] = 1u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_NONCANONICAL_ORDER);
    memcpy(mutated, valid, length);
    mutated[24] = 4u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_RESERVED_BITS);
    memcpy(mutated, valid, length);
    memcpy(&mutated[26], &valid[17], 8u);
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_DUPLICATE_BITMAP);
    memcpy(mutated, valid, length);
    memcpy(swap, &mutated[26], 8u);
    memcpy(&mutated[26], &mutated[35], 8u);
    memcpy(&mutated[35], swap, 8u);
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_NONCANONICAL_ORDER);
}

static void test_fields_features_and_surfaces(const char *fixture_path) {
    uint8_t                      valid[TEST_BUFFER_SIZE];
    uint8_t                      mutated[TEST_BUFFER_SIZE];
    size_t                       length = fixture_hex(fixture_path, "payload.hex", valid, sizeof(valid));
    noah_profile_rgb_v1_limits_t limits = fixture_limits(fixture_path);

    memcpy(mutated, valid, length);
    mutated[43] = 1u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_NONCANONICAL_ORDER);
    memcpy(mutated, valid, length);
    mutated[47] = 2u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_ENUM);
    memcpy(mutated, valid, length);
    mutated[46] = 201u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_BRIGHTNESS_EXCEEDED);
    memcpy(mutated, valid, length);
    mutated[58] = 3u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_SELECTOR);
    memcpy(mutated, valid, length);
    mutated[62] = 3u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_REFERENCE);
    memcpy(mutated, valid, length);
    mutated[68] = 3u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_ENUM);
    memcpy(mutated, valid, length);
    mutated[72] = 6u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_ID);
    memcpy(mutated, valid, length);
    mutated[77] = 0u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_NONCANONICAL_ORDER);
    memcpy(mutated, valid, length);
    mutated[76] = 5u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_ENUM);
    memcpy(mutated, valid, length);
    mutated[102] = 6u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_SELECTOR);
    memcpy(mutated, valid, length);
    mutated[115] = 5u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_ENUM);
    memcpy(mutated, valid, length);
    mutated[119] = 3u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_REFERENCE);
    memcpy(mutated, valid, length);
    mutated[141] = 2u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_ENUM);
    memcpy(mutated, valid, length);
    mutated[142] = 5u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_ENUM);
    memcpy(mutated, valid, length);
    mutated[148] = 4u;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_SELECTOR);

    limits.logical_layer_count = 4u;
    expect_decode(valid, length, &limits, NOAH_PROFILE_RGB_V1_INCOMPLETE_SURFACE);
    limits                        = fixture_limits(fixture_path);
    limits.tap_branch_color_count = 3u;
    expect_decode(valid, length, &limits, NOAH_PROFILE_RGB_V1_INCOMPLETE_SURFACE);
    limits = fixture_limits(fixture_path);
    limits.compiled_stage_mask &= (uint16_t)~NOAH_PROFILE_RGB_V1_STAGE_AUTOMOUSE;
    expect_decode(valid, length, &limits, NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE);
    memcpy(mutated, valid, length);
    mutated[2] &= (uint8_t)~NOAH_PROFILE_RGB_V1_STAGE_AUTOMOUSE;
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE_DATA);
    memcpy(mutated, valid, length);
    mutated[2] = 0u;
    mutated[3] = 0u;
    limits     = fixture_limits(fixture_path);
    expect_decode(mutated, length, &limits, NOAH_PROFILE_RGB_V1_OK);

    limits.supported_pointing_mode_mask = 0x1fu;
    expect_decode(valid, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_ID);

    limits                  = fixture_limits(fixture_path);
    limits.max_payload_size = (uint16_t)(length - 1u);
    expect_decode(valid, length, &limits, NOAH_PROFILE_RGB_V1_CAPACITY_EXCEEDED);
    limits                     = fixture_limits(fixture_path);
    limits.compiled_stage_mask = 0x40u;
    expect_decode(valid, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT);
    limits                        = fixture_limits(fixture_path);
    limits.tap_branch_color_count = 0u;
    expect_decode(valid, length, &limits, NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT);
}

static void test_canonical_empty_forms(void) {
    uint8_t                      payload[50] = {0};
    noah_profile_rgb_v1_limits_t limits      = noah_profile_rgb_v1_default_limits();

    payload[0]                          = NOAH_PROFILE_RGB_V1_FORMAT_VERSION;
    payload[2]                          = NOAH_PROFILE_RGB_V1_STAGE_LAYER;
    payload[5]                          = 1u;
    payload[12]                         = NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT;
    payload[13]                         = NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE;
    limits.compiled_stage_mask          = NOAH_PROFILE_RGB_V1_STAGE_LAYER;
    limits.logical_layer_count          = 1u;
    limits.supported_pointing_mode_mask = 0u;
    expect_decode(payload, sizeof(payload), &limits, NOAH_PROFILE_RGB_V1_OK);
    payload[21] = 1u;
    expect_decode(payload, sizeof(payload), &limits, NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE_DATA);
    payload[21] = 0u;
    payload[25] = 1u;
    expect_decode(payload, sizeof(payload), &limits, NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE_DATA);
    payload[25] = 0u;
    payload[29] = 1u;
    expect_decode(payload, sizeof(payload), &limits, NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE_DATA);
}

static void test_maximum_dictionary_and_rows(void) {
    // 128 groups and one table's 255 rows (D-F63).
    uint8_t                                 payload[2477] = {0};
    size_t                                  offset;
    noah_profile_rgb_v1_limits_t            limits = noah_profile_rgb_v1_default_limits();
    noah_profile_rgb_v1_view_t              view;
    noah_profile_rgb_v1_error_t             error;
    instrumented_reader_t                   context;
    noah_profile_reader_t                   reader;
    noah_profile_rgb_v1_validation_t        validation;
    noah_profile_rgb_v1_validation_result_t progress;
    size_t                                  steps = 0u;

    payload[0]  = NOAH_PROFILE_RGB_V1_FORMAT_VERSION;
    payload[2]  = NOAH_PROFILE_RGB_V1_STAGE_LAYER;
    payload[4]  = NOAH_PROFILE_RGB_V1_MAX_GROUPS;
    payload[5]  = 1u;
    payload[6]  = NOAH_PROFILE_RGB_V1_MAX_TABLE_GROUP_ROWS;
    payload[12] = NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT;
    payload[13] = NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE;
    offset      = NOAH_PROFILE_RGB_V1_HEADER_SIZE;
    for (uint16_t group = 0u; group < NOAH_PROFILE_RGB_V1_MAX_GROUPS; group++) {
        payload[offset]      = (uint8_t)group;
        payload[offset + 1u] = (uint8_t)group;
        offset += 9u;
    }
    offset += 5u; // one canonical layer-zero row
    for (uint16_t row = 0u; row < NOAH_PROFILE_RGB_V1_MAX_TABLE_GROUP_ROWS; row++) {
        payload[offset]      = NOAH_PROFILE_RGB_V1_SELECTOR_ALL;
        payload[offset + 4u] = (uint8_t)(row % NOAH_PROFILE_RGB_V1_MAX_GROUPS);
        offset += 5u;
    }
    assert(offset + 4u + 4u + 11u + 10u == sizeof(payload));
    limits.compiled_stage_mask          = NOAH_PROFILE_RGB_V1_STAGE_LAYER;
    limits.logical_layer_count          = 1u;
    limits.supported_pointing_mode_mask = 0u;
    expect_result(noah_profile_rgb_v1_decode(payload, sizeof(payload), &limits, &view, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(view.group_count == NOAH_PROFILE_RGB_V1_MAX_GROUPS);
    assert(view.layer_group_count == NOAH_PROFILE_RGB_V1_MAX_TABLE_GROUP_ROWS);

    context = (instrumented_reader_t){.bytes = payload, .length = sizeof(payload)};
    reader  = (noah_profile_reader_t){.read = instrumented_read, .context = &context, .length = sizeof(payload)};
    assert(noah_profile_rgb_v1_validation_begin(&validation, &reader, 0u, sizeof(payload), &limits, &error) == NOAH_PROFILE_RGB_V1_VALIDATION_IN_PROGRESS);
    progress = drive_incremental(&validation, &context, &error, &steps);
    assert(progress == NOAH_PROFILE_RGB_V1_VALIDATION_VALID);
    // The header, 128 groups, a layer colour, 255 rows and four fixed records.
    assert(context.calls == 389u && context.bytes_read == sizeof(payload));
    assert(context.max_read == NOAH_PROFILE_RGB_V1_HEADER_SIZE);
    assert(steps == 395u);

    // A 129th group is refused at the header.
    payload[4] = NOAH_PROFILE_RGB_V1_MAX_GROUPS + 1u;
    expect_decode(payload, sizeof(payload), &limits, NOAH_PROFILE_RGB_V1_CAPACITY_EXCEEDED);
}

// 640 rows in all across the layer, pointing and behaviour-feedback tables is the
// ceiling (D-F63); one more is refused, and a fault in a table's last row,
// row 254, is located there.
static void test_total_rows_at_ceiling(void) {
    enum {
        LAYER_ROWS = NOAH_PROFILE_RGB_V1_MAX_TABLE_GROUP_ROWS,
        PD_ROWS    = NOAH_PROFILE_RGB_V1_MAX_TABLE_GROUP_ROWS,
        KEY_ROWS   = NOAH_PROFILE_RGB_V1_MAX_STAGE_GROUP_ROWS - LAYER_ROWS - PD_ROWS,
        SIZE       = NOAH_PROFILE_RGB_V1_FIXED_PAYLOAD_SIZE + 9u + 5u + 5u * LAYER_ROWS + 5u * NOAH_PROFILE_RGB_V1_MAX_POINTING_MODES + 5u * PD_ROWS + 3u * NOAH_PROFILE_RGB_V1_MAX_TAP_BRANCH_COLORS + 5u * KEY_ROWS,
    };
    static uint8_t               payload[SIZE + 5u];
    size_t                       offset = NOAH_PROFILE_RGB_V1_HEADER_SIZE;
    size_t                       last_layer_row;
    noah_profile_rgb_v1_limits_t limits = noah_profile_rgb_v1_default_limits();
    noah_profile_rgb_v1_view_t   view;
    noah_profile_rgb_v1_error_t  error;

    memset(payload, 0, sizeof(payload));
    payload[0]  = NOAH_PROFILE_RGB_V1_FORMAT_VERSION;
    payload[2]  = NOAH_PROFILE_RGB_V1_STAGE_LAYER | NOAH_PROFILE_RGB_V1_STAGE_POINTING_MODE | NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR;
    payload[4]  = 1u;
    payload[5]  = 1u;
    payload[6]  = LAYER_ROWS;
    payload[7]  = NOAH_PROFILE_RGB_V1_MAX_POINTING_MODES;
    payload[8]  = PD_ROWS;
    payload[10] = NOAH_PROFILE_RGB_V1_MAX_TAP_BRANCH_COLORS;
    payload[11] = KEY_ROWS;
    payload[12] = NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT;
    payload[13] = NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE;
    offset += 9u; // one empty group
    offset += 5u; // layer zero's colour
    for (uint16_t row = 0u; row < LAYER_ROWS; row++, offset += 5u) {
        payload[offset] = row % 2u == 0u ? 0u : NOAH_PROFILE_RGB_V1_SELECTOR_ALL;
    }
    last_layer_row = offset - 5u;
    offset += 4u; // auto-mouse fade
    for (uint8_t slot = 0u; slot < NOAH_PROFILE_RGB_V1_MAX_POINTING_MODES; slot++, offset += 5u) {
        payload[offset] = slot;
    }
    for (uint16_t row = 0u; row < PD_ROWS; row++, offset += 5u) {
        payload[offset] = (uint8_t)(row % 33u == 32u ? NOAH_PROFILE_RGB_V1_SELECTOR_ALL : row % 33u);
    }
    offset += 4u;                                                   // combo feedback
    offset += 3u * NOAH_PROFILE_RGB_V1_MAX_TAP_BRANCH_COLORS + 11u; // tap-branch colours, behaviour feedback
    for (uint16_t row = 0u; row < KEY_ROWS; row++, offset += 5u) {
        payload[offset] = (uint8_t)(row % 5u == 4u ? NOAH_PROFILE_RGB_V1_SELECTOR_ALL : row % 5u);
    }
    offset += 10u; // replacement feedback
    assert(offset == SIZE);
    limits.logical_layer_count = 1u;
    expect_result(noah_profile_rgb_v1_decode(payload, SIZE, &limits, &view, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(view.layer_group_count == LAYER_ROWS && view.pd_group_count == PD_ROWS && view.key_group_count == KEY_ROWS);

    payload[11] = KEY_ROWS + 1u;
    expect_decode(payload, SIZE + 5u, &limits, NOAH_PROFILE_RGB_V1_CAPACITY_EXCEEDED);
    payload[11] = KEY_ROWS;

    payload[last_layer_row + 4u] = 1u;
    expect_result(noah_profile_rgb_v1_decode(payload, SIZE, &limits, &view, &error), NOAH_PROFILE_RGB_V1_INVALID_REFERENCE);
    assert(error.table == NOAH_PROFILE_RGB_V1_TABLE_LAYER_GROUPS && error.row == LAYER_ROWS - 1u && error.offset == last_layer_row + 4u);
}

// Replacement feedback: its record and rows read back as written, and each
// fault is refused where it is, with the table and field ids the spec lists.
static void test_replacement_feedback_records_and_refusals(void) {
    enum { RECORD = 49, ROW_0 = 59, ROW_1 = 64, SIZE = 69 };
    uint8_t                                    valid[SIZE] = {0};
    uint8_t                                    payload[SIZE];
    noah_profile_rgb_v1_limits_t               limits = noah_profile_rgb_v1_default_limits();
    noah_profile_rgb_v1_limits_t               without;
    noah_profile_rgb_v1_view_t                 view;
    noah_profile_rgb_v1_error_t                error;
    noah_profile_rgb_v1_autocorrect_feedback_t record;
    noah_profile_rgb_v1_group_row_t            row;

    assert(NOAH_PROFILE_RGB_V1_TABLE_AUTOCORRECT_FEEDBACK == 12 && NOAH_PROFILE_RGB_V1_TABLE_AUTOCORRECT_GROUPS == 13 && NOAH_PROFILE_RGB_V1_STAGE_AUTOCORRECT == 0x20);
    valid[0]  = NOAH_PROFILE_RGB_V1_FORMAT_VERSION;
    valid[2]  = NOAH_PROFILE_RGB_V1_STAGE_LAYER | NOAH_PROFILE_RGB_V1_STAGE_AUTOCORRECT;
    valid[4]  = 1u; // one LED set
    valid[5]  = 1u; // one layer colour
    valid[12] = NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT;
    valid[13] = NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE;
    valid[14] = 2u; // two replacement rows
    valid[17] = 0x01u;
    memcpy(&valid[RECORD], (const uint8_t[]){1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 4u}, NOAH_PROFILE_RGB_V1_AUTOCORRECT_FEEDBACK_SIZE);
    memcpy(&valid[ROW_0], (const uint8_t[]){0xffu, 0u, 0u, 0u, 0u}, NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE);
    memcpy(&valid[ROW_1], (const uint8_t[]){2u, 10u, 11u, 12u, 0u}, NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE);
    limits.compiled_stage_mask          = NOAH_PROFILE_RGB_V1_STAGE_LAYER | NOAH_PROFILE_RGB_V1_STAGE_AUTOCORRECT;
    limits.logical_layer_count          = 1u;
    limits.supported_pointing_mode_mask = 0u;
    limits.maximum_brightness           = 200u;

    expect_result(noah_profile_rgb_v1_decode(valid, SIZE, &limits, &view, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(view.autocorrect_group_count == 2u && view.stage_enable_mask == valid[2]);
    expect_result(noah_profile_rgb_v1_autocorrect_feedback(&view, &record, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(record.colors[NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_TEXT].h == 1u && record.colors[NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_TEXT].v == 3u);
    assert(record.colors[NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_MACRO].h == 4u && record.colors[NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_MACRO].s == 5u);
    assert(record.colors[NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_UNDO].h == 7u && record.colors[NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_UNDO].v == 9u && record.locality == 4u);
    expect_result(noah_profile_rgb_v1_autocorrect_group_at(&view, 0u, &row, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(row.selector == NOAH_PROFILE_RGB_V1_SELECTOR_ALL && row.color.v == 0u && row.group_id == 0u);
    expect_result(noah_profile_rgb_v1_autocorrect_group_at(&view, 1u, &row, &error), NOAH_PROFILE_RGB_V1_OK);
    assert(row.selector == NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_UNDO && row.color.h == 10u && row.color.v == 12u && row.group_id == 0u);
    expect_result(noah_profile_rgb_v1_autocorrect_group_at(&view, 2u, &row, &error), NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT);

    // The stage switched off keeps its record and rows.
    memcpy(payload, valid, SIZE);
    payload[2] = NOAH_PROFILE_RGB_V1_STAGE_LAYER;
    expect_decode(payload, SIZE, &limits, NOAH_PROFILE_RGB_V1_OK);

    memcpy(payload, valid, SIZE);
    payload[RECORD + 9u] = 5u;
    expect_result(noah_profile_rgb_v1_decode(payload, SIZE, &limits, &view, &error), NOAH_PROFILE_RGB_V1_INVALID_ENUM);
    assert(error.offset == RECORD + 9u && error.table == NOAH_PROFILE_RGB_V1_TABLE_AUTOCORRECT_FEEDBACK && error.row == 0u && error.field == NOAH_PROFILE_RGB_V1_FIELD_LOCALITY);

    memcpy(payload, valid, SIZE);
    payload[RECORD + 5u] = 201u;
    expect_result(noah_profile_rgb_v1_decode(payload, SIZE, &limits, &view, &error), NOAH_PROFILE_RGB_V1_BRIGHTNESS_EXCEEDED);
    assert(error.offset == RECORD + 5u && error.table == NOAH_PROFILE_RGB_V1_TABLE_AUTOCORRECT_FEEDBACK && error.field == NOAH_PROFILE_RGB_V1_FIELD_COLOR);

    memcpy(payload, valid, SIZE);
    payload[ROW_1] = NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_COUNT;
    expect_result(noah_profile_rgb_v1_decode(payload, SIZE, &limits, &view, &error), NOAH_PROFILE_RGB_V1_INVALID_SELECTOR);
    assert(error.offset == ROW_1 && error.table == NOAH_PROFILE_RGB_V1_TABLE_AUTOCORRECT_GROUPS && error.row == 1u && error.field == NOAH_PROFILE_RGB_V1_FIELD_SEMANTIC);

    memcpy(payload, valid, SIZE);
    payload[ROW_1 + 3u] = 201u;
    expect_result(noah_profile_rgb_v1_decode(payload, SIZE, &limits, &view, &error), NOAH_PROFILE_RGB_V1_BRIGHTNESS_EXCEEDED);
    assert(error.offset == ROW_1 + 3u && error.table == NOAH_PROFILE_RGB_V1_TABLE_AUTOCORRECT_GROUPS && error.row == 1u);

    memcpy(payload, valid, SIZE);
    payload[ROW_0 + 4u] = 1u;
    expect_result(noah_profile_rgb_v1_decode(payload, SIZE, &limits, &view, &error), NOAH_PROFILE_RGB_V1_INVALID_REFERENCE);
    assert(error.offset == ROW_0 + 4u && error.table == NOAH_PROFILE_RGB_V1_TABLE_AUTOCORRECT_GROUPS && error.row == 0u && error.field == NOAH_PROFILE_RGB_V1_FIELD_GROUP_ID);

    // A build without the stage refuses its bit, then any data for it: a
    // colour, a locality or a row, each on its own.
    without                     = limits;
    without.compiled_stage_mask = NOAH_PROFILE_RGB_V1_STAGE_LAYER;
    expect_result(noah_profile_rgb_v1_decode(valid, SIZE, &without, &view, &error), NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE);
    assert(error.offset == 2u && error.table == NOAH_PROFILE_RGB_V1_TABLE_HEADER && error.field == NOAH_PROFILE_RGB_V1_FIELD_STAGE_MASK);
    memcpy(payload, valid, SIZE);
    payload[2] = NOAH_PROFILE_RGB_V1_STAGE_LAYER;
    expect_result(noah_profile_rgb_v1_decode(payload, SIZE, &without, &view, &error), NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE_DATA);
    assert(error.offset == RECORD && error.table == NOAH_PROFILE_RGB_V1_TABLE_AUTOCORRECT_FEEDBACK && error.row == 0u);
    memset(&payload[RECORD], 0, NOAH_PROFILE_RGB_V1_AUTOCORRECT_FEEDBACK_SIZE);
    expect_decode(payload, SIZE, &without, NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE_DATA);
    payload[14] = 0u;
    expect_decode(payload, RECORD + NOAH_PROFILE_RGB_V1_AUTOCORRECT_FEEDBACK_SIZE, &without, NOAH_PROFILE_RGB_V1_OK);
    payload[RECORD + 9u] = 1u;
    expect_decode(payload, RECORD + NOAH_PROFILE_RGB_V1_AUTOCORRECT_FEEDBACK_SIZE, &without, NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE_DATA);
    payload[RECORD + 9u] = 0u;
    payload[RECORD + 7u] = 1u;
    expect_decode(payload, RECORD + NOAH_PROFILE_RGB_V1_AUTOCORRECT_FEEDBACK_SIZE, &without, NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE_DATA);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    test_shared_golden_and_accessors(argv[1]);
    test_reader_is_bounded(argv[1]);
    test_incremental_reader_budget_and_failures(argv[1]);
    test_incremental_error_locations(argv[1]);
    test_headers_lengths_and_dictionary(argv[1]);
    test_fields_features_and_surfaces(argv[1]);
    test_canonical_empty_forms();
    test_maximum_dictionary_and_rows();
    test_total_rows_at_ceiling();
    test_replacement_feedback_records_and_refusals();
    puts("profile RGB v1 tests passed");
    return 0;
}
