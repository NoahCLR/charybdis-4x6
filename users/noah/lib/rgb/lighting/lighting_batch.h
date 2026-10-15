#pragma once
#include "lighting.h"
#include "lighting_batch_arena.h"
#include <stddef.h>

// Exact internal ordered-batch codec. Identity kind/id belongs to its history
// cell, so only slot/outcome/actual owners/sides are stored per member. No LED
// or final-colour cache: current keymap queries still work after remapping.
enum { NOAH_LIGHTING_BATCH_SUBJECTS = 64u, NOAH_LIGHTING_BATCH_BYTES = 1u + NOAH_LIGHTING_BATCH_SUBJECTS * (3u + KEY_ORIGIN_BITMAP_SIZE) };
_Static_assert((unsigned)NOAH_LIGHTING_BATCH_BYTES <= (unsigned)NOAH_LIGHTING_ARENA_OBJECT_BYTES_MAX, "all accepted ordered batches fit typed arena length");
_Static_assert(KEY_ORIGIN_BITMAP_SIZE <= 8u, "batch key index encoding covers up to64 owner bits");
typedef struct {
    const uint8_t *bytes;
    size_t         size, offset;
    uint8_t        remaining, slot, outcome, thing_kind, thing;
} noah_lighting_batch_reader_t;
// Incremental writer appends while the actual fact yields its natural order.
// Caller supplies full bound; no truncation, eviction or silent partial batch.
typedef struct {
    uint8_t *bytes;
    size_t   size, used;
    uint8_t  count, slot, outcome;
} noah_lighting_batch_writer_t;
void noah_lighting_batch_write_begin(noah_lighting_batch_writer_t *writer, uint8_t *bytes, size_t size);
bool noah_lighting_batch_write(noah_lighting_batch_writer_t *writer, const noah_lighting_subject_t *subject);
bool noah_lighting_batch_read_begin(noah_lighting_batch_reader_t *reader, const uint8_t *bytes, size_t size, uint8_t thing_kind, uint8_t thing);
bool noah_lighting_batch_read(noah_lighting_batch_reader_t *reader, noah_lighting_subject_t *subject);
