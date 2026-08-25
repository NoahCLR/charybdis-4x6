#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "users/noah/lib/profile/schema/profile_validator_v1.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"

enum {
    TEST_ROW_COUNT                 = NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS,
    TEST_STEPS_PER_ROW            = 2u,
    TEST_STEP_COUNT                = TEST_ROW_COUNT * TEST_STEPS_PER_ROW,
    TEST_ACTION_ABI                = 0x12345678u,
    // Regression ceilings for the current disconnected validator, not an
    // acceptance budget for matrix-scan routing. The measured work remains a
    // tracked blocker until domain validation is made incrementally bounded.
    DOMAIN_DECODE_READ_CALL_LIMIT  = 900u,
    DOMAIN_DECODE_BYTE_LIMIT       = 3216u,
    CROSS_REFERENCE_CALL_LIMIT     = 912u,
    CROSS_REFERENCE_BYTE_LIMIT     = 3240u,
    TEST_MAXIMUM_RGB_PAYLOAD_SIZE  = 344u,
    RGB_DECODE_READ_CALL_LIMIT     = 54u,
    RGB_DECODE_BYTE_LIMIT          = TEST_MAXIMUM_RGB_PAYLOAD_SIZE,
};

typedef struct {
    const uint8_t *bytes;
    size_t         length;
    size_t         step_calls;
    size_t         step_bytes;
    size_t         step_max_read;
} instrumented_reader_t;

typedef struct {
    size_t calls;
    size_t bytes;
    size_t max_read;
    size_t steps;
} phase_budget_t;

static noah_key_behavior_step_v1_t steps[TEST_ROW_COUNT][TEST_STEPS_PER_ROW];
static noah_key_behavior_row_v1_t  rows[TEST_ROW_COUNT];
static uint8_t                     behavior_payload[NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_PAYLOAD_SIZE];
static uint8_t                     rgb_payload[TEST_MAXIMUM_RGB_PAYLOAD_SIZE];
static uint8_t                     profile_blob[NOAH_PROFILE_BLOB_V1_MAX_SIZE];

static bool instrumented_read(void *context, size_t offset, uint8_t *target, size_t length) {
    instrumented_reader_t *reader = context;

    if (!reader || !target || length == 0u || offset > reader->length || length > reader->length - offset) {
        return false;
    }
    reader->step_calls++;
    reader->step_bytes += length;
    if (length > reader->step_max_read) reader->step_max_read = length;
    memcpy(target, &reader->bytes[offset], length);
    return true;
}

static noah_profile_action_v1_t qmk_action(uint16_t keycode) {
    return (noah_profile_action_v1_t){
        .kind    = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE,
        .operand = keycode,
    };
}

static size_t build_maximum_behavior_profile(void) {
    noah_profile_codec_v1_error_t error;
    size_t                        behavior_length;
    size_t                        profile_length;

    for (uint16_t row_index = 0u; row_index < TEST_ROW_COUNT; row_index++) {
        for (uint8_t step_index = 0u; step_index < TEST_STEPS_PER_ROW; step_index++) {
            const uint16_t action_base = (uint16_t)(0x200u + row_index * 8u + step_index * 3u);
            steps[row_index][step_index] = (noah_key_behavior_step_v1_t){
                .tap_index     = step_index,
                .presence_mask = NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP | NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_HOLD | NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_LONG_HOLD,
                .tap           = qmk_action(action_base),
                .hold = {
                    .mode      = NOAH_KEY_BEHAVIOR_HOLD_V1_REPEAT_WHILE_HELD,
                    .repeat_hz = NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_REPEAT_HZ,
                    .action    = qmk_action((uint16_t)(action_base + 1u)),
                },
                .long_hold = {
                    .mode   = NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_AT_THRESHOLD,
                    .action = qmk_action((uint16_t)(action_base + 2u)),
                },
            };
        }
        rows[row_index] = (noah_key_behavior_row_v1_t){
            .target           = qmk_action((uint16_t)(0x100u + row_index)),
            .tap_hold_term    = 0xffffu,
            .longer_hold_term = 0xffffu,
            .multi_tap_term   = 0xffffu,
            .flags            = NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FLAG_AUTO_MOUSE,
            .steps            = steps[row_index],
            .step_count       = TEST_STEPS_PER_ROW,
        };
    }

    assert(noah_key_behavior_domain_v1_encode(rows, TEST_ROW_COUNT, NULL, NULL, behavior_payload, sizeof(behavior_payload), &behavior_length, &error) == NOAH_PROFILE_CODEC_V1_OK);
    assert(behavior_length == 3204u);
    const noah_profile_domain_v1_t domain = {
        .id             = NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS,
        .version        = NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIOR_VERSION,
        .payload        = behavior_payload,
        .payload_length = behavior_length,
    };
    assert(noah_profile_blob_v1_encode(&domain, 1u, profile_blob, sizeof(profile_blob), &profile_length, &error) == NOAH_PROFILE_CODEC_V1_OK);
    assert(profile_length == 3216u);
    return profile_length;
}

