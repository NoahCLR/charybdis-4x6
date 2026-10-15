#include "lighting_retained.h"
#include <string.h>

enum { RULE, THING, FLAGS, LIFETIME, ORDER = 6u, BATCH = 14u, DIRTY = 1u, RENDER = 2u, SEEN = 4u };
static uint32_t read32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) | ((uint32_t)bytes[3] << 24u);
}
static void write32(uint8_t *bytes, uint32_t value) {
    for (uint8_t i = 0u; i < 4u; i++)
        bytes[i] = (uint8_t)(value >> (8u * i));
}
static noah_lighting_lifetime_t lifetime(const noah_lighting_retained_t *cell) {
    noah_lighting_lifetime_t value;
    memcpy(value.bytes, cell->bytes + LIFETIME, sizeof value.bytes);
    return value;
}
static void release_unused(noah_lighting_retained_t *cell, noah_lighting_batch_arena_t *arena) {
    noah_lighting_batch_ref_t reference = noah_lighting_retained_batch(cell);
    if (reference && !(cell->bytes[FLAGS] & (DIRTY | RENDER))) {
        (void)noah_lighting_batch_release(arena, reference);
        write32(cell->bytes + BATCH, 0u);
    }
}
void noah_lighting_retained_init(noah_lighting_retained_t *cell, uint8_t rule, uint8_t thing) {
    memset(cell, 0, sizeof *cell);
    cell->bytes[RULE]  = rule;
    cell->bytes[THING] = thing;
}
noah_lighting_batch_ref_t noah_lighting_retained_batch(const noah_lighting_retained_t *cell) {
    return read32(cell->bytes + BATCH);
}
uint8_t noah_lighting_retained_rule(const noah_lighting_retained_t *cell) {
    return cell->bytes[RULE];
}
uint8_t noah_lighting_retained_thing(const noah_lighting_retained_t *cell) {
    return cell->bytes[THING];
}
bool noah_lighting_retained_needed(const noah_lighting_retained_t *cell) {
    noah_lighting_lifetime_t value = lifetime(cell);
    return (cell->bytes[FLAGS] & (DIRTY | RENDER)) || noah_lighting_lifetime_matching(&value);
}
uint64_t noah_lighting_retained_order(const noah_lighting_retained_t *cell) {
    return (uint64_t)read32(cell->bytes + ORDER) | ((uint64_t)read32(cell->bytes + ORDER + 4u) << 32u);
}
bool noah_lighting_retained_dirty(const noah_lighting_retained_t *cell) {
    return (cell->bytes[FLAGS] & DIRTY) != 0u;
}
void noah_lighting_retained_install_event(noah_lighting_retained_t *cell, noah_lighting_batch_arena_t *arena, const noah_lighting_lifetime_parameters_t *parameters, noah_lighting_batch_ref_t owned_reference, uint64_t order, bool source_delivery) {
    if ((cell->bytes[FLAGS] & SEEN) && noah_lighting_retained_order(cell) == order) {
        (void)noah_lighting_batch_release(arena, owned_reference);
        return;
    }
    noah_lighting_lifetime_t next = {{0}};
    (void)noah_lighting_lifetime_observe(&next, parameters, 0u, NOAH_LIGHTING_LIFETIME_EVENT);
    noah_lighting_batch_ref_t old = noah_lighting_retained_batch(cell);
    write32(cell->bytes + BATCH, owned_reference);
    write32(cell->bytes + ORDER, (uint32_t)order);
    write32(cell->bytes + ORDER + 4u, (uint32_t)(order >> 32u));
    memcpy(cell->bytes + LIFETIME, next.bytes, sizeof next.bytes);
    cell->bytes[FLAGS] = RENDER | SEEN | (source_delivery ? DIRTY : 0u);
    if (old) (void)noah_lighting_batch_release(arena, old);
}
bool noah_lighting_retained_observe(noah_lighting_retained_t *cell, noah_lighting_batch_arena_t *arena, const noah_lighting_lifetime_parameters_t *parameters, uint32_t elapsed, uint8_t observation, uint8_t kind, const uint8_t *batch, uint16_t size, uint64_t order, bool source_delivery) {
    // Per-group commit can retry its already applied cells without restarting
    // their observation clocks. Transport still deduplicates committed packets.
    if (observation == NOAH_LIGHTING_LIFETIME_EVENT && (cell->bytes[FLAGS] & SEEN) && noah_lighting_retained_order(cell) == order) return true;
    noah_lighting_lifetime_t  next      = lifetime(cell);
    bool                      entering  = observation == NOAH_LIGHTING_LIFETIME_MATCH && !noah_lighting_lifetime_matching(&next);
    uint8_t                   action    = noah_lighting_lifetime_observe(&next, parameters, elapsed, observation);
    noah_lighting_batch_ref_t reference = noah_lighting_retained_batch(cell);
    if (action == NOAH_LIGHTING_BATCH_REPLACE) {
        // Reserve before committing lifetime/dirty state. The complete owner
        // must prove its capacity, not use failure to evict accepted histories.
        if (!noah_lighting_batch_replace(arena, &reference, kind, cell->bytes[THING], batch, size, NULL)) return false;
        write32(cell->bytes + BATCH, reference);
        cell->bytes[FLAGS] |= RENDER;
        if (observation == NOAH_LIGHTING_LIFETIME_EVENT) {
            cell->bytes[FLAGS] |= SEEN;
            if (source_delivery) cell->bytes[FLAGS] |= DIRTY;
        }
    } else if (action == NOAH_LIGHTING_BATCH_RELEASE_RENDER)
        cell->bytes[FLAGS] &= (uint8_t)~RENDER;
    if (entering || observation == NOAH_LIGHTING_LIFETIME_EVENT) {
        write32(cell->bytes + ORDER, (uint32_t)order);
        write32(cell->bytes + ORDER + 4u, (uint32_t)(order >> 32u));
    }
    memcpy(cell->bytes + LIFETIME, next.bytes, sizeof next.bytes);
    release_unused(cell, arena);
    return true;
}
bool noah_lighting_retained_visible(const noah_lighting_retained_t *cell, const noah_lighting_lifetime_parameters_t *parameters) {
    noah_lighting_lifetime_t value = lifetime(cell);
    return (cell->bytes[FLAGS] & RENDER) && noah_lighting_lifetime_visible(&value, parameters);
}
uint32_t noah_lighting_retained_wake(const noah_lighting_retained_t *cell, const noah_lighting_lifetime_parameters_t *parameters) {
    noah_lighting_lifetime_t value = lifetime(cell);
    return noah_lighting_lifetime_wake(&value, parameters);
}
void noah_lighting_retained_seed(noah_lighting_retained_t *cell, noah_lighting_batch_arena_t *arena) {
    cell->bytes[FLAGS] &= (uint8_t)~DIRTY;
    if ((cell->bytes[FLAGS] & (SEEN | RENDER)) == (SEEN | RENDER)) cell->bytes[FLAGS] |= DIRTY;
    release_unused(cell, arena);
}
bool noah_lighting_retained_ack(noah_lighting_retained_t *cell, noah_lighting_batch_arena_t *arena, uint64_t captured_order) {
    if (!noah_lighting_retained_dirty(cell) || noah_lighting_retained_order(cell) != captured_order) return false;
    cell->bytes[FLAGS] &= (uint8_t)~DIRTY;
    release_unused(cell, arena);
    return true;
}
bool noah_lighting_retained_pin(const noah_lighting_retained_t *cell, noah_lighting_batch_arena_t *arena, noah_lighting_retained_t *snapshot) {
    noah_lighting_batch_ref_t reference = noah_lighting_retained_batch(cell);
    if (cell == snapshot || !reference || !(cell->bytes[FLAGS] & RENDER) || !noah_lighting_batch_retain(arena, reference)) return false;
    *snapshot = *cell;
    snapshot->bytes[FLAGS] &= (uint8_t)~DIRTY;
    return true;
}
void noah_lighting_retained_discard(noah_lighting_retained_t *cell, noah_lighting_batch_arena_t *arena) {
    noah_lighting_batch_ref_t reference = noah_lighting_retained_batch(cell);
    if (reference) (void)noah_lighting_batch_release(arena, reference);
    noah_lighting_retained_init(cell, cell->bytes[RULE], cell->bytes[THING]);
}
