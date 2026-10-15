#include "lighting_owned_batch.h"

bool noah_lighting_owned_batch_begin(const noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference, noah_lighting_owned_batch_reader_t *reader) {
    uint8_t type_value;
    if (!noah_lighting_record_type(arena, reference, &type_value) || type_value != NOAH_LIGHTING_ARENA_BATCH) return false;

    noah_lighting_owned_batch_reader_t next = {.reference = reference, .offset = 1u};
    if (!noah_lighting_batch_describe(arena, reference, &next.thing_kind, &next.thing, &next.size, NULL) || !next.size || !noah_lighting_batch_copy(arena, reference, 0u, &next.remaining, 1u) || next.remaining > NOAH_LIGHTING_BATCH_SUBJECTS) return false;
    *reader = next;
    return true;
}
bool noah_lighting_owned_batch_next(const noah_lighting_batch_arena_t *arena, noah_lighting_owned_batch_reader_t *reader, noah_lighting_subject_t *subject) {
    if (!reader->remaining || reader->offset >= reader->size) return false;
    // One encoded subject is at most a flag, two paint fields and the compiled
    // owner bitmap. The count prefix lets the existing codec decode exactly
    // one subject, inheriting the previous member's slot/outcome as usual.
    uint8_t  encoded[1u + 3u + KEY_ORIGIN_BITMAP_SIZE];
    uint16_t size = reader->size - reader->offset;
    if (size > sizeof encoded - 1u) size = sizeof encoded - 1u;
    encoded[0] = 1u;
    if (!noah_lighting_batch_copy(arena, reader->reference, reader->offset, encoded + 1u, size)) return false;
    noah_lighting_batch_reader_t member;
    if (!noah_lighting_batch_read_begin(&member, encoded, size + 1u, reader->thing_kind, reader->thing)) return false;
    member.slot    = reader->slot;
    member.outcome = reader->outcome;
    if (!noah_lighting_batch_read(&member, subject)) return false;
    reader->offset += (uint16_t)(member.offset - 1u);
    reader->remaining--;
    reader->slot    = subject->slot;
    reader->outcome = subject->outcome;
    return true;
}