static void record_phase_budget(phase_budget_t *budget, const instrumented_reader_t *reader) {
    if (reader->step_calls > budget->calls) budget->calls = reader->step_calls;
    if (reader->step_bytes > budget->bytes) budget->bytes = reader->step_bytes;
    if (reader->step_max_read > budget->max_read) budget->max_read = reader->step_max_read;
    budget->steps++;
}

static void run_maximum_rgb_profile(void) {
    size_t offset;
    size_t profile_length;
    noah_profile_codec_v1_error_t codec_error;

    memset(rgb_payload, 0, sizeof(rgb_payload));
    rgb_payload[0]  = NOAH_PROFILE_RGB_V1_FORMAT_VERSION;
    rgb_payload[2]  = NOAH_PROFILE_RGB_V1_STAGE_LAYER;
    rgb_payload[4]  = NOAH_PROFILE_RGB_V1_MAX_GROUPS;
    rgb_payload[5]  = 1u;
    rgb_payload[6]  = NOAH_PROFILE_RGB_V1_MAX_STAGE_GROUP_ROWS;
    rgb_payload[12] = NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT;
    rgb_payload[13] = NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE;
    offset = NOAH_PROFILE_RGB_V1_HEADER_SIZE;
    for (uint8_t group = 0u; group < NOAH_PROFILE_RGB_V1_MAX_GROUPS; group++) {
        rgb_payload[offset]      = group;
        rgb_payload[offset + 1u] = group;
        offset += 9u;
    }
    offset += 5u;
    for (uint8_t row = 0u; row < NOAH_PROFILE_RGB_V1_MAX_STAGE_GROUP_ROWS; row++) {
        rgb_payload[offset]      = NOAH_PROFILE_RGB_V1_SELECTOR_ALL;
        rgb_payload[offset + 4u] = 0u;
        offset += 5u;
    }
    assert(offset + 4u + 4u + 11u == sizeof(rgb_payload));

    const noah_profile_domain_v1_t domain = {
        .id             = NOAH_PROFILE_DOMAIN_V1_RGB,
        .version        = NOAH_PROFILE_DOMAIN_V1_RGB_VERSION,
        .payload        = rgb_payload,
        .payload_length = sizeof(rgb_payload),
    };
    assert(noah_profile_blob_v1_encode(&domain, 1u, profile_blob, sizeof(profile_blob), &profile_length, &codec_error) == NOAH_PROFILE_CODEC_V1_OK);
    assert(profile_length == sizeof(rgb_payload) + NOAH_PROFILE_BLOB_V1_HEADER_SIZE + NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE);

    instrumented_reader_t reader_state = {
        .bytes  = profile_blob,
        .length = profile_length,
    };
    noah_profile_reader_t reader = {
        .read    = instrumented_read,
        .context = &reader_state,
        .length  = profile_length,
    };
    noah_profile_validator_v1_compatibility_t compatibility = noah_profile_validator_v1_default_compatibility(TEST_ACTION_ABI);
    noah_profile_validator_v1_declaration_t declaration = {
        .schema_major      = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR,
        .schema_minor      = NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR,
        .domain_mask       = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB,
        .byte_length       = profile_length,
        .crc32             = noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, profile_blob, profile_length)),
        .digest            = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, profile_blob, profile_length),
        .action_abi_digest = TEST_ACTION_ABI,
    };
    noah_profile_validator_v1_t validator;
    noah_profile_validator_v1_error_t error;
    phase_budget_t budgets[NOAH_PROFILE_VALIDATOR_V1_PHASE_REJECTED + 1u] = {{0}};
    size_t total_steps = 0u;

    compatibility.required_domain_mask             = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    compatibility.allowed_domain_mask              = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    compatibility.logical_layer_count              = 1u;
    compatibility.supported_pd_mode_mask           = 0u;
    compatibility.rgb_limits.compiled_stage_mask   = NOAH_PROFILE_RGB_V1_STAGE_LAYER;
    compatibility.rgb_limits.logical_layer_count   = 1u;
    compatibility.rgb_limits.supported_pd_mode_mask = 0u;
    assert(noah_profile_validator_v1_begin(&validator, &reader, 0u, &declaration, &compatibility, &error) == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS);

    noah_profile_validator_v1_result_t result = NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
    while (result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
        const noah_profile_validator_v1_phase_t phase = validator.phase;
        reader_state.step_calls    = 0u;
        reader_state.step_bytes    = 0u;
        reader_state.step_max_read = 0u;
        result = noah_profile_validator_v1_step(&validator, NOAH_PROFILE_VALIDATOR_V1_CHECKSUM_CHUNK_MAX, &error);
        record_phase_budget(&budgets[phase], &reader_state);
        assert(++total_steps < 1000u);
    }
    assert(result == NOAH_PROFILE_VALIDATOR_V1_VALID);
    assert(validator.profile.rgb.group_count == NOAH_PROFILE_RGB_V1_MAX_GROUPS);
    assert(validator.profile.rgb.layer_group_count == NOAH_PROFILE_RGB_V1_MAX_STAGE_GROUP_ROWS);

    const phase_budget_t decode = budgets[NOAH_PROFILE_VALIDATOR_V1_PHASE_DOMAIN_DECODE];
    assert(decode.steps == 1u && decode.max_read <= NOAH_PROFILE_RGB_V1_HEADER_SIZE);
    assert(decode.calls <= RGB_DECODE_READ_CALL_LIMIT && decode.bytes <= RGB_DECODE_BYTE_LIMIT);
    printf("RGB domain decode max/step: %zu reads, %zu bytes for %zu-byte maximal RGB payload\n", decode.calls, decode.bytes, sizeof(rgb_payload));
}

