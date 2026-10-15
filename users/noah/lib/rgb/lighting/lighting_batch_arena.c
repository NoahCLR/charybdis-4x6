#include "lighting_batch_arena.h"
#include <limits.h>
#include <string.h>

enum { REF = 0u, COUNT = 4u, SIZE = 8u, KIND = 10u, THING = 11u, HEADER = NOAH_LIGHTING_BATCH_ENTRY_HEADER };
static uint16_t read16(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}
static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void write16(uint8_t *p, uint16_t n) {
    p[0] = (uint8_t)n;
    p[1] = (uint8_t)(n >> 8);
}
static void write32(uint8_t *p, uint32_t n) {
    for (uint8_t i = 0u; i < 4u; ++i)
        p[i] = (uint8_t)(n >> (8u * i));
}
static uint16_t entry_length(const uint8_t *entry) {
    return read16(entry + SIZE) & NOAH_LIGHTING_ARENA_OBJECT_BYTES_MAX;
}
static uint8_t type(const uint8_t *entry) {
    return (uint8_t)(read16(entry + SIZE) >> 14u);
}
static uint32_t find(const noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference) {
    if (reference != 0u)
        for (uint32_t offset = 0u; offset < arena->used; offset += HEADER + entry_length(&arena->bytes[offset]))
            if (read32(&arena->bytes[offset + REF]) == reference) return offset;
    return arena->used;
}
void noah_lighting_batch_arena_init(noah_lighting_batch_arena_t *arena, uint8_t *bytes, uint32_t capacity) {
    *arena = (noah_lighting_batch_arena_t){.bytes = bytes, .capacity = bytes ? capacity : 0u, .next_reference = 1u};
}
static bool append(noah_lighting_batch_arena_t *arena, uint8_t type_value, uint8_t kind, uint8_t thing, const uint8_t *bytes, uint16_t size, noah_lighting_batch_ref_t *reference) {
    if (arena->capacity - arena->used < (uint32_t)HEADER + size) return false;
    // Source workspace is caller-owned, outside this arena. In particular,
    // append never retains a producer pointer or borrows a resolved fact frame.
    uint32_t reference_next = arena->next_reference;
    while (reference_next == 0u || find(arena, reference_next) != arena->used)
        ++reference_next;
    uint8_t *entry = &arena->bytes[arena->used];
    write32(&entry[REF], reference_next);
    write32(&entry[COUNT], 1u);
    write16(&entry[SIZE], (uint16_t)(size | (uint16_t)type_value << 14u));
    entry[KIND]  = kind;
    entry[THING] = thing;
    memcpy(&entry[HEADER], bytes, size);
    arena->used += HEADER + size;
    arena->next_reference = reference_next + 1u;
    *reference            = reference_next;
    return true;
}
bool noah_lighting_batch_acquire(noah_lighting_batch_arena_t *arena, uint8_t kind, uint8_t thing, const uint8_t *bytes, uint16_t size, noah_lighting_batch_ref_t *reference, noah_lighting_batch_search_t *search) {
    if (!arena || !reference || !bytes || size == 0u || size > NOAH_LIGHTING_ARENA_OBJECT_BYTES_MAX) return false;
    for (uint32_t offset = 0u; offset < arena->used; offset += HEADER + entry_length(&arena->bytes[offset])) {
        const uint8_t *entry = &arena->bytes[offset];
        if (search) ++search->entries;
        if (type(entry) != NOAH_LIGHTING_ARENA_BATCH || entry[KIND] != kind || entry[THING] != thing || entry_length(entry) != size) continue;
        uint16_t equal = 0u;
        while (equal < size) {
            if (search) ++search->equality_bytes;
            if (entry[HEADER + equal] != bytes[equal]) break;
            ++equal;
        }
        if (equal != size) continue;
        uint32_t count = read32(&entry[COUNT]);
        if (count == UINT32_MAX) return false;
        write32(&arena->bytes[offset + COUNT], count + 1u);
        *reference = read32(&entry[REF]);
        return true;
    }
    return append(arena, NOAH_LIGHTING_ARENA_BATCH, kind, thing, bytes, size, reference);
}

bool noah_lighting_batch_retain(noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference) {
    return noah_lighting_batch_retain_many(arena, reference, 1u);
}
bool noah_lighting_batch_retain_many(noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference, uint32_t added) {
    uint32_t offset = find(arena, reference);
    if (offset == arena->used) return false;
    uint32_t count = read32(&arena->bytes[offset + COUNT]);
    if (added > UINT32_MAX - count) return false;
    write32(&arena->bytes[offset + COUNT], count + added);
    return true;
}

