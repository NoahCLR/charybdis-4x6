#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/schema/profile_compiled_defaults_v1.h"
#include "users/noah/lib/profile/schema/profile_validator_v1.h"
#include "users/noah/lib/profile/runtime/profile_action_runtime_v1.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/noah_keymap.h"

enum {
    OUTPUT_CAPACITY = NOAH_PROFILE_BLOB_V1_MAX_SIZE,
};

static uint8_t     output[OUTPUT_CAPACITY];
static size_t      output_length;
static size_t      largest_write;
static const char *fixture_path;

#define NOAH_PD_MODE_TEST_ROW(name, mode_keycode, handler, key_handler, reset, dpi, mode_traits, lifecycle) [PD_MODE_INDEX_##name] = {.mode_flag = PD_MODE_##name, .keycode = (mode_keycode), .lock_action = mode_keycode##_LOCK, .traits = (mode_traits)},
const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {NOAH_PD_MODE_LIST(NOAH_PD_MODE_TEST_ROW)};
#undef NOAH_PD_MODE_TEST_ROW

static bool collect(void *context, const uint8_t *bytes, size_t length) {
    (void)context;
    assert(output_length + length <= sizeof(output));
    memcpy(&output[output_length], bytes, length);
    output_length += length;
    if (length > largest_write) largest_write = length;
    return true;
}

static bool reject_write(void *context, const uint8_t *bytes, size_t length) {
    (void)context;
    (void)bytes;
    (void)length;
    return false;
}

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u);
}

static bool fixture_value(const char *key, char *value, size_t capacity) {
    FILE  *file = fopen(fixture_path, "r");
    char   line[9000];
    size_t key_length = strlen(key);

    assert(file != NULL);
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, key, key_length) == 0 && line[key_length] == '=') {
            char  *start  = &line[key_length + 1u];
            size_t length = strcspn(start, "\r\n");
            assert(length + 1u <= capacity);
            memcpy(value, start, length);
            value[length] = '\0';
            fclose(file);
            return true;
        }
    }
    fclose(file);
    return false;
}

static uint32_t fixture_u32(const char *key, int base) {
    char          value[32];
    char         *end;
    unsigned long parsed;
    assert(fixture_value(key, value, sizeof(value)));
    parsed = strtoul(value, &end, base);
    assert(*value != '\0' && *end == '\0' && parsed <= UINT32_MAX);
    return (uint32_t)parsed;
}

static uint8_t hex_nibble(char value) {
    if (value >= '0' && value <= '9') return (uint8_t)(value - '0');
    if (value >= 'a' && value <= 'f') return (uint8_t)(value - 'a' + 10);
    if (value >= 'A' && value <= 'F') return (uint8_t)(value - 'A' + 10);
    assert(false);
    return 0u;
}

static void assert_golden(const noah_profile_compiled_v1_t *profile) {
    char hex[OUTPUT_CAPACITY * 2u + 1u];
    assert(fixture_path != NULL);
    assert(fixture_value("profile.full.hex", hex, sizeof(hex)));
    assert(strlen(hex) == output_length * 2u);
    for (size_t index = 0u; index < output_length; index++) {
        assert(output[index] == (uint8_t)((hex_nibble(hex[index * 2u]) << 4u) | hex_nibble(hex[index * 2u + 1u])));
    }
    assert(profile->metadata.byte_length == fixture_u32("profile.byte_length", 10));
    assert(profile->metadata.crc32 == fixture_u32("profile.crc32", 16));
    assert(profile->metadata.digest == fixture_u32("profile.fnv1a32", 16));
    assert(profile->metadata.action_abi_digest == fixture_u32("profile.action_abi", 16));
#ifndef NOAH_LEGACY_SNAPSHOT_BRIDGE
    assert(profile->metadata.action_abi_row_visits == fixture_u32("profile.action_abi_row_visits", 10));
#endif
}

static noah_profile_validator_v1_compatibility_t compatibility(uint32_t action_abi_digest) {
    noah_profile_validator_v1_compatibility_t value = noah_profile_validator_v1_default_compatibility(action_abi_digest);
    value.required_domain_mask                      = (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS);
    value.logical_layer_count                       = LAYER_COUNT;
    value.supported_pd_mode_mask                    = (uint8_t)((1u << PD_MODE_COUNT) - 1u);
    value.via_macro_slot_count                      = VIA_MACRO_SLOT_COUNT;
    value.hardcoded_macro_slot_count                = HARDCODED_MACRO_SLOT_COUNT;
    value.rgb_limits.logical_layer_count            = LAYER_COUNT;
    value.rgb_limits.supported_pd_mode_mask         = value.supported_pd_mode_mask;
    value.rgb_limits.maximum_brightness             = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
    value.rgb_limits.tap_branch_color_count         = KEY_BEHAVIOR_MAX_TAP_COUNT - 1u;
    value.rgb_limits.compiled_stage_mask            = NOAH_PROFILE_RGB_V1_STAGE_MASK_ALL;
    return value;
}

