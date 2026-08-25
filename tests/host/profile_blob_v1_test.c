#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/schema/profile_blob_v1.h"

enum {
    TEST_BUFFER_SIZE = NOAH_PROFILE_BLOB_V1_MAX_SIZE + 32u,
};

typedef struct {
    const char *name;
    uint8_t     kind;
} action_fixture_t;

static const action_fixture_t action_fixtures[] = {
    {"NONE", NOAH_PROFILE_ACTION_V1_NONE},
    {"QMK_KEYCODE", NOAH_PROFILE_ACTION_V1_QMK_KEYCODE},
    {"LAYER_MOMENTARY", NOAH_PROFILE_ACTION_V1_LAYER_MOMENTARY},
    {"LAYER_LOCK", NOAH_PROFILE_ACTION_V1_LAYER_LOCK},
    {"PD_MODE_MOMENTARY", NOAH_PROFILE_ACTION_V1_PD_MODE_MOMENTARY},
    {"PD_MODE_LOCK", NOAH_PROFILE_ACTION_V1_PD_MODE_LOCK},
    {"VIA_MACRO", NOAH_PROFILE_ACTION_V1_VIA_MACRO},
    {"HARDCODED_MACRO", NOAH_PROFILE_ACTION_V1_HARDCODED_MACRO},
};

static void expect_result(noah_profile_codec_v1_result_t actual, noah_profile_codec_v1_result_t expected) {
    if (actual != expected) {
        fprintf(stderr, "codec result mismatch: got %u expected %u\n", (unsigned)actual, (unsigned)expected);
        abort();
    }
}

static bool fixture_value(const char *path, const char *key, char *value, size_t value_capacity) {
    FILE  *file;
    char   line[TEST_BUFFER_SIZE * 2u];
    size_t key_length = strlen(key);

    file = fopen(path, "r");
    if (!file) {
        return false;
    }
    while (fgets(line, sizeof(line), file)) {
        size_t line_length;

        if (strncmp(line, key, key_length) != 0 || line[key_length] != '=') {
            continue;
        }
        line_length = strcspn(&line[key_length + 1u], "\r\n");
        if (line_length + 1u > value_capacity) {
            fclose(file);
            return false;
        }
        memcpy(value, &line[key_length + 1u], line_length);
        value[line_length] = '\0';
        fclose(file);
        return true;
    }
    fclose(file);
    return false;
}

