// The autocorrect list domain (0x80 v1): the golden vectors of
// tests/fixtures/autocorrect_domain_v1.json against the validator, the
// lookups an engine makes, the validator's bounded steps, and that no
// malformed list makes a lookup or a validation loop or read out of range.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/schema/profile_autocorrect_v1.h"

#define CHECK(condition)                                                              \
    do {                                                                              \
        if (!(condition)) {                                                           \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            exit(1);                                                                  \
        }                                                                             \
    } while (0)

enum { MAX_BYTES = NOAH_PROFILE_AUTOCORRECT_V1_MAX_SIZE + 16u };

typedef struct {
    char    code[32], name[64];
    int     table, row;
    uint8_t bytes[MAX_BYTES];
    size_t  length;
} vector_t;

static const char *const code_names[] = {
    [NOAH_PROFILE_AUTOCORRECT_V1_OK] = "OK",
    [NOAH_PROFILE_AUTOCORRECT_V1_INVALID_ARGUMENT] = "INVALID_ARGUMENT",
    [NOAH_PROFILE_AUTOCORRECT_V1_READ_ERROR] = "READ_ERROR",
    [NOAH_PROFILE_AUTOCORRECT_V1_INVALID_LENGTH] = "INVALID_LENGTH",
    [NOAH_PROFILE_AUTOCORRECT_V1_CAPACITY_EXCEEDED] = "CAPACITY_EXCEEDED",
    [NOAH_PROFILE_AUTOCORRECT_V1_INVALID_OFFSET] = "INVALID_OFFSET",
    [NOAH_PROFILE_AUTOCORRECT_V1_INVALID_TEXT_LENGTH] = "INVALID_TEXT_LENGTH",
    [NOAH_PROFILE_AUTOCORRECT_V1_RESERVED] = "RESERVED",
    [NOAH_PROFILE_AUTOCORRECT_V1_INVALID_TEXT] = "INVALID_TEXT",
    [NOAH_PROFILE_AUTOCORRECT_V1_ORDER] = "ORDER",
    [NOAH_PROFILE_AUTOCORRECT_V1_INVALID_MATCH] = "INVALID_MATCH",
    [NOAH_PROFILE_AUTOCORRECT_V1_INVALID_REFERENCE] = "INVALID_REFERENCE",
    [NOAH_PROFILE_AUTOCORRECT_V1_FIRST_TYPO] = "FIRST_TYPO",
    [NOAH_PROFILE_AUTOCORRECT_V1_NOT_A_WORD] = "NOT_A_WORD",
    [NOAH_PROFILE_AUTOCORRECT_V1_NOT_FOLDED] = "NOT_FOLDED",
    [NOAH_PROFILE_AUTOCORRECT_V1_TYPO_IS_CORRECTION] = "TYPO_IS_CORRECTION",
    [NOAH_PROFILE_AUTOCORRECT_V1_PART_INDEX] = "PART_INDEX",
};
// The codes travel as a validation error's field id.
_Static_assert(NOAH_PROFILE_AUTOCORRECT_V1_INVALID_LENGTH == 3 && NOAH_PROFILE_AUTOCORRECT_V1_ORDER == 9 && NOAH_PROFILE_AUTOCORRECT_V1_PART_INDEX == 16, "refusal codes are fixed");

// One line of the converted fixture: code, table, row (-1 for none), name, hex.
static bool read_vector(FILE *file, vector_t *vector) {
    static char hex[2u * MAX_BYTES + 2u];
    if (fscanf(file, "%31s %d %d %63s %69680s", vector->code, &vector->table, &vector->row, vector->name, hex) != 5) return false;
    vector->length = strcmp(hex, "-") == 0 ? 0u : strlen(hex) / 2u;
    CHECK(vector->length <= sizeof(vector->bytes));
    for (size_t i = 0; i < vector->length; i++) {
        unsigned value;
        CHECK(sscanf(&hex[2u * i], "%2x", &value) == 1);
        vector->bytes[i] = (uint8_t)value;
    }
    return true;
}

// A reader that counts, and refuses anything outside the payload.
typedef struct {
    const uint8_t *bytes;
    size_t         length, calls, most;
} counted_t;

static bool counted_read(void *context, size_t offset, uint8_t *target, size_t length) {
    counted_t *counted = context;
    CHECK(offset <= counted->length && length <= counted->length - offset);
    counted->calls++;
    if (length > counted->most) counted->most = length;
    memcpy(target, counted->bytes + offset, length);
    return true;
}

