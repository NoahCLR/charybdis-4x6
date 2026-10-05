#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/profile/schema/profile_pd_v1.h"

int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    uint8_t bytes[NOAH_PROFILE_PD_V1_SIZE + 1];
    size_t count = 0, accepted = 0, rejected = 0;
    noah_profile_pd_v1_error_t error;
    for (;;) {
        int expected = fgetc(file);
        if (expected == EOF) break;
        int low = fgetc(file), high = fgetc(file);
        assert(low >= 0 && high >= 0);
        size_t length = (size_t)low | ((size_t)high << 8);
        assert(length <= sizeof(bytes));
        assert(fread(bytes, 1, length, file) == length);
        noah_profile_pd_v1_result_t result = noah_profile_pd_v1_validate(bytes, length, &error);
        if ((result == NOAH_PROFILE_PD_V1_OK) != (expected != 0)) {
            fprintf(stderr, "PD corpus case %zu: expected valid=%d, code=%u offset=%zu\n", count, expected, result, error.offset);
            return 1;
        }
        assert(error.code == result);
        if (expected) accepted++; else rejected++;
        count++;
    }
    assert(!ferror(file));
    fclose(file);
    assert(accepted > 100 && rejected > 1000);
    assert(noah_profile_pd_v1_validate(NULL, 0, &error) == NOAH_PROFILE_PD_V1_INVALID_ARGUMENT);
    assert(noah_profile_pd_v1_validate_record(bytes, 96, 8, NULL) == NOAH_PROFILE_PD_V1_INVALID_ARGUMENT);
    assert(noah_profile_pd_v1_validate_record(bytes, 95, 0, NULL) == NOAH_PROFILE_PD_V1_INVALID_LENGTH);
    // Byte 87 of a directional record: repeat 0 or once per movement 1; 88
    // and 89 stay reserved. A scrolling record's byte 87 is its sustain ratio.
    uint8_t record[96];
    const noah_pd_config_t vertical = {.id = 1, .name = "Up / Down", .kind = 1, .threshold_y = 60, .directions = {[2] = {4, 0, 0}, [3] = {5, 0, 0}}};
    const noah_pd_config_t eight    = {.id = 1, .name = "Eight", .kind = 1, .axis = NOAH_PD_AXIS_EIGHT, .threshold_x = 40, .threshold_y = 40, .directions = {{4, 0, 0}}};
    const noah_pd_config_t *directional[] = {&vertical, &eight};
    for (size_t i = 0; i < 2; i++) {
        noah_profile_pd_v1_encode_record(directional[i], record);
        assert(noah_profile_pd_v1_validate_record(record, 96, 1, &error) == NOAH_PROFILE_PD_V1_OK);
        record[87] = NOAH_PD_DIRECTION_OUTPUT_ONCE;
        assert(noah_profile_pd_v1_validate_record(record, 96, 1, &error) == NOAH_PROFILE_PD_V1_OK);
        record[87] = 2;
        assert(noah_profile_pd_v1_validate_record(record, 96, 1, &error) == NOAH_PROFILE_PD_V1_INVALID_POLICY && error.offset == 87);
        record[87] = NOAH_PD_DIRECTION_OUTPUT_ONCE;
        for (size_t reserved = 88; reserved < 90; reserved++) {
            record[reserved] = 1;
            assert(noah_profile_pd_v1_validate_record(record, 96, 1, &error) == NOAH_PROFILE_PD_V1_RESERVED && error.offset == 88);
            record[reserved] = 0;
        }
    }
    // Byte 3 of a scrolling record: both axes 0, horizontal only 1, vertical
    // only 2.
    const noah_pd_config_t scroll = {.id = 0, .name = "Scroll", .kind = 2, .scroll = {2, 3, 6, 8, 8, 80, 55}, .scroll_policy = {7, 4, 5, 4, 4, 0}};
    noah_profile_pd_v1_encode_record(&scroll, record);
    for (uint8_t axes = NOAH_PD_SCROLL_BOTH; axes <= NOAH_PD_SCROLL_VERTICAL; axes++) {
        record[3] = axes;
        assert(noah_profile_pd_v1_validate_record(record, 96, 0, &error) == NOAH_PROFILE_PD_V1_OK);
    }
    record[3] = NOAH_PD_SCROLL_VERTICAL + 1;
    assert(noah_profile_pd_v1_validate_record(record, 96, 0, &error) == NOAH_PROFILE_PD_V1_INVALID_PARAMETER && error.offset == 3);
    assert(noah_profile_pd_v1_tap_key_valid(0x082e));
    assert(noah_profile_pd_v1_tap_key_valid(0xaa));
    assert(!noah_profile_pd_v1_tap_key_valid(0x08aa));
    assert(!noah_profile_pd_v1_tap_key_valid(0x7e80));
    assert(!noah_profile_pd_v1_tap_key_valid(0x7e40));
    printf("PD domain v1: %zu cross-language cases (%zu valid, %zu rejected) passed\n", count, accepted, rejected);
    return 0;
}
