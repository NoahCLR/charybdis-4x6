#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/schema/profile_validator_v1.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"

enum {
    TEST_BUFFER_SIZE  = NOAH_PROFILE_BLOB_V1_MAX_SIZE + 16u,
    ACTION_ABI_DIGEST = UINT32_C(0x12345678),
};

typedef struct {
    const uint8_t *bytes;
    size_t         length;
    size_t         calls;
    size_t         fail_call;
    size_t         step_calls;
    size_t         step_bytes;
    size_t         step_max_read;
} instrumented_reader_t;

static bool fixture_value(const char *path, const char *key, char *value, size_t capacity) {
    FILE  *file = fopen(path, "r");
    char   line[TEST_BUFFER_SIZE * 2u + 128u];
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

static unsigned long fixture_ulong(const char *path, const char *key, int base) {
    char          encoded[32];
    char         *end;
    unsigned long value;

    assert(fixture_value(path, key, encoded, sizeof(encoded)));
    value = strtoul(encoded, &end, base);
    assert(*encoded != '\0' && *end == '\0');
    return value;
}

static bool instrumented_read(void *context_value, size_t offset, uint8_t *target, size_t length) {
    instrumented_reader_t *context = context_value;

    context->calls++;
    context->step_calls++;
    context->step_bytes += length;
    if (length > context->step_max_read) context->step_max_read = length;
    if (context->fail_call != 0u && context->calls == context->fail_call) return false;
    assert(offset <= context->length && length <= context->length - offset);
    if (length != 0u) memcpy(target, &context->bytes[offset], length);
    return true;
}

static void reset_step_counts(instrumented_reader_t *reader) {
    reader->step_calls    = 0u;
    reader->step_bytes    = 0u;
    reader->step_max_read = 0u;
}

static noah_profile_validator_v1_declaration_t declaration_for(const uint8_t *bytes, size_t length, uint8_t domain_mask) {
    noah_profile_validator_v1_declaration_t declaration = {
        .schema_major      = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR,
        .schema_minor      = NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR,
        .domain_mask       = domain_mask,
        .flags             = 0u,
        .byte_length       = (uint16_t)length,
        .crc32             = noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, length)),
        .digest            = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, bytes, length),
        .action_abi_digest = ACTION_ABI_DIGEST,
    };
    return declaration;
}

static noah_profile_validator_v1_compatibility_t compatibility(void) {
    noah_profile_validator_v1_compatibility_t value = noah_profile_validator_v1_default_compatibility(ACTION_ABI_DIGEST);

    value.required_domain_mask              = (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS);
    value.allowed_domain_mask               = NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS;
    value.logical_layer_count               = 3u;
    value.supported_pd_mode_mask            = NOAH_PROFILE_RGB_V1_PD_MODE_MASK_ALL;
    value.via_macro_slot_count              = 11u;
    value.hardcoded_macro_slot_count        = 3u;
    value.rgb_limits.compiled_stage_mask    = NOAH_PROFILE_RGB_V1_STAGE_MASK_ALL;
    value.rgb_limits.logical_layer_count    = value.logical_layer_count;
    value.rgb_limits.maximum_brightness     = 200u;
    value.rgb_limits.tap_branch_color_count = 4u;
    value.rgb_limits.supported_pd_mode_mask = value.supported_pd_mode_mask;
    return value;
}

static noah_profile_validator_v1_result_t validate(const uint8_t *bytes, size_t length, const noah_profile_validator_v1_declaration_t *declaration, const noah_profile_validator_v1_compatibility_t *compatible, noah_profile_validator_v1_error_t *error) {
    instrumented_reader_t              state  = {.bytes = bytes, .length = length};
    noah_profile_reader_t              reader = {.read = instrumented_read, .context = &state, .length = length};
    noah_profile_validator_v1_t        validator;
    noah_profile_validator_v1_result_t result = noah_profile_validator_v1_begin(&validator, &reader, 0u, declaration, compatible, error);

    for (unsigned iteration = 0u; result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS && iteration < 1000u; iteration++) {
        result = noah_profile_validator_v1_step(&validator, NOAH_PROFILE_VALIDATOR_V1_CHECKSUM_CHUNK_MAX, error);
    }
    assert(result != NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS);
    return result;
}