// Validation through the bounded reader: one read of at most 20 bytes a step,
// and no more steps than the rows the ceilings allow could need.
static noah_profile_autocorrect_v1_result_t validate_bounded(const uint8_t *bytes, size_t length, noah_profile_autocorrect_v1_error_t *error) {
    counted_t                                     counted = {.bytes = bytes, .length = length};
    noah_profile_reader_t                         reader  = {.read = counted_read, .context = &counted, .length = length};
    noah_profile_autocorrect_v1_validation_t      state   = {0};
    noah_profile_autocorrect_v1_validation_step_t step;
    size_t                                        steps = 0u;
    // A row, two reads of text, and for a typo its correction's row, two
    // reads of that text and a part-index row.
    const size_t most_steps = 1u + 4u * NOAH_PROFILE_AUTOCORRECT_V1_MAX_CORRECTIONS + 7u * NOAH_PROFILE_AUTOCORRECT_V1_MAX_TYPOS;
    do {
        size_t before = counted.calls;
        step          = noah_profile_autocorrect_v1_validation_step(&state, &reader, 0u, length, error);
        CHECK(counted.calls - before <= 1u);
        CHECK(++steps <= most_steps);
    } while (step == NOAH_PROFILE_AUTOCORRECT_V1_VALIDATING);
    CHECK(counted.most <= NOAH_PROFILE_AUTOCORRECT_V1_STEP_READ_MAX);
    if (step == NOAH_PROFILE_AUTOCORRECT_V1_COMPLETE) return NOAH_PROFILE_AUTOCORRECT_V1_OK;
    return error->code;
}

static vector_t small;

static size_t check_vectors(const char *path) {
    static vector_t vector;
    FILE           *file  = fopen(path, "r");
    size_t          count = 0u;
    bool            seen[sizeof(code_names) / sizeof(code_names[0])] = {false};
    CHECK(file != NULL);
    while (read_vector(file, &vector)) {
        noah_profile_autocorrect_v1_error_t  error  = {0};
        noah_profile_autocorrect_v1_result_t result = validate_bounded(vector.bytes, vector.length, &error);
        if (strcmp(code_names[result], vector.code) != 0 || (result != NOAH_PROFILE_AUTOCORRECT_V1_OK && ((vector.table < 0 ? UINT8_MAX : vector.table) != error.table || (vector.row < 0 ? NOAH_PROFILE_AUTOCORRECT_V1_LOCATION_NONE : vector.row) != error.row))) {
            fprintf(stderr, "%s: expected %s at table %d row %d, got %s at table %u row %u (offset %zu)\n", vector.name, vector.code, vector.table, vector.row, code_names[result], (unsigned)error.table, (unsigned)error.row, error.offset);
            exit(1);
        }
        // The one-call form agrees.
        CHECK(noah_profile_autocorrect_v1_validate(vector.bytes, vector.length, NULL) == result);
        seen[result] = true;
        if (strcmp(vector.name, "small") == 0) small = vector;
        count++;
    }
    fclose(file);
    // Every refusal a client can cause has a vector.
    for (size_t code = NOAH_PROFILE_AUTOCORRECT_V1_INVALID_LENGTH; code < sizeof(seen); code++) CHECK(seen[code]);
    CHECK(seen[NOAH_PROFILE_AUTOCORRECT_V1_OK] && small.length != 0u);
    return count;
}

static void test_fold(void) {
    uint32_t folded = 0u;
    CHECK(noah_profile_autocorrect_v1_fold('A', &folded) && folded == 'a');
    CHECK(noah_profile_autocorrect_v1_fold('a', &folded) && folded == 'a');
    CHECK(noah_profile_autocorrect_v1_fold('7', &folded) && folded == '7');
    CHECK(noah_profile_autocorrect_v1_fold(0xC9u, &folded) && folded == 0xE9u); // É, é
    CHECK(noah_profile_autocorrect_v1_fold(0xDFu, &folded) && folded == 0xDFu); // ß
    CHECK(noah_profile_autocorrect_v1_fold('z', NULL));
    CHECK(!noah_profile_autocorrect_v1_fold(' ', &folded));
    CHECK(!noah_profile_autocorrect_v1_fold('\'', &folded));
    CHECK(!noah_profile_autocorrect_v1_fold('-', &folded));
    CHECK(!noah_profile_autocorrect_v1_fold(0x436u, &folded)); // ж: no host layout types it
    CHECK(!noah_profile_autocorrect_v1_fold(0x10FFFFu, &folded));
}

static bool find(const noah_profile_autocorrect_v1_list_t *list, const char *typo, noah_profile_autocorrect_v1_typo_t *found) {
    return noah_profile_autocorrect_v1_find(list, (const uint8_t *)typo, (uint8_t)strlen(typo), found);
}

static void expect_correction(const noah_profile_autocorrect_v1_list_t *list, uint16_t correction, const char *expected) {
    uint8_t text[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX], length = 0u;
    CHECK(noah_profile_autocorrect_v1_correction(list, correction, text, &length));
    CHECK(length == strlen(expected) && memcmp(text, expected, length) == 0);
}

