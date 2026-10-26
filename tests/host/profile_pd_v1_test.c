#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
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

// Each line of the converted pd_mode_domain_v4.json: expected code name,
// payload-relative offset, case name, payload hex.
static size_t check_v4_vectors(const char *path) {
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
            fprintf(stderr, "PD v4 vector %s: expected %s at %zu, got %s at %zu\n", name, code, offset, result_names[result], error.offset);
            assert(false);
        }
        assert(error.code == result);
        if (strcmp(name, "version-3-payload") == 0) assert(result != NOAH_PROFILE_PD_V1_OK);
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

    // Header: version 4, capacity 32, record size 128, count, four zero bytes.
    memcpy(payload, (const uint8_t[]){NOAH_PROFILE_PD_V1_VERSION, 32, 128, 0, 0, 0, 0, 0}, 8);
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
    uint8_t payload[8 + 2 * 128] = {NOAH_PROFILE_PD_V1_VERSION, 32, 128, 2};
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

// A frozen 96-byte record as a current one: the old 24-byte name field is
// copied verbatim to the name area and its length is its first zero (33, past
// the limit, when it has none), so a name the old rule refused (unterminated,
// cut UTF-8, a control, junk after the terminator) is refused again.
static void current_from_frozen(const uint8_t *frozen, uint8_t out[NOAH_PROFILE_PD_V1_RECORD_SIZE]) {
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
    record[16] = 1; // version 2's name field past the link stays zero
    assert(noah_profile_pd_v1_validate_record(record, sizeof(record), 0, &error) == NOAH_PROFILE_PD_V1_RESERVED && error.offset == 16);
}

