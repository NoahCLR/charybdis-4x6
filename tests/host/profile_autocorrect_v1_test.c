// The autocorrect list domain (0x80 v2): the golden vectors of
// tests/fixtures/autocorrect_domain_v2.json against the validator, the
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
// read_vector's field width, a literal fscanf needs, held to the buffer.
_Static_assert(2u * MAX_BYTES - 4u == 73776u, "the hex field width follows the largest list");

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
    [NOAH_PROFILE_AUTOCORRECT_V1_INVALID_KIND] = "INVALID_KIND",
    [NOAH_PROFILE_AUTOCORRECT_V1_INVALID_TEXT] = "INVALID_TEXT",
    [NOAH_PROFILE_AUTOCORRECT_V1_ORDER] = "ORDER",
    [NOAH_PROFILE_AUTOCORRECT_V1_INVALID_MATCH] = "INVALID_MATCH",
    [NOAH_PROFILE_AUTOCORRECT_V1_INVALID_REFERENCE] = "INVALID_REFERENCE",
    [NOAH_PROFILE_AUTOCORRECT_V1_FIRST_TYPO] = "FIRST_TYPO",
    [NOAH_PROFILE_AUTOCORRECT_V1_NOT_A_WORD] = "NOT_A_WORD",
    [NOAH_PROFILE_AUTOCORRECT_V1_NOT_FOLDED] = "NOT_FOLDED",
    [NOAH_PROFILE_AUTOCORRECT_V1_TYPO_IS_CORRECTION] = "TYPO_IS_CORRECTION",
    [NOAH_PROFILE_AUTOCORRECT_V1_PART_INDEX] = "PART_INDEX",
    [NOAH_PROFILE_AUTOCORRECT_V1_INVALID_MACRO] = "INVALID_MACRO",
    [NOAH_PROFILE_AUTOCORRECT_V1_LEADER_INDEX] = "LEADER_INDEX",
};
// The codes travel as a validation error's field id.
_Static_assert(NOAH_PROFILE_AUTOCORRECT_V1_INVALID_LENGTH == 3 && NOAH_PROFILE_AUTOCORRECT_V1_ORDER == 9 && NOAH_PROFILE_AUTOCORRECT_V1_PART_INDEX == 16 && NOAH_PROFILE_AUTOCORRECT_V1_INVALID_MACRO == 17 && NOAH_PROFILE_AUTOCORRECT_V1_LEADER_INDEX == 18, "refusal codes are fixed");

// One line of the converted fixture: code, table, row (-1 for none), name, hex.
static bool read_vector(FILE *file, vector_t *vector) {
    static char hex[2u * MAX_BYTES + 2u];
    if (fscanf(file, "%31s %d %d %63s %73776s", vector->code, &vector->table, &vector->row, vector->name, hex) != 5) return false;
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
    // A correction's row, its first typo's and its text, twenty bytes a
    // read; and for a typo its row, two reads of text, its correction's row,
    // two reads of that text and a part-index row.
    const size_t most_steps = 1u + 2u * NOAH_PROFILE_AUTOCORRECT_V1_MAX_CORRECTIONS + (NOAH_PROFILE_AUTOCORRECT_V1_MAX_TEXT_BYTES / NOAH_PROFILE_AUTOCORRECT_V1_STEP_READ_MAX + NOAH_PROFILE_AUTOCORRECT_V1_MAX_CORRECTIONS) + 7u * NOAH_PROFILE_AUTOCORRECT_V1_MAX_TYPOS;
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

static vector_t small, long_list, macros, leader;

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
        seen[result] = true;
        if (strcmp(vector.name, "small") == 0) small = vector;
        if (strcmp(vector.name, "long") == 0) long_list = vector;
        if (strcmp(vector.name, "macros") == 0) macros = vector;
        if (strcmp(vector.name, "leader") == 0) leader = vector;
        count++;
    }
    fclose(file);
    // Every refusal a client can cause has a vector.
    for (size_t code = NOAH_PROFILE_AUTOCORRECT_V1_INVALID_LENGTH; code < sizeof(seen); code++) CHECK(seen[code]);
    CHECK(seen[NOAH_PROFILE_AUTOCORRECT_V1_OK] && small.length != 0u && long_list.length != 0u && macros.length != 0u && leader.length != 0u);
    return count;
}