// The fixture's "small" list, as an engine reads it.
static void test_lookups(void) {
    noah_profile_reader_t              reader = noah_profile_reader_from_memory(small.bytes, small.length);
    noah_profile_autocorrect_v1_list_t list;
    noah_profile_autocorrect_v1_typo_t typo, other;
    uint8_t                            text[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX];

    CHECK(noah_profile_autocorrect_v1_open(&list, &reader, 0u, small.length));
    CHECK(list.corrections == 10u && list.typos == 15u && list.part_typos == 3u);

    // A whole-word typo, and its correction as written.
    CHECK(find(&list, "teh", &typo) && typo.match == NOAH_PROFILE_AUTOCORRECT_V1_MATCH_WHOLE_WORD && typo.length == 3u);
    expect_correction(&list, typo.correction, "the");
    CHECK(find(&list, "iphone", &typo));
    expect_correction(&list, typo.correction, "iPhone");
    CHECK(find(&list, "alot", &typo));
    expect_correction(&list, typo.correction, "a lot");
    CHECK(find(&list, "dosen't", &typo));
    expect_correction(&list, typo.correction, "doesn't");
    CHECK(find(&list, "caf\xc3\xa8", &typo));
    expect_correction(&list, typo.correction, "caf\xc3\xa9");

    // Several typos name one stored correction.
    CHECK(find(&list, "becuase", &typo) && find(&list, "becasue", &other) && other.correction == typo.correction);
    CHECK(find(&list, "beacuse", &other) && other.correction == typo.correction && other.index != typo.index);
    expect_correction(&list, typo.correction, "because");

    // Nothing but a whole typo is found: no prefix, no longer word, no
    // correction, nothing empty or too long.
    CHECK(!find(&list, "te", &typo) && !find(&list, "tehh", &typo) && !find(&list, "the", &typo) && !find(&list, "zzz", &typo) && !find(&list, "a", &typo));
    CHECK(!noah_profile_autocorrect_v1_find(&list, (const uint8_t *)"teh", 0u, &typo));
    CHECK(!noah_profile_autocorrect_v1_find(&list, small.bytes, 33u, &typo));

    // The part-of-word typos, in typo order, each with its match.
    static const struct {
        const char *typo, *correction;
        uint8_t     match;
    } parts[] = {{"mnet", "ment", NOAH_PROFILE_AUTOCORRECT_V1_MATCH_END}, {"nu", "un", NOAH_PROFILE_AUTOCORRECT_V1_MATCH_START}, {"tihng", "thing", NOAH_PROFILE_AUTOCORRECT_V1_MATCH_ANYWHERE}};
    for (uint16_t position = 0u; position < 3u; position++) {
        CHECK(noah_profile_autocorrect_v1_part_at(&list, position, &typo, text));
        CHECK(typo.match == parts[position].match && typo.length == strlen(parts[position].typo) && memcmp(text, parts[position].typo, typo.length) == 0);
        expect_correction(&list, typo.correction, parts[position].correction);
        // A part-of-word typo is found by its bytes too.
        CHECK(find(&list, parts[position].typo, &other) && other.index == typo.index);
    }
    CHECK(!noah_profile_autocorrect_v1_part_at(&list, 3u, &typo, text));
    CHECK(!noah_profile_autocorrect_v1_typo_at(&list, 15u, &typo, text));
    CHECK(!noah_profile_autocorrect_v1_correction(&list, 10u, text, &typo.length));

    // Every typo is found by its own bytes, at its own row.
    for (uint16_t index = 0u; index < list.typos; index++) {
        CHECK(noah_profile_autocorrect_v1_typo_at(&list, index, &typo, text));
        CHECK(noah_profile_autocorrect_v1_find(&list, text, typo.length, &other) && other.index == index);
    }

    // A list at an offset inside a larger reader, as a profile holds it.
    static uint8_t framed[MAX_BYTES];
    memset(framed, 0xA5, sizeof(framed));
    memcpy(&framed[7], small.bytes, small.length);
    reader = noah_profile_reader_from_memory(framed, small.length + 14u);
    CHECK(noah_profile_autocorrect_v1_open(&list, &reader, 7u, small.length));
    CHECK(find(&list, "hte", &typo));
    expect_correction(&list, typo.correction, "the");

    // A range that is not the list's length opens nothing.
    CHECK(!noah_profile_autocorrect_v1_open(&list, &reader, 7u, small.length - 1u) && list.typos == 0u);
    CHECK(!noah_profile_autocorrect_v1_open(&list, &reader, 7u, 7u));
    CHECK(!find(&list, "teh", &typo));
}

