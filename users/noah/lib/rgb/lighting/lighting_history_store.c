#include "lighting_history_store.h"
#include <string.h>

enum { MEMBERS = 0u, CELL = 1u, MASK = CELL + sizeof(noah_lighting_retained_t), BITMAP = 255u };
typedef struct {
    uint8_t  bytes[NOAH_LIGHTING_HISTORY_COHORT_BYTES];
    uint16_t size;
} cohort_t;
static bool member(const cohort_t *cohort, uint8_t rule) {
    if (cohort->bytes[MEMBERS] == BITMAP) return (cohort->bytes[MASK + rule / 8u] & (1u << (rule % 8u))) != 0u;
    for (uint8_t i = 0u; i < cohort->bytes[MEMBERS]; i++)
        if (cohort->bytes[MASK + i] == rule) return true;
    return false;
}
static void membership(cohort_t *cohort, const uint8_t mask[32]) {
    uint16_t count = 0u;
    for (uint16_t rule = 0u; rule < 255u; rule++)
        if (mask[rule / 8u] & (1u << (rule % 8u))) count++;
    if (count >= 32u) {
        cohort->bytes[MEMBERS] = BITMAP;
        memcpy(cohort->bytes + MASK, mask, 32u);
        cohort->size = MASK + 32u;
    } else {
        cohort->bytes[MEMBERS] = (uint8_t)count;
        uint8_t at             = 0u;
        for (uint16_t rule = 0u; rule < 255u; rule++)
            if (mask[rule / 8u] & (1u << (rule % 8u))) cohort->bytes[MASK + at++] = (uint8_t)rule;
        cohort->size = MASK + count;
    }
}
static bool load(const noah_lighting_history_store_t *store, noah_lighting_batch_ref_t reference, cohort_t *cohort) {
    uint16_t size;
    if (!noah_lighting_batch_describe(store->arena, reference, NULL, NULL, &size, NULL) || size > sizeof cohort->bytes || size <= MASK) return false;
    cohort->size = size;
    return noah_lighting_batch_copy(store->arena, reference, 0u, cohort->bytes, size);
}
static noah_lighting_batch_ref_t find(const noah_lighting_history_store_t *store, uint8_t type, uint8_t rule, uint8_t thing, cohort_t *cohort) {
    uint32_t                  offset = 0u;
    noah_lighting_batch_ref_t ref;
    while (noah_lighting_record_next(store->arena, type, &offset, &ref)) {
        uint8_t id;
        if (!noah_lighting_batch_describe(store->arena, ref, NULL, &id, NULL, NULL) || id != thing || !load(store, ref, cohort)) continue;
        if (member(cohort, rule)) return ref;
    }
    return 0u;
}
static void cell_from(const cohort_t *cohort, uint8_t rule, noah_lighting_retained_t *cell) {
    memcpy(cell->bytes, cohort->bytes + CELL, sizeof cell->bytes);
    cell->bytes[0] = rule;
}
static void mask_from(const cohort_t *cohort, uint8_t mask[32]) {
    memset(mask, 0, 32u);
    for (uint16_t rule = 0u; rule < 255u; rule++)
        if (member(cohort, (uint8_t)rule)) mask[rule / 8u] |= 1u << (rule % 8u);
}
void noah_lighting_history_store_init(noah_lighting_history_store_t *store, noah_lighting_batch_arena_t *arena) {
    store->arena = arena;
}
bool noah_lighting_history_store_read(const noah_lighting_history_store_t *store, uint8_t type, uint8_t rule, uint8_t thing, noah_lighting_retained_t *cell) {
    cohort_t cohort;
    if (!cell || rule == 255u || !find(store, type, rule, thing, &cohort)) return false;
    cell_from(&cohort, rule, cell);
    return true;
}
static void remove_from(noah_lighting_history_store_t *store, noah_lighting_batch_ref_t reference, cohort_t *cohort, uint8_t rule) {
    uint8_t mask[32];
    mask_from(cohort, mask);
    mask[rule / 8u] &= (uint8_t)~(1u << (rule % 8u));
    membership(cohort, mask);
    if (cohort->size == MASK) {
        noah_lighting_retained_t cell;
        cell_from(cohort, rule, &cell);
        noah_lighting_retained_discard(&cell, store->arena);
        (void)noah_lighting_batch_release(store->arena, reference);
    } else
        (void)noah_lighting_record_update(store->arena, reference, cohort->bytes, cohort->size);
}
void noah_lighting_history_store_remove(noah_lighting_history_store_t *store, uint8_t rule, uint8_t thing) {
    cohort_t                  cohort;
    noah_lighting_batch_ref_t ref = find(store, NOAH_LIGHTING_ARENA_CURRENT, rule, thing, &cohort);
    if (ref) remove_from(store, ref, &cohort, rule);
}
bool noah_lighting_history_store_write(noah_lighting_history_store_t *store, const noah_lighting_retained_t *cell) {
    if (!cell || noah_lighting_retained_rule(cell) == 255u) return false;
    uint8_t                   rule = noah_lighting_retained_rule(cell), thing = noah_lighting_retained_thing(cell);
    cohort_t                  old;
    noah_lighting_batch_ref_t old_ref = find(store, NOAH_LIGHTING_ARENA_CURRENT, rule, thing, &old);
    if (!noah_lighting_retained_needed(cell)) {
        if (old_ref) remove_from(store, old_ref, &old, rule);
        // An unneeded cell owns no payload by the retained-cell invariant.
        return true;
    }
    uint8_t canonical[18];
    memcpy(canonical, cell->bytes, sizeof canonical);
    canonical[0]                     = 0u;
    uint32_t                  offset = 0u;
    noah_lighting_batch_ref_t ref;
    cohort_t                  cohort;
    while (noah_lighting_record_next(store->arena, NOAH_LIGHTING_ARENA_CURRENT, &offset, &ref)) {
        if (!load(store, ref, &cohort) || memcmp(cohort.bytes + CELL, canonical, sizeof canonical)) continue;
        uint8_t mask[32];
        mask_from(&cohort, mask);
        mask[rule / 8u] |= 1u << (rule % 8u);
        membership(&cohort, mask);
        if (!noah_lighting_record_update(store->arena, ref, cohort.bytes, cohort.size)) return false;
        // Destination cohort already owns exactly one full-payload reference.
        noah_lighting_batch_ref_t batch = noah_lighting_retained_batch(cell);
        if (batch) (void)noah_lighting_batch_release(store->arena, batch);
        if (old_ref && old_ref != ref) remove_from(store, old_ref, &old, rule);
        return true;
    }
    uint8_t mask[32] = {0};
    mask[rule / 8u] |= 1u << (rule % 8u);
    memcpy(cohort.bytes + CELL, canonical, sizeof canonical);
    membership(&cohort, mask);
    // If this rule is the only old member, in-place metadata replacement is
    // safe: its previous payload remains owned until this non-failing commit.
    if (old_ref && old.bytes[MEMBERS] == 1u) {
        if (!noah_lighting_record_update(store->arena, old_ref, cohort.bytes, cohort.size)) return false;
        noah_lighting_retained_t previous;
        cell_from(&old, rule, &previous);
        noah_lighting_retained_discard(&previous, store->arena);
        return true;
    }
    if (!noah_lighting_record_acquire(store->arena, NOAH_LIGHTING_ARENA_CURRENT, 0u, thing, cohort.bytes, cohort.size, &ref)) return false;
    if (old_ref) remove_from(store, old_ref, &old, rule);
    return true;
}
void noah_lighting_history_store_cursor(const noah_lighting_history_store_t *store, noah_lighting_history_cursor_t *cursor) {
    (void)store;
    cursor->after = UINT32_MAX;
}
bool noah_lighting_history_store_next(const noah_lighting_history_store_t *store, uint8_t type, noah_lighting_history_cursor_t *cursor, noah_lighting_retained_t *cell) {
    uint32_t                  offset = 0u, best = UINT32_MAX;
    noah_lighting_batch_ref_t ref;
    cohort_t                  cohort;
    while (noah_lighting_record_next(store->arena, type, &offset, &ref)) {
        if (!load(store, ref, &cohort)) return false;
        uint8_t  thing = cohort.bytes[CELL + 1u];
        uint16_t first = cursor->after == UINT32_MAX ? 0u : (uint16_t)(cursor->after >> 8u) + (thing <= (uint8_t)cursor->after ? 1u : 0u);
        for (uint16_t rule = first; rule < 255u; rule++)
            if (member(&cohort, (uint8_t)rule)) {
                uint32_t key = rule * 256u + thing;
                if (key < best) {
                    best = key;
                    cell_from(&cohort, (uint8_t)rule, cell);
                }
                break;
            }
    }
    if (best == UINT32_MAX) return false;
    cursor->after = best;
    return true;
}
void noah_lighting_history_store_clear(noah_lighting_history_store_t *store, uint8_t type) {
    uint32_t                  offset = 0u;
    noah_lighting_batch_ref_t ref;
    cohort_t                  cohort;
    while (noah_lighting_record_next(store->arena, type, &offset, &ref)) {
        if (load(store, ref, &cohort)) {
            noah_lighting_retained_t cell;
            cell_from(&cohort, 0u, &cell);
            noah_lighting_retained_discard(&cell, store->arena);
        }
        (void)noah_lighting_batch_release(store->arena, ref);
        offset = 0u; // structural mutation invalidates byte offset
    }
}
bool noah_lighting_history_store_freeze(noah_lighting_history_store_t *store) {
    uint32_t                  offset = 0u, required = 0u;
    noah_lighting_batch_ref_t ref;
    while (noah_lighting_record_next(store->arena, NOAH_LIGHTING_ARENA_FRAME, &offset, &ref))
        return false;
    offset = 0u;
    while (noah_lighting_record_next(store->arena, NOAH_LIGHTING_ARENA_CURRENT, &offset, &ref)) {
        uint16_t size;
        if (!noah_lighting_batch_describe(store->arena, ref, NULL, NULL, &size, NULL)) return false;
        required += NOAH_LIGHTING_BATCH_ENTRY_HEADER + size;
    }
    if (required > store->arena->capacity - store->arena->used) return false;
    offset = 0u;
    cohort_t cohort;
    while (noah_lighting_record_next(store->arena, NOAH_LIGHTING_ARENA_CURRENT, &offset, &ref)) {
        if (!load(store, ref, &cohort)) goto failed;
        noah_lighting_retained_t cell;
        cell_from(&cohort, 0u, &cell);
        noah_lighting_batch_ref_t batch = noah_lighting_retained_batch(&cell), frame;
        if (batch && !noah_lighting_batch_retain(store->arena, batch)) goto failed;
        if (!noah_lighting_record_acquire(store->arena, NOAH_LIGHTING_ARENA_FRAME, 0u, noah_lighting_retained_thing(&cell), cohort.bytes, cohort.size, &frame)) {
            if (batch) (void)noah_lighting_batch_release(store->arena, batch);
            goto failed;
        }
    }
    return true;
failed:
    noah_lighting_history_store_clear(store, NOAH_LIGHTING_ARENA_FRAME);
    return false;
}