// A small letter's capital is the one word character that folds to it: held
// against every row of the word table, in both directions.
extern const uint16_t noah_profile_autocorrect_v1_words[][2];
extern const uint16_t noah_profile_autocorrect_v1_word_count;

static void test_capital(void) {
    uint32_t capital;
    unsigned capitals = 0u;

    for (uint16_t index = 0u; index < noah_profile_autocorrect_v1_word_count; index++) {
        uint16_t character = noah_profile_autocorrect_v1_words[index][0], folded = noah_profile_autocorrect_v1_words[index][1];
        bool     is_folded_to = false;

        if (folded != character) {
            capitals++;
            CHECK(noah_profile_autocorrect_v1_capital(folded, &capital) && capital == character);
            // A capital has no capital of its own.
            CHECK(!noah_profile_autocorrect_v1_capital(character, &capital));
            continue;
        }
        for (uint16_t other = 0u; other < noah_profile_autocorrect_v1_word_count; other++)
            is_folded_to = is_folded_to || (noah_profile_autocorrect_v1_words[other][1] == character && noah_profile_autocorrect_v1_words[other][0] != character);
        CHECK(noah_profile_autocorrect_v1_capital(character, NULL) == is_folded_to);
    }
    CHECK(capitals > 26u);
    CHECK(noah_profile_autocorrect_v1_capital('a', &capital) && capital == 'A');
    CHECK(noah_profile_autocorrect_v1_capital(0xE9u, &capital) && capital == 0xC9u); // é, É
    CHECK(!noah_profile_autocorrect_v1_capital('7', &capital));
    CHECK(!noah_profile_autocorrect_v1_capital(0xFB01u, &capital)); // ﬁ: a letter without one
    CHECK(!noah_profile_autocorrect_v1_capital(' ', &capital));
    CHECK(!noah_profile_autocorrect_v1_capital(0x10FFFFu, &capital));
}

// One character of a list's text at a time, as the engine reads a correction.
static void test_character(void) {
    static const uint8_t text[] = {'a', 0xC3u, 0xA9u, 0xE2u, 0x82u, 0xACu, 0xF0u, 0x9Fu, 0x98u, 0x80u};
    uint32_t             character;

    CHECK(noah_profile_autocorrect_v1_character(&text[0], 10u, &character) == 1u && character == 'a');
    CHECK(noah_profile_autocorrect_v1_character(&text[1], 9u, &character) == 2u && character == 0xE9u);
    CHECK(noah_profile_autocorrect_v1_character(&text[3], 7u, &character) == 3u && character == 0x20ACu);
    CHECK(noah_profile_autocorrect_v1_character(&text[6], 4u, &character) == 4u && character == 0x1F600u);
    // Cut off, a continuation byte alone, and a control character are none.
    CHECK(noah_profile_autocorrect_v1_character(&text[3], 2u, &character) == 0u);
    CHECK(noah_profile_autocorrect_v1_character(&text[2], 1u, &character) == 0u);
    CHECK(noah_profile_autocorrect_v1_character((const uint8_t *)"\n", 1u, &character) == 0u);
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
    uint8_t                                  text[NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_MAX];
    noah_profile_autocorrect_v1_correction_t stored;
    CHECK(noah_profile_autocorrect_v1_correction(list, correction, &stored, text));
    CHECK(stored.kind == NOAH_PROFILE_AUTOCORRECT_V1_KIND_TEXT && stored.length == strlen(expected) && memcmp(text, expected, stored.length) == 0);
}

