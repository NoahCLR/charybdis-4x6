// RGB domain version 5: sixteen layer colours, 32 PD rows and the replacement
// feedback record and rows, the golden vectors shared with the app.
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
    // Room for the largest domain's vectors, 4,649 bytes (D-F63).
    static char    hex[16384];
    static uint8_t payload[8192];
    char           code[32], name[64];
    unsigned       stage, layers, brightness, branches;
    unsigned long  pd_mask;
    size_t         offset, cases = 0, rejected = 0;

    assert(argc == 2);
    assert(NOAH_PROFILE_RGB_V1_FORMAT_VERSION == 5 && NOAH_PROFILE_RGB_V1_MAX_POINTING_MODES == 32);
    assert(NOAH_PROFILE_RGB_V1_POINTING_MODE_MASK_ALL == UINT32_MAX);
    FILE *file = fopen(argv[1], "r");
    assert(file);
    assert(fscanf(file, "%u %u %u %u %lu", &stage, &layers, &brightness, &branches, &pd_mask) == 5);
    noah_profile_rgb_v1_limits_t limits = noah_profile_rgb_v1_default_limits();
    limits.compiled_stage_mask    = (uint16_t)stage;
    limits.logical_layer_count    = (uint8_t)layers;
    limits.maximum_brightness     = (uint8_t)brightness;
    limits.tap_branch_color_count = (uint8_t)branches;
    limits.supported_pointing_mode_mask = (uint32_t)pd_mask;
    while (fscanf(file, "%31s %zu %63s %16383s", code, &offset, name, hex) == 4) {
        size_t length = strlen(hex) / 2;
        assert(length <= sizeof(payload));
        for (size_t i = 0; i < length; i++) payload[i] = (uint8_t)(hex_nibble(hex[2 * i]) << 4 | hex_nibble(hex[2 * i + 1]));
        noah_profile_rgb_v1_view_t   view;
        noah_profile_rgb_v1_error_t  error;
        noah_profile_rgb_v1_result_t result = noah_profile_rgb_v1_decode(payload, length, &limits, &view, &error);
        if (strcmp(result_names[result], code) != 0 || (result != NOAH_PROFILE_RGB_V1_OK && error.offset != offset)) {
            fprintf(stderr, "RGB v5 vector %s: expected %s at %zu, got %s at %zu\n", name, code, offset, result_names[result], error.offset);
            return 1;
        }
        if (result == NOAH_PROFILE_RGB_V1_OK) {
            // One row per slot, in slot order.
            assert(view.pd_color_count == 32);
            for (uint8_t slot = 0; slot < 32; slot++) {
                noah_profile_rgb_v1_pd_color_t row;
                assert(noah_profile_rgb_v1_pd_color_at(&view, slot, &row, &error) == NOAH_PROFILE_RGB_V1_OK && row.pointing_mode_id == slot);
            }
            // The replacement record and its rows end the payload: header
            // byte 14 counts the rows, and each reads back as its bytes.
            const uint8_t                             *record = &payload[length - (size_t)payload[14] * NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE - NOAH_PROFILE_RGB_V1_AUTOCORRECT_FEEDBACK_SIZE];
            noah_profile_rgb_v1_autocorrect_feedback_t feedback;
            assert(view.autocorrect_group_count == payload[14]);
            assert(noah_profile_rgb_v1_autocorrect_feedback(&view, &feedback, &error) == NOAH_PROFILE_RGB_V1_OK);
            for (uint8_t semantic = 0; semantic < NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_COUNT; semantic++) {
                assert(feedback.colors[semantic].h == record[3 * semantic] && feedback.colors[semantic].s == record[3 * semantic + 1] && feedback.colors[semantic].v == record[3 * semantic + 2]);
            }
            assert(feedback.locality == record[9]);
            for (uint8_t index = 0; index < view.autocorrect_group_count; index++) {
                const uint8_t                  *bytes = &record[NOAH_PROFILE_RGB_V1_AUTOCORRECT_FEEDBACK_SIZE + (size_t)index * NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE];
                noah_profile_rgb_v1_group_row_t row;
                assert(noah_profile_rgb_v1_autocorrect_group_at(&view, index, &row, &error) == NOAH_PROFILE_RGB_V1_OK);
                assert(row.selector == bytes[0] && row.color.h == bytes[1] && row.color.s == bytes[2] && row.color.v == bytes[3] && row.group_id == bytes[4]);
            }
        } else {
            rejected++;
        }
        cases++;
    }
    assert(feof(file));
    fclose(file);
    assert(cases >= 29 && rejected >= 19);
    printf("RGB domain v5: %zu vectors (%zu rejected) passed\n", cases, rejected);
    return 0;
}
