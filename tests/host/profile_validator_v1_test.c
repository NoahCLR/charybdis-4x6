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
    value.supported_pd_mode_mask            = 63u;
    value.via_macro_slot_count              = 11u;
    value.custom_key_count        = 3u;
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

    for (unsigned iteration = 0u; result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS && iteration < 8000u; iteration++) {
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

#define expect_result(actual, expected) expect_result_at(actual, expected, __LINE__)
static void expect_result_at(noah_profile_validator_v1_result_t actual, noah_profile_validator_v1_result_t expected, unsigned line) {
    if (actual != expected) {
        fprintf(stderr, "validator result mismatch at %u: got %u expected %u\n", line, (unsigned)actual, (unsigned)expected);
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

    for (unsigned iteration = 0u; result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS && iteration < 8000u; iteration++) {
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
        } else if (prior_phase == NOAH_PROFILE_VALIDATOR_V1_PHASE_DOMAIN_HEADER && validator.envelope.index < validator.envelope.count) {
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
    uint8_t                                   empty[NOAH_PROFILE_BLOB_V1_HEADER_SIZE] = {'N', 'L', 'P', '1', 3u, 0u, 0u, 1u};
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
    bytes[behavior_envelope + 1u] = NOAH_PROFILE_RGB_VERSION;
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
    bytes[8]    = 0x60u;
    declaration = declaration_for(bytes, length, (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS));
    expect_result(validate(bytes, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_UNSUPPORTED_DOMAIN);

    memcpy(bytes, valid, length);
    bytes[12]   = 2u;
    declaration = declaration_for(bytes, length, (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS));
    expect_result(validate(bytes, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    assert(error.detail_kind == NOAH_PROFILE_VALIDATOR_V1_DETAIL_RGB && error.detail_code == NOAH_PROFILE_RGB_V1_INVALID_VERSION && error.byte_offset == 12u);

    memcpy(bytes, valid, length);
    bytes[behavior_payload + 1u] = 1u; // the reserved header byte
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
    compatible.custom_key_count        = 2u;
    expect_behavior_reference_failure(behavior, length, &compatible, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_ACTION);
}

static unsigned placement_seen[NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_COMBO_OUTPUT + 1u];
static int      placement_refused = -1;

static bool logging_placement(const noah_profile_action_v1_t *action, noah_profile_validator_v1_placement_t placement) {
    (void)action;
    placement_seen[placement]++;
    return (int)placement != placement_refused;
}

static const noah_profile_validator_v1_runtime_t logging_runtime = {.placement_supported = logging_placement};

static void reset_placement_log(int refused) {
    memset(placement_seen, 0, sizeof(placement_seen));
    placement_refused = refused;
}

// The keyboard's placement check sees every behaviour action with where it is
// placed, and a refusal is reported at the action it refused, after the
// domain decodes cleanly. Without a runtime nothing is checked.
static void test_behavior_placement_hook(const char *fixture_path) {
    uint8_t                                   full[TEST_BUFFER_SIZE];
    uint8_t                                   behavior[TEST_BUFFER_SIZE];
    size_t                                    full_length     = fixture_hex(fixture_path, "profile.full.hex", full, sizeof(full));
    size_t                                    envelope_offset = fixture_ulong(fixture_path, "profile.behavior_envelope_offset", 10);
    size_t                                    length          = behavior_only_blob(full, full_length, envelope_offset, behavior);
    noah_profile_validator_v1_compatibility_t compatible      = compatibility();
    noah_profile_validator_v1_declaration_t   declaration     = declaration_for(behavior, length, NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS);
    noah_profile_validator_v1_error_t         error;

    compatible.allowed_domain_mask  = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS;
    compatible.required_domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS;
    compatible.runtime              = &logging_runtime;

    reset_placement_log(-1);
    expect_result(validate(behavior, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_VALID);
    assert(placement_seen[NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_KEY] == 2u);
    assert(placement_seen[NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_TAP] == 2u);
    assert(placement_seen[NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_HOLD_OTHER] == 2u);
    assert(placement_seen[NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_COMBO_OUTPUT] == 0u);

    reset_placement_log(NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_KEY);
    expect_behavior_reference_failure(behavior, length, &compatible, NOAH_KEY_BEHAVIOR_FIELD_V1_TARGET);
    reset_placement_log(NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_TAP);
    expect_behavior_reference_failure(behavior, length, &compatible, NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_ACTION);
    reset_placement_log(NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_HOLD_OTHER);
    expect_result(validate(behavior, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_REFERENCE);
    assert(error.field_id == NOAH_KEY_BEHAVIOR_FIELD_V1_HOLD_ACTION || error.field_id == NOAH_KEY_BEHAVIOR_FIELD_V1_LONG_HOLD_ACTION);

    compatible.runtime = &(const noah_profile_validator_v1_runtime_t){0};
    expect_result(validate(behavior, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_VALID);
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

// Version 3 rows: input count, flags, window, allowed layers, output, then
// four bytes per input.
enum { COMBO_TEST_ROWS = 128, COMBO_TEST_INPUTS = 16, COMBO_TEST_ROW = 12 + 4 * COMBO_TEST_INPUTS, COMBO_TEST_DOMAIN = 8 + COMBO_TEST_ROWS * COMBO_TEST_ROW };
static size_t combo_row(uint8_t *out, uint8_t inputs, uint8_t flags, uint16_t term, uint32_t allowed, uint16_t output, uint16_t first_input) {
    uint8_t *p = out;
    *p++       = inputs;
    *p++       = flags;
    *p++       = (uint8_t)term;
    *p++       = (uint8_t)(term >> 8);
    for (unsigned byte = 0; byte < 4; byte++)
        *p++ = (uint8_t)(allowed >> (8 * byte));
    *p++ = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, *p++ = 0, *p++ = (uint8_t)output, *p++ = (uint8_t)(output >> 8);
    for (unsigned input = 0; input < inputs; input++) {
        uint16_t key = (uint16_t)(first_input + input);
        *p++ = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, *p++ = 0, *p++ = (uint8_t)key, *p++ = (uint8_t)(key >> 8);
    }
    return (size_t)(p - out);
}
static size_t combo_profile(uint8_t *bytes, uint8_t rows, uint8_t inputs, uint16_t domain_length) {
    const uint8_t head[] = {'N', 'L', 'P', '1', 3, 0, 1, 1, 0x30, NOAH_PROFILE_DOMAIN_VERSION_COMBOS, (uint8_t)domain_length, (uint8_t)(domain_length >> 8), rows, 0, 0, 0, 60, 0, 200, 0};
    size_t        length = sizeof(head);
    memcpy(bytes, head, sizeof(head));
    for (unsigned row = 0; row < rows; row++)
        length += combo_row(&bytes[length], inputs, 0, 45, 0xffffu, 41, 4);
    return length;
}

// The whole table: 128 combos of sixteen inputs each.
static void test_combo_domain(void) {
    static uint8_t bytes[8 + 4 + COMBO_TEST_DOMAIN + 1];
    size_t         length = combo_profile(bytes, COMBO_TEST_ROWS, COMBO_TEST_INPUTS, COMBO_TEST_DOMAIN);
    assert(length == sizeof(bytes) - 1u);
    noah_profile_validator_v1_compatibility_t compatible = compatibility();
    compatible.required_domain_mask                      = 0;
    noah_profile_validator_v1_declaration_t declaration  = declaration_for(bytes, length, 4);
    noah_profile_validator_v1_error_t       error;
    instrumented_reader_t                   state  = {.bytes = bytes, .length = length};
    noah_profile_reader_t                   reader = {.read = instrumented_read, .context = &state, .length = length};
    noah_profile_validator_v1_t             validator;
    noah_profile_validator_v1_result_t      result = noah_profile_validator_v1_begin(&validator, &reader, 0, &declaration, &compatible, &error);
    unsigned                                steps  = 0;
    for (; result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS && steps < 4000; steps++) {
        reset_step_counts(&state);
        result = noah_profile_validator_v1_step(&validator, 20, &error);
        assert(state.step_calls <= 1 && state.step_bytes <= 20);
    }
    expect_result(result, NOAH_PROFILE_VALIDATOR_V1_VALID);
    noah_profile_domain_range_t    combos;
    noah_profile_combo_v1_header_t header;
    noah_profile_combo_v1_row_t    row;
    assert(noah_profile_blob_v1_find_domain(&reader, 0, length, NOAH_PROFILE_DOMAIN_V1_COMBOS, &combos) && combos.offset == 12);
    assert(noah_profile_combo_v1_read_header(&reader, 0, combos, &header) && header.row_count == COMBO_TEST_ROWS);
    static const uint8_t rows_checked[] = {0, 31, 32, 63, 64, 127};
    for (unsigned index = 0; index < sizeof(rows_checked); index++) {
        assert(noah_profile_combo_v1_read_row(&reader, 0, combos, rows_checked[index], &row));
        assert(row.input_count == 16 && row.inputs[15].operand == 19 && row.allowed_layers == 0xffffu && row.output.operand == 41);
    }
    assert(!noah_profile_combo_v1_read_row(&reader, 0, combos, 128, &row));
    // The first row's fields; bytes[20] starts it.
    const struct {
        unsigned offset;
        uint8_t  value;
        bool     valid;
    } edits[] = {
        {13, 1, false},   // a reserved header byte
        {12, 129, false}, // a 129th row
        {20, 17, false},  // a seventeenth input
        {20, 1, false},   // a one-key combo
        {21, 3, false},   // must hold and must tap
        {21, 0x10, false},
        {21, 0x08, true}, // disabled is kept
        {21, 0x04, true},
        {24, 0, true}, {25, 0, true}, // no layer allowed
        {26, 1, false},  // layer 16
        {27, 0x80, false},
        {34, 0, false},  // a KC_NO output
        {36, 2, false},  // input 0 a layer action past the bank (operand 4)
    };
    for (unsigned index = 0; index < sizeof(edits) / sizeof(edits[0]); index++) {
        uint8_t old           = bytes[edits[index].offset];
        bytes[edits[index].offset] = edits[index].value;
        declaration           = declaration_for(bytes, length, 4);
        result                = validate(bytes, length, &declaration, &compatible, &error);
        if (edits[index].valid) {
            expect_result(result, NOAH_PROFILE_VALIDATOR_V1_VALID);
        } else {
            expect_result(result, NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
            assert(error.domain_id == 0x30);
        }
        bytes[edits[index].offset] = old;
    }
    // A duplicate input, at the sixteenth.
    memcpy(&bytes[20 + 12 + 4 * 15], &bytes[20 + 12], 4);
    declaration = declaration_for(bytes, length, 4);
    expect_result(validate(bytes, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    assert(error.row_index == 0u);
    combo_row(&bytes[20], COMBO_TEST_INPUTS, 0, 45, 0xffffu, 41, 4);
    // The rows must end at the domain's end: one byte short, or one over.
    bytes[10]--;
    declaration = declaration_for(bytes, length - 1u, 4);
    expect_result(validate(bytes, length - 1u, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    bytes[10] += 2;
    bytes[length] = 0;
    declaration   = declaration_for(bytes, length + 1u, 4);
    expect_result(validate(bytes, length + 1u, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    bytes[10]--;
    // A combo output the keyboard cannot run is refused; its inputs are keys
    // and not asked about.
    compatible.runtime = &logging_runtime;
    reset_placement_log(-1);
    declaration = declaration_for(bytes, length, 4);
    expect_result(validate(bytes, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_VALID);
    assert(placement_seen[NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_COMBO_OUTPUT] == COMBO_TEST_ROWS);
    reset_placement_log(NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_COMBO_OUTPUT);
    expect_result(validate(bytes, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    assert(error.domain_id == 0x30 && error.row_index == 0u);
    compatible.runtime = NULL;
    // An explicitly empty table disables all combos; missing domain is fallback.
    length      = combo_profile(bytes, 0, 2, 8);
    declaration = declaration_for(bytes, length, 4);
    expect_result(validate(bytes, length, &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_VALID);
}

// Rows of different lengths, the default window, and retired versions.
static void test_combo_domain_v3(void) {
    uint8_t bytes[8 + 4 + 8 + 20 + 24] = {'N', 'L', 'P', '1', 3, 0, 1, 1, 0x30, NOAH_PROFILE_DOMAIN_VERSION_COMBOS, 52, 0, 2, 0, 0, 0, 60, 0, 150, 0};
    combo_row(&bytes[20], 2, 0, 0, 0xffffu, 41, 4);  // follows the default window
    combo_row(&bytes[40], 3, 6, 45, 0x0005u, 41, 6); // its own window, tap and order, layers 0 and 2
    noah_profile_validator_v1_compatibility_t compatible = compatibility();
    compatible.required_domain_mask                      = 0;
    noah_profile_validator_v1_declaration_t declaration  = declaration_for(bytes, sizeof(bytes), 4);
    noah_profile_validator_v1_error_t       error;
    instrumented_reader_t                   state  = {.bytes = bytes, .length = sizeof(bytes)};
    noah_profile_reader_t                   reader = {.read = instrumented_read, .context = &state, .length = sizeof(bytes)};
    expect_result(validate(bytes, sizeof(bytes), &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_VALID);
    noah_profile_domain_range_t    combos;
    noah_profile_combo_v1_header_t header;
    noah_profile_combo_v1_row_t    row;
    assert(noah_profile_blob_v1_find_domain(&reader, 0, sizeof(bytes), NOAH_PROFILE_DOMAIN_V1_COMBOS, &combos) && combos.offset == 12);
    assert(noah_profile_combo_v1_read_header(&reader, 0, combos, &header) && header.row_count == 2 && header.default_term_ms == 60 && header.hold_term_ms == 150);
    assert(noah_profile_combo_v1_read_row(&reader, 0, combos, 1, &row) && row.input_count == 3 && row.term_ms == 45 && row.flags == 6 && row.allowed_layers == 5u && row.inputs[2].operand == 8);
    // A zero default window is refused.
    bytes[16] = 0;
    declaration = declaration_for(bytes, sizeof(bytes), 4);
    expect_result(validate(bytes, sizeof(bytes), &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    bytes[16] = 60;
    // Only the current version exists: version 2's 28-byte rows are not read.
    bytes[9]    = NOAH_PROFILE_DOMAIN_VERSION_COMBOS - 1u;
    declaration = declaration_for(bytes, sizeof(bytes), 4);
    expect_result(validate(bytes, sizeof(bytes), &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    bytes[9] = NOAH_PROFILE_DOMAIN_VERSION_COMBOS;
    // A count that runs the second row past the domain is refused.
    bytes[40]   = 4;
    declaration = declaration_for(bytes, sizeof(bytes), 4);
    expect_result(validate(bytes, sizeof(bytes), &declaration, &compatible, &error), NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    test_golden_incremental_phases(argv[1]);
    test_identity_capacity_and_masks(argv[1]);
    test_blob_and_domain_rejections(argv[1]);
    test_stable_action_cross_references(argv[1]);
    test_behavior_placement_hook(argv[1]);
    test_read_failures(argv[1]);
    test_combo_domain();
    test_combo_domain_v3();
    puts("profile validator v1 host tests passed");
    return 0;
}
