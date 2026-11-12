#include "profile_autocorrect_v1.h"

#include <string.h>

extern const uint16_t noah_profile_autocorrect_v1_words[][2];
extern const uint16_t noah_profile_autocorrect_v1_word_count;
extern const uint16_t noah_profile_autocorrect_v1_capitals[][2];
extern const uint16_t noah_profile_autocorrect_v1_capital_count;

enum {
    PHASE_HEADER = 0u,
    PHASE_CORRECTION_ROW,
    PHASE_CORRECTION_TEXT,
    PHASE_CORRECTION_FIRST_TYPO,
    PHASE_TYPO_ROW,
    PHASE_TYPO_TEXT,
    PHASE_TYPO_CORRECTION,
    PHASE_TYPO_CORRECTION_TEXT,
    PHASE_TYPO_INDEXES, // between steps never: where the typo's index rows are is decided at once
    PHASE_TYPO_PART,
    PHASE_TYPO_LEADER,
    PHASE_COMPLETE,
};

// Correction and typo rows share one size and the place of their text
// offset, length, kind or match, and 16-bit link.
#define ROW_SIZE NOAH_PROFILE_AUTOCORRECT_V1_TYPO_ROW_SIZE
#define NO_TABLE UINT8_MAX

_Static_assert((unsigned)NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_ROW_SIZE == (unsigned)NOAH_PROFILE_AUTOCORRECT_V1_TYPO_ROW_SIZE, "correction and typo rows share one size");
_Static_assert(NOAH_PROFILE_AUTOCORRECT_V1_MAX_TEXT_BYTES <= UINT16_MAX && NOAH_PROFILE_AUTOCORRECT_V1_MAX_SIZE <= UINT16_MAX, "text offsets are 16 bits");
#if UINTPTR_MAX == UINT32_MAX
// Regression policies for the 32-bit target, not SRAM limits: the validation
// state stays well inside the whole-profile validator's domain union, and
// the list handle is its range and its five counts.
_Static_assert(sizeof(noah_profile_autocorrect_v1_validation_t) <= NOAH_PROFILE_AUTOCORRECT_V1_EMBEDDED_VALIDATION_BUDGET, "autocorrect validation state exceeded its firmware regression policy");
_Static_assert(sizeof(noah_profile_autocorrect_v1_list_t) <= NOAH_PROFILE_AUTOCORRECT_V1_EMBEDDED_LIST_BUDGET, "autocorrect list handle exceeded its firmware regression policy");
#endif
_Static_assert((unsigned)NOAH_PROFILE_AUTOCORRECT_V1_TYPO_ROW_SIZE <= (unsigned)NOAH_PROFILE_AUTOCORRECT_V1_STEP_READ_MAX && (unsigned)NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX <= 2u * NOAH_PROFILE_AUTOCORRECT_V1_STEP_READ_MAX && (unsigned)NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_MAX <= 7u * NOAH_PROFILE_AUTOCORRECT_V1_STEP_READ_MAX, "a row is one read, a typo at most two and a correction at most seven");
_Static_assert((unsigned)NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX <= (unsigned)NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_MAX && NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_MAX <= UINT8_MAX, "one buffer holds either text, counted in a byte");

// A character's partner in a generated table of pairs ascending by their
// first character.
static bool paired(const uint16_t table[][2], uint16_t count, uint32_t character, uint32_t *partner) {
    uint16_t low = 0u, high = count;
    while (low < high) {
        uint16_t middle = (uint16_t)(low + (high - low) / 2u);
        uint16_t entry  = table[middle][0];
        if (entry == character) {
            if (partner) *partner = table[middle][1];
            return true;
        }
        if (entry < character) {
            low = (uint16_t)(middle + 1u);
        } else {
            high = middle;
        }
    }
    return false;
}

bool noah_profile_autocorrect_v1_fold(uint32_t character, uint32_t *folded) {
    return paired(noah_profile_autocorrect_v1_words, noah_profile_autocorrect_v1_word_count, character, folded);
}

bool noah_profile_autocorrect_v1_capital(uint32_t character, uint32_t *capital) {
    return paired(noah_profile_autocorrect_v1_capitals, noah_profile_autocorrect_v1_capital_count, character, capital);
}

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | (uint16_t)bytes[1] << 8u);
}