static void validate_whole_profile(const noah_profile_compiled_v1_t *profile) {
    noah_profile_reader_t                   reader        = noah_profile_compiled_v1_reader(profile);
    noah_profile_reader_t                   copied_reader = reader;
    noah_profile_validator_v1_declaration_t declaration   = {
        .schema_major      = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR,
        .schema_minor      = NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR,
        .domain_mask       = profile->metadata.domain_mask,
        .byte_length       = profile->metadata.byte_length,
        .crc32             = profile->metadata.crc32,
        .digest            = profile->metadata.digest,
        .action_abi_digest = profile->metadata.action_abi_digest,
    };
    noah_profile_validator_v1_compatibility_t compatible = compatibility(profile->metadata.action_abi_digest);
    noah_profile_validator_v1_t               validator;
    noah_profile_validator_v1_error_t         error;
    noah_profile_validator_v1_result_t        result = noah_profile_validator_v1_begin(&validator, &copied_reader, 0u, &declaration, &compatible, &error);

    assert(result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS);
    for (size_t steps = 0u; result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS && steps < 1000u; steps++) {
        result = noah_profile_validator_v1_step(&validator, NOAH_PROFILE_VALIDATOR_V1_CHECKSUM_CHUNK_MAX, &error);
    }
    if (result != NOAH_PROFILE_VALIDATOR_V1_VALID) {
        fprintf(stderr, "validator failed: code=%u offset=%zu domain=%u table=%u row=%u step=%u field=%u detail=%u\n", (unsigned)result, error.byte_offset, error.domain_id, error.table_id, error.row_index, error.step_index, error.field_id, error.detail_code);
    }
    assert(result == NOAH_PROFILE_VALIDATOR_V1_VALID);
}

