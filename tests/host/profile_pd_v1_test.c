#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/profile/schema/profile_pd_v1.h"

static const char *const result_names[] = {
    "OK", "INVALID_ARGUMENT", "INVALID_LENGTH", "INVALID_HEADER", "INVALID_ID", "RESERVED",
    "INVALID_NAME", "INVALID_POLICY", "INVALID_ACTION", "INVALID_PARAMETER", "NONCANONICAL",
};

static uint8_t hex_nibble(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    assert(c >= 'a' && c <= 'f');
    return (uint8_t)(c - 'a' + 10);
}

// Each line of the converted pd_mode_domain_v2.json: expected code name,
// payload-relative offset, case name, payload hex.
static size_t check_v2_vectors(const char *path) {
    static char    hex[6400];
    static uint8_t payload[NOAH_PROFILE_PD_V1_MAX_SIZE + 96];
    char           code[32], name[64];
    size_t         offset, cases = 0;
    FILE          *file = fopen(path, "r");
    assert(file);
    while (fscanf(file, "%31s %zu %63s %6199s", code, &offset, name, hex) == 4) {
        size_t length = strlen(hex) / 2;
        assert(length <= sizeof(payload));
        for (size_t i = 0; i < length; i++) payload[i] = (uint8_t)(hex_nibble(hex[2 * i]) << 4 | hex_nibble(hex[2 * i + 1]));
        noah_profile_pd_v1_error_t  error;
        noah_profile_pd_v1_result_t result = noah_profile_pd_v1_validate(payload, length, &error);
        if (strcmp(result_names[result], code) != 0 || error.offset != offset) {
            fprintf(stderr, "PD v2 vector %s: expected %s at %zu, got %s at %zu\n", name, code, offset, result_names[result], error.offset);
            assert(false);
        }
        assert(error.code == result);
        // A version-2 payload is never a version-1 one, and the reverse.
        assert(noah_profile_pd_v1_validate_legacy(payload, length, NULL) != NOAH_PROFILE_PD_V1_OK || strcmp(name, "version-1-payload") == 0);
        cases++;
    }
    assert(feof(file));
    fclose(file);
    return cases;
}

static void check_sparse_rules(void) {
    uint8_t                    payload[NOAH_PROFILE_PD_V1_MAX_SIZE];
    uint8_t                    count = 0;
    noah_profile_pd_v1_error_t error;
    const noah_pd_config_t     named = {.id = 31, .name = "Kept"};
    const noah_pd_config_t     empty = {.id = 30};

    // Header: version 2, capacity 32, record size 96, count, four zero bytes.
    memcpy(payload, (const uint8_t[]){2, 32, 96, 0, 0, 0, 0, 0}, 8);
    assert(noah_profile_pd_v1_validate_header(payload, 8, &count, &error) == NOAH_PROFILE_PD_V1_OK && count == 0);
    assert(noah_profile_pd_v1_validate_header(payload, 104, &count, &error) == NOAH_PROFILE_PD_V1_INVALID_LENGTH);
    assert(noah_profile_pd_v1_validate_header(NULL, 8, &count, &error) == NOAH_PROFILE_PD_V1_INVALID_ARGUMENT);
    for (uint8_t n = 0; n <= 32; n++) {
        payload[3] = n;
        assert(noah_profile_pd_v1_validate_header(payload, 8 + 96u * n, &count, &error) == NOAH_PROFILE_PD_V1_OK && count == n);
    }
    payload[3] = 33;
    assert(noah_profile_pd_v1_validate_header(payload, 8 + 96u * 33, &count, &error) == NOAH_PROFILE_PD_V1_INVALID_HEADER && error.offset == 3);
    payload[3] = 0;
    assert(noah_profile_pd_v1_validate(payload, 7, &error) == NOAH_PROFILE_PD_V1_INVALID_LENGTH);

    // Slot 31, the last ID, stored as a disabled slot with a name; slot 30
    // disabled without a name may only be omitted.
    uint8_t record[96];
    noah_profile_pd_v1_encode_record(&named, record);
    assert(noah_profile_pd_v1_record_present(record));
    assert(noah_profile_pd_v1_validate_entry(record, 96, 0, &error) == NOAH_PROFILE_PD_V1_OK);
    assert(noah_profile_pd_v1_validate_entry(record, 96, 31, &error) == NOAH_PROFILE_PD_V1_OK);
    assert(noah_profile_pd_v1_validate_entry(record, 96, 32, &error) == NOAH_PROFILE_PD_V1_INVALID_ID && error.offset == 0);
    assert(noah_profile_pd_v1_validate_entry(record, 95, 0, &error) == NOAH_PROFILE_PD_V1_INVALID_LENGTH);
    noah_profile_pd_v1_encode_record(&empty, record);
    assert(!noah_profile_pd_v1_record_present(record));
    assert(noah_profile_pd_v1_validate_record(record, 96, 30, &error) == NOAH_PROFILE_PD_V1_OK);
    assert(noah_profile_pd_v1_validate_entry(record, 96, 0, &error) == NOAH_PROFILE_PD_V1_NONCANONICAL && error.offset == 1);
    record[0] = 32;
    assert(noah_profile_pd_v1_validate_entry(record, 96, 0, &error) == NOAH_PROFILE_PD_V1_INVALID_ID);
    assert(noah_profile_pd_v1_validate_record(record, 96, 32, &error) == NOAH_PROFILE_PD_V1_INVALID_ARGUMENT);
}

int main(int argc, char **argv) {
    assert(argc == 3);
    check_sparse_rules();
    size_t v2_cases = check_v2_vectors(argv[2]);
    assert(v2_cases >= 18);
    // The frozen cross-language corpus is the retired eight-slot version 1:
    // its records are version 2's, so it keeps checking every record rule.
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    uint8_t bytes[NOAH_PROFILE_PD_V1_LEGACY_SIZE + 1];
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
        noah_profile_pd_v1_result_t result = noah_profile_pd_v1_validate_legacy(bytes, length, &error);
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
    assert(noah_profile_pd_v1_validate_legacy(NULL, 0, &error) == NOAH_PROFILE_PD_V1_INVALID_ARGUMENT);
    assert(noah_profile_pd_v1_validate_record(bytes, 96, 32, NULL) == NOAH_PROFILE_PD_V1_INVALID_ARGUMENT);
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
    printf("PD domain: %zu version-2 vectors and %zu version-1 cross-language record cases (%zu valid, %zu rejected) passed\n", v2_cases, count, accepted, rejected);
    return 0;
}