uint8_t noah_profile_autocorrect_v1_character(const uint8_t *bytes, uint8_t count, uint32_t *character) {
    uint8_t  lead  = bytes[0];
    uint8_t  size  = lead < 0x80u ? 1u : lead >= 0xC2u && lead <= 0xDFu ? 2u : lead >= 0xE0u && lead <= 0xEFu ? 3u : lead >= 0xF0u && lead <= 0xF4u ? 4u : 0u;
    uint32_t value = size == 1u ? lead : lead & (size == 2u ? 0x1Fu : size == 3u ? 0x0Fu : 0x07u);
    if (size == 0u || size > count) return 0u;
    for (uint8_t i = 1u; i < size; i++) {
        if ((bytes[i] & 0xC0u) != 0x80u) return 0u;
        value = (value << 6u) | (bytes[i] & 0x3Fu);
    }
    if (value < 0x20u || (value >= 0x7Fu && value < 0xA0u) || (size == 3u && value < 0x800u) || (size == 4u && value < 0x10000u) || (value >= 0xD800u && value <= 0xDFFFu) || value > 0x10FFFFu) return 0u;
    *character = value;
    return size;
}

// What a text may hold: a correction any text; a typo folded word
// characters, with apostrophes only between them.
typedef enum {
    TEXT_CORRECTION = 0u,
    TEXT_WORD,
} text_kind_t;

static noah_profile_autocorrect_v1_result_t check_text(const uint8_t *text, uint8_t length, text_kind_t kind) {
    for (uint8_t i = 0u; i < length;) {
        uint32_t character, folded;
        uint8_t  size = noah_profile_autocorrect_v1_character(&text[i], (uint8_t)(length - i), &character);
        if (size == 0u) return NOAH_PROFILE_AUTOCORRECT_V1_INVALID_TEXT;
        if (kind == TEXT_WORD && !(character == NOAH_PROFILE_AUTOCORRECT_V1_APOSTROPHE && i != 0u && i + size != length)) {
            if (!noah_profile_autocorrect_v1_fold(character, &folded)) return NOAH_PROFILE_AUTOCORRECT_V1_NOT_A_WORD;
            if (folded != character) return NOAH_PROFILE_AUTOCORRECT_V1_NOT_FOLDED;
        }
        i = (uint8_t)(i + size);
    }
    return NOAH_PROFILE_AUTOCORRECT_V1_OK;
}

static bool counts_fit(uint16_t corrections, uint16_t typos, uint16_t part_typos, uint16_t leader_typos, uint16_t text_bytes) {
    return corrections <= NOAH_PROFILE_AUTOCORRECT_V1_MAX_CORRECTIONS && typos <= NOAH_PROFILE_AUTOCORRECT_V1_MAX_TYPOS && part_typos <= typos && leader_typos <= typos && text_bytes <= NOAH_PROFILE_AUTOCORRECT_V1_MAX_TEXT_BYTES;
}

static size_t typo_rows_offset(uint16_t corrections) {
    return NOAH_PROFILE_AUTOCORRECT_V1_HEADER_SIZE + (size_t)ROW_SIZE * corrections;
}

static size_t part_index_offset(uint16_t corrections, uint16_t typos) {
    return typo_rows_offset(corrections) + (size_t)ROW_SIZE * typos;
}

static size_t leader_index_offset(uint16_t corrections, uint16_t typos, uint16_t part_typos) {
    return part_index_offset(corrections, typos) + (size_t)NOAH_PROFILE_AUTOCORRECT_V1_INDEX_ROW_SIZE * part_typos;
}

static size_t text_area_offset(uint16_t corrections, uint16_t typos, uint16_t part_typos, uint16_t leader_typos) {
    return leader_index_offset(corrections, typos, part_typos) + (size_t)NOAH_PROFILE_AUTOCORRECT_V1_INDEX_ROW_SIZE * leader_typos;
}

// ── Validation ──────────────────────────────────────────────────────────────

typedef noah_profile_autocorrect_v1_validation_t validation_t;

static noah_profile_autocorrect_v1_validation_step_t reject(noah_profile_autocorrect_v1_error_t *error, noah_profile_autocorrect_v1_result_t code, size_t offset, uint8_t table, uint16_t row) {
    if (error) *error = (noah_profile_autocorrect_v1_error_t){.code = code, .offset = offset, .table = table, .row = row};
    return NOAH_PROFILE_AUTOCORRECT_V1_REJECTED;
}

static size_t text_area(const validation_t *state) {
    return text_area_offset(state->corrections, state->typos, state->part_typos, state->leader_typos);
}

static bool walking_typos(const validation_t *state) {
    return state->phase >= PHASE_TYPO_ROW;
}