// An empty list holds nothing and finds nothing.
static void test_empty_list(void) {
    static const uint8_t               empty[NOAH_PROFILE_AUTOCORRECT_V1_HEADER_SIZE] = {0};
    noah_profile_reader_t              reader = noah_profile_reader_from_memory(empty, sizeof(empty));
    noah_profile_autocorrect_v1_list_t list;
    noah_profile_autocorrect_v1_typo_t typo;
    CHECK(noah_profile_autocorrect_v1_validate(empty, sizeof(empty), NULL) == NOAH_PROFILE_AUTOCORRECT_V1_OK);
    CHECK(noah_profile_autocorrect_v1_open(&list, &reader, 0u, sizeof(empty)));
    CHECK(!find(&list, "teh", &typo) && !noah_profile_autocorrect_v1_part_at(&list, 0u, &typo, NULL));
}

// Every single-byte change to a valid list: validation ends within its step
// bound either way, and every lookup on the changed bytes stays inside the
// payload (the counted reader refuses anything else) and ends.
static void test_malformed_lists_stay_bounded(void) {
    static uint8_t                      bytes[MAX_BYTES];
    noah_profile_autocorrect_v1_error_t error;
    size_t                              refused = 0u;
    static const uint8_t                changes[] = {0x00u, 0x01u, 0x7Fu, 0x80u, 0xFFu};
    for (size_t at = 0u; at < small.length; at++) {
        for (size_t change = 0u; change < sizeof(changes); change++) {
            memcpy(bytes, small.bytes, small.length);
            if (bytes[at] == changes[change]) continue;
            bytes[at] = changes[change];
            refused += validate_bounded(bytes, small.length, &error) != NOAH_PROFILE_AUTOCORRECT_V1_OK;

            counted_t                          counted = {.bytes = bytes, .length = small.length};
            noah_profile_reader_t              reader  = {.read = counted_read, .context = &counted, .length = small.length};
            noah_profile_autocorrect_v1_list_t list;
            noah_profile_autocorrect_v1_typo_t typo;
            uint8_t                            text[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX], length;
            if (!noah_profile_autocorrect_v1_open(&list, &reader, 0u, small.length)) continue;
            counted.calls = 0u;
            (void)find(&list, "teh", &typo);
            (void)find(&list, "zzzzzzzz", &typo);
            // Eleven rows at most, a row and its text each.
            CHECK(counted.calls <= 2u * 2u * 11u);
            for (uint16_t index = 0u; index < list.typos; index++) (void)noah_profile_autocorrect_v1_typo_at(&list, index, &typo, text);
            for (uint16_t position = 0u; position < list.part_typos; position++) (void)noah_profile_autocorrect_v1_part_at(&list, position, &typo, text);
            for (uint16_t correction = 0u; correction < list.corrections; correction++) (void)noah_profile_autocorrect_v1_correction(&list, correction, text, &length);
        }
    }
    // Nearly every change is refused; a letter changed to another valid,
    // still ordered letter is simply another list.
    CHECK(refused > 4u * small.length);
}

static void test_arguments(void) {
    noah_profile_autocorrect_v1_error_t      error;
    noah_profile_autocorrect_v1_validation_t state  = {0};
    noah_profile_reader_t                    reader = noah_profile_reader_from_memory(small.bytes, small.length);
    CHECK(noah_profile_autocorrect_v1_validate(NULL, 8u, &error) == NOAH_PROFILE_AUTOCORRECT_V1_INVALID_ARGUMENT);
    CHECK(noah_profile_autocorrect_v1_validation_step(NULL, &reader, 0u, small.length, &error) == NOAH_PROFILE_AUTOCORRECT_V1_REJECTED && error.code == NOAH_PROFILE_AUTOCORRECT_V1_INVALID_ARGUMENT);
    CHECK(noah_profile_autocorrect_v1_validation_step(&state, NULL, 0u, small.length, &error) == NOAH_PROFILE_AUTOCORRECT_V1_REJECTED);
    // A reader that ends before the payload does is a read error, not a pass.
    reader = noah_profile_reader_from_memory(small.bytes, small.length - 1u);
    do {
    } while (noah_profile_autocorrect_v1_validation_step(&state, &reader, 0u, small.length, &error) == NOAH_PROFILE_AUTOCORRECT_V1_VALIDATING);
    CHECK(error.code == NOAH_PROFILE_AUTOCORRECT_V1_READ_ERROR);
    CHECK(!noah_profile_autocorrect_v1_open(NULL, &reader, 0u, small.length));
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    size_t vectors = check_vectors(argv[1]);
    test_fold();
    test_lookups();
    test_empty_list();
    test_malformed_lists_stay_bounded();
    test_arguments();
    printf("autocorrect domain v1 tests passed (%zu vectors)\n", vectors);
    return 0;
}
