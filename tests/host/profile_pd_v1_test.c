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

// Each line of the converted pd_mode_domain_v3.json: expected code name,
// payload-relative offset, case name, payload hex.
static size_t check_v3_vectors(const char *path) {
    static char    hex[2 * (NOAH_PROFILE_PD_V1_MAX_SIZE + NOAH_PROFILE_PD_V1_RECORD_SIZE) + 2];
    static uint8_t payload[NOAH_PROFILE_PD_V1_MAX_SIZE + NOAH_PROFILE_PD_V1_RECORD_SIZE];
    char           code[32], name[64];
    size_t         offset, cases = 0;
    FILE          *file = fopen(path, "r");
    assert(file);
    while (fscanf(file, "%31s %zu %63s %8465s", code, &offset, name, hex) == 4) {
        size_t length = strlen(hex) / 2;
        assert(length <= sizeof(payload));
        for (size_t i = 0; i < length; i++) payload[i] = (uint8_t)(hex_nibble(hex[2 * i]) << 4 | hex_nibble(hex[2 * i + 1]));
        noah_profile_pd_v1_error_t  error;
        noah_profile_pd_v1_result_t result = noah_profile_pd_v1_validate(payload, length, &error);
        if (strcmp(result_names[result], code) != 0 || error.offset != offset) {
            fprintf(stderr, "PD v3 vector %s: expected %s at %zu, got %s at %zu\n", name, code, offset, result_names[result], error.offset);
            assert(false);
        }
        assert(error.code == result);
        if (strcmp(name, "version-2-payload") == 0) assert(result != NOAH_PROFILE_PD_V1_OK);
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

    // Header: version 3, capacity 32, record size 128, count, four zero bytes.
    memcpy(payload, (const uint8_t[]){3, 32, 128, 0, 0, 0, 0, 0}, 8);
    assert(noah_profile_pd_v1_validate_header(payload, 8, &count, &error) == NOAH_PROFILE_PD_V1_OK && count == 0);
    assert(noah_profile_pd_v1_validate_header(payload, 136, &count, &error) == NOAH_PROFILE_PD_V1_INVALID_LENGTH);
    assert(noah_profile_pd_v1_validate_header(NULL, 8, &count, &error) == NOAH_PROFILE_PD_V1_INVALID_ARGUMENT);
    for (uint8_t n = 0; n <= 32; n++) {
        payload[3] = n;
        assert(noah_profile_pd_v1_validate_header(payload, 8 + 128u * n, &count, &error) == NOAH_PROFILE_PD_V1_OK && count == n);
    }
    payload[3] = 33;
    assert(noah_profile_pd_v1_validate_header(payload, 8 + 128u * 33, &count, &error) == NOAH_PROFILE_PD_V1_INVALID_HEADER && error.offset == 3);
    payload[3] = 0;
    assert(noah_profile_pd_v1_validate(payload, 7, &error) == NOAH_PROFILE_PD_V1_INVALID_LENGTH);

    // Slot 31, the last ID, stored as a disabled slot with a name; slot 30
    // disabled without a name may only be omitted.
    uint8_t record[NOAH_PROFILE_PD_V1_RECORD_SIZE];
    noah_profile_pd_v1_encode_record(&named, record);
    assert(noah_profile_pd_v1_record_present(record));
    assert(noah_profile_pd_v1_validate_entry(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, 0, &error) == NOAH_PROFILE_PD_V1_OK);
    assert(noah_profile_pd_v1_validate_entry(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, 31, &error) == NOAH_PROFILE_PD_V1_OK);
    assert(noah_profile_pd_v1_validate_entry(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, 32, &error) == NOAH_PROFILE_PD_V1_INVALID_ID && error.offset == 0);
    assert(noah_profile_pd_v1_validate_entry(record, NOAH_PROFILE_PD_V1_RECORD_SIZE - 1, 0, &error) == NOAH_PROFILE_PD_V1_INVALID_LENGTH);
    noah_profile_pd_v1_encode_record(&empty, record);
    assert(!noah_profile_pd_v1_record_present(record));
    assert(noah_profile_pd_v1_validate_record(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, 30, &error) == NOAH_PROFILE_PD_V1_OK);
    assert(noah_profile_pd_v1_validate_entry(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, 0, &error) == NOAH_PROFILE_PD_V1_NONCANONICAL && error.offset == 1);
    record[0] = 32;
    assert(noah_profile_pd_v1_validate_entry(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, 0, &error) == NOAH_PROFILE_PD_V1_INVALID_ID);
    assert(noah_profile_pd_v1_validate_record(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, 32, &error) == NOAH_PROFILE_PD_V1_INVALID_ARGUMENT);
}

static size_t iterator_reads, iterator_fail_at;
static bool iterator_read(void *context, size_t offset, uint8_t *out, size_t length) {
    assert(length <= 20u);
    iterator_reads++;
    if (iterator_reads == iterator_fail_at) return false;
    memcpy(out, (const uint8_t *)context + offset, length);
    return true;
}
static void test_bounded_iterator(void) {
    uint8_t payload[8 + 2 * 128] = {3, 32, 128, 2};
    noah_pd_config_t config = {.id = 0, .name = "First"};
    noah_profile_pd_v1_encode_record(&config, payload + 8);
    config.id = 31; strcpy(config.name, "Last");
    noah_profile_pd_v1_encode_record(&config, payload + 8 + 128);
    noah_profile_reader_t reader = {.read = iterator_read, .context = payload, .length = sizeof(payload)};
    noah_profile_pd_v1_iterator_t iterator = {0};
    noah_profile_pd_v1_error_t error;
    iterator_reads = iterator_fail_at = 0;
    unsigned records = 0;
    while (!noah_profile_pd_v1_iterator_complete(&iterator)) {
        size_t before = iterator_reads;
        noah_profile_pd_v1_iteration_t result = noah_profile_pd_v1_iterator_step(&iterator, &reader, 0, sizeof(payload), &error);
        assert(iterator_reads == before + 1u && result != NOAH_PROFILE_PD_V1_REJECTED);
        if (result == NOAH_PROFILE_PD_V1_RECORD) {
            assert(iterator.bytes[0] == (records ? 31 : 0));
            assert(memcmp(iterator.bytes, payload + 8 + records * 128, 128) == 0);
            records++;
        }
    }
    // The header, then seven reads of at most 20 bytes a record.
    assert(records == 2 && iterator_reads == 1 + 2 * 7);
    size_t before = iterator_reads;
    assert(noah_profile_pd_v1_iterator_step(&iterator, &reader, 0, sizeof(payload), &error) == NOAH_PROFILE_PD_V1_COMPLETE && iterator_reads == before);
    iterator = (noah_profile_pd_v1_iterator_t){0}; iterator_reads = 0; iterator_fail_at = 3;
    assert(noah_profile_pd_v1_iterator_step(&iterator, &reader, 0, sizeof(payload), &error) == NOAH_PROFILE_PD_V1_ITERATING);
    assert(noah_profile_pd_v1_iterator_step(&iterator, &reader, 0, sizeof(payload), &error) == NOAH_PROFILE_PD_V1_ITERATING);
    assert(noah_profile_pd_v1_iterator_step(&iterator, &reader, 0, sizeof(payload), &error) == NOAH_PROFILE_PD_V1_REJECTED);
    assert(error.code == NOAH_PROFILE_PD_V1_READ_ERROR && error.offset == 28 && iterator.cursor.index == 0);
}

// A frozen 96-byte record as a version-3 one: the old 24-byte name field is
// copied verbatim to the name area and its length is its first zero (33, past
// the limit, when it has none), so a name the old rule refused (unterminated,
// cut UTF-8, a control, junk after the terminator) is refused again.
static void v3_from_frozen(const uint8_t *frozen, uint8_t out[NOAH_PROFILE_PD_V1_RECORD_SIZE]) {
    uint8_t length = 0;
    while (length < 24 && frozen[8 + length]) length++;
    memset(out, 0, NOAH_PROFILE_PD_V1_RECORD_SIZE);
    memcpy(out, frozen, 96);
    memset(out + 9, 0, 23);
    out[NOAH_PROFILE_PD_V1_NAME_LENGTH] = length < 24 ? length : 33;
    memcpy(out + NOAH_PROFILE_PD_V1_NAME_OFFSET, frozen + 8, 24);
}

// The name rule is the shared one: up to 32 bytes, multibyte text counted in
// bytes, the rest zero.
static void check_names(void) {
    noah_pd_config_t           config = {.id = 0};
    uint8_t                    record[NOAH_PROFILE_PD_V1_RECORD_SIZE];
    noah_profile_pd_v1_error_t error;
    memcpy(config.name, "Thirty-two bytes of mode name!!!", 32); // no terminator
    noah_profile_pd_v1_encode_record(&config, record);
    assert(record[8] == 32 && noah_profile_pd_v1_record_present(record));
    assert(noah_profile_pd_v1_validate_record(record, sizeof(record), 0, &error) == NOAH_PROFILE_PD_V1_OK);
    memset(config.name, 0, sizeof(config.name));
    memcpy(config.name, "D\xc3\xa9" "filement \xe2\x80\x93 \xf0\x9f\x96\xb1", 20);
    noah_profile_pd_v1_encode_record(&config, record);
    assert(record[8] == 20 && noah_profile_pd_v1_validate_record(record, sizeof(record), 0, &error) == NOAH_PROFILE_PD_V1_OK);
    record[8] = 19; // cuts the four-byte sequence
    assert(noah_profile_pd_v1_validate_record(record, sizeof(record), 0, &error) == NOAH_PROFILE_PD_V1_INVALID_NAME && error.offset == 8);
    record[8] = 20;
    record[9] = 1; // version 2's name field stays zero
    assert(noah_profile_pd_v1_validate_record(record, sizeof(record), 0, &error) == NOAH_PROFILE_PD_V1_RESERVED && error.offset == 9);
}

int main(int argc, char **argv) {
    test_bounded_iterator();
    assert(argc == 3);
    check_sparse_rules();
    check_names();
    size_t v3_cases = check_v3_vectors(argv[2]);
    assert(v3_cases >= 25);
    // The frozen cross-language corpus is the retired eight-slot version 1:
    // its records are version 2's, so it keeps checking every record rule.
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    uint8_t                    bytes[777];
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
        // Frozen vectors exercise the unchanged 96-byte record rules. The
        // retired envelope is test input only; production has no decoder.
        noah_profile_pd_v1_result_t result = NOAH_PROFILE_PD_V1_INVALID_HEADER;
        error.code                         = result;
        error.offset                       = 0;
        const uint8_t frozen_header[8]     = {1, 8, 96, 0, 0, 0, 0, 0};
        if (length == 776 && memcmp(bytes, frozen_header, 8) == 0) {
            result = NOAH_PROFILE_PD_V1_OK;
            for (uint8_t slot = 0; slot < 8; slot++) {
                uint8_t record[NOAH_PROFILE_PD_V1_RECORD_SIZE];
                v3_from_frozen(bytes + 8 + slot * 96, record);
                result = noah_profile_pd_v1_validate_record(record, sizeof(record), slot, &error);
                if (result != NOAH_PROFILE_PD_V1_OK) break;
            }
        }
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
    assert(noah_profile_pd_v1_validate_record(bytes, NOAH_PROFILE_PD_V1_RECORD_SIZE, 32, NULL) == NOAH_PROFILE_PD_V1_INVALID_ARGUMENT);
    assert(noah_profile_pd_v1_validate_record(bytes, 96, 0, NULL) == NOAH_PROFILE_PD_V1_INVALID_LENGTH);
    // Byte 87 of a directional record: repeat 0 or once per movement 1; 88
    // and 89 stay reserved. A scrolling record's byte 87 is its sustain ratio.
    uint8_t record[NOAH_PROFILE_PD_V1_RECORD_SIZE];
    const noah_pd_config_t vertical = {.id = 1, .name = "Up / Down", .kind = 1, .threshold_y = 60, .directions = {[2] = {4, 0, 0}, [3] = {5, 0, 0}}};
    const noah_pd_config_t eight    = {.id = 1, .name = "Eight", .kind = 1, .axis = NOAH_PD_AXIS_EIGHT, .threshold_x = 40, .threshold_y = 40, .directions = {{4, 0, 0}}};
    const noah_pd_config_t *directional[] = {&vertical, &eight};
    for (size_t i = 0; i < 2; i++) {
        noah_profile_pd_v1_encode_record(directional[i], record);
        assert(noah_profile_pd_v1_validate_record(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, 1, &error) == NOAH_PROFILE_PD_V1_OK);
        record[87] = NOAH_PD_DIRECTION_OUTPUT_ONCE;
        assert(noah_profile_pd_v1_validate_record(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, 1, &error) == NOAH_PROFILE_PD_V1_OK);
        record[87] = 2;
        assert(noah_profile_pd_v1_validate_record(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, 1, &error) == NOAH_PROFILE_PD_V1_INVALID_POLICY && error.offset == 87);
        record[87] = NOAH_PD_DIRECTION_OUTPUT_ONCE;
        for (size_t reserved = 88; reserved < 90; reserved++) {
            record[reserved] = 1;
            assert(noah_profile_pd_v1_validate_record(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, 1, &error) == NOAH_PROFILE_PD_V1_RESERVED && error.offset == 88);
            record[reserved] = 0;
        }
    }
    // Byte 3 of a scrolling record: both axes 0, horizontal only 1, vertical
    // only 2.
    const noah_pd_config_t scroll = {.id = 0, .name = "Scroll", .kind = 2, .scroll = {2, 3, 6, 8, 8, 80, 55}, .scroll_policy = {7, 4, 5, 4, 4, 0}};
    noah_profile_pd_v1_encode_record(&scroll, record);
    for (uint8_t axes = NOAH_PD_SCROLL_BOTH; axes <= NOAH_PD_SCROLL_VERTICAL; axes++) {
        record[3] = axes;
        assert(noah_profile_pd_v1_validate_record(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, 0, &error) == NOAH_PROFILE_PD_V1_OK);
    }
    record[3] = NOAH_PD_SCROLL_VERTICAL + 1;
    assert(noah_profile_pd_v1_validate_record(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, 0, &error) == NOAH_PROFILE_PD_V1_INVALID_PARAMETER && error.offset == 3);
    assert(noah_profile_pd_v1_tap_key_valid(0x082e));
    assert(noah_profile_pd_v1_tap_key_valid(0xaa));
    assert(!noah_profile_pd_v1_tap_key_valid(0x08aa));
    assert(!noah_profile_pd_v1_tap_key_valid(0x7e80));
    assert(!noah_profile_pd_v1_tap_key_valid(0x7e40));
    printf("PD domain: %zu version-3 vectors and %zu frozen cross-language record cases (%zu valid, %zu rejected) passed\n", v3_cases, count, accepted, rejected);
    return 0;
}