static void expect_macro(const noah_profile_autocorrect_v1_list_t *list, const char *typo, uint16_t slot) {
    noah_profile_autocorrect_v1_typo_t       found;
    noah_profile_autocorrect_v1_correction_t stored;
    CHECK(noah_profile_autocorrect_v1_find(list, (const uint8_t *)typo, (uint8_t)strlen(typo), &found) && found.match == NOAH_PROFILE_AUTOCORRECT_V1_MATCH_WHOLE_WORD);
    // A macro correction has a slot and no text, asked for or not.
    CHECK(noah_profile_autocorrect_v1_correction(list, found.correction, &stored, NULL));
    CHECK(stored.kind == NOAH_PROFILE_AUTOCORRECT_V1_KIND_MACRO && stored.macro == slot && stored.length == 0u);
}

// A correction of 128 bytes is read whole, and macro corrections give their
// slots, after the text ones.
static void test_long_and_macro_corrections(void) {
    noah_profile_reader_t                    reader = noah_profile_reader_from_memory(long_list.bytes, long_list.length);
    noah_profile_autocorrect_v1_list_t       list;
    noah_profile_autocorrect_v1_typo_t       typo;
    uint8_t                                  text[NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_MAX];
    noah_profile_autocorrect_v1_correction_t stored;

    CHECK(noah_profile_autocorrect_v1_open(&list, &reader, 0u, long_list.length));
    CHECK(noah_profile_autocorrect_v1_find(&list, (const uint8_t *)"sigoff", 6u, &typo));
    CHECK(noah_profile_autocorrect_v1_correction(&list, typo.correction, &stored, text));
    CHECK(stored.length == NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_MAX && memcmp(text, "Kind regards, ", 14u) == 0 && text[127] == '0');
    CHECK(noah_profile_autocorrect_v1_find(&list, (const uint8_t *)"pnumber", 7u, &typo));
    expect_correction(&list, typo.correction, "+31 6 1234 5678");

    reader = noah_profile_reader_from_memory(macros.bytes, macros.length);
    CHECK(noah_profile_autocorrect_v1_open(&list, &reader, 0u, macros.length));
    CHECK(list.corrections == 4u && list.typos == 5u && list.part_typos == 0u);
    expect_macro(&list, "addr", 3u);
    expect_macro(&list, "adres", 3u);
    expect_macro(&list, "sigfull", 127u);
    CHECK(noah_profile_autocorrect_v1_find(&list, (const uint8_t *)"myemail", 7u, &typo));
    expect_correction(&list, typo.correction, "sam@example.com");
}

static bool after_leader(const noah_profile_autocorrect_v1_list_t *list, const char *held, noah_profile_autocorrect_v1_leader_t *found) {
    return noah_profile_autocorrect_v1_leader(list, (const uint8_t *)held, (uint8_t)strlen(held), found);
}