static uint8_t table_of(const validation_t *state) {
    return walking_typos(state) ? NOAH_PROFILE_AUTOCORRECT_V1_TABLE_TYPOS : NOAH_PROFILE_AUTOCORRECT_V1_TABLE_CORRECTIONS;
}

// The next row of the table being walked, or the end of the walk.
static void next_row(validation_t *state) {
    state->previous_length = state->length;
    state->row++;
    if (!walking_typos(state)) {
        if (state->row < state->corrections) {
            state->phase = PHASE_CORRECTION_ROW;
            return;
        }
        state->row             = 0u;
        state->previous_length = 0u;
    }
    state->phase = state->row < state->typos ? PHASE_TYPO_ROW : PHASE_COMPLETE;
}

// A table row's text offset and length, common to both tables; `most` is the
// table's longest text.
static noah_profile_autocorrect_v1_result_t begin_text(validation_t *state, const uint8_t row[ROW_SIZE], uint8_t most) {
    uint8_t length = row[2];
    if (length == 0u || length > most) return NOAH_PROFILE_AUTOCORRECT_V1_INVALID_TEXT_LENGTH;
    if (read_u16(row) != state->text_offset || length > state->text_bytes - state->text_offset) return NOAH_PROFILE_AUTOCORRECT_V1_INVALID_OFFSET;
    state->length = length;
    state->have   = 0u;
    state->above  = false;
    return NOAH_PROFILE_AUTOCORRECT_V1_OK;
}

// More of the current text, compared with the row before it as it replaces
// that row in the buffer. OK with the text complete, checked and consumed.
static noah_profile_autocorrect_v1_result_t take_text(validation_t *state, const uint8_t *bytes, uint8_t count, bool *complete) {
    for (uint8_t i = 0u; i < count; i++) {
        uint8_t at = (uint8_t)(state->have + i);
        if (!state->above && at < state->previous_length) {
            if (bytes[i] < state->text[at]) return NOAH_PROFILE_AUTOCORRECT_V1_ORDER;
            state->above = bytes[i] > state->text[at];
        }
        state->text[at] = bytes[i];
    }
    state->have = (uint8_t)(state->have + count);
    *complete   = state->have == state->length;
    if (!*complete) return NOAH_PROFILE_AUTOCORRECT_V1_OK;
    // Equal as far as the row before it goes: above only when longer.
    if (!state->above && state->length <= state->previous_length) return NOAH_PROFILE_AUTOCORRECT_V1_ORDER;
    state->text_offset = (uint16_t)(state->text_offset + state->length);
    return check_text(state->text, state->length, walking_typos(state) ? TEXT_WORD : TEXT_CORRECTION);
}

// A correction row's kind byte, read: false for a kind, trigger or bit there
// is not.
static bool kind_byte(uint8_t byte, uint8_t *kind, uint8_t *trigger) {
    *kind    = byte & NOAH_PROFILE_AUTOCORRECT_V1_KIND_BIT;
    *trigger = (uint8_t)(byte >> NOAH_PROFILE_AUTOCORRECT_V1_TRIGGER_SHIFT);
    return *trigger <= NOAH_PROFILE_AUTOCORRECT_V1_TRIGGER_BOTH;
}

// Where a typo's walk goes once its own row is checked: its part-index row
// when it matches part of a word and its correction fires as a word ends,
// then its leader-index row when its correction fires after the leader,
// then the next typo. One past an index's end is one the index leaves out.
static noah_profile_autocorrect_v1_result_t to_indexes(validation_t *state, bool part_done) {
    if (!part_done && state->match != NOAH_PROFILE_AUTOCORRECT_V1_MATCH_WHOLE_WORD && state->trigger != NOAH_PROFILE_AUTOCORRECT_V1_TRIGGER_LEADER) {
        if (state->part >= state->part_typos) return NOAH_PROFILE_AUTOCORRECT_V1_PART_INDEX;
        state->phase = PHASE_TYPO_PART;
        return NOAH_PROFILE_AUTOCORRECT_V1_OK;
    }
    if (state->trigger != NOAH_PROFILE_AUTOCORRECT_V1_TRIGGER_WORD) {
        if (state->leader >= state->leader_typos) return NOAH_PROFILE_AUTOCORRECT_V1_LEADER_INDEX;
        state->phase = PHASE_TYPO_LEADER;
        return NOAH_PROFILE_AUTOCORRECT_V1_OK;
    }
    next_row(state);
    return NOAH_PROFILE_AUTOCORRECT_V1_OK;
}