int main(void) {
    const size_t profile_length = build_maximum_behavior_profile();
    instrumented_reader_t reader_state = {
        .bytes  = profile_blob,
        .length = profile_length,
    };
    noah_profile_reader_t reader = {
        .read    = instrumented_read,
        .context = &reader_state,
        .length  = profile_length,
    };
    noah_profile_validator_v1_compatibility_t compatibility = noah_profile_validator_v1_default_compatibility(TEST_ACTION_ABI);
    noah_profile_validator_v1_declaration_t declaration = {
        .schema_major      = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR,
        .schema_minor      = NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR,
        .domain_mask       = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS,
        .byte_length       = profile_length,
        .crc32             = noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, profile_blob, profile_length)),
        .digest            = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, profile_blob, profile_length),
        .action_abi_digest = TEST_ACTION_ABI,
    };
    noah_profile_validator_v1_t       validator;
    noah_profile_validator_v1_error_t error;
    phase_budget_t budgets[NOAH_PROFILE_VALIDATOR_V1_PHASE_REJECTED + 1u] = {{0}};
    size_t total_steps = 0u;
    clock_t started = clock();

    compatibility.required_domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS;
    compatibility.allowed_domain_mask  = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS;
    assert(noah_profile_validator_v1_begin(&validator, &reader, 0u, &declaration, &compatibility, &error) == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS);

    noah_profile_validator_v1_result_t result = NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
    while (result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
        const noah_profile_validator_v1_phase_t phase = validator.phase;
        reader_state.step_calls    = 0u;
        reader_state.step_bytes    = 0u;
        reader_state.step_max_read = 0u;
        result = noah_profile_validator_v1_step(&validator, NOAH_PROFILE_VALIDATOR_V1_CHECKSUM_CHUNK_MAX, &error);
        assert(phase <= NOAH_PROFILE_VALIDATOR_V1_PHASE_REJECTED);
        record_phase_budget(&budgets[phase], &reader_state);
        assert(++total_steps < 1000u);
    }
    assert(result == NOAH_PROFILE_VALIDATOR_V1_VALID);
    assert(profile_length * 4u >= NOAH_PROFILE_BLOB_V1_MAX_SIZE * 3u);
    assert(validator.profile.key_behaviors.row_count == TEST_ROW_COUNT);
    assert(validator.profile.key_behaviors.populated_step_count == TEST_STEP_COUNT);

    const phase_budget_t checksum = budgets[NOAH_PROFILE_VALIDATOR_V1_PHASE_CHECKSUM];
    const phase_budget_t header   = budgets[NOAH_PROFILE_VALIDATOR_V1_PHASE_BLOB_HEADER];
    const phase_budget_t envelope = budgets[NOAH_PROFILE_VALIDATOR_V1_PHASE_DOMAIN_HEADER];
    const phase_budget_t decode   = budgets[NOAH_PROFILE_VALIDATOR_V1_PHASE_DOMAIN_DECODE];
    const phase_budget_t row      = budgets[NOAH_PROFILE_VALIDATOR_V1_PHASE_CROSS_REFERENCE_ROW];
    const phase_budget_t step     = budgets[NOAH_PROFILE_VALIDATOR_V1_PHASE_CROSS_REFERENCE_STEP];

    assert(checksum.calls == 1u && checksum.bytes <= NOAH_PROFILE_VALIDATOR_V1_CHECKSUM_CHUNK_MAX && checksum.max_read <= NOAH_PROFILE_VALIDATOR_V1_CHECKSUM_CHUNK_MAX);
    assert(header.calls == 1u && header.bytes == NOAH_PROFILE_BLOB_V1_HEADER_SIZE);
    assert(envelope.calls == 1u && envelope.bytes == NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE);
    assert(decode.steps == 1u && decode.max_read <= NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FIXED_SIZE);
    assert(row.steps == TEST_ROW_COUNT + 1u && row.max_read <= NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FIXED_SIZE);
    assert(step.steps == TEST_STEP_COUNT && step.max_read <= NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FIXED_SIZE);
    assert(decode.calls <= DOMAIN_DECODE_READ_CALL_LIMIT && decode.bytes <= DOMAIN_DECODE_BYTE_LIMIT);
    assert(row.calls <= CROSS_REFERENCE_CALL_LIMIT && row.bytes <= CROSS_REFERENCE_BYTE_LIMIT);
    assert(step.calls <= CROSS_REFERENCE_CALL_LIMIT && step.bytes <= CROSS_REFERENCE_BYTE_LIMIT);

    printf("profile validator work budget: %zu-byte valid profile, %zu steps, %.3f ms host CPU\n", profile_length, total_steps, 1000.0 * (double)(clock() - started) / CLOCKS_PER_SEC);
    printf("domain decode max/step: %zu reads, %zu bytes; cross-row: %zu reads, %zu bytes; cross-step: %zu reads, %zu bytes\n", decode.calls, decode.bytes, row.calls, row.bytes, step.calls, step.bytes);
    run_maximum_rgb_profile();
    return 0;
}