static uint8_t hex_nibble(char value) {
    if (value >= '0' && value <= '9') {
        return (uint8_t)(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return (uint8_t)(value - 'a' + 10);
    }
    if (value >= 'A' && value <= 'F') {
        return (uint8_t)(value - 'A' + 10);
    }
    abort();
}

static size_t fixture_hex(const char *path, const char *key, uint8_t *output, size_t capacity) {
    char   encoded[TEST_BUFFER_SIZE * 2u];
    size_t index;
    size_t length;

    assert(fixture_value(path, key, encoded, sizeof(encoded)));
    length = strlen(encoded);
    assert((length % 2u) == 0u);
    assert(length / 2u <= capacity);
    for (index = 0u; index < length; index += 2u) {
        output[index / 2u] = (uint8_t)((hex_nibble(encoded[index]) << 4u) | hex_nibble(encoded[index + 1u]));
    }
    return length / 2u;
}

static uint32_t fixture_u32(const char *path, const char *key, int base) {
    char         encoded[64];
    char        *end;
    unsigned long value;

    assert(fixture_value(path, key, encoded, sizeof(encoded)));
    value = strtoul(encoded, &end, base);
    assert(*encoded != '\0' && *end == '\0' && value <= UINT32_MAX);
    return (uint32_t)value;
}

static void test_shared_blob_vectors(const char *fixture_path) {
    uint8_t                       expected[TEST_BUFFER_SIZE];
    uint8_t                       encoded[TEST_BUFFER_SIZE];
    size_t                        expected_length;
    size_t                        written;
    noah_profile_blob_v1_t        blob;
    noah_profile_codec_v1_error_t error;
    static const uint8_t          rgb_payload[]      = {0xDEu, 0xADu, 0xBEu, 0xEFu};
    static const uint8_t          behavior_payload[] = {0x00u, 0x01u, 0x02u};
    const noah_profile_domain_v1_t unsorted[]        = {
        {.id = NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS, .version = 1u, .payload = behavior_payload, .payload_length = sizeof(behavior_payload)},
        {.id = NOAH_PROFILE_DOMAIN_V1_RGB, .version = 1u, .payload = rgb_payload, .payload_length = sizeof(rgb_payload)},
    };

    expected_length = fixture_hex(fixture_path, "blob.empty.hex", expected, sizeof(expected));
    expect_result(noah_profile_blob_v1_encode(NULL, 0u, encoded, sizeof(encoded), &written, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(written == expected_length);
    assert(memcmp(encoded, expected, written) == 0);
    expect_result(noah_profile_blob_v1_decode(encoded, written, &blob, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(blob.domain_count == 0u);
    assert(blob.byte_length == written);
    assert(blob.digest == fixture_u32(fixture_path, "blob.empty.fnv1a32", 16));
    assert(blob.crc32 == fixture_u32(fixture_path, "blob.empty.crc32", 16));

    expected_length = fixture_hex(fixture_path, "blob.ordered.hex", expected, sizeof(expected));
    expect_result(noah_profile_blob_v1_encode(unsorted, sizeof(unsorted) / sizeof(unsorted[0]), encoded, sizeof(encoded), &written, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(written == expected_length);
    assert(memcmp(encoded, expected, written) == 0);
    expect_result(noah_profile_blob_v1_decode(encoded, written, &blob, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(blob.domain_count == 2u);
    assert(blob.domains[0].id == NOAH_PROFILE_DOMAIN_V1_RGB);
    assert(blob.domains[0].payload_length == sizeof(rgb_payload));
    assert(memcmp(blob.domains[0].payload, rgb_payload, sizeof(rgb_payload)) == 0);
    assert(blob.domains[1].id == NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS);
    assert(blob.domains[1].payload_length == sizeof(behavior_payload));
    assert(memcmp(blob.domains[1].payload, behavior_payload, sizeof(behavior_payload)) == 0);
    assert(blob.digest == fixture_u32(fixture_path, "blob.ordered.fnv1a32", 16));
    assert(blob.crc32 == fixture_u32(fixture_path, "blob.ordered.crc32", 16));
}

static void test_domain_envelopes(void) {
    static const uint8_t         payload[] = {0xAAu, 0xBBu};
    noah_profile_domain_v1_t     input     = {.id = NOAH_PROFILE_DOMAIN_V1_RGB, .version = 1u, .payload = payload, .payload_length = sizeof(payload)};
    noah_profile_domain_v1_t     decoded;
    noah_profile_codec_v1_error_t error;
    uint8_t                      encoded[16];
    uint8_t                      stream[18] = {0xCCu};
    size_t                       written;
    size_t                       next_offset;

    expect_result(noah_profile_domain_v1_encode(&input, encoded, sizeof(encoded), &written, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(written == 6u);
    assert(memcmp(encoded, (const uint8_t[]){0x10u, 0x01u, 0x02u, 0x00u, 0xAAu, 0xBBu}, 6u) == 0);
    expect_result(noah_profile_domain_v1_decode(encoded, written, &decoded, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(decoded.id == input.id && decoded.version == input.version);
    assert(decoded.payload_length == input.payload_length && memcmp(decoded.payload, input.payload, input.payload_length) == 0);

    memcpy(&stream[1], encoded, written);
    stream[written + 1u] = 0xDDu;
    expect_result(noah_profile_domain_v1_read(stream, written + 2u, 1u, &decoded, &next_offset, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(next_offset == written + 1u);
    expect_result(noah_profile_domain_v1_decode(&stream[1], written + 1u, &decoded, &error), NOAH_PROFILE_CODEC_V1_TRAILING_BYTES);
    assert(error.offset == written);

    for (size_t length = 0u; length < written; length++) {
        expect_result(noah_profile_domain_v1_decode(encoded, length, &decoded, &error), NOAH_PROFILE_CODEC_V1_TRUNCATED);
    }
    encoded[0] = 0x30u;
    expect_result(noah_profile_domain_v1_decode(encoded, written, &decoded, &error), NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN);
    encoded[0] = NOAH_PROFILE_DOMAIN_V1_RGB;
    encoded[1] = 2u;
    expect_result(noah_profile_domain_v1_decode(encoded, written, &decoded, &error), NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN_VERSION);
    encoded[1] = 1u;
    encoded[2] = 0xFFu;
    encoded[3] = 0xFFu;
    expect_result(noah_profile_domain_v1_decode(encoded, written, &decoded, &error), NOAH_PROFILE_CODEC_V1_TRUNCATED);

    input.payload_length = UINT16_MAX + (size_t)1u;
    expect_result(noah_profile_domain_v1_encode(&input, encoded, sizeof(encoded), &written, &error), NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED);
    input.payload_length = sizeof(payload);
    expect_result(noah_profile_domain_v1_encode(&input, encoded, 5u, &written, &error), NOAH_PROFILE_CODEC_V1_OUTPUT_TOO_SMALL);
    input.payload        = NULL;
    expect_result(noah_profile_domain_v1_encode(&input, encoded, sizeof(encoded), &written, &error), NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);
    expect_result(noah_profile_domain_v1_read(encoded, sizeof(encoded), sizeof(encoded) + 1u, &decoded, &next_offset, &error), NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);
}

static void test_blob_rejections(const char *fixture_path) {
    uint8_t                       valid[TEST_BUFFER_SIZE];
    uint8_t                       mutated[TEST_BUFFER_SIZE];
    size_t                        valid_length;
    noah_profile_blob_v1_t        blob;
    noah_profile_codec_v1_error_t error;

    valid_length = fixture_hex(fixture_path, "blob.ordered.hex", valid, sizeof(valid));
    for (size_t length = 0u; length < valid_length; length++) {
        assert(noah_profile_blob_v1_decode(valid, length, &blob, &error) != NOAH_PROFILE_CODEC_V1_OK);
    }

    memcpy(mutated, valid, valid_length);
    mutated[0] = 0u;
    expect_result(noah_profile_blob_v1_decode(mutated, valid_length, &blob, &error), NOAH_PROFILE_CODEC_V1_INVALID_MAGIC);
    memcpy(mutated, valid, valid_length);
    mutated[4] = 2u;
    expect_result(noah_profile_blob_v1_decode(mutated, valid_length, &blob, &error), NOAH_PROFILE_CODEC_V1_INCOMPATIBLE_SCHEMA);
    memcpy(mutated, valid, valid_length);
    mutated[5] = 1u;
    expect_result(noah_profile_blob_v1_decode(mutated, valid_length, &blob, &error), NOAH_PROFILE_CODEC_V1_INCOMPATIBLE_SCHEMA);
    for (uint8_t bit = 1u; bit < 8u; bit++) {
        memcpy(mutated, valid, valid_length);
        mutated[7] = (uint8_t)(1u | (1u << bit));
        expect_result(noah_profile_blob_v1_decode(mutated, valid_length, &blob, &error), NOAH_PROFILE_CODEC_V1_RESERVED_FLAGS);
    }
    memcpy(mutated, valid, valid_length);
    mutated[7] = 0u;
    expect_result(noah_profile_blob_v1_decode(mutated, valid_length, &blob, &error), NOAH_PROFILE_CODEC_V1_NONCANONICAL);

    memcpy(mutated, valid, valid_length);
    mutated[8] = 0x30u;
    expect_result(noah_profile_blob_v1_decode(mutated, valid_length, &blob, &error), NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN);
    assert(error.domain_index == 0u && error.domain_id == 0x30u);
    memcpy(mutated, valid, valid_length);
    mutated[9] = 2u;
    expect_result(noah_profile_blob_v1_decode(mutated, valid_length, &blob, &error), NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN_VERSION);

    memcpy(mutated, valid, valid_length);
    mutated[16] = NOAH_PROFILE_DOMAIN_V1_RGB;
    expect_result(noah_profile_blob_v1_decode(mutated, valid_length, &blob, &error), NOAH_PROFILE_CODEC_V1_DUPLICATE_DOMAIN);
    memcpy(mutated, valid, valid_length);
    memcpy(&mutated[8], &valid[16], valid_length - 16u);
    memcpy(&mutated[8u + valid_length - 16u], &valid[8], 8u);
    expect_result(noah_profile_blob_v1_decode(mutated, valid_length, &blob, &error), NOAH_PROFILE_CODEC_V1_DOMAIN_ORDER);

    memcpy(mutated, valid, valid_length);
    mutated[10] = 0xFFu;
    mutated[11] = 0xFFu;
    expect_result(noah_profile_blob_v1_decode(mutated, valid_length, &blob, &error), NOAH_PROFILE_CODEC_V1_TRUNCATED);
    for (size_t extra = 1u; extra <= 16u; extra++) {
        memcpy(mutated, valid, valid_length);
        memset(&mutated[valid_length], 0xA5, extra);
        expect_result(noah_profile_blob_v1_decode(mutated, valid_length + extra, &blob, &error), NOAH_PROFILE_CODEC_V1_TRAILING_BYTES);
    }
    memset(mutated, 0u, NOAH_PROFILE_BLOB_V1_MAX_SIZE + 1u);
    expect_result(noah_profile_blob_v1_decode(mutated, NOAH_PROFILE_BLOB_V1_MAX_SIZE + 1u, &blob, &error), NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED);
}

static void test_blob_encoder_bounds(void) {
    uint8_t                       output[NOAH_PROFILE_BLOB_V1_MAX_SIZE];
    uint8_t                       byte = 0u;
    size_t                        written;
    noah_profile_codec_v1_error_t error;
    noah_profile_domain_v1_t      domains[3] = {
        {.id = NOAH_PROFILE_DOMAIN_V1_RGB, .version = 1u, .payload = NULL, .payload_length = 0u},
        {.id = NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS, .version = 1u, .payload = NULL, .payload_length = 0u},
        {.id = NOAH_PROFILE_DOMAIN_V1_RGB, .version = 1u, .payload = NULL, .payload_length = 0u},
    };

    expect_result(noah_profile_blob_v1_encode(domains, 2u, output, 15u, &written, &error), NOAH_PROFILE_CODEC_V1_OUTPUT_TOO_SMALL);
    expect_result(noah_profile_blob_v1_encode(domains, 3u, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_DUPLICATE_DOMAIN);
    domains[1].id = 0x30u;
    expect_result(noah_profile_blob_v1_encode(domains, 2u, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN);
    domains[1].id      = NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS;
    domains[1].version = 2u;
    expect_result(noah_profile_blob_v1_encode(domains, 2u, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN_VERSION);
    domains[1].version        = 1u;
    domains[0].payload        = &byte;
    domains[0].payload_length = NOAH_PROFILE_BLOB_V1_MAX_SIZE;
    expect_result(noah_profile_blob_v1_encode(domains, 2u, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED);
    expect_result(noah_profile_blob_v1_encode(NULL, 1u, output, sizeof(output), &written, &error), NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);
    expect_result(noah_profile_blob_v1_encode(NULL, 0u, NULL, 0u, &written, &error), NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);
}

static void test_shared_action_vectors(const char *fixture_path) {
    noah_profile_codec_v1_error_t error;

    for (size_t index = 0u; index < sizeof(action_fixtures) / sizeof(action_fixtures[0]); index++) {
        char                     key[96];
        uint8_t                  expected[NOAH_PROFILE_BLOB_V1_ACTION_SIZE];
        uint8_t                  encoded[NOAH_PROFILE_BLOB_V1_ACTION_SIZE];
        noah_profile_action_v1_t input;
        noah_profile_action_v1_t decoded;

        snprintf(key, sizeof(key), "action.%s.operand", action_fixtures[index].name);
        input.kind    = action_fixtures[index].kind;
        input.flags   = 0u;
        input.operand = (uint16_t)fixture_u32(fixture_path, key, 10);
        snprintf(key, sizeof(key), "action.%s.hex", action_fixtures[index].name);
        assert(fixture_hex(fixture_path, key, expected, sizeof(expected)) == sizeof(expected));
        expect_result(noah_profile_action_v1_encode(&input, NULL, encoded, &error), NOAH_PROFILE_CODEC_V1_OK);
        assert(memcmp(encoded, expected, sizeof(encoded)) == 0);
        expect_result(noah_profile_action_v1_decode(expected, sizeof(expected), NULL, &decoded, &error), NOAH_PROFILE_CODEC_V1_OK);
        assert(decoded.kind == input.kind);
        assert(decoded.flags == input.flags);
        assert(decoded.operand == input.operand);
    }
}

static void expect_invalid_operand(uint8_t kind, uint16_t operand, const noah_profile_action_v1_limits_t *limits) {
    noah_profile_action_v1_t     action = {.kind = kind, .flags = 0u, .operand = operand};
    noah_profile_codec_v1_error_t error;
    uint8_t                      encoded[NOAH_PROFILE_BLOB_V1_ACTION_SIZE];

    expect_result(noah_profile_action_v1_encode(&action, limits, encoded, &error), NOAH_PROFILE_CODEC_V1_INVALID_OPERAND);
    assert(error.offset == 2u);
}

static void test_action_rejections_and_streams(void) {
    noah_profile_action_v1_limits_t limits = noah_profile_action_v1_default_limits();
    noah_profile_action_v1_t        action;
    noah_profile_codec_v1_error_t   error;
    uint8_t                         bytes[8] = {0u};
    size_t                          next_offset;

    for (uint16_t kind = 8u; kind <= UINT8_MAX; kind++) {
        bytes[0] = (uint8_t)kind;
        expect_result(noah_profile_action_v1_decode(bytes, 4u, NULL, &action, &error), NOAH_PROFILE_CODEC_V1_UNKNOWN_ACTION_KIND);
    }
    bytes[0] = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE;
    for (uint16_t flags = 1u; flags <= UINT8_MAX; flags++) {
        bytes[1] = (uint8_t)flags;
        expect_result(noah_profile_action_v1_decode(bytes, 4u, NULL, &action, &error), NOAH_PROFILE_CODEC_V1_RESERVED_FLAGS);
    }
    bytes[1] = 0u;
    bytes[0] = NOAH_PROFILE_ACTION_V1_NONE;
    bytes[2] = 1u;
    expect_result(noah_profile_action_v1_decode(bytes, 4u, NULL, &action, &error), NOAH_PROFILE_CODEC_V1_INVALID_OPERAND);

    expect_invalid_operand(NOAH_PROFILE_ACTION_V1_LAYER_MOMENTARY, 8u, NULL);
    expect_invalid_operand(NOAH_PROFILE_ACTION_V1_LAYER_LOCK, 8u, NULL);
    expect_invalid_operand(NOAH_PROFILE_ACTION_V1_PD_MODE_MOMENTARY, 6u, NULL);
    expect_invalid_operand(NOAH_PROFILE_ACTION_V1_PD_MODE_LOCK, 6u, NULL);
    expect_invalid_operand(NOAH_PROFILE_ACTION_V1_VIA_MACRO, 64u, NULL);
    expect_invalid_operand(NOAH_PROFILE_ACTION_V1_HARDCODED_MACRO, 16u, NULL);

    limits.max_logical_layers = 0u;
    expect_invalid_operand(NOAH_PROFILE_ACTION_V1_LAYER_MOMENTARY, 0u, &limits);
    limits.max_logical_layers = UINT32_C(0x10000);
    action = (noah_profile_action_v1_t){.kind = NOAH_PROFILE_ACTION_V1_LAYER_MOMENTARY, .operand = UINT16_MAX};
    expect_result(noah_profile_action_v1_encode(&action, &limits, bytes, &error), NOAH_PROFILE_CODEC_V1_OK);
    limits.max_logical_layers = UINT32_C(0x10001);
    expect_result(noah_profile_action_v1_encode(&action, &limits, bytes, &error), NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);

    action = (noah_profile_action_v1_t){.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = UINT16_MAX};
    expect_result(noah_profile_action_v1_encode(&action, NULL, &bytes[1], &error), NOAH_PROFILE_CODEC_V1_OK);
    bytes[0] = 0xAAu;
    bytes[5] = 0xBBu;
    expect_result(noah_profile_action_v1_read(bytes, 6u, 1u, NULL, &action, &next_offset, &error), NOAH_PROFILE_CODEC_V1_OK);
    assert(next_offset == 5u && action.operand == UINT16_MAX);
    expect_result(noah_profile_action_v1_read(bytes, 6u, 3u, NULL, &action, &next_offset, &error), NOAH_PROFILE_CODEC_V1_TRUNCATED);

    for (size_t length = 0u; length <= 5u; length++) {
        if (length == NOAH_PROFILE_BLOB_V1_ACTION_SIZE) {
            continue;
        }
        expect_result(noah_profile_action_v1_decode(bytes, length, NULL, &action, &error), NOAH_PROFILE_CODEC_V1_INVALID_LENGTH);
    }
    expect_result(noah_profile_action_v1_encode(NULL, NULL, bytes, &error), NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);
    expect_result(noah_profile_action_v1_decode(NULL, 4u, NULL, &action, &error), NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT);
}

static void test_deterministic_malformed_corpus(void) {
    uint8_t                       bytes[96];
    noah_profile_blob_v1_t        blob;
    noah_profile_codec_v1_error_t error;
    uint32_t                      state = UINT32_C(0xC0DEC0DE);

    for (size_t case_index = 0u; case_index < 2048u; case_index++) {
        size_t length;

        state  = state * UINT32_C(1664525) + UINT32_C(1013904223);
        length = (size_t)(state % sizeof(bytes));
        for (size_t index = 0u; index < length; index++) {
            state        = state * UINT32_C(1664525) + UINT32_C(1013904223);
            bytes[index] = (uint8_t)(state >> 24u);
        }
        (void)noah_profile_blob_v1_decode(bytes, length, &blob, &error);
    }
}

int main(int argc, char **argv) {
    assert(argc == 2);
    test_shared_blob_vectors(argv[1]);
    test_domain_envelopes();
    test_blob_rejections(argv[1]);
    test_blob_encoder_bounds();
    test_shared_action_vectors(argv[1]);
    test_action_rejections_and_streams();
    test_deterministic_malformed_corpus();
    puts("profile blob v1 tests passed");
    return 0;
}