noah_profile_autocorrect_v1_validation_step_t noah_profile_autocorrect_v1_validation_step(noah_profile_autocorrect_v1_validation_t *state, const noah_profile_reader_t *reader, size_t base, size_t length, noah_profile_autocorrect_v1_error_t *error) {
    uint8_t                              bytes[NOAH_PROFILE_AUTOCORRECT_V1_STEP_READ_MAX];
    size_t                               offset;
    uint8_t                              count;
    noah_profile_autocorrect_v1_result_t result = NOAH_PROFILE_AUTOCORRECT_V1_OK;

    if (!state || !reader || base > SIZE_MAX - length) return reject(error, NOAH_PROFILE_AUTOCORRECT_V1_INVALID_ARGUMENT, 0u, NO_TABLE, NOAH_PROFILE_AUTOCORRECT_V1_LOCATION_NONE);
    if (length < NOAH_PROFILE_AUTOCORRECT_V1_HEADER_SIZE) return reject(error, NOAH_PROFILE_AUTOCORRECT_V1_INVALID_LENGTH, 0u, NO_TABLE, NOAH_PROFILE_AUTOCORRECT_V1_LOCATION_NONE);

    // Where this step reads, and how much.
    switch (state->phase) {
        case PHASE_HEADER:
            offset = 0u;
            count  = NOAH_PROFILE_AUTOCORRECT_V1_HEADER_SIZE;
            break;
        case PHASE_CORRECTION_ROW:
            offset = NOAH_PROFILE_AUTOCORRECT_V1_HEADER_SIZE + (size_t)ROW_SIZE * state->row;
            count  = ROW_SIZE;
            break;
        case PHASE_TYPO_ROW:
            offset = typo_rows_offset(state->corrections) + (size_t)ROW_SIZE * state->row;
            count  = ROW_SIZE;
            break;
        case PHASE_CORRECTION_TEXT:
        case PHASE_TYPO_TEXT:
            offset = text_area(state) + state->text_offset + state->have;
            count  = (uint8_t)(state->length - state->have);
            break;
        case PHASE_CORRECTION_FIRST_TYPO:
            offset = typo_rows_offset(state->corrections) + (size_t)ROW_SIZE * state->link;
            count  = ROW_SIZE;
            break;
        case PHASE_TYPO_CORRECTION:
            offset = NOAH_PROFILE_AUTOCORRECT_V1_HEADER_SIZE + (size_t)ROW_SIZE * state->link;
            count  = ROW_SIZE;
            break;
        case PHASE_TYPO_CORRECTION_TEXT:
            // text_offset is past the typo; link is now the correction's text offset.
            offset = text_area(state) + state->link + state->have;
            count  = (uint8_t)(state->length - state->have);
            break;
        case PHASE_TYPO_PART:
            offset = part_index_offset(state->corrections, state->typos) + (size_t)NOAH_PROFILE_AUTOCORRECT_V1_INDEX_ROW_SIZE * state->part;
            count  = NOAH_PROFILE_AUTOCORRECT_V1_INDEX_ROW_SIZE;
            break;
        case PHASE_TYPO_LEADER:
            offset = leader_index_offset(state->corrections, state->typos, state->part_typos) + (size_t)NOAH_PROFILE_AUTOCORRECT_V1_INDEX_ROW_SIZE * state->leader;
            count  = NOAH_PROFILE_AUTOCORRECT_V1_INDEX_ROW_SIZE;
            break;
        default:
            return NOAH_PROFILE_AUTOCORRECT_V1_COMPLETE;
    }
    if (count > sizeof(bytes)) count = sizeof(bytes);
    if (!noah_profile_reader_read(reader, base + offset, bytes, count)) return reject(error, NOAH_PROFILE_AUTOCORRECT_V1_READ_ERROR, offset, NO_TABLE, NOAH_PROFILE_AUTOCORRECT_V1_LOCATION_NONE);

    switch (state->phase) {
        case PHASE_HEADER: {
            uint16_t corrections = read_u16(&bytes[0]), typos = read_u16(&bytes[2]), part_typos = read_u16(&bytes[4]), leader_typos = read_u16(&bytes[6]), text_bytes = read_u16(&bytes[8]);
            if (!counts_fit(corrections, typos, part_typos, leader_typos, text_bytes)) return reject(error, NOAH_PROFILE_AUTOCORRECT_V1_CAPACITY_EXCEEDED, 0u, NO_TABLE, NOAH_PROFILE_AUTOCORRECT_V1_LOCATION_NONE);
            if (text_area_offset(corrections, typos, part_typos, leader_typos) + text_bytes != length) return reject(error, NOAH_PROFILE_AUTOCORRECT_V1_INVALID_LENGTH, 0u, NO_TABLE, NOAH_PROFILE_AUTOCORRECT_V1_LOCATION_NONE);
            if (typos != 0u && corrections == 0u) return reject(error, NOAH_PROFILE_AUTOCORRECT_V1_INVALID_REFERENCE, 0u, NO_TABLE, NOAH_PROFILE_AUTOCORRECT_V1_LOCATION_NONE);
            state->corrections  = corrections;
            state->typos        = typos;
            state->part_typos   = part_typos;
            state->leader_typos = leader_typos;
            state->text_bytes   = text_bytes;
            state->phase        = corrections != 0u ? PHASE_CORRECTION_ROW : typos != 0u ? PHASE_TYPO_ROW : PHASE_COMPLETE;
            break;
        }
        case PHASE_CORRECTION_ROW: {
            uint8_t kind, trigger;
            state->link = read_u16(&bytes[4]);
            if (!kind_byte(bytes[3], &kind, &trigger)) {
                result = NOAH_PROFILE_AUTOCORRECT_V1_INVALID_KIND;
            } else if (kind == NOAH_PROFILE_AUTOCORRECT_V1_KIND_MACRO) {
                // A macro correction has no text: its slot stands where the
                // offset would, and the slots ascend after every text one.
                uint16_t macro = read_u16(bytes);
                if (bytes[2] != 0u) {
                    result = NOAH_PROFILE_AUTOCORRECT_V1_INVALID_TEXT_LENGTH;
                } else if (macro >= NOAH_PROFILE_AUTOCORRECT_V1_MACRO_SLOTS) {
                    result = NOAH_PROFILE_AUTOCORRECT_V1_INVALID_MACRO;
                } else if (state->macros && macro <= state->macro) {
                    result = NOAH_PROFILE_AUTOCORRECT_V1_ORDER;
                }
                state->macros = true;
                state->macro  = (uint8_t)macro;
                state->length = 0u;
                state->phase  = PHASE_CORRECTION_FIRST_TYPO;
            } else {
                result = begin_text(state, bytes, NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_MAX);
                if (result == NOAH_PROFILE_AUTOCORRECT_V1_OK && state->macros) result = NOAH_PROFILE_AUTOCORRECT_V1_ORDER;
                state->phase = PHASE_CORRECTION_TEXT;
            }
            if (result == NOAH_PROFILE_AUTOCORRECT_V1_OK && state->link >= state->typos) result = NOAH_PROFILE_AUTOCORRECT_V1_INVALID_REFERENCE;
            break;
        }
        case PHASE_TYPO_ROW:
            result       = begin_text(state, bytes, NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX);
            state->match = bytes[3];
            state->link  = read_u16(&bytes[4]);
            if (result == NOAH_PROFILE_AUTOCORRECT_V1_OK && state->match > NOAH_PROFILE_AUTOCORRECT_V1_MATCH_ANYWHERE) result = NOAH_PROFILE_AUTOCORRECT_V1_INVALID_MATCH;
            if (result == NOAH_PROFILE_AUTOCORRECT_V1_OK && state->link >= state->corrections) result = NOAH_PROFILE_AUTOCORRECT_V1_INVALID_REFERENCE;
            state->phase = PHASE_TYPO_TEXT;
            break;
        case PHASE_CORRECTION_TEXT:
        case PHASE_TYPO_TEXT: {
            bool complete;
            offset -= state->have;
            result = take_text(state, bytes, count, &complete);
            if (result == NOAH_PROFILE_AUTOCORRECT_V1_OK && complete) state->phase = walking_typos(state) ? PHASE_TYPO_CORRECTION : PHASE_CORRECTION_FIRST_TYPO;
            break;
        }
        case PHASE_CORRECTION_FIRST_TYPO:
            // Its first typo names it; no lower typo may, checked from the
            // typo's side below.
            if (read_u16(&bytes[4]) != state->row) {
                result = NOAH_PROFILE_AUTOCORRECT_V1_FIRST_TYPO;
                offset = NOAH_PROFILE_AUTOCORRECT_V1_HEADER_SIZE + (size_t)ROW_SIZE * state->row;
                break;
            }
            next_row(state);
            break;
        case PHASE_TYPO_CORRECTION: {
            uint8_t kind;
            offset = typo_rows_offset(state->corrections) + (size_t)ROW_SIZE * state->row;
            if (read_u16(&bytes[4]) > state->row) {
                result = NOAH_PROFILE_AUTOCORRECT_V1_FIRST_TYPO;
                break;
            }
            // Its row was checked in the corrections' walk.
            (void)kind_byte(bytes[3], &kind, &state->trigger);
            // A macro plays for the whole word.
            if (kind == NOAH_PROFILE_AUTOCORRECT_V1_KIND_MACRO) {
                if (state->match != NOAH_PROFILE_AUTOCORRECT_V1_MATCH_WHOLE_WORD) {
                    result = NOAH_PROFILE_AUTOCORRECT_V1_INVALID_MATCH;
                    break;
                }
                state->phase = PHASE_TYPO_INDEXES;
                break;
            }
            // Only a correction of the typo's length can equal it. Its row was
            // checked in the corrections' walk, so its text is inside the text.
            if (bytes[2] == state->length) {
                state->link  = read_u16(bytes);
                state->have  = 0u;
                state->phase = PHASE_TYPO_CORRECTION_TEXT;
                break;
            }
            state->phase = PHASE_TYPO_INDEXES;
            break;
        }
        case PHASE_TYPO_CORRECTION_TEXT:
            if (memcmp(bytes, &state->text[state->have], count) != 0) {
                state->phase = PHASE_TYPO_INDEXES;
                break;
            }
            state->have = (uint8_t)(state->have + count);
            if (state->have == state->length) {
                result = NOAH_PROFILE_AUTOCORRECT_V1_TYPO_IS_CORRECTION;
                offset = typo_rows_offset(state->corrections) + (size_t)ROW_SIZE * state->row;
            }
            break;
        case PHASE_TYPO_PART:
            if (read_u16(bytes) != state->row) {
                result = NOAH_PROFILE_AUTOCORRECT_V1_PART_INDEX;
                break;
            }
            state->part++;
            result = to_indexes(state, true);
            if (result != NOAH_PROFILE_AUTOCORRECT_V1_OK) offset = typo_rows_offset(state->corrections) + (size_t)ROW_SIZE * state->row;
            break;
        case PHASE_TYPO_LEADER:
            if (read_u16(bytes) != state->row) {
                result = NOAH_PROFILE_AUTOCORRECT_V1_LEADER_INDEX;
                break;
            }
            state->leader++;
            next_row(state);
            break;
        default:
            break;
    }
    if (result != NOAH_PROFILE_AUTOCORRECT_V1_OK) return reject(error, result, offset, table_of(state), state->row);

    // The typo's own row is checked: on to its index rows, if it has any.
    if (state->phase == PHASE_TYPO_INDEXES) {
        result = to_indexes(state, false);
        if (result != NOAH_PROFILE_AUTOCORRECT_V1_OK) return reject(error, result, typo_rows_offset(state->corrections) + (size_t)ROW_SIZE * state->row, NOAH_PROFILE_AUTOCORRECT_V1_TABLE_TYPOS, state->row);
    }
    if (state->phase != PHASE_COMPLETE) return NOAH_PROFILE_AUTOCORRECT_V1_VALIDATING;
    // Every index row and every text byte belongs to a row.
    if (state->part != state->part_typos) return reject(error, NOAH_PROFILE_AUTOCORRECT_V1_PART_INDEX, part_index_offset(state->corrections, state->typos) + (size_t)NOAH_PROFILE_AUTOCORRECT_V1_INDEX_ROW_SIZE * state->part, NOAH_PROFILE_AUTOCORRECT_V1_TABLE_PART_INDEX, state->part);
    if (state->leader != state->leader_typos) return reject(error, NOAH_PROFILE_AUTOCORRECT_V1_LEADER_INDEX, leader_index_offset(state->corrections, state->typos, state->part_typos) + (size_t)NOAH_PROFILE_AUTOCORRECT_V1_INDEX_ROW_SIZE * state->leader, NOAH_PROFILE_AUTOCORRECT_V1_TABLE_LEADER_INDEX, state->leader);
    if (state->text_offset != state->text_bytes) return reject(error, NOAH_PROFILE_AUTOCORRECT_V1_INVALID_LENGTH, text_area(state) + state->text_offset, NO_TABLE, NOAH_PROFILE_AUTOCORRECT_V1_LOCATION_NONE);
    return NOAH_PROFILE_AUTOCORRECT_V1_COMPLETE;
}

