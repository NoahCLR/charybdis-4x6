#include "lighting_history_store.h"
#include <string.h>

// One packed immutable assignment block per existing thing address. Rules,
// kind, thing and timing parameters are not repeated in each assignment.
enum { MASK = 0u, ORDER = 32u, BATCH = 40u, LIFE = 44u, FLAGS = 47u, WIDTH = 48u, HEADER = 52u, ADDRESSES = 129u };
typedef struct {
    uint8_t bytes[HEADER];
} block_t;
static uint8_t address(uint8_t thing) {
    return thing == 255u ? 128u : thing;
}
static uint8_t thing_at(uint8_t at) {
    return at == 128u ? 255u : at;
}
static uint64_t little(const uint8_t *bytes, uint8_t count) {
    uint64_t value = 0u;
    for (uint8_t i = 0; i < count; i++)
        value |= (uint64_t)bytes[i] << (8u * i);
    return value;
}
static void put(uint8_t *bytes, uint8_t count, uint64_t value) {
    for (uint8_t i = 0; i < count; i++)
        bytes[i] = (uint8_t)(value >> (8u * i));
}
static bool member(const block_t *block, uint8_t rule) {
    return rule < 255u && (block->bytes[rule / 8u] & (1u << (rule % 8u)));
}
static uint8_t bits(uint64_t value) {
    uint8_t count = 0;
    while (value) {
        count++;
        value >>= 1u;
    }
    return count;
}
static bool load(const noah_lighting_history_store_t *store, noah_lighting_batch_ref_t ref, block_t *block) {
    return ref && noah_lighting_batch_copy(store->arena, ref, 0u, block->bytes, HEADER);
}
static uint16_t rank(const block_t *block, uint8_t rule) {
    uint16_t n = 0;
    for (uint16_t r = 0; r < rule; r++)
        n += member(block, (uint8_t)r);
    return n;
}
static bool read_cell(const noah_lighting_history_store_t *store, noah_lighting_batch_ref_t ref, const block_t *block, uint8_t rule, uint8_t thing, noah_lighting_retained_t *cell) {
    if (!member(block, rule)) return false;
    uint16_t stride = 0u;
    for (uint8_t i = 0; i < 4; i++)
        stride += block->bytes[WIDTH + i];
    uint16_t start       = rank(block, rule) * stride;
    uint8_t  encoded[17] = {0};
    uint16_t count       = (start % 8u + stride + 7u) / 8u;
    if (count && !noah_lighting_batch_copy(store->arena, ref, HEADER + start / 8u, encoded, count)) return false;
    uint16_t at        = start % 8u;
    uint64_t values[4] = {0};
    for (uint8_t field = 0; field < 4; field++)
        for (uint8_t bit = 0; bit < block->bytes[WIDTH + field]; bit++, at++)
            values[field] |= (uint64_t)((encoded[at / 8u] >> (at % 8u)) & 1u) << bit;
    noah_lighting_retained_init(cell, rule, thing);
    put(cell->bytes + 6u, 8u, little(block->bytes + ORDER, 8u) + values[0]);
    put(cell->bytes + 14u, 4u, little(block->bytes + BATCH, 4u) + values[1]);
    put(cell->bytes + 3u, 3u, little(block->bytes + LIFE, 3u) + values[2]);
    cell->bytes[2] = block->bytes[FLAGS] + values[3];
    return true;
}
static noah_lighting_batch_ref_t reference(const noah_lighting_history_store_t *store, uint8_t type, uint8_t thing) {
    if (thing >= 128u && thing != 255u) return 0u;
    return type == NOAH_LIGHTING_ARENA_FRAME ? store->frame[address(thing)] : store->current[address(thing)];
}
void noah_lighting_history_store_init(noah_lighting_history_store_t *store, noah_lighting_batch_arena_t *arena) {
    memset(store, 0, sizeof *store);
    store->arena = arena;
}
bool noah_lighting_history_store_read(const noah_lighting_history_store_t *store, uint8_t type, uint8_t rule, uint8_t thing, noah_lighting_retained_t *cell) {
    block_t                   block;
    noah_lighting_batch_ref_t ref = reference(store, type, thing);
    return cell && load(store, ref, &block) && read_cell(store, ref, &block, rule, thing, cell);
}
static void release_block(noah_lighting_history_store_t *store, noah_lighting_batch_ref_t ref) {
    if (!ref) return;
    uint32_t owners;
    uint8_t  thing;
    block_t  block;
    if (noah_lighting_batch_describe(store->arena, ref, NULL, &thing, NULL, &owners) && owners == 1u && load(store, ref, &block))
        for (uint16_t rule = 0; rule < 255u; rule++) {
            noah_lighting_retained_t cell;
            if (read_cell(store, ref, &block, (uint8_t)rule, thing, &cell)) {
                noah_lighting_batch_ref_t batch = noah_lighting_retained_batch(&cell);
                if (batch) (void)noah_lighting_batch_release(store->arena, batch);
            }
        }
    (void)noah_lighting_batch_release(store->arena, ref);
}
typedef struct {
    const noah_lighting_retained_t *cell;
    const uint8_t                  *rules;
    bool                            remove, frame;
} patch_t;
static bool patched(const patch_t *patch, uint8_t rule) {
    if (patch->frame) return false;
    return patch->rules ? (patch->rules[rule / 8u] & (1u << (rule % 8u))) : noah_lighting_retained_rule(patch->cell) == rule;
}
static bool projected(const noah_lighting_history_store_t *store, noah_lighting_batch_ref_t old, const block_t *header, uint8_t thing, uint8_t rule, const patch_t *patch, noah_lighting_retained_t *cell) {
    if (patch->frame) {
        if (!old || !read_cell(store, old, header, rule, thing, cell) || !noah_lighting_retained_rendering(cell) || !noah_lighting_retained_batch(cell)) return false;
        cell->bytes[2] &= (uint8_t)~1u; // frozen rendering does not own delivery
        return true;
    }
    if (patched(patch, rule)) {
        if (patch->remove || !noah_lighting_retained_needed(patch->cell)) return false;
        *cell          = *patch->cell;
        cell->bytes[0] = rule;
        cell->bytes[1] = thing;
        return true;
    }
    return old && read_cell(store, old, header, rule, thing, cell);
}
static void values_of(const noah_lighting_retained_t *cell, uint64_t values[4]) {
    values[0] = noah_lighting_retained_order(cell);
    values[1] = noah_lighting_retained_batch(cell);
    values[2] = little(cell->bytes + 3u, 3u);
    values[3] = cell->bytes[2];
}
typedef struct {
    uint16_t offset;
    uint8_t  byte, used;
} writer_t;
static bool write_bits(noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t ref, writer_t *writer, uint64_t value, uint8_t width) {
    for (uint8_t bit = 0; bit < width; bit++) {
        writer->byte |= (uint8_t)(((value >> bit) & 1u) << writer->used++);
        if (writer->used == 8u) {
            if (!noah_lighting_record_store(arena, ref, writer->offset++, &writer->byte, 1u)) return false;
            writer->byte = writer->used = 0u;
        }
    }
    return true;
}
static bool rebuild(noah_lighting_history_store_t *store, uint8_t thing, const patch_t *patch) {
    if (thing >= 128u && thing != 255u) return false;
    uint8_t                   index      = address(thing);
    noah_lighting_batch_ref_t old        = store->current[index];
    block_t                   old_header = {0}, next = {{0}};
    if (old && !load(store, old, &old_header)) return false;
    uint64_t minimum[4] = {UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX}, maximum[4] = {0};
    uint16_t members = 0u;
    for (uint16_t rule = 0; rule < 255u; rule++) {
        noah_lighting_retained_t cell;
        if (!projected(store, old, &old_header, thing, (uint8_t)rule, patch, &cell)) continue;
        next.bytes[rule / 8u] |= 1u << (rule % 8u);
        members++;
        uint64_t values[4];
        values_of(&cell, values);
        for (uint8_t field = 0; field < 4; field++) {
            if (values[field] < minimum[field]) minimum[field] = values[field];
            if (values[field] > maximum[field]) maximum[field] = values[field];
        }
    }
    if (!members) {
        if (patch->frame)
            store->frame[index] = 0u;
        else {
            store->current[index] = 0u;
            release_block(store, old);
        }
        return true;
    }
    uint16_t stride = 0u;
    for (uint8_t field = 0; field < 4; field++) {
        next.bytes[WIDTH + field] = bits(maximum[field] - minimum[field]);
        stride += next.bytes[WIDTH + field];
    }
    put(next.bytes + ORDER, 8u, minimum[0]);
    put(next.bytes + BATCH, 4u, minimum[1]);
    put(next.bytes + LIFE, 3u, minimum[2]);
    next.bytes[FLAGS]              = (uint8_t)minimum[3];
    uint16_t                  size = HEADER + (members * stride + 7u) / 8u;
    noah_lighting_batch_ref_t reserved;
    if (!noah_lighting_record_reserve(store->arena, thing, size, &reserved)) return false;
    if (!noah_lighting_record_store(store->arena, reserved, 0u, next.bytes, HEADER)) {
        (void)noah_lighting_batch_release(store->arena, reserved);
        return false;
    }
    writer_t writer         = {.offset = HEADER};
    uint16_t acquired_until = 0u;
    bool     supplied       = false;
    for (uint16_t rule = 0; rule < 255u; rule++) {
        noah_lighting_retained_t cell;
        if (!projected(store, old, &old_header, thing, (uint8_t)rule, patch, &cell)) continue;
        noah_lighting_batch_ref_t batch    = noah_lighting_retained_batch(&cell);
        bool                      incoming = patched(patch, (uint8_t)rule) && !supplied;
        if (batch && !incoming && !noah_lighting_batch_retain(store->arena, batch)) goto failed;
        if (incoming) supplied = true;
        acquired_until = rule + 1u;
        uint64_t values[4];
        values_of(&cell, values);
        for (uint8_t field = 0; field < 4; field++)
            if (!write_bits(store->arena, reserved, &writer, values[field] - minimum[field], next.bytes[WIDTH + field])) goto failed;
    }
    if (writer.used && !noah_lighting_record_store(store->arena, reserved, writer.offset, &writer.byte, 1u)) goto failed;
    if (patch->frame)
        store->frame[index] = reserved;
    else {
        store->current[index] = reserved;
        release_block(store, old);
    }
    return true;
failed:
    supplied = false;
    for (uint16_t rule = 0; rule < acquired_until; rule++) {
        noah_lighting_retained_t cell;
        if (!projected(store, old, &old_header, thing, (uint8_t)rule, patch, &cell)) continue;
        bool incoming = patched(patch, (uint8_t)rule) && !supplied;
        if (incoming) supplied = true;
        noah_lighting_batch_ref_t batch = noah_lighting_retained_batch(&cell);
        if (batch && !incoming) (void)noah_lighting_batch_release(store->arena, batch);
    }
    (void)noah_lighting_batch_release(store->arena, reserved);
    return false;
}
bool noah_lighting_history_store_write(noah_lighting_history_store_t *store, const noah_lighting_retained_t *cell) {
    if (!cell || noah_lighting_retained_rule(cell) == 255u) return false;
    patch_t patch = {.cell = cell};
    return rebuild(store, noah_lighting_retained_thing(cell), &patch);
}
void noah_lighting_history_store_remove(noah_lighting_history_store_t *store, uint8_t rule, uint8_t thing) {
    noah_lighting_retained_t cell;
    noah_lighting_retained_init(&cell, rule, thing);
    patch_t patch = {.cell = &cell, .remove = true};
    (void)rebuild(store, thing, &patch);
}
bool noah_lighting_history_store_group(noah_lighting_history_store_t *store, const noah_lighting_retained_t *cell, const uint8_t rules[32]) {
    if (!cell || !rules || rules[31] & 0x80u || !noah_lighting_retained_needed(cell)) return false;
    bool any = false;
    for (uint8_t i = 0; i < 32u; i++)
        any |= rules[i] != 0u;
    if (!any) return false;
    patch_t patch = {.cell = cell, .rules = rules};
    return rebuild(store, noah_lighting_retained_thing(cell), &patch);
}
void noah_lighting_history_store_cursor(const noah_lighting_history_store_t *store, noah_lighting_history_cursor_t *cursor) {
    (void)store;
    cursor->after = UINT32_MAX;
}
bool noah_lighting_history_store_next(const noah_lighting_history_store_t *store, uint8_t type, noah_lighting_history_cursor_t *cursor, noah_lighting_retained_t *cell) {
    uint32_t start = cursor->after == UINT32_MAX ? 0u : cursor->after + 1u;
    for (uint16_t rule = start / 256u; rule < 255u; rule++)
        for (uint16_t at = 0; at < ADDRESSES; at++) {
            uint8_t  thing = thing_at((uint8_t)at);
            uint32_t key   = rule * 256u + thing;
            if (key < start) continue;
            if (noah_lighting_history_store_read(store, type, (uint8_t)rule, thing, cell)) {
                cursor->after = key;
                return true;
            }
        }
    return false;
}
bool noah_lighting_history_store_rule_next(const noah_lighting_history_store_t *store, uint8_t type, uint8_t rule, uint16_t *after, noah_lighting_retained_t *cell) {
    for (uint16_t at = 0; at < ADDRESSES; at++) {
        uint8_t thing = thing_at((uint8_t)at);
        if (*after != UINT16_MAX && thing <= *after) continue;
        if (noah_lighting_history_store_read(store, type, rule, thing, cell)) {
            *after = thing;
            return true;
        }
    }
    return false;
}
void noah_lighting_history_store_clear(noah_lighting_history_store_t *store, uint8_t type) {
    noah_lighting_batch_ref_t *refs = type == NOAH_LIGHTING_ARENA_FRAME ? store->frame : store->current;
    for (uint16_t at = 0; at < ADDRESSES; at++) {
        noah_lighting_batch_ref_t ref = refs[at];
        refs[at]                      = 0;
        release_block(store, ref);
    }
}
bool noah_lighting_history_store_freeze(noah_lighting_history_store_t *store) {
    for (uint16_t at = 0; at < ADDRESSES; at++)
        if (store->frame[at]) return false;
    for (uint16_t at = 0; at < ADDRESSES; at++)
        if (store->current[at]) {
            block_t block;
            bool    all = true, any = false;
            if (!load(store, store->current[at], &block)) goto failed;
            for (uint16_t rule = 0; rule < 255u; rule++) {
                noah_lighting_retained_t cell;
                if (!read_cell(store, store->current[at], &block, (uint8_t)rule, thing_at((uint8_t)at), &cell)) continue;
                bool rendering = noah_lighting_retained_rendering(&cell) && noah_lighting_retained_batch(&cell);
                all &= rendering;
                any |= rendering;
            }
            if (!any) continue; // delivery-only expired markers never enter a frame
            if (all) {
                if (!noah_lighting_batch_retain(store->arena, store->current[at])) goto failed;
                store->frame[at] = store->current[at];
            } else {
                patch_t frame = {.frame = true};
                if (!rebuild(store, thing_at((uint8_t)at), &frame)) goto failed;
            }
        }
    return true;
failed:
    noah_lighting_history_store_clear(store, NOAH_LIGHTING_ARENA_FRAME);
    return false;
}