static size_t behavior_only_blob(const uint8_t *full, size_t full_length, size_t envelope_offset, uint8_t *output) {
    size_t envelope_length;

    assert(envelope_offset + NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE <= full_length);
    envelope_length = NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE + (size_t)full[envelope_offset + 2u] + ((size_t)full[envelope_offset + 3u] << 8u);
    memcpy(output, full, NOAH_PROFILE_BLOB_V1_HEADER_SIZE);
    output[6] = 1u;
    memcpy(&output[NOAH_PROFILE_BLOB_V1_HEADER_SIZE], &full[envelope_offset], envelope_length);
    return NOAH_PROFILE_BLOB_V1_HEADER_SIZE + envelope_length;
}

static void expect_result(noah_profile_validator_v1_result_t actual, noah_profile_validator_v1_result_t expected) {
    if (actual != expected) {
        fprintf(stderr, "validator result mismatch: got %u expected %u\n", (unsigned)actual, (unsigned)expected);
        abort();
    }
}

static void test_golden_incremental_phases(const char *fixture_path) {
    uint8_t                                   bytes[TEST_BUFFER_SIZE + 10u];
    size_t                                    length      = fixture_hex(fixture_path, "profile.full.hex", &bytes[5], TEST_BUFFER_SIZE);
    instrumented_reader_t                     state       = {.bytes = bytes, .length = length + 10u};
    noah_profile_reader_t                     reader      = {.read = instrumented_read, .context = &state, .length = length + 10u};
    noah_profile_validator_v1_declaration_t   declaration = declaration_for(&bytes[5], length, (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS));
    noah_profile_validator_v1_compatibility_t compatible  = compatibility();
    noah_profile_validator_v1_t               validator;
    noah_profile_validator_v1_profile_t       profile;
    noah_profile_validator_v1_error_t         error;
    noah_profile_validator_v1_result_t        result;

    assert(length == fixture_ulong(fixture_path, "profile.full.length", 10));
    assert(declaration.crc32 == fixture_ulong(fixture_path, "profile.full.crc32", 16));
    assert(declaration.digest == fixture_ulong(fixture_path, "profile.full.fnv1a32", 16));
    assert(declaration.action_abi_digest == fixture_ulong(fixture_path, "profile.action_abi", 10));
    result = noah_profile_validator_v1_begin(&validator, &reader, 5u, &declaration, &compatible, &error);
    expect_result(result, NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS);
    assert(state.calls == 0u && validator.phase == NOAH_PROFILE_VALIDATOR_V1_PHASE_CHECKSUM);

    for (unsigned iteration = 0u; result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS && iteration < 1000u; iteration++) {
        noah_profile_validator_v1_phase_t prior_phase           = validator.phase;
        size_t                            prior_checksum_offset = validator.checksum_offset;

        reset_step_counts(&state);
        result = noah_profile_validator_v1_step(&validator, 7u, &error);
        assert(state.step_calls <= 1u);
        assert(state.step_bytes <= NOAH_PROFILE_VALIDATOR_V1_STEP_READ_MAX);
        assert(state.step_max_read <= NOAH_PROFILE_VALIDATOR_V1_STEP_READ_MAX);
        if (prior_phase == NOAH_PROFILE_VALIDATOR_V1_PHASE_CHECKSUM) {
            assert(state.step_calls == 1u && state.step_bytes <= 7u && state.step_max_read <= 7u);
            assert(validator.checksum_offset > prior_checksum_offset);
        } else if (prior_phase == NOAH_PROFILE_VALIDATOR_V1_PHASE_BLOB_HEADER) {
            assert(state.step_calls == 1u && state.step_max_read == NOAH_PROFILE_BLOB_V1_HEADER_SIZE);
        } else if (prior_phase == NOAH_PROFILE_VALIDATOR_V1_PHASE_DOMAIN_HEADER && validator.domain_index < validator.declared_domain_count) {
            assert(state.step_calls == 1u && state.step_max_read == NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE);
        }
    }
    expect_result(result, NOAH_PROFILE_VALIDATOR_V1_VALID);
    assert(validator.phase == NOAH_PROFILE_VALIDATOR_V1_PHASE_VALID);
    expect_result(noah_profile_validator_v1_profile(&validator, &profile, &error), NOAH_PROFILE_VALIDATOR_V1_VALID);
    assert(profile.domain_mask == (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS) && profile.domain_count == 2u);
    assert(profile.byte_length == length && profile.crc32 == declaration.crc32 && profile.digest == declaration.digest);
    assert(profile.rgb.layer_color_count == 3u && profile.key_behaviors.row_count == 2u);
}