// ── Lookups ─────────────────────────────────────────────────────────────────

typedef noah_profile_autocorrect_v1_list_t list_t;

static bool list_read(const list_t *list, size_t offset, uint8_t *bytes, size_t count) {
    return noah_profile_reader_read(&list->reader, list->base + offset, bytes, count);
}

// A row's text of at most `most` bytes, bounded by the text area whatever the
// row says.
static bool row_text(const list_t *list, const uint8_t row[ROW_SIZE], uint8_t most, uint8_t *text, uint8_t *length) {
    uint16_t offset = read_u16(row);
    *length         = row[2];
    if (*length == 0u || *length > most || offset > list->text_bytes || *length > list->text_bytes - offset) return false;
    // A typo's bytes a read, as a storage reader takes them; a longer
    // correction in as many reads as it needs.
    for (uint8_t have = 0u; text && have < *length;) {
        uint8_t count = (uint8_t)(*length - have) > NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX ? NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX : (uint8_t)(*length - have);
        if (!list_read(list, text_area_offset(list->corrections, list->typos, list->part_typos, list->leader_typos) + offset + have, &text[have], count)) return false;
        have = (uint8_t)(have + count);
    }
    return true;
}

bool noah_profile_autocorrect_v1_open(noah_profile_autocorrect_v1_list_t *list, const noah_profile_reader_t *reader, size_t base, size_t length) {
    uint8_t header[NOAH_PROFILE_AUTOCORRECT_V1_HEADER_SIZE];
    if (!list) return false;
    memset(list, 0, sizeof(*list));
    if (!reader || length < sizeof(header) || base > SIZE_MAX - length || !noah_profile_reader_read(reader, base, header, sizeof(header))) return false;
    uint16_t corrections = read_u16(&header[0]), typos = read_u16(&header[2]), part_typos = read_u16(&header[4]), leader_typos = read_u16(&header[6]), text_bytes = read_u16(&header[8]);
    if (!counts_fit(corrections, typos, part_typos, leader_typos, text_bytes) || text_area_offset(corrections, typos, part_typos, leader_typos) + text_bytes != length) return false;
    *list = (noah_profile_autocorrect_v1_list_t){.reader = *reader, .base = base, .corrections = corrections, .typos = typos, .part_typos = part_typos, .leader_typos = leader_typos, .text_bytes = text_bytes};
    return true;
}