static void test_real_authored_profile(void) {
    noah_profile_compiled_v1_t                profile;
    noah_profile_validator_v1_compatibility_t runtime_compatibility;
    noah_profile_compiled_v1_error_t          error;
    noah_profile_blob_v1_t                    decoded;
    noah_profile_codec_v1_error_t             codec_error;
    noah_profile_reader_t                     reader;
    noah_profile_reader_t                     copied_reader;

    uint8_t slice[37];

    assert(noah_profile_compiled_v1_open(NULL, &error) == NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT);
    assert(noah_profile_compiled_v1_open(&profile, &error) == NOAH_PROFILE_COMPILED_V1_OK);
    assert(profile.metadata.domain_mask == NOAH_PROFILE_COMPILED_V1_DOMAIN_MASK_ALL);
    assert(profile.metadata.byte_length > NOAH_PROFILE_BLOB_V1_HEADER_SIZE);
    assert(profile.metadata.byte_length <= NOAH_PROFILE_BLOB_V1_MAX_SIZE);
    assert(profile.metadata.crc32 != 0u);
    assert(profile.metadata.digest != 0u);
    assert(profile.metadata.action_abi_digest != 0u);
    assert(profile.metadata.action_abi_row_visits == 0);
    assert(profile.metadata.action_abi_row_visits <= NOAH_PROFILE_COMPILED_V1_ACTION_ABI_ROW_VISITS_MAX);
    assert(sizeof(profile) <= 20u);
    assert(!noah_profile_compiled_v1_compatibility(NULL, &runtime_compatibility));
    assert(!noah_profile_compiled_v1_compatibility(&profile, NULL));
    assert(noah_profile_compiled_v1_compatibility(&profile, &runtime_compatibility));
    assert(runtime_compatibility.required_domain_mask == 0u);
#ifdef COMBO_ENABLE
    assert(runtime_compatibility.allowed_domain_mask == (profile.metadata.domain_mask | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_COMBOS));
    assert(runtime_compatibility.combo_to_native != NULL);
#else
    assert(runtime_compatibility.allowed_domain_mask == profile.metadata.domain_mask);
#endif
    assert(runtime_compatibility.action_abi_digest == profile.metadata.action_abi_digest);
    assert(runtime_compatibility.logical_layer_count == LAYER_COUNT);
    assert(runtime_compatibility.supported_pd_mode_mask == (uint8_t)((UINT32_C(1) << PD_MODE_COUNT) - 1u));
    assert(runtime_compatibility.via_macro_slot_count == VIA_MACRO_SLOT_COUNT);
    assert(runtime_compatibility.hardcoded_macro_slot_count == HARDCODED_MACRO_SLOT_COUNT);
    assert(runtime_compatibility.rgb_limits.logical_layer_count == LAYER_COUNT);
    assert(runtime_compatibility.rgb_limits.maximum_brightness == RGB_MATRIX_MAXIMUM_BRIGHTNESS);
    assert(runtime_compatibility.rgb_limits.compiled_stage_mask == NOAH_PROFILE_RGB_V1_STAGE_MASK_ALL);

    output_length = 0u;
    largest_write = 0u;
    assert(noah_profile_compiled_v1_write(NULL, collect, NULL, &error) == NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT);
    assert(noah_profile_compiled_v1_write(&profile, NULL, NULL, &error) == NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT);
    assert(noah_profile_compiled_v1_write(&profile, reject_write, NULL, &error) == NOAH_PROFILE_COMPILED_V1_WRITE_ERROR);
    assert(noah_profile_compiled_v1_write(&profile, collect, NULL, &error) == NOAH_PROFILE_COMPILED_V1_OK);
    assert(output_length == profile.metadata.byte_length);
    assert(largest_write <= NOAH_PROFILE_RGB_V1_HEADER_SIZE);
    assert(noah_profile_blob_v1_decode(output, output_length, &decoded, &codec_error) == NOAH_PROFILE_CODEC_V1_OK);
    assert(decoded.domain_count == 2u);
    assert(decoded.domains[0].id == NOAH_PROFILE_DOMAIN_V1_RGB);
    assert(decoded.domains[1].id == NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS);
    assert(decoded.crc32 == profile.metadata.crc32);
    assert(decoded.digest == profile.metadata.digest);
    assert(decoded.domains[1].payload[0] == key_behavior_count);
    assert(read_u16(&output[10]) == decoded.domains[0].payload_length);
    assert_golden(&profile);

    reader        = noah_profile_compiled_v1_reader(&profile);
    copied_reader = reader;
    assert(NOAH_PROFILE_COMPILED_V1_READER_REPLAY_MAX == NOAH_PROFILE_BLOB_V1_MAX_SIZE);
    assert(noah_profile_reader_read(&copied_reader, 0u, slice, sizeof(slice)));
    assert(memcmp(slice, output, sizeof(slice)) == 0);
    for (size_t offset = 0u; offset < output_length; offset += sizeof(slice)) {
        size_t length = output_length - offset;
        if (length > sizeof(slice)) length = sizeof(slice);
        memset(slice, 0, sizeof(slice));
        assert(noah_profile_reader_read(&reader, offset, slice, length));
        assert(memcmp(slice, &output[offset], length) == 0);
    }
    assert(!noah_profile_reader_read(&reader, output_length - 1u, slice, 2u));
    validate_whole_profile(&profile);

    printf("compiled profile v1: %u bytes crc32=%08x fnv1a=%08x action_abi=%08x\n", profile.metadata.byte_length, (unsigned)profile.metadata.crc32, (unsigned)profile.metadata.digest, (unsigned)profile.metadata.action_abi_digest);
}