// What the leader holds, against the fixture's "leader" list (D-F71): the
// exact typo of a correction that fires after the leader, if any, and whether
// a longer one begins with what is held. A typo of a correction that fires
// only as a word ends is never one, and a part-of-word typo of one that fires
// both ways is in both indexes.
static void test_leader(void) {
    noah_profile_reader_t                reader = noah_profile_reader_from_memory(leader.bytes, leader.length);
    noah_profile_autocorrect_v1_list_t   list;
    noah_profile_autocorrect_v1_leader_t found;
    noah_profile_autocorrect_v1_typo_t   typo;
    uint8_t                              text[NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_MAX];
    static const struct {
        const char *held;
        bool        exact, longer;
    } cases[] = {
        {"a", false, true},  {"ab", false, true},   {"abc", true, true},     {"abcd", true, false},  {"abcde", false, false},
        {"ad", false, false}, {"adn", false, false}, {"k", false, true},      {"kr", true, false},    {"k ", false, false},
        {"pn", true, false}, {"pnx", false, false}, {"s", false, true},      {"sig", true, true},    {"sigg", true, false},
        {"t", false, true},  {"te", false, true},   {"teh", true, false},    {"h", false, true},     {"hte", true, false},
        {"r", false, true},  {"recieve", true, false}, {"\xc3\xa9", false, true}, {"\xc3\xa9\xc3\xa9", true, false},
        {"z", false, false}, {" ", false, false},
    };
    static const char *const order[] = {"abc", "abcd", "hte", "kr", "pn", "recieve", "sig", "sigg", "teh", "\xc3\xa9\xc3\xa9"};
    noah_profile_autocorrect_v1_correction_t stored;

    CHECK(noah_profile_autocorrect_v1_open(&list, &reader, 0u, leader.length));
    CHECK(list.corrections == 9u && list.typos == 11u && list.part_typos == 1u && list.leader_typos == 10u);
    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); i++) {
        CHECK(after_leader(&list, cases[i].held, &found));
        if (found.exact != cases[i].exact || found.longer != cases[i].longer) {
            fprintf(stderr, "after the leader \"%s\": exact %d longer %d\n", cases[i].held, found.exact, found.longer);
            exit(1);
        }
        if (found.exact) CHECK(found.typo.length == strlen(cases[i].held));
    }
    CHECK(after_leader(&list, "pn", &found));
    expect_correction(&list, found.typo.correction, "+31 6 1234 5678");
    CHECK(noah_profile_autocorrect_v1_correction(&list, found.typo.correction, &stored, NULL) && stored.trigger == NOAH_PROFILE_AUTOCORRECT_V1_TRIGGER_LEADER);
    // A correction that fires both ways: its whole-word typos and its
    // part-of-word one, whole after the leader.
    CHECK(after_leader(&list, "teh", &found));
    expect_correction(&list, found.typo.correction, "the");
    CHECK(noah_profile_autocorrect_v1_correction(&list, found.typo.correction, &stored, NULL) && stored.trigger == NOAH_PROFILE_AUTOCORRECT_V1_TRIGGER_BOTH);
    CHECK(after_leader(&list, "recieve", &found) && found.typo.match == NOAH_PROFILE_AUTOCORRECT_V1_MATCH_START);
    expect_correction(&list, found.typo.correction, "receive");
    CHECK(noah_profile_autocorrect_v1_part_at(&list, 0u, &typo, text) && typo.index == found.typo.index && !noah_profile_autocorrect_v1_part_at(&list, 1u, &typo, text));
    // A macro that fires after the leader, by either of its typos.
    CHECK(after_leader(&list, "sig", &found));
    CHECK(noah_profile_autocorrect_v1_correction(&list, found.typo.correction, &stored, NULL) && stored.kind == NOAH_PROFILE_AUTOCORRECT_V1_KIND_MACRO && stored.macro == 7u && stored.trigger == NOAH_PROFILE_AUTOCORRECT_V1_TRIGGER_LEADER);
    expect_macro(&list, "sigg", 7u);
    // One that fires as a word ends is found by its bytes, never by the leader.
    CHECK(find(&list, "adn", &typo) && typo.match == NOAH_PROFILE_AUTOCORRECT_V1_MATCH_WHOLE_WORD);
    CHECK(noah_profile_autocorrect_v1_correction(&list, typo.correction, &stored, NULL) && stored.trigger == NOAH_PROFILE_AUTOCORRECT_V1_TRIGGER_WORD);
    // In typo order, and only these.
    for (uint16_t position = 0u; position < 10u; position++) {
        CHECK(noah_profile_autocorrect_v1_leader_at(&list, position, &typo, text));
        CHECK(typo.length == strlen(order[position]) && memcmp(text, order[position], typo.length) == 0);
    }
    CHECK(!noah_profile_autocorrect_v1_leader_at(&list, 10u, &typo, text));
    // Nothing held, or more than a typo can be, finds nothing.
    CHECK(!noah_profile_autocorrect_v1_leader(&list, (const uint8_t *)"a", 0u, &found));
    CHECK(!noah_profile_autocorrect_v1_leader(&list, leader.bytes, 33u, &found));

    // The small list has none: whatever is held starts nothing.
    reader = noah_profile_reader_from_memory(small.bytes, small.length);
    CHECK(noah_profile_autocorrect_v1_open(&list, &reader, 0u, small.length) && list.leader_typos == 0u);
    CHECK(after_leader(&list, "t", &found) && !found.exact && !found.longer);
}