bool noah_profile_autocorrect_v1_typo_at(const noah_profile_autocorrect_v1_list_t *list, uint16_t index, noah_profile_autocorrect_v1_typo_t *typo, uint8_t text[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX]) {
    uint8_t row[ROW_SIZE], length;
    if (!list || !typo || index >= list->typos || !list_read(list, typo_rows_offset(list->corrections) + (size_t)ROW_SIZE * index, row, sizeof(row)) || !row_text(list, row, NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX, text, &length)) return false;
    if (row[3] > NOAH_PROFILE_AUTOCORRECT_V1_MATCH_ANYWHERE || read_u16(&row[4]) >= list->corrections) return false;
    *typo = (noah_profile_autocorrect_v1_typo_t){.index = index, .correction = read_u16(&row[4]), .match = row[3], .length = length};
    return true;
}

bool noah_profile_autocorrect_v1_find(const noah_profile_autocorrect_v1_list_t *list, const uint8_t *folded, uint8_t length, noah_profile_autocorrect_v1_typo_t *typo) {
    uint8_t  text[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX];
    uint16_t low = 0u, high;
    if (!list || !folded || !typo || length == 0u || length > NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX) return false;
    high = list->typos;
    // The interval halves every pass, so a malformed list ends it as soon.
    while (low < high) {
        uint16_t middle = (uint16_t)(low + (high - low) / 2u);
        if (!noah_profile_autocorrect_v1_typo_at(list, middle, typo, text)) return false;
        uint8_t shared = typo->length < length ? typo->length : length;
        int     order  = memcmp(text, folded, shared);
        if (order == 0 && typo->length == length) return true;
        if (order < 0 || (order == 0 && typo->length < length)) {
            low = (uint16_t)(middle + 1u);
        } else {
            high = middle;
        }
    }
    return false;
}