// Bytes 11..15 link a slot to layers (D-F62): byte 11 what ends a mode its
// linked layers started, nothing (0), a higher layer (1) or any other layer
// (2), which needs linked layers; bytes 12..13 the linked layers, bit n layer
// n; bytes 14..15 the layers its key turns on, any mix. Bytes 9 and 10 and
// 16..31 are reserved. No link leaves every link byte zero.
static void check_links(void) {
    const noah_pd_config_t     mode = {.id = 2, .name = "Linked", .kind = 1, .threshold_y = 60, .directions = {[2] = {4, 0, 0}, [3] = {5, 0, 0}}};
    uint8_t                    record[NOAH_PROFILE_PD_V1_RECORD_SIZE];
    noah_profile_pd_v1_error_t error;

    assert(offsetof(noah_pd_config_t, link_ends) == 11 && offsetof(noah_pd_config_t, linked_layers) == 12 && offsetof(noah_pd_config_t, link_turns_on) == 14 && offsetof(noah_pd_config_t, link_reserved) == 16);
    const uint16_t sets[] = {0x0000, 0x0001, 0x0040, 0x8000, 0x0141, 0xffff};
    for (size_t linked = 0; linked < sizeof(sets) / sizeof(sets[0]); linked++) {
        for (size_t turns_on = 0; turns_on < sizeof(sets) / sizeof(sets[0]); turns_on++) {
            for (uint8_t ends = NOAH_PD_LINK_ENDS_NOTHING; ends <= NOAH_PD_LINK_ENDS_OTHER_LAYER; ends++) {
                noah_profile_pd_v1_encode_record(&mode, record);
                record[11] = ends;
                record[12] = (uint8_t)sets[linked], record[13] = (uint8_t)(sets[linked] >> 8);
                record[14] = (uint8_t)sets[turns_on], record[15] = (uint8_t)(sets[turns_on] >> 8);
                noah_profile_pd_v1_result_t result = noah_profile_pd_v1_validate_record(record, sizeof(record), 2, &error);
                if (!sets[linked] && ends) {
                    assert(result == NOAH_PROFILE_PD_V1_INVALID_PARAMETER && error.offset == 11);
                } else {
                    assert(result == NOAH_PROFILE_PD_V1_OK);
                }
            }
        }
    }
    noah_profile_pd_v1_encode_record(&mode, record);
    record[11] = 3, record[12] = 0x08;
    assert(noah_profile_pd_v1_validate_record(record, sizeof(record), 2, &error) == NOAH_PROFILE_PD_V1_INVALID_POLICY && error.offset == 11);
    // The reserved link bytes: 9 and 10 before the link, 16..31 after it.
    record[11] = 0;
    for (size_t at = 9; at < 32; at++) {
        if (at >= 11 && at < 16) continue;
        record[at] = 1;
        assert(noah_profile_pd_v1_validate_record(record, sizeof(record), 2, &error) == NOAH_PROFILE_PD_V1_RESERVED && error.offset == (at < 11 ? 9u : 16u));
        record[at] = 0;
    }

    // An empty slot has no link.
    const noah_pd_config_t empty = {.id = 2, .name = "Empty"};
    for (uint8_t at = 11; at <= 15; at++) {
        noah_profile_pd_v1_encode_record(&empty, record);
        record[at] = 1;
        assert(noah_profile_pd_v1_validate_record(record, sizeof(record), 2, &error) == NOAH_PROFILE_PD_V1_INVALID_PARAMETER && error.offset == 2);
    }

    // No layer starts two modes: a later slot's linked layers that an earlier
    // slot links are refused at byte 12. The layers keys turn on may repeat,
    // and may be layers another slot links.
    uint8_t                payload[8 + 3 * NOAH_PROFILE_PD_V1_RECORD_SIZE] = {NOAH_PROFILE_PD_V1_VERSION, 32, 128, 3};
    const noah_pd_config_t first  = {.id = 0, .name = "First", .kind = 1, .threshold_y = 60, .link_turns_on = 0x0060};
    noah_pd_config_t       second = {.id = 4, .name = "Second", .kind = 1, .threshold_y = 60, .linked_layers = 0x0140, .link_ends = NOAH_PD_LINK_ENDS_HIGHER_LAYER, .link_turns_on = 0x0120};
    noah_pd_config_t       third  = {.id = 9, .name = "Third", .kind = 2, .scroll = {2, 3, 6, 8, 8, 80, 55}, .scroll_policy = {7, 4, 5, 4, 4, 0}, .linked_layers = 0x8080, .link_ends = NOAH_PD_LINK_ENDS_OTHER_LAYER, .link_turns_on = 0x81e0};
    noah_profile_pd_v1_encode_record(&first, payload + 8);
    noah_profile_pd_v1_encode_record(&second, payload + 8 + 128);
    noah_profile_pd_v1_encode_record(&third, payload + 8 + 256);
    assert(noah_profile_pd_v1_validate(payload, sizeof(payload), &error) == NOAH_PROFILE_PD_V1_OK);
    third.linked_layers = 0x8100;
    noah_profile_pd_v1_encode_record(&third, payload + 8 + 256);
    assert(noah_profile_pd_v1_validate(payload, sizeof(payload), &error) == NOAH_PROFILE_PD_V1_INVALID_PARAMETER && error.offset == 8 + 256 + 12);
    // A layer an earlier key turns on may start a later slot's mode.
    third.linked_layers = 0x8020;
    noah_profile_pd_v1_encode_record(&third, payload + 8 + 256);
    assert(noah_profile_pd_v1_validate(payload, sizeof(payload), &error) == NOAH_PROFILE_PD_V1_OK);
    // Unlinked slots start nothing, so their zero link bytes repeat freely.
    second = (noah_pd_config_t){.id = 4, .name = "Second", .kind = 1, .threshold_y = 60};
    third.linked_layers = 0, third.link_ends = NOAH_PD_LINK_ENDS_NOTHING, third.link_turns_on = 0;
    noah_profile_pd_v1_encode_record(&second, payload + 8 + 128);
    noah_profile_pd_v1_encode_record(&third, payload + 8 + 256);
    assert(noah_profile_pd_v1_validate(payload, sizeof(payload), &error) == NOAH_PROFILE_PD_V1_OK);
}