// The fixture's "small" list, as an engine reads it.
static void test_lookups(void) {
    noah_profile_reader_t              reader = noah_profile_reader_from_memory(small.bytes, small.length);
    noah_profile_autocorrect_v1_list_t list;
    noah_profile_autocorrect_v1_typo_t       typo, other;
    uint8_t                                  text[NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_MAX];
    noah_profile_autocorrect_v1_correction_t stored;

    CHECK(noah_profile_autocorrect_v1_open(&list, &reader, 0u, small.length));
    CHECK(list.corrections == 10u && list.typos == 15u && list.part_typos == 3u && list.leader_typos == 0u);

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
    CHECK(!noah_profile_autocorrect_v1_correction(&list, 10u, &stored, text));

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
    noah_profile_autocorrect_v1_error_t error;
    CHECK(validate_bounded(empty, sizeof(empty), &error) == NOAH_PROFILE_AUTOCORRECT_V1_OK);
    CHECK(noah_profile_autocorrect_v1_open(&list, &reader, 0u, sizeof(empty)));
    CHECK(!find(&list, "teh", &typo) && !noah_profile_autocorrect_v1_part_at(&list, 0u, &typo, NULL));
}

// Whether a list fires as a word ends: a correction whose trigger is not the
// leader alone. Corrections are read in row order, so a macro that fires as
// typed after text that fires only after the leader still counts.
static void test_fires_as_typed(void) {
    // Header, then rows: "x" after the leader alone, typed "y".
    static const uint8_t leader_only[] = {
        1u, 0u, 1u, 0u, 0u, 0u, 1u, 0u, 2u, 0u,
        0u, 0u, 1u, 2u, 0u, 0u,
        1u, 0u, 1u, 0u, 0u, 0u,
        0u, 0u,
        'x', 'y',
    };
    // "x" after the leader alone, typed "a", then macro slot 3 as a word
    // ends, typed "b".
    static const uint8_t macro_as_typed[] = {
        2u, 0u, 2u, 0u, 0u, 0u, 1u, 0u, 3u, 0u,
        0u, 0u, 1u, 2u, 0u, 0u,
        3u, 0u, 0u, 1u, 1u, 0u,
        1u, 0u, 1u, 0u, 0u, 0u,
        2u, 0u, 1u, 0u, 1u, 0u,
        0u, 0u,
        'x', 'a', 'b',
    };
    static const uint8_t                empty[NOAH_PROFILE_AUTOCORRECT_V1_HEADER_SIZE] = {0};
    noah_profile_autocorrect_v1_list_t  list;
    noah_profile_autocorrect_v1_error_t error;
    static const struct {
        const uint8_t *bytes;
        size_t         length;
        bool           fires;
    } cases[] = {
        {empty, sizeof(empty), false},
        {leader_only, sizeof(leader_only), false},
        {macro_as_typed, sizeof(macro_as_typed), true},
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); i++) {
        noah_profile_reader_t reader = noah_profile_reader_from_memory(cases[i].bytes, cases[i].length);
        CHECK(validate_bounded(cases[i].bytes, cases[i].length, &error) == NOAH_PROFILE_AUTOCORRECT_V1_OK);
        CHECK(noah_profile_autocorrect_v1_open(&list, &reader, 0u, cases[i].length));
        CHECK(noah_profile_autocorrect_v1_fires_as_typed(&list) == cases[i].fires);
    }
    // The fixture's lists: the small one fires as typed only, the leader
    // one has a correction that fires as a word ends among its leader ones.
    for (size_t i = 0u; i < 2u; i++) {
        const vector_t       *vector = i ? &leader : &small;
        noah_profile_reader_t reader = noah_profile_reader_from_memory(vector->bytes, vector->length);
        CHECK(noah_profile_autocorrect_v1_open(&list, &reader, 0u, vector->length) && noah_profile_autocorrect_v1_fires_as_typed(&list));
    }
    CHECK(!noah_profile_autocorrect_v1_fires_as_typed(NULL));
}

