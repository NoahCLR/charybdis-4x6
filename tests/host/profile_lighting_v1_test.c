// RGB domain version 5: the shared codec and conversion vectors
// (tests/fixtures/lighting_vectors_v1.json), checked against the firmware's
// validator and version-4 conversion. run_profile_lighting_v1_tests.sh
// flattens the JSON into one line per vector.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/schema/profile_lighting_v1.h"

static const char *const result_names[] = {
    "OK", "INVALID_ARGUMENT", "READ_ERROR", "TRUNCATED", "TRAILING_BYTES", "CAPACITY_EXCEEDED", "INVALID_VERSION", "RESERVED_BITS", "INCOMPATIBLE_GEOMETRY",
    "BRIGHTNESS_EXCEEDED", "UNSUPPORTED_KIND", "NONCANONICAL_ORDER", "NONCANONICAL_ENCODING", "INVALID_ENUM", "INVALID_LENGTH", "INVALID_STRUCTURE", "INVALID_PARAMETER", "INVALID_REFERENCE",
};
_Static_assert(sizeof(result_names) / sizeof(result_names[0]) == NOAH_PROFILE_LIGHTING_V1_INVALID_REFERENCE + 1, "one name per result");

static size_t from_hex(const char *hex, uint8_t *out, size_t capacity) {
    size_t length = strlen(hex) / 2u;
    assert(length <= capacity);
    for (size_t i = 0; i < length; i++) {
        unsigned value;
        assert(sscanf(hex + 2 * i, "%2x", &value) == 1);
        out[i] = (uint8_t)value;
    }
    return length;
}

static noah_profile_lighting_v1_limits_t read_limits(FILE *file) {
    noah_profile_lighting_v1_limits_t limits = noah_profile_lighting_v1_default_limits();
    unsigned depth, layers, brightness;
    unsigned long modes, facts, targets, paints, timings, rule_ext, domain_ext, things;
    assert(fscanf(file, "%u %u %u %lu %lu %lu %lu %lu %lu %lu %lu", &depth, &layers, &brightness, &modes, &facts, &targets, &paints, &timings, &rule_ext, &domain_ext, &things) == 11);
    limits.tap_depth           = (uint8_t)depth;
    limits.logical_layer_count = (uint8_t)layers;
    limits.maximum_brightness  = (uint8_t)brightness;
    limits.supported_mode_mask = (uint32_t)modes;
    limits.facts               = (uint32_t)facts;
    limits.targets             = (uint32_t)targets;
    limits.paints              = (uint32_t)paints;
    limits.timings             = (uint32_t)timings;
    limits.rule_extensions     = (uint32_t)rule_ext;
    limits.domain_extensions   = (uint32_t)domain_ext;
    limits.things              = (uint32_t)things;
    return limits;
}

int main(int argc, char **argv) {
    static char    hex[2][40000];
    static uint8_t payload[20000], expected[20000], converted[4096];
    char           kind[16], code[32], name[96];
    size_t         offset, codec = 0, rejected = 0, conversions = 0, capabilities = 0;

    assert(argc == 2);
    FILE *file = fopen(argv[1], "r");
    assert(file);
    noah_profile_lighting_v1_limits_t limits = read_limits(file);
    while (fscanf(file, "%15s", kind) == 1) {
        if (strcmp(kind, "CODEC") == 0) {
            assert(fscanf(file, "%31s %zu %95s %39999s", code, &offset, name, hex[0]) == 4);
            size_t                            length = from_hex(hex[0], payload, sizeof(payload));
            noah_profile_lighting_v1_view_t   view;
            noah_profile_lighting_v1_error_t  error = {0};
            noah_profile_lighting_v1_result_t result = noah_profile_lighting_v1_decode(payload, length, &limits, &view, &error);
            if (strcmp(result_names[result], code) != 0 || (result != NOAH_PROFILE_LIGHTING_V1_OK && error.offset != offset)) {
                fprintf(stderr, "lighting codec vector %s: expected %s at %zu, got %s at %zu\n", name, code, offset, result_names[result], error.offset);
                return 1;
            }
            rejected += result != NOAH_PROFILE_LIGHTING_V1_OK;
            codec++;
        } else if (strcmp(kind, "CAPABILITY") == 0) {
            unsigned page;
            uint8_t  answer[25];
            assert(fscanf(file, "%u %39999s", &page, hex[0]) == 2);
            size_t  expected_length = from_hex(hex[0], expected, sizeof(expected));
            uint8_t length          = noah_profile_lighting_v1_capability_page(&limits, (uint8_t)page, answer);
            if (length != expected_length || memcmp(answer, expected, length) != 0) {
                fprintf(stderr, "lighting capability page %u differs\n", page);
                return 1;
            }
            assert(noah_profile_lighting_v1_capability_page(&limits, (uint8_t)(page + 1u), answer) == (page + 1u < NOAH_PROFILE_LIGHTING_V1_CAPABILITY_PAGES ? 18u : 0u));
            capabilities++;
        } else {
            assert(strcmp(kind, "CONVERT") == 0);
            assert(fscanf(file, "%95s", name) == 1);
            noah_profile_lighting_v1_limits_t target = read_limits(file);
            assert(fscanf(file, "%39999s %39999s", hex[0], hex[1]) == 2);
            size_t                       v4_length = from_hex(hex[0], payload, sizeof(payload));
            size_t                       expected_length = from_hex(hex[1], expected, sizeof(expected));
            noah_profile_rgb_v1_limits_t v4_limits = noah_profile_rgb_v1_default_limits();
            noah_profile_rgb_v1_view_t   v4;
            size_t                       length = 0;
            v4_limits.logical_layer_count    = target.logical_layer_count;
            v4_limits.tap_branch_color_count = (uint8_t)(target.tap_depth - 1u);
            v4_limits.maximum_brightness     = target.maximum_brightness;
            noah_profile_rgb_v1_error_t v4_error = {0};
            if (noah_profile_rgb_v1_decode(payload, v4_length, &v4_limits, &v4, &v4_error) != NOAH_PROFILE_RGB_V1_OK) {
                fprintf(stderr, "lighting conversion vector %s: its version-4 payload does not decode (code %u at %zu, depth %u)\n", name, (unsigned)v4_error.code, v4_error.offset, (unsigned)target.tap_depth);
                return 1;
            }
            if (noah_profile_lighting_v1_convert_rgb_v4(&v4, &target, converted, sizeof(converted), &length) != NOAH_PROFILE_LIGHTING_V1_OK || length != expected_length || memcmp(converted, expected, length) != 0) {
                fprintf(stderr, "lighting conversion vector %s differs (%zu bytes, expected %zu)\n", name, length, expected_length);
                for (size_t i = 0; i < length; i++) fprintf(stderr, "%02x", converted[i]);
                fprintf(stderr, "\n");
                return 1;
            }
            noah_profile_lighting_v1_view_t view;
            assert(noah_profile_lighting_v1_decode(converted, length, &target, &view, NULL) == NOAH_PROFILE_LIGHTING_V1_OK);
            conversions++;
        }
    }
    fclose(file);
    assert(codec >= 50 && rejected >= 45 && conversions >= 5 && capabilities == NOAH_PROFILE_LIGHTING_V1_CAPABILITY_PAGES);
    printf("lighting domain v5: %zu codec vectors (%zu rejected), %zu conversions, %zu capability pages passed\n", codec, rejected, conversions, capabilities);
    return 0;
}