// Version 4 retired the mouse button records: bytes 52..69 are reserved zero
// in every slot.
static void check_retired_buttons(void) {
    const noah_pd_config_t     mode = {.id = 4, .name = "Arrow", .kind = 1, .axis = NOAH_PD_AXIS_DOMINANT, .threshold_x = 40, .threshold_y = 50};
    uint8_t                    record[NOAH_PROFILE_PD_V1_RECORD_SIZE];
    noah_profile_pd_v1_error_t error;

    for (size_t at = 52; at < 70; at++) {
        noah_profile_pd_v1_encode_record(&mode, record);
        assert(noah_profile_pd_v1_validate_record(record, sizeof(record), 4, &error) == NOAH_PROFILE_PD_V1_OK);
        record[at] = 1;
        assert(noah_profile_pd_v1_validate_record(record, sizeof(record), 4, &error) == NOAH_PROFILE_PD_V1_RESERVED && error.offset == 52);
    }
}

// A frozen case as the current validator sees it, its button records cleared
// when asked.
static noah_profile_pd_v1_result_t frozen_validate(const uint8_t *bytes, size_t length, bool clear_buttons, noah_profile_pd_v1_error_t *error) {
    const uint8_t               frozen_header[8] = {1, 8, 96, 0, 0, 0, 0, 0};
    noah_profile_pd_v1_result_t result           = NOAH_PROFILE_PD_V1_INVALID_HEADER;

    error->code   = result;
    error->offset = 0;
    if (length == 776 && memcmp(bytes, frozen_header, 8) == 0) {
        for (uint8_t slot = 0; slot < 8; slot++) {
            uint8_t record[NOAH_PROFILE_PD_V1_RECORD_SIZE];
            current_from_frozen(bytes + 8 + slot * 96, record);
            if (clear_buttons) memset(record + 52, 0, 18);
            result = noah_profile_pd_v1_validate_record(record, sizeof(record), slot, error);
            if (result != NOAH_PROFILE_PD_V1_OK) break;
        }
    }
    return result;
}