// Every single-byte change to a valid list: validation ends within its step
// bound either way, and every lookup on the changed bytes stays inside the
// payload (the counted reader refuses anything else) and ends.
static void malformed_lists_stay_bounded(const vector_t *valid) {
    static uint8_t                      bytes[MAX_BYTES];
    noah_profile_autocorrect_v1_error_t error;
    size_t                              refused = 0u;
    static const uint8_t                changes[] = {0x00u, 0x01u, 0x7Fu, 0x80u, 0xFFu};
    for (size_t at = 0u; at < valid->length; at++) {
        for (size_t change = 0u; change < sizeof(changes); change++) {
            memcpy(bytes, valid->bytes, valid->length);
            if (bytes[at] == changes[change]) continue;
            bytes[at] = changes[change];
            refused += validate_bounded(bytes, valid->length, &error) != NOAH_PROFILE_AUTOCORRECT_V1_OK;

            counted_t                          counted = {.bytes = bytes, .length = valid->length};
            noah_profile_reader_t              reader  = {.read = counted_read, .context = &counted, .length = valid->length};
            noah_profile_autocorrect_v1_list_t list;
            noah_profile_autocorrect_v1_typo_t typo;
            uint8_t                                  text[NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_MAX];
            noah_profile_autocorrect_v1_correction_t stored;
            noah_profile_autocorrect_v1_leader_t     found;
            if (!noah_profile_autocorrect_v1_open(&list, &reader, 0u, valid->length)) continue;
            counted.calls = 0u;
            (void)find(&list, "teh", &typo);
            (void)find(&list, "zzzzzzzz", &typo);
            // Eleven rows at most, a row and its text each.
            CHECK(counted.calls <= 2u * 2u * 11u);
            counted.calls = 0u;
            (void)after_leader(&list, "ab", &found);
            // Thirteen positions at most, an index row, a row and its text each.
            CHECK(counted.calls <= 3u * 13u);
            for (uint16_t index = 0u; index < list.typos; index++) (void)noah_profile_autocorrect_v1_typo_at(&list, index, &typo, text);
            for (uint16_t position = 0u; position < list.part_typos; position++) (void)noah_profile_autocorrect_v1_part_at(&list, position, &typo, text);
            for (uint16_t position = 0u; position < list.leader_typos; position++) (void)noah_profile_autocorrect_v1_leader_at(&list, position, &typo, text);
            for (uint16_t correction = 0u; correction < list.corrections; correction++) (void)noah_profile_autocorrect_v1_correction(&list, correction, &stored, text);
        }
    }
    // Nearly every change is refused; a letter changed to another valid,
    // still ordered letter is simply another list.
    CHECK(refused > 4u * valid->length);
}

static void test_malformed_lists_stay_bounded(void) {
    malformed_lists_stay_bounded(&small);
    malformed_lists_stay_bounded(&leader);
}

static void test_arguments(void) {
    noah_profile_autocorrect_v1_error_t      error;
    noah_profile_autocorrect_v1_validation_t state  = {0};
    noah_profile_reader_t                    reader = noah_profile_reader_from_memory(small.bytes, small.length);
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
    test_capital();
    test_character();
    test_lookups();
    test_long_and_macro_corrections();
    test_leader();
    test_empty_list();
    test_fires_as_typed();
    test_malformed_lists_stay_bounded();
    test_arguments();
    printf("autocorrect domain v2 tests passed (%zu vectors)\n", vectors);
    return 0;
}
