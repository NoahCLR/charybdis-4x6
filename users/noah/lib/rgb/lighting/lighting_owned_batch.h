#pragma once
#include "lighting_batch_arena.h"
#include "lighting_batch.h"

// Borrows a caller-owned reference, never an arena address. The owner must
// retain it until iteration ends; compaction and other-version updates are safe.
typedef struct {
    uint64_t                  priority;
    noah_lighting_batch_ref_t reference;
    uint16_t                  size, offset;
    uint8_t                   remaining, slot, outcome, thing_kind, thing;
} noah_lighting_owned_batch_reader_t;
bool noah_lighting_owned_batch_begin(const noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference, noah_lighting_owned_batch_reader_t *reader);
bool noah_lighting_owned_batch_next(const noah_lighting_batch_arena_t *arena, noah_lighting_owned_batch_reader_t *reader, noah_lighting_subject_t *subject);