int main(int argc, char **argv) {
    test_bounded_iterator();
    assert(argc == 3);
    check_sparse_rules();
    check_names();
    check_links();
    check_retired_buttons();
    size_t v4_cases = check_v4_vectors(argv[2]);
    assert(v4_cases >= 25);
    // The frozen cross-language corpus is the retired eight-slot version 1:
    // its records are version 2's, so it keeps checking every record rule.
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    uint8_t                    bytes[777];
    size_t count = 0, accepted = 0, rejected = 0, retired = 0;
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
        // Version 4 retired the mouse button records: a case that stored one
        // is refused as it is, and one accepted before is accepted again with
        // those bytes cleared, so it keeps checking every other record rule.
        const uint8_t frozen_header[8] = {1, 8, 96, 0, 0, 0, 0, 0};
        bool          frozen           = length == 776 && memcmp(bytes, frozen_header, 8) == 0;
        bool          stored_buttons   = false;
        if (frozen) {
            for (uint8_t slot = 0; slot < 8; slot++) {
                for (size_t at = 52; at < 70; at++) stored_buttons |= bytes[8 + slot * 96 + at] != 0;
            }
        }
        noah_profile_pd_v1_result_t result = frozen_validate(bytes, length, false, &error);
        if ((result == NOAH_PROFILE_PD_V1_OK) != (expected != 0 && !stored_buttons)) {
            fprintf(stderr, "PD corpus case %zu: expected valid=%d, code=%u offset=%zu\n", count, expected != 0 && !stored_buttons, result, error.offset);
            return 1;
        }
        if (stored_buttons && expected) {
            noah_profile_pd_v1_error_t cleared;
            if (frozen_validate(bytes, length, true, &cleared) != NOAH_PROFILE_PD_V1_OK) {
                fprintf(stderr, "PD corpus case %zu: refused with its button records cleared, code=%u offset=%zu\n", count, cleared.code, cleared.offset);
                return 1;
            }
        }
        assert(error.code == result);
        if (expected) accepted++; else rejected++;
        if (stored_buttons && expected) retired++;
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
    // Plain mouse buttons 1-8 are taps (feature bit 28). Movement, wheel,
    // acceleration and modified mouse codes stay out.
    for (uint16_t key = 0xc3; key <= 0xdf; key++) {
        assert(noah_profile_pd_v1_tap_key_valid(key) == (key >= 0xd1 && key <= 0xd8));
    }
    assert(!noah_profile_pd_v1_tap_key_valid(0x01d3));
    assert(!noah_profile_pd_v1_tap_key_valid(0x02d4));
    // Buttons 3 and 4 in a direction, under every modifier policy; a masked
    // policy still needs its mask.
    noah_profile_pd_v1_encode_record(&vertical, record);
    const uint8_t clicks[][4] = {{0xd3, 0, 0, 0}, {0xd4, 0, 1, 0x02}, {0xd8, 0, 2, 0}, {0xd1, 0, 0, 0}};
    for (size_t i = 0; i < 4; i++) {
        memcpy(record + 44, clicks[i], 4);
        assert(noah_profile_pd_v1_validate_record(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, 1, &error) == NOAH_PROFILE_PD_V1_OK);
    }
    const uint8_t rejected_mouse[][4] = {{0xd0, 0, 0, 0}, {0xd9, 0, 0, 0}, {0xd3, 0x01, 0, 0}, {0xd3, 0, 1, 0}};
    for (size_t i = 0; i < 4; i++) {
        memcpy(record + 44, rejected_mouse[i], 4);
        assert(noah_profile_pd_v1_validate_record(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, 1, &error) == NOAH_PROFILE_PD_V1_INVALID_ACTION && error.offset == 44);
    }
    // Macros 0-127 are accepted at every tap place, inherit only.
    const noah_pd_config_t macros = {.id = 0, .name = "Macros", .kind = 1, .axis = NOAH_PD_AXIS_EIGHT, .threshold_x = 10, .threshold_y = 10};
    const size_t places[] = {36, 40, 44, 48, 70, 74, 78, 82};
    for (size_t place = 0; place < sizeof(places) / sizeof(places[0]); place++) {
        for (uint16_t key = 0x7700; key <= 0x777f; key++) {
            noah_profile_pd_v1_encode_record(&macros, record);
            size_t at = places[place];
            record[at] = (uint8_t)key; record[at + 1] = (uint8_t)(key >> 8);
            assert(noah_profile_pd_v1_validate_record(record, sizeof(record), 0, &error) == NOAH_PROFILE_PD_V1_OK);
            record[at + 2] = 1; record[at + 3] = 2;
            assert(noah_profile_pd_v1_validate_record(record, sizeof(record), 0, &error) == NOAH_PROFILE_PD_V1_INVALID_ACTION);
            record[at + 2] = 2; record[at + 3] = 0;
            assert(noah_profile_pd_v1_validate_record(record, sizeof(record), 0, &error) == NOAH_PROFILE_PD_V1_INVALID_ACTION);
            record[at + 2] = 0; record[at + 3] = 2;
            assert(noah_profile_pd_v1_validate_record(record, sizeof(record), 0, &error) == NOAH_PROFILE_PD_V1_INVALID_ACTION);
        }
    }
    assert(!noah_profile_pd_v1_tap_key_valid(0x76ff));
    assert(!noah_profile_pd_v1_tap_key_valid(0x7780));
    assert(!noah_profile_pd_v1_tap_key_valid(0x7f00));
    printf("PD domain: %zu version-4 vectors and %zu frozen cross-language record cases (%zu valid, %zu of them only with their button records cleared; %zu rejected) passed\n", v4_cases, count, accepted, retired, rejected);
    return 0;
}