bool noah_profile_autocorrect_v1_part_at(const noah_profile_autocorrect_v1_list_t *list, uint16_t position, noah_profile_autocorrect_v1_typo_t *typo, uint8_t text[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX]) {
    uint8_t row[NOAH_PROFILE_AUTOCORRECT_V1_INDEX_ROW_SIZE];
    if (!list || position >= list->part_typos || !list_read(list, part_index_offset(list->corrections, list->typos) + (size_t)sizeof(row) * position, row, sizeof(row))) return false;
    return noah_profile_autocorrect_v1_typo_at(list, read_u16(row), typo, text) && typo->match != NOAH_PROFILE_AUTOCORRECT_V1_MATCH_WHOLE_WORD;
}

bool noah_profile_autocorrect_v1_leader_at(const noah_profile_autocorrect_v1_list_t *list, uint16_t position, noah_profile_autocorrect_v1_typo_t *typo, uint8_t text[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX]) {
    uint8_t row[NOAH_PROFILE_AUTOCORRECT_V1_INDEX_ROW_SIZE];
    if (!list || position >= list->leader_typos || !list_read(list, leader_index_offset(list->corrections, list->typos, list->part_typos) + (size_t)sizeof(row) * position, row, sizeof(row))) return false;
    return noah_profile_autocorrect_v1_typo_at(list, read_u16(row), typo, text);
}

