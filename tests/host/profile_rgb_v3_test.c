// RGB domain version 3: the 32-slot golden vectors shared with the app.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/profile/schema/profile_rgb_v1.h"

static const char *const result_names[] = {
    "OK", "INVALID_ARGUMENT", "TRUNCATED", "TRAILING_BYTES", "READ_ERROR", "CAPACITY_EXCEEDED",
    "INVALID_VERSION", "RESERVED_BITS", "INCOMPATIBLE_GEOMETRY", "NONCANONICAL_ORDER", "DUPLICATE_BITMAP",
    "INVALID_REFERENCE", "INVALID_SELECTOR", "INVALID_ENUM", "BRIGHTNESS_EXCEEDED", "INVALID_ID",
    "UNSUPPORTED_STAGE", "UNSUPPORTED_STAGE_DATA", "INCOMPLETE_SURFACE",
};
_Static_assert(sizeof(result_names) / sizeof(result_names[0]) == NOAH_PROFILE_RGB_V1_INCOMPLETE_SURFACE + 1, "one name per result");

static uint8_t hex_nibble(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    assert(c >= 'a' && c <= 'f');
    return (uint8_t)(c - 'a' + 10);
}

// argv[1]: limits line "stage layers brightness tap_branches pd_mask", then
// one line per vector: expected code, payload-relative offset, name, hex.
int main(int argc, char **argv) {
    static char    hex[4096];
    static uint8_t payload[2048];
    char           code[32], name[64];
    unsigned       stage, layers, brightness, branches;
    unsigned long  pd_mask;
    size_t         offset, cases = 0, rejected = 0;

    assert(argc == 2);
    assert(NOAH_PROFILE_RGB_V1_FORMAT_VERSION == 3 && NOAH_PROFILE_RGB_V1_MAX_PD_MODES == 32);
    assert(NOAH_PROFILE_RGB_V1_PD_MODE_MASK_ALL == UINT32_MAX);
    FILE *file = fopen(argv[1], "r");
    assert(file);
    assert(fscanf(file, "%u %u %u %u %lu", &stage, &layers, &brightness, &branches, &pd_mask) == 5);
    noah_profile_rgb_v1_limits_t limits = noah_profile_rgb_v1_default_limits();
    limits.compiled_stage_mask    = (uint16_t)stage;
    limits.logical_layer_count    = (uint8_t)layers;
    limits.maximum_brightness     = (uint8_t)brightness;
    limits.tap_branch_color_count = (uint8_t)branches;
    limits.supported_pd_mode_mask = (uint32_t)pd_mask;
    while (fscanf(file, "%31s %zu %63s %4095s", code, &offset, name, hex) == 4) {
        size_t length = strlen(hex) / 2;
        assert(length <= sizeof(payload));
        for (size_t i = 0; i < length; i++) payload[i] = (uint8_t)(hex_nibble(hex[2 * i]) << 4 | hex_nibble(hex[2 * i + 1]));
        noah_profile_rgb_v1_view_t   view;
        noah_profile_rgb_v1_error_t  error;
        noah_profile_rgb_v1_result_t result = noah_profile_rgb_v1_decode(payload, length, &limits, &view, &error);
        if (strcmp(result_names[result], code) != 0 || (result != NOAH_PROFILE_RGB_V1_OK && error.offset != offset)) {
            fprintf(stderr, "RGB v3 vector %s: expected %s at %zu, got %s at %zu\n", name, code, offset, result_names[result], error.offset);
            return 1;
        }
        if (result == NOAH_PROFILE_RGB_V1_OK) {
            // One row per slot, in slot order.
            assert(view.pd_color_count == 32);
            for (uint8_t slot = 0; slot < 32; slot++) {
                noah_profile_rgb_v1_pd_color_t row;
                assert(noah_profile_rgb_v1_pd_color_at(&view, slot, &row, &error) == NOAH_PROFILE_RGB_V1_OK && row.pd_mode_id == slot);
            }
        } else {
            rejected++;
        }
        cases++;
    }
    assert(feof(file));
    fclose(file);
    assert(cases >= 7 && rejected >= 5);
    printf("RGB domain v3: %zu vectors (%zu rejected) passed\n", cases, rejected);
    return 0;
}
