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
    assert(noah_profile_pd_v1_tap_key_valid(0x082e));
    assert(noah_profile_pd_v1_tap_key_valid(0xaa));
    assert(!noah_profile_pd_v1_tap_key_valid(0x08aa));
    assert(!noah_profile_pd_v1_tap_key_valid(0x7e80));
    assert(!noah_profile_pd_v1_tap_key_valid(0x7e40));
    printf("PD domain v1: %zu cross-language cases (%zu valid, %zu rejected) passed\n", count, accepted, rejected);
    return 0;
}