static void test_identity_capacity_and_masks(const char *fixture_path) {
    uint8_t                                   full[TEST_BUFFER_SIZE];
    uint8_t                                   empty[NOAH_PROFILE_BLOB_V1_HEADER_SIZE] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};
    size_t                                    full_length                             = fixture_hex(fixture_path, "profile.full.hex", full, sizeof(full));
    noah_profile_validator_v1_compatibility_t compatible                              = compatibility();
    noah_profile_validator_v1_declaration_t   declaration                             = declaration_for(full, full_length, (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS));
    noah_profile_validator_v1_error_t         error;
    instrumented_reader_t                     state  = {.bytes = full, .length = full_length};
    noah_profile_reader_t                     reader = {.read = instrumented_read, .context = &state, .length = full_length};
    noah_profile_validator_v1_t               validator;

    declaration.action_abi_digest++;
    expect_result(noah_profile_validator_v1_begin(&validator, &reader, 0u, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INCOMPATIBLE_ACTION_ABI);
    assert(state.calls == 0u);
    declaration.action_abi_digest--;
    declaration.schema_minor = 1u;
    expect_result(noah_profile_validator_v1_begin(&validator, &reader, 0u, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INCOMPATIBLE_SCHEMA);
    declaration.schema_minor = 0u;
    declaration.flags        = 1u;
    expect_result(noah_profile_validator_v1_begin(&validator, &reader, 0u, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_BLOB);
    declaration.flags        = 0u;
    compatible.max_blob_size = (uint16_t)(full_length - 1u);
    expect_result(noah_profile_validator_v1_begin(&validator, &reader, 0u, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_CAPACITY_EXCEEDED);

    declaration = declaration_for(empty, sizeof(empty), 0u);
    compatible  = noah_profile_validator_v1_default_compatibility(ACTION_ABI_DIGEST);
    expect_result(validate(empty, sizeof(empty), &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_VALID);
    compatible.required_domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    expect_result(validate(empty, sizeof(empty), &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_MISSING_DOMAIN);

    declaration             = declaration_for(full, full_length, (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS));
    compatible              = compatibility();
    declaration.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    expect_result(validate(full, full_length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_DOMAIN_MASK_MISMATCH);
    declaration.domain_mask         = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS;
    compatible.allowed_domain_mask  = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    compatible.required_domain_mask = 0u;
    expect_result(noah_profile_validator_v1_begin(&validator, &reader, 0u, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_UNSUPPORTED_DOMAIN);
}

static void test_blob_and_domain_rejections(const char *fixture_path) {
    uint8_t                                   valid[TEST_BUFFER_SIZE];
    uint8_t                                   bytes[TEST_BUFFER_SIZE];
    uint8_t                                   reversed[TEST_BUFFER_SIZE];
    size_t                                    length            = fixture_hex(fixture_path, "profile.full.hex", valid, sizeof(valid));
    size_t                                    behavior_envelope = fixture_ulong(fixture_path, "profile.behavior_envelope_offset", 10);
    size_t                                    behavior_payload  = fixture_ulong(fixture_path, "profile.behavior_payload_offset", 10);
    noah_profile_validator_v1_compatibility_t compatible        = compatibility();
    noah_profile_validator_v1_declaration_t   declaration;
    noah_profile_validator_v1_error_t         error;

    memcpy(bytes, valid, length);
    bytes[0]    = 'X';
    declaration = declaration_for(bytes, length, (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS));
    expect_result(validate(bytes, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_BLOB);
    assert(error.detail_kind == NOAH_PROFILE_VALIDATOR_V1_DETAIL_BLOB && error.detail_code == NOAH_PROFILE_CODEC_V1_INVALID_MAGIC);

    memcpy(bytes, valid, length);
    bytes[6]    = 1u;
    declaration = declaration_for(bytes, length, (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS));
    expect_result(validate(bytes, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_BLOB);
    assert(error.detail_code == NOAH_PROFILE_CODEC_V1_TRAILING_BYTES);

    memcpy(bytes, valid, length);
    bytes[behavior_envelope] = NOAH_PROFILE_DOMAIN_V1_RGB;
    declaration              = declaration_for(bytes, length, (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS));
    expect_result(validate(bytes, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_BLOB);
    assert(error.detail_code == NOAH_PROFILE_CODEC_V1_DUPLICATE_DOMAIN && error.domain_index == 1u);

    memcpy(reversed, valid, NOAH_PROFILE_BLOB_V1_HEADER_SIZE);
    memcpy(&reversed[NOAH_PROFILE_BLOB_V1_HEADER_SIZE], &valid[behavior_envelope], length - behavior_envelope);
    memcpy(&reversed[NOAH_PROFILE_BLOB_V1_HEADER_SIZE + length - behavior_envelope], &valid[NOAH_PROFILE_BLOB_V1_HEADER_SIZE], behavior_envelope - NOAH_PROFILE_BLOB_V1_HEADER_SIZE);
    declaration = declaration_for(reversed, length, (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS));
    expect_result(validate(reversed, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_BLOB);
    assert(error.detail_code == NOAH_PROFILE_CODEC_V1_DOMAIN_ORDER && error.domain_index == 1u);

    memcpy(bytes, valid, length);
    bytes[8]    = 0x50u;
    declaration = declaration_for(bytes, length, (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS));
    expect_result(validate(bytes, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_UNSUPPORTED_DOMAIN);

    memcpy(bytes, valid, length);
    bytes[12]   = 2u;
    declaration = declaration_for(bytes, length, (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS));
    expect_result(validate(bytes, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    assert(error.detail_kind == NOAH_PROFILE_VALIDATOR_V1_DETAIL_RGB && error.detail_code == NOAH_PROFILE_RGB_V1_INVALID_VERSION && error.byte_offset == 12u);

    memcpy(bytes, valid, length);
    bytes[behavior_payload + 2u] = 1u;
    declaration                  = declaration_for(bytes, length, (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS));
    expect_result(validate(bytes, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    assert(error.detail_kind == NOAH_PROFILE_VALIDATOR_V1_DETAIL_KEY_BEHAVIOR && error.detail_code == NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS);

    declaration                         = declaration_for(valid, length, (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS));
    compatible.behavior_limits.max_rows = 1u;
    expect_result(validate(valid, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_CAPACITY_EXCEEDED);

    compatible = compatibility();
    memcpy(bytes, valid, length);
    bytes[20] ^= 1u;
    expect_result(validate(bytes, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_CHECKSUM_MISMATCH);
}

static void expect_behavior_reference_failure(const uint8_t *blob, size_t length, noah_profile_validator_v1_compatibility_t *compatible, uint8_t expected_field) {
    noah_profile_validator_v1_declaration_t declaration = declaration_for(blob, length, NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS);
    noah_profile_validator_v1_error_t       error;

    expect_result(validate(blob, length, &declaration, compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_REFERENCE);
    assert(error.domain_index == 0u && error.domain_id == NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS && error.field_id == expected_field);
}

static void test_stable_action_cross_references(const char *fixture_path) {
    uint8_t                                   full[TEST_BUFFER_SIZE];
    uint8_t                                   behavior[TEST_BUFFER_SIZE];
    size_t                                    full_length     = fixture_hex(fixture_path, "profile.full.hex", full, sizeof(full));
    size_t                                    envelope_offset = fixture_ulong(fixture_path, "profile.behavior_envelope_offset", 10);
    size_t                                    length          = behavior_only_blob(full, full_length, envelope_offset, behavior);
    noah_profile_validator_v1_compatibility_t compatible      = compatibility();
    noah_profile_validator_v1_declaration_t   declaration;
    noah_profile_validator_v1_error_t         error;

    compatible.allowed_domain_mask  = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS;
    compatible.required_domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS;
    declaration                     = declaration_for(behavior, length, NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS);
    expect_result(validate(behavior, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_VALID);

    compatible.via_macro_slot_count = 10u;
    expect_behavior_reference_failure(behavior, length, &compatible, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_ACTION);
    compatible.via_macro_slot_count           = 11u;
    compatible.logical_layer_count            = 2u;
    compatible.rgb_limits.logical_layer_count = 2u;
    expect_behavior_reference_failure(behavior, length, &compatible, NOAH_KEY_BEHAVIOR_FIELD_V1_LONG_HOLD_ACTION);
    compatible.logical_layer_count               = 3u;
    compatible.rgb_limits.logical_layer_count    = 3u;
    compatible.supported_pd_mode_mask            = (uint8_t)(NOAH_PROFILE_RGB_V1_PD_MODE_MASK_ALL & (uint8_t)~(1u << 2u));
    compatible.rgb_limits.supported_pd_mode_mask = compatible.supported_pd_mode_mask;
    expect_behavior_reference_failure(behavior, length, &compatible, NOAH_KEY_BEHAVIOR_FIELD_V1_TARGET);
    compatible.supported_pd_mode_mask            = NOAH_PROFILE_RGB_V1_PD_MODE_MASK_ALL;
    compatible.rgb_limits.supported_pd_mode_mask = compatible.supported_pd_mode_mask;
    compatible.hardcoded_macro_slot_count        = 2u;
    expect_behavior_reference_failure(behavior, length, &compatible, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_ACTION);
}

static void test_read_failures(const char *fixture_path) {
    uint8_t                                   full[TEST_BUFFER_SIZE];
    size_t                                    length      = fixture_hex(fixture_path, "profile.full.hex", full, sizeof(full));
    noah_profile_validator_v1_declaration_t   declaration = declaration_for(full, length, (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS));
    noah_profile_validator_v1_compatibility_t compatible  = compatibility();
    instrumented_reader_t                     state       = {.bytes = full, .length = length, .fail_call = 1u};
    noah_profile_reader_t                     reader      = {.read = instrumented_read, .context = &state, .length = length};
    noah_profile_validator_v1_t               validator;
    noah_profile_validator_v1_error_t         error;
    noah_profile_validator_v1_result_t        result;

    expect_result(noah_profile_validator_v1_begin(&validator, &reader, 0u, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS);
    expect_result(noah_profile_validator_v1_step(&validator, 20u, &error), NOAH_PROFILE_VALIDATOR_V1_READ_ERROR);
    assert(error.byte_offset == 0u);

    memset(&state, 0, sizeof(state));
    state.bytes  = full;
    state.length = length;
    expect_result(noah_profile_validator_v1_begin(&validator, &reader, 0u, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS);
    while (validator.phase == NOAH_PROFILE_VALIDATOR_V1_PHASE_CHECKSUM) {
        expect_result(noah_profile_validator_v1_step(&validator, 20u, &error), NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS);
    }
    state.fail_call = state.calls + 1u;
    expect_result(noah_profile_validator_v1_step(&validator, 20u, &error), NOAH_PROFILE_VALIDATOR_V1_READ_ERROR);
    assert(error.byte_offset == 0u);

    memset(&state, 0, sizeof(state));
    state.bytes  = full;
    state.length = length;
    expect_result(noah_profile_validator_v1_begin(&validator, &reader, 0u, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS);
    do {
        result = noah_profile_validator_v1_step(&validator, 20u, &error);
    } while (result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS && validator.phase != NOAH_PROFILE_VALIDATOR_V1_PHASE_DOMAIN_DECODE);
    assert(validator.phase == NOAH_PROFILE_VALIDATOR_V1_PHASE_DOMAIN_DECODE);
    state.fail_call = state.calls + 1u;
    expect_result(noah_profile_validator_v1_step(&validator, 20u, &error), NOAH_PROFILE_VALIDATOR_V1_READ_ERROR);
    assert(error.domain_id == NOAH_PROFILE_DOMAIN_V1_RGB && error.detail_kind == NOAH_PROFILE_VALIDATOR_V1_DETAIL_RGB);
}

static void test_combo_domain(void) {
    uint8_t       bytes[8 + 4 + 4 + 32 * 28] = {'N', 'L', 'P', '1', 1, 0, 1, 1, 0x30, 1, 0x84, 3, 32, 0, 0, 0};
    const uint8_t row[28]                    = {2, 0, 45, 0, 200, 0, 0, 0, 1, 0, 41, 0, 1, 0, 4, 0, 1, 0, 5};
    for (unsigned index = 0; index < 32; index++)
        memcpy(&bytes[16 + 28 * index], row, sizeof(row));
    noah_profile_validator_v1_compatibility_t compatible = compatibility();
    compatible.required_domain_mask                      = 0;
    noah_profile_validator_v1_declaration_t declaration  = declaration_for(bytes, sizeof(bytes), 4);
    noah_profile_validator_v1_error_t       error;
    instrumented_reader_t                   state  = {.bytes = bytes, .length = sizeof(bytes)};
    noah_profile_reader_t                   reader = {.read = instrumented_read, .context = &state, .length = sizeof(bytes)};
    noah_profile_validator_v1_t             validator;
    noah_profile_validator_v1_result_t      result = noah_profile_validator_v1_begin(&validator, &reader, 0, &declaration, &compatible, &error);
    for (unsigned step = 0; result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS && step < 500; step++) {
        reset_step_counts(&state);
        result = noah_profile_validator_v1_step(&validator, 20, &error);
        assert(state.step_calls <= 1 && state.step_bytes <= 20);
    }
    expect_result(result, NOAH_PROFILE_VALIDATOR_V1_VALID);
    assert(validator.profile.combos.row_count == 32 && validator.profile.combos.payload_offset == 12);
    const unsigned bad_offsets[] = {13, 14, 15, 22, 23, 40};
    for (unsigned index = 0; index < sizeof(bad_offsets) / sizeof(bad_offsets[0]); index++) {
        unsigned offset = bad_offsets[index];
        uint8_t  old    = bytes[offset];
        bytes[offset]   = 1;
        declaration     = declaration_for(bytes, sizeof(bytes), 4);
        expect_result(validate(bytes, sizeof(bytes), &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
        assert(error.domain_id == 0x30);
        bytes[offset] = old;
    }
    // Distinct semantic operands may not reference an absent logical layer.
    bytes[28]   = 2;
    bytes[30]   = 3;
    declaration = declaration_for(bytes, sizeof(bytes), 4);
    expect_result(validate(bytes, sizeof(bytes), &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    memcpy(&bytes[16], row, sizeof(row));
    // One global hold threshold: a later row cannot silently store another.
    bytes[48]   = 199;
    declaration = declaration_for(bytes, sizeof(bytes), 4);
    expect_result(validate(bytes, sizeof(bytes), &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    memcpy(&bytes[44], row, sizeof(row));
    memcpy(&bytes[32], &bytes[28], 4); // duplicate input
    declaration = declaration_for(bytes, sizeof(bytes), 4);
    expect_result(validate(bytes, sizeof(bytes), &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    // An explicitly empty table disables all combos; missing domain is fallback.
    bytes[10]   = 4;
    bytes[11]   = 0;
    bytes[12]   = 0;
    declaration = declaration_for(bytes, 16, 4);
    expect_result(validate(bytes, 16, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_VALID);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    test_golden_incremental_phases(argv[1]);
    test_identity_capacity_and_masks(argv[1]);
    test_blob_and_domain_rejections(argv[1]);
    test_stable_action_cross_references(argv[1]);
    test_read_failures(argv[1]);
    test_combo_domain();
    puts("profile validator v1 host tests passed");
    return 0;
}