static void test_semantic_action_translation(void) {
    noah_profile_action_v1_t action;
    uint16_t                 native;

    assert(noah_profile_compiled_v1_action(KC_A, NULL) == NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT);
    assert(noah_profile_compiled_v1_action(KC_NO, &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_NONE);
    assert(noah_profile_compiled_v1_action(KC_A, &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_QMK_KEYCODE && action.operand == KC_A);
    assert(noah_profile_compiled_v1_action(MO(LAYER_NAV), &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_LAYER_MOMENTARY && action.operand == LAYER_NAV);
    assert(noah_profile_compiled_v1_action(LOCK_LAYER(LAYER_SYM), &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_LAYER_LOCK && action.operand == LAYER_SYM);
    assert(noah_profile_compiled_v1_action(VIA_MACRO_10, &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_VIA_MACRO && action.operand == 10u);
    assert(noah_profile_compiled_v1_action(MACRO_7, &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_HARDCODED_MACRO && action.operand == 7u);
    assert(noah_profile_compiled_v1_action(pd_modes[PD_MODE_INDEX_ZOOM].keycode, &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_PD_MODE_MOMENTARY && action.operand == PD_MODE_INDEX_ZOOM);
    assert(noah_profile_compiled_v1_action(pd_modes[PD_MODE_INDEX_ARROW].lock_action, &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_PD_MODE_LOCK && action.operand == PD_MODE_INDEX_ARROW);

    const uint16_t round_trip_actions[] = {
        KC_NO, KC_A, MO(LAYER_NAV), LOCK_LAYER(LAYER_SYM), VIA_MACRO_10, MACRO_7, pd_modes[PD_MODE_INDEX_ZOOM].keycode, pd_modes[PD_MODE_INDEX_ARROW].lock_action,
    };
    for (size_t index = 0u; index < ARRAY_SIZE(round_trip_actions); index++) {
        assert(noah_profile_action_runtime_v1_from_native(round_trip_actions[index], &action) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK);
        assert(noah_profile_action_runtime_v1_to_native(&action, &native) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK);
        assert(native == round_trip_actions[index]);
    }
    action = (noah_profile_action_v1_t){.kind = NOAH_PROFILE_ACTION_V1_LAYER_LOCK, .operand = LAYER_COUNT};
    assert(noah_profile_action_runtime_v1_to_native(&action, &native) == NOAH_PROFILE_ACTION_RUNTIME_V1_UNSUPPORTED && native == KC_NO);
    action = (noah_profile_action_v1_t){.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .flags = 1u, .operand = KC_A};
    assert(noah_profile_action_runtime_v1_to_native(&action, &native) == NOAH_PROFILE_ACTION_RUNTIME_V1_INVALID_ARGUMENT);
}

static void validate_import_on_empty_firmware(const noah_profile_compiled_v1_t *compiled, const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file);
    size_t length = fread(output, 1, sizeof(output), file);
    assert(feof(file));
    fclose(file);
    noah_profile_reader_t                     reader = noah_profile_reader_from_memory(output, length);
    noah_profile_validator_v1_compatibility_t compatible;
    assert(noah_profile_compiled_v1_compatibility(compiled, &compatible));
    noah_profile_validator_v1_declaration_t declaration = {
        .schema_major      = 1,
        .schema_minor      = 0,
        .domain_mask       = 15,
        .byte_length       = length,
        .crc32             = noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, output, length)),
        .digest            = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, output, length),
        .action_abi_digest = compiled->metadata.action_abi_digest,
    };
    noah_profile_validator_v1_t        validator;
    noah_profile_validator_v1_error_t  error;
    noah_profile_validator_v1_result_t result = noah_profile_validator_v1_begin(&validator, &reader, 0, &declaration, &compatible, &error);
    for (size_t step = 0; result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS && step < 5000; step++)
        result = noah_profile_validator_v1_step(&validator, 20, &error);
    if (result != NOAH_PROFILE_VALIDATOR_V1_VALID) fprintf(stderr, "portable import failed: %u domain=%u field=%u byte=%zu\n", result, error.domain_id, error.field_id, error.byte_offset);
    assert(result == NOAH_PROFILE_VALIDATOR_V1_VALID);
    assert(validator.profile.domain_mask == 15);
    assert(validator.profile.settings.length >= 344);
}

int main(int argc, char **argv) {
    assert(argc >= 2 && argc <= 4);
    if (argc == 3 && strcmp(argv[2], "--write-fixture") == 0) {
        noah_profile_compiled_v1_t profile;
        assert(noah_profile_compiled_v1_open(&profile, NULL) == NOAH_PROFILE_COMPILED_V1_OK);
        assert(noah_profile_compiled_v1_write(&profile, collect, NULL, NULL) == NOAH_PROFILE_COMPILED_V1_OK);
        FILE *file = fopen(argv[1], "w");
        assert(file);
        fprintf(file, "profile.byte_length=%u\nprofile.crc32=%08x\nprofile.fnv1a32=%08x\nprofile.action_abi=%08x\nprofile.action_abi_row_visits=%u\nprofile.full.hex=", profile.metadata.byte_length, (unsigned)profile.metadata.crc32, (unsigned)profile.metadata.digest, (unsigned)profile.metadata.action_abi_digest, profile.metadata.action_abi_row_visits);
        for (size_t i = 0; i < output_length; i++)
            fprintf(file, "%02x", output[i]);
        fputc('\n', file);
        fclose(file);
        return 0;
    }
    if (argc == 4 && strcmp(argv[2], "--empty-profile") == 0) {
        noah_profile_compiled_v1_t profile;
        fixture_path = argv[1];
        assert(key_behavior_count == 0 && noah_combo_count == 0);
        assert(noah_profile_compiled_v1_open(&profile, NULL) == NOAH_PROFILE_COMPILED_V1_OK);
        assert(profile.metadata.action_abi_digest == fixture_u32("profile.action_abi", 16));
        validate_import_on_empty_firmware(&profile, argv[3]);
        puts("empty firmware accepts the app's complete populated profile and preserves its action ABI");
        return 0;
    }
    fixture_path = argv[1];
    test_semantic_action_translation();
    test_real_authored_profile();
    puts("compiled profile defaults v1 host tests passed");
    return 0;
}
