#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Main-owned, dense immutable entries. References identify entries, never byte
// addresses; release compacts immediately without invalidating other references.
// The caller chooses and accounts for storage; this module owns no heap or bank.
typedef uint32_t noah_lighting_batch_ref_t;
typedef struct {
    uint8_t *bytes;
    uint32_t capacity, used, next_reference;
} noah_lighting_batch_arena_t;
typedef struct {
    uint32_t entries, equality_bytes;
} noah_lighting_batch_search_t;
enum { NOAH_LIGHTING_BATCH_ENTRY_HEADER = 12u };
void noah_lighting_batch_arena_init(noah_lighting_batch_arena_t *arena, uint8_t *bytes, uint32_t capacity);
// Full kind/id/ordered-byte equality is the only sharing criterion. Failed
// admission leaves the arena and the caller's reference unchanged. It is NOT
// permission for a production owner to drop an otherwise accepted history.
bool noah_lighting_batch_acquire(noah_lighting_batch_arena_t *arena, uint8_t kind, uint8_t thing, const uint8_t *bytes, uint16_t size, noah_lighting_batch_ref_t *reference, noah_lighting_batch_search_t *search);
// Atomically replaces one owned reference after reserving/interning the new
// payload. Other rendering/delivery owners keep their old reference unchanged.
bool noah_lighting_batch_replace(noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t *reference, uint8_t kind, uint8_t thing, const uint8_t *bytes, uint16_t size, noah_lighting_batch_search_t *search);
bool noah_lighting_batch_retain(noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference);
bool noah_lighting_batch_retain_many(noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference, uint32_t count);
bool noah_lighting_batch_release(noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference);
// Copies only requested bytes. No movable arena pointer escapes, including
// while a frozen renderer is reading a pinned old version across updates.
bool noah_lighting_batch_copy(const noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference, uint16_t offset, void *bytes, uint16_t size);
bool noah_lighting_batch_describe(const noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference, uint8_t *kind, uint8_t *thing, uint16_t *size, uint32_t *references);