bool noah_lighting_history_store_group(noah_lighting_history_store_t *store, const noah_lighting_retained_t *cell, const uint8_t rules[32]) {
    if (!cell || !rules || (rules[31] & 0x80u) || !noah_lighting_retained_needed(cell)) return false;
    bool any = false;
    for (uint8_t i = 0u; i < 32u; i++)
        any |= rules[i] != 0u;
    if (!any) return false;
    uint8_t canonical[18];
    memcpy(canonical, cell->bytes, sizeof canonical);
    canonical[0] = 0u;
    cohort_t                  cohort;
    noah_lighting_batch_ref_t destination = 0u, ref;
    uint32_t                  offset      = 0u;
    while (noah_lighting_record_next(store->arena, NOAH_LIGHTING_ARENA_CURRENT, &offset, &ref))
        if (load(store, ref, &cohort) && !memcmp(cohort.bytes + CELL, canonical, sizeof canonical)) {
            destination = ref;
            break;
        }
    uint8_t mask[32];
    if (destination)
        mask_from(&cohort, mask);
    else {
        memcpy(cohort.bytes + CELL, canonical, sizeof canonical);
        memset(mask, 0, sizeof mask);
    }
    for (uint8_t i = 0u; i < 32u; i++)
        mask[i] |= rules[i];
    membership(&cohort, mask);
    // After destination ownership commits, every other mutation only shrinks
    // or releases metadata. No allocation/callback/failure remains in commit.
    if (destination) {
        if (!noah_lighting_record_update(store->arena, destination, cohort.bytes, cohort.size)) return false;
        noah_lighting_batch_ref_t batch = noah_lighting_retained_batch(cell);
        if (batch) (void)noah_lighting_batch_release(store->arena, batch);
    } else if (!noah_lighting_record_acquire(store->arena, NOAH_LIGHTING_ARENA_CURRENT, 0u, noah_lighting_retained_thing(cell), cohort.bytes, cohort.size, &destination))
        return false;
    offset = 0u;
    while (noah_lighting_record_next(store->arena, NOAH_LIGHTING_ARENA_CURRENT, &offset, &ref)) {
        if (ref == destination || !load(store, ref, &cohort) || cohort.bytes[CELL + 1u] != noah_lighting_retained_thing(cell)) continue;
        mask_from(&cohort, mask);
        bool changed = false;
        for (uint8_t i = 0u; i < 32u; i++) {
            if (mask[i] & rules[i]) changed = true;
            mask[i] &= (uint8_t)~rules[i];
        }
        if (!changed) continue;
        membership(&cohort, mask);
        if (cohort.size == MASK) {
            noah_lighting_retained_t previous;
            cell_from(&cohort, 0u, &previous);
            noah_lighting_retained_discard(&previous, store->arena);
            (void)noah_lighting_batch_release(store->arena, ref);
        } else
            (void)noah_lighting_record_update(store->arena, ref, cohort.bytes, cohort.size);
        // Byte offsets moved, so continue after this stable reference in a
        // fresh read-only scan; processed cohorts no longer intersect rules.
        offset = 0u;
    }
    return true;
}

bool noah_lighting_history_store_rule_next(const noah_lighting_history_store_t *store, uint8_t type, uint8_t rule, uint16_t *after, noah_lighting_retained_t *cell) {
    uint32_t                  offset = 0u;
    noah_lighting_batch_ref_t ref;
    uint16_t                  best = 256u;
    cohort_t                  cohort;
    while (noah_lighting_record_next(store->arena, type, &offset, &ref)) {
        if (!load(store, ref, &cohort)) return false;
        uint8_t thing = cohort.bytes[CELL + 1u];
        if ((*after == UINT16_MAX || thing > *after) && thing < best && member(&cohort, rule)) {
            best = thing;
            cell_from(&cohort, rule, cell);
        }
    }
    if (best == 256u) return false;
    *after = best;
    return true;
}
