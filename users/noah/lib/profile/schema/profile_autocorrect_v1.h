// ───────────────────────────────────────────────────────────────────────────
// Autocorrect List — profile domain 0x80 version 1
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "profile_reader.h"
#include "profile_versions.h"

// The list as written (D-F68): each correction once, and every typo with
// where it matches and the correction it names. Integers little-endian:
//
//   header        corrections u16, typos u16, part-of-word typos u16,
//                 text bytes u16
//   corrections   6 bytes each: text offset u16, length u8, zero,
//                 first typo u16
//   typos         6 bytes each: text offset u16, length u8, match u8,
//                 correction u16
//   part index    one u16 typo index per typo whose match is not whole word,
//                 ascending
//   text          every correction's bytes, then every typo's, in row order,
//                 back to back
//
// Corrections and typos each ascend strictly by their UTF-8 bytes, so a list
// has one encoding, a typo is found by binary search, and a typo cannot name
// two corrections. A correction's first typo is the lowest typo row naming
// it, which is how the validator proves every correction is named without
// remembering them all. The contract is
// docs/spec/profile-wire-v1/autocorrect-domain.md.
enum {
    NOAH_PROFILE_AUTOCORRECT_V1_VERSION             = NOAH_PROFILE_DOMAIN_VERSION_AUTOCORRECT,
    NOAH_PROFILE_AUTOCORRECT_V1_HEADER_SIZE         = 8u,
    NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_ROW_SIZE = 6u,
    NOAH_PROFILE_AUTOCORRECT_V1_TYPO_ROW_SIZE       = 6u,
    NOAH_PROFILE_AUTOCORRECT_V1_PART_ROW_SIZE       = 2u,
    // Counted ceilings (D-F14). A realistic full list, 1,024 typos and as
    // many corrections of ten bytes each, fills the text exactly.
    NOAH_PROFILE_AUTOCORRECT_V1_MAX_CORRECTIONS = 1024u,
    NOAH_PROFILE_AUTOCORRECT_V1_MAX_TYPOS       = 1024u,
    NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX        = 32u,
    NOAH_PROFILE_AUTOCORRECT_V1_MAX_TEXT_BYTES  = 20480u,
    NOAH_PROFILE_AUTOCORRECT_V1_MAX_SIZE        = NOAH_PROFILE_AUTOCORRECT_V1_HEADER_SIZE + NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_ROW_SIZE * NOAH_PROFILE_AUTOCORRECT_V1_MAX_CORRECTIONS + (NOAH_PROFILE_AUTOCORRECT_V1_TYPO_ROW_SIZE + NOAH_PROFILE_AUTOCORRECT_V1_PART_ROW_SIZE) * NOAH_PROFILE_AUTOCORRECT_V1_MAX_TYPOS + NOAH_PROFILE_AUTOCORRECT_V1_MAX_TEXT_BYTES,
    NOAH_PROFILE_AUTOCORRECT_V1_STEP_READ_MAX   = 20u,
    NOAH_PROFILE_AUTOCORRECT_V1_APOSTROPHE      = 0x27u,
    NOAH_PROFILE_AUTOCORRECT_V1_LOCATION_NONE   = 0xFFFFu,
    // Regression policies for the 32-bit target's state, not SRAM limits.
    NOAH_PROFILE_AUTOCORRECT_V1_EMBEDDED_VALIDATION_BUDGET = 64u,
    NOAH_PROFILE_AUTOCORRECT_V1_EMBEDDED_LIST_BUDGET       = 24u,
};

// Where a typo matches a word.
typedef enum {
    NOAH_PROFILE_AUTOCORRECT_V1_MATCH_WHOLE_WORD = 0u,
    NOAH_PROFILE_AUTOCORRECT_V1_MATCH_START,
    NOAH_PROFILE_AUTOCORRECT_V1_MATCH_END,
    NOAH_PROFILE_AUTOCORRECT_V1_MATCH_ANYWHERE,
} noah_profile_autocorrect_v1_match_t;

// The tables an error names.
enum {
    NOAH_PROFILE_AUTOCORRECT_V1_TABLE_CORRECTIONS = 0u,
    NOAH_PROFILE_AUTOCORRECT_V1_TABLE_TYPOS,
    NOAH_PROFILE_AUTOCORRECT_V1_TABLE_PART_INDEX,
};

// Why a list is refused. The numbers travel as a validation error's field
// id, so they never change.
typedef enum {
    NOAH_PROFILE_AUTOCORRECT_V1_OK = 0u,
    NOAH_PROFILE_AUTOCORRECT_V1_INVALID_ARGUMENT,
    NOAH_PROFILE_AUTOCORRECT_V1_READ_ERROR,
    // The domain is not the length its header counts.
    NOAH_PROFILE_AUTOCORRECT_V1_INVALID_LENGTH,
    // A count or the text is over its ceiling.
    NOAH_PROFILE_AUTOCORRECT_V1_CAPACITY_EXCEEDED,
    // A text does not begin where the one before it ended.
    NOAH_PROFILE_AUTOCORRECT_V1_INVALID_OFFSET,
    // A correction or typo is empty or over 32 bytes.
    NOAH_PROFILE_AUTOCORRECT_V1_INVALID_TEXT_LENGTH,
    NOAH_PROFILE_AUTOCORRECT_V1_RESERVED,
    // Not canonical UTF-8, or a control character.
    NOAH_PROFILE_AUTOCORRECT_V1_INVALID_TEXT,
    // Not above the row before it: out of order, or stored twice.
    NOAH_PROFILE_AUTOCORRECT_V1_ORDER,
    NOAH_PROFILE_AUTOCORRECT_V1_INVALID_MATCH,
    // A correction or typo index past its table.
    NOAH_PROFILE_AUTOCORRECT_V1_INVALID_REFERENCE,
    // A correction's first typo does not name it (no typo may), or a lower
    // typo does.
    NOAH_PROFILE_AUTOCORRECT_V1_FIRST_TYPO,
    // A typo holds something other than word characters and apostrophes
    // between them.
    NOAH_PROFILE_AUTOCORRECT_V1_NOT_A_WORD,
    NOAH_PROFILE_AUTOCORRECT_V1_NOT_FOLDED,
    NOAH_PROFILE_AUTOCORRECT_V1_TYPO_IS_CORRECTION,
    // The part index is not exactly the part-of-word typos, in order.
    NOAH_PROFILE_AUTOCORRECT_V1_PART_INDEX,
} noah_profile_autocorrect_v1_result_t;