bool noah_lighting_batch_replace(noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t *reference, uint8_t kind, uint8_t thing, const uint8_t *bytes, uint16_t size, noah_lighting_batch_search_t *search) {
    if (!arena || !reference || (*reference && find(arena, *reference) == arena->used)) return false;
    if (*reference && type(arena->bytes + find(arena, *reference)) != NOAH_LIGHTING_ARENA_BATCH) return false;
    noah_lighting_batch_ref_t acquired;
    if (!noah_lighting_batch_acquire(arena, kind, thing, bytes, size, &acquired, search)) return false;
    if (*reference) (void)noah_lighting_batch_release(arena, *reference);
    *reference = acquired;
    return true;
}
bool noah_lighting_batch_release(noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference) {
    uint32_t offset = find(arena, reference);
    if (offset == arena->used) return false;
    uint32_t count = read32(&arena->bytes[offset + COUNT]);
    if (count > 1u) {
        write32(&arena->bytes[offset + COUNT], count - 1u);
        return true;
    }
    uint32_t end = offset + HEADER + entry_length(&arena->bytes[offset]);
    memmove(&arena->bytes[offset], &arena->bytes[end], arena->used - end);
    arena->used -= end - offset;
    return true;
}
bool noah_lighting_batch_copy(const noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference, uint16_t offset, void *bytes, uint16_t size) {
    uint32_t entry = find(arena, reference);
    if (entry == arena->used || !bytes) return false;
    uint16_t length = entry_length(&arena->bytes[entry]);
    if (offset > length || size > length - offset) return false;
    memcpy(bytes, &arena->bytes[entry + HEADER + offset], size);
    return true;
}
bool noah_lighting_batch_describe(const noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference, uint8_t *kind, uint8_t *thing, uint16_t *size, uint32_t *references) {
    uint32_t offset = find(arena, reference);
    if (offset == arena->used) return false;
    if (kind) *kind = arena->bytes[offset + KIND];
    if (thing) *thing = arena->bytes[offset + THING];
    if (size) *size = entry_length(&arena->bytes[offset]);
    if (references) *references = read32(&arena->bytes[offset + COUNT]);
    return true;
}

bool noah_lighting_record_acquire(noah_lighting_batch_arena_t *arena, uint8_t type_value, uint8_t kind, uint8_t thing, const uint8_t *bytes, uint16_t size, noah_lighting_batch_ref_t *reference) {
    if (!arena || !bytes || !size || size > NOAH_LIGHTING_ARENA_OBJECT_BYTES_MAX || !reference || (type_value != NOAH_LIGHTING_ARENA_CURRENT && type_value != NOAH_LIGHTING_ARENA_FRAME)) return false;
    return append(arena, type_value, kind, thing, bytes, size, reference);
}
bool noah_lighting_record_type(const noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference, uint8_t *type_value) {
    uint32_t offset = find(arena, reference);
    if (offset == arena->used || !type_value) return false;
    *type_value = type(arena->bytes + offset);
    return true;
}
bool noah_lighting_record_update(noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference, const uint8_t *bytes, uint16_t size) {
    uint32_t offset = find(arena, reference);
    if (offset == arena->used || !bytes || !size || size > NOAH_LIGHTING_ARENA_OBJECT_BYTES_MAX || type(arena->bytes + offset) != NOAH_LIGHTING_ARENA_CURRENT || read32(arena->bytes + offset + COUNT) != 1u) return false;
    uint16_t old = entry_length(arena->bytes + offset);
    if (size > old && (uint32_t)(size - old) > arena->capacity - arena->used) return false;
    uint32_t tail = offset + HEADER + old;
    memmove(arena->bytes + offset + HEADER + size, arena->bytes + tail, arena->used - tail);
    memcpy(arena->bytes + offset + HEADER, bytes, size);
    write16(arena->bytes + offset + SIZE, (uint16_t)(size | NOAH_LIGHTING_ARENA_CURRENT << 14u));
    arena->used = arena->used - old + size;
    return true;
}
bool noah_lighting_record_next(const noah_lighting_batch_arena_t *arena, uint8_t type_value, uint32_t *offset, noah_lighting_batch_ref_t *reference) {
    if (!offset || !reference || type_value > NOAH_LIGHTING_ARENA_FRAME) return false;
    while (*offset < arena->used) {
        const uint8_t *entry = arena->bytes + *offset;
        *offset += HEADER + entry_length(entry);
        if (type(entry) == type_value) {
            *reference = read32(entry + REF);
            return true;
        }
    }
    return false;
}