// Whether `text` begins with all of `held`.
static bool begins_with(const uint8_t *text, uint8_t length, const uint8_t *held, uint8_t held_length) {
    return length >= held_length && memcmp(text, held, held_length) == 0;
}

bool noah_profile_autocorrect_v1_leader(const noah_profile_autocorrect_v1_list_t *list, const uint8_t *folded, uint8_t length, noah_profile_autocorrect_v1_leader_t *found) {
    noah_profile_autocorrect_v1_typo_t typo;
    uint8_t                            text[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX];
    uint16_t                           low = 0u, high;
    if (!list || !folded || !found || length == 0u || length > NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX) return false;
    *found = (noah_profile_autocorrect_v1_leader_t){0};
    high   = list->leader_typos;
    // The first position not below `folded`, a text before any longer one
    // it begins: there `folded` is, if it is a typo, and after it whatever
    // begins with it. The interval halves every pass, so a malformed list
    // ends it as soon.
    while (low < high) {
        uint16_t middle = (uint16_t)(low + (high - low) / 2u);
        if (!noah_profile_autocorrect_v1_leader_at(list, middle, &typo, text)) return false;
        uint8_t shared = typo.length < length ? typo.length : length;
        int     order  = memcmp(text, folded, shared);
        if (order < 0 || (order == 0 && typo.length < length)) {
            low = (uint16_t)(middle + 1u);
        } else {
            high = middle;
        }
    }
    if (low < list->leader_typos) {
        if (!noah_profile_autocorrect_v1_leader_at(list, low, &typo, text)) return false;
        if (typo.length == length && memcmp(text, folded, length) == 0) {
            found->typo  = typo;
            found->exact = true;
            low++;
            if (low < list->leader_typos && !noah_profile_autocorrect_v1_leader_at(list, low, &typo, text)) return false;
        }
        found->longer = low < list->leader_typos && typo.length > length && begins_with(text, typo.length, folded, length);
    }
    return true;
}

bool noah_profile_autocorrect_v1_correction(const noah_profile_autocorrect_v1_list_t *list, uint16_t index, noah_profile_autocorrect_v1_correction_t *correction, uint8_t text[NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_MAX]) {
    uint8_t row[ROW_SIZE], length = 0u, kind, trigger;
    if (!list || !correction || index >= list->corrections || !list_read(list, NOAH_PROFILE_AUTOCORRECT_V1_HEADER_SIZE + (size_t)ROW_SIZE * index, row, sizeof(row)) || !kind_byte(row[3], &kind, &trigger)) return false;
    if (kind == NOAH_PROFILE_AUTOCORRECT_V1_KIND_MACRO) {
        if (row[2] != 0u || read_u16(row) >= NOAH_PROFILE_AUTOCORRECT_V1_MACRO_SLOTS) return false;
        *correction = (noah_profile_autocorrect_v1_correction_t){.kind = NOAH_PROFILE_AUTOCORRECT_V1_KIND_MACRO, .trigger = trigger, .macro = read_u16(row)};
        return true;
    }
    if (!row_text(list, row, NOAH_PROFILE_AUTOCORRECT_V1_CORRECTION_MAX, text, &length)) return false;
    *correction = (noah_profile_autocorrect_v1_correction_t){.kind = NOAH_PROFILE_AUTOCORRECT_V1_KIND_TEXT, .trigger = trigger, .length = length};
    return true;
}

bool noah_profile_autocorrect_v1_fires_as_typed(const noah_profile_autocorrect_v1_list_t *list) {
    noah_profile_autocorrect_v1_correction_t correction;
    if (!list) return false;
    for (uint16_t index = 0u; index < list->corrections; index++) {
        if (noah_profile_autocorrect_v1_correction(list, index, &correction, NULL) && correction.trigger != NOAH_PROFILE_AUTOCORRECT_V1_TRIGGER_LEADER) return true;
    }
    return false;
}