typedef struct {
    noah_profile_autocorrect_v1_result_t code;
    size_t                               offset; // in the payload
    uint8_t                              table;  // UINT8_MAX for the header
    uint16_t                             row;    // LOCATION_NONE for the header
} noah_profile_autocorrect_v1_error_t;

// Word characters: every letter and digit a host layout types, each with the
// character it folds to (itself, for a small letter or a digit), generated
// from the host layouts into profile_autocorrect_words_v1.c. False for any
// other character. A typo holds folded characters only, so a typed word is
// folded with this before it is looked up.
bool noah_profile_autocorrect_v1_fold(uint32_t character, uint32_t *folded);
// A small letter's capital: the one word character that folds to it. False
// for a capital, a digit and a letter without one.
bool noah_profile_autocorrect_v1_capital(uint32_t character, uint32_t *capital);

// ── Validation ──────────────────────────────────────────────────────────────

// Caller-owned, zero-initialized before the first step. `text` is the text
// being read and then the row before the next one, which is all ordering
// needs.
typedef struct {
    uint8_t  text[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX];
    uint32_t text_offset; // text bytes consumed: where the next text begins
    uint16_t corrections, typos, part_typos, text_bytes;
    uint16_t row;  // in the table being walked
    uint16_t part; // part-index rows consumed
    uint16_t link; // the correction row's first typo, or the typo row's correction
    uint8_t  phase;
    uint8_t  length, have;     // the current text and how much of it is read
    uint8_t  previous_length;  // the row before it, zero for a table's first
    uint8_t  match;
    bool     above;            // the bytes read so far already sort above it
} noah_profile_autocorrect_v1_validation_t;

typedef enum {
    NOAH_PROFILE_AUTOCORRECT_V1_VALIDATING = 0u,
    NOAH_PROFILE_AUTOCORRECT_V1_COMPLETE,
    NOAH_PROFILE_AUTOCORRECT_V1_REJECTED,
} noah_profile_autocorrect_v1_validation_step_t;

// One reader operation of at most STEP_READ_MAX bytes a step, and never more
// steps than the header's counts allow, whatever the bytes say. `base` is the
// payload's offset in the reader, `length` its length.
noah_profile_autocorrect_v1_validation_step_t noah_profile_autocorrect_v1_validation_step(noah_profile_autocorrect_v1_validation_t *state, const noah_profile_reader_t *reader, size_t base, size_t length, noah_profile_autocorrect_v1_error_t *error);

// ── Lookups ─────────────────────────────────────────────────────────────────

// A list read in place: its range in a reader and its header's counts. Every
// lookup bounds what it reads by these, so no stored offset, length or index
// reaches outside the payload, and none loops.
typedef struct {
    noah_profile_reader_t reader;
    size_t                base;
    uint16_t              corrections, typos, part_typos, text_bytes;
} noah_profile_autocorrect_v1_list_t;

typedef struct {
    uint16_t index;      // its row
    uint16_t correction; // the correction row it names
    uint8_t  match;      // noah_profile_autocorrect_v1_match_t
    uint8_t  length;     // bytes of UTF-8
} noah_profile_autocorrect_v1_typo_t;

// False, leaving an empty list, unless the header's counts are within the
// ceilings and make exactly `length` bytes.
bool noah_profile_autocorrect_v1_open(noah_profile_autocorrect_v1_list_t *list, const noah_profile_reader_t *reader, size_t base, size_t length);
// A typo by its row; `text`, when given, receives its bytes.
bool noah_profile_autocorrect_v1_typo_at(const noah_profile_autocorrect_v1_list_t *list, uint16_t index, noah_profile_autocorrect_v1_typo_t *typo, uint8_t text[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX]);
// The typo that is exactly these folded bytes, whatever its match: at most
// eleven rows read.
bool noah_profile_autocorrect_v1_find(const noah_profile_autocorrect_v1_list_t *list, const uint8_t *folded, uint8_t length, noah_profile_autocorrect_v1_typo_t *typo);
// The part-of-word typos, by position 0..part_typos-1 in typo order.
bool noah_profile_autocorrect_v1_part_at(const noah_profile_autocorrect_v1_list_t *list, uint16_t position, noah_profile_autocorrect_v1_typo_t *typo, uint8_t text[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX]);
// A correction's bytes, as written.
bool noah_profile_autocorrect_v1_correction(const noah_profile_autocorrect_v1_list_t *list, uint16_t correction, uint8_t text[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX], uint8_t *length);
