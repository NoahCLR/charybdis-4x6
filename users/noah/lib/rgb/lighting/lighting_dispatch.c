// ────────────────────────────────────────────────────────────────────────────
// Lighting Dispatch
// ────────────────────────────────────────────────────────────────────────────
//
// Every switch here is generated from the registry rows, so a kind reaches
// its module by a direct call (a reviewed stack edge, D-F13) and the engine
// names no kind. An id without a row falls to the default: nothing.

#include "lighting.h"

#include <string.h>

#ifdef RGB_MATRIX_ENABLE

bool noah_lighting_fact_next(uint8_t kind, const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    (void)facts;
    (void)params;
    (void)cursor;
    (void)subject;
    switch (kind) {
#    define NOAH_LIGHTING_FACT_CASE(id, name, thing, layout, slots, selector, signatures, live) \
        case id:                                             \
            return noah_lighting_fact_##name##_next(facts, params, cursor, subject);
        NOAH_LIGHTING_FACT_ROWS(NOAH_LIGHTING_FACT_CASE)
#    undef NOAH_LIGHTING_FACT_CASE
        default:
            return false;
    }
}

uint8_t noah_lighting_fact_subject_slots(uint8_t kind) {
    switch (kind) {
#    define NOAH_LIGHTING_FACT_SLOTS(id, name, thing, layout, slots, selector, signatures, live) case id: return slots;
        NOAH_LIGHTING_FACT_ROWS(NOAH_LIGHTING_FACT_SLOTS)
#    undef NOAH_LIGHTING_FACT_SLOTS
        default: return 1u;
    }
}
uint8_t noah_lighting_fact_selector_parameter(uint8_t kind) {
    switch (kind) {
#    define NOAH_LIGHTING_FACT_SELECTOR(id, name, thing, layout, slots, selector, signatures, live) case id: return selector;
        NOAH_LIGHTING_FACT_ROWS(NOAH_LIGHTING_FACT_SELECTOR)
#    undef NOAH_LIGHTING_FACT_SELECTOR
        default: return 255u;
    }
}

noah_lighting_leds_t noah_lighting_target(uint8_t kind, const noah_lighting_scene_t *scene, const uint8_t *params, const noah_lighting_subject_t *subject) {
    (void)scene;
    (void)params;
    (void)subject;
    switch (kind) {
#    define NOAH_LIGHTING_TARGET_CASE(id, name, layout, place) \
        case id:                                               \
            return noah_lighting_target_##name(scene, params, subject);
        NOAH_LIGHTING_TARGET_ROWS(NOAH_LIGHTING_TARGET_CASE)
#    undef NOAH_LIGHTING_TARGET_CASE
        default:
            return 0u;
    }
}

bool noah_lighting_paint(uint8_t kind, const noah_lighting_paint_context_t *context, rgb_t *colour) {
    (void)context;
    (void)colour;
    switch (kind) {
#    define NOAH_LIGHTING_PAINT_CASE(id, name, layout, below, timed) \
        case id:                                                     \
            return noah_lighting_paint_##name(context, colour);
        NOAH_LIGHTING_PAINT_ROWS(NOAH_LIGHTING_PAINT_CASE)
#    undef NOAH_LIGHTING_PAINT_CASE
        default:
            return false;
    }
}

bool noah_lighting_paint_changes_with_time(uint8_t kind) {
    switch (kind) {
#    define NOAH_LIGHTING_PAINT_TIMED_CASE(id, name, layout, below, timed) \
        case id:                                                           \
            return (timed) != 0;
        NOAH_LIGHTING_PAINT_ROWS(NOAH_LIGHTING_PAINT_TIMED_CASE)
#    undef NOAH_LIGHTING_PAINT_TIMED_CASE
        default:
            return false;
    }
}

uint8_t noah_lighting_timing(uint8_t kind, const uint8_t *params, uint32_t elapsed, uint32_t *next_change, bool *animating) {
    (void)params;
    (void)elapsed;
    *next_change = 0u;
    *animating   = false;
    switch (kind) {
#    define NOAH_LIGHTING_TIMING_CASE(id, name, layout, timed)                  \
        case id:                                                                \
            *animating = (timed) != 0;                                          \
            return noah_lighting_timing_##name(params, elapsed, next_change);
        NOAH_LIGHTING_TIMING_ROWS(NOAH_LIGHTING_TIMING_CASE)
#    undef NOAH_LIGHTING_TIMING_CASE
        default:
            return UINT8_MAX;
    }
}

// ─── The fact frame ─────────────────────────────────────────────────────────

// At most one module slice is staged. No full-frame duplicate or large
// stack copy is needed to compare collection or module-specific idle values.
typedef union {
    uint8_t none;
#    define NOAH_LIGHTING_SCRATCH(name, bytes, synced, gated) uint8_t name[bytes];
    NOAH_LIGHTING_SLICE_ROWS(NOAH_LIGHTING_SCRATCH)
#    undef NOAH_LIGHTING_SCRATCH
} slice_scratch_t;

bool __attribute__((noinline)) noah_lighting_facts_collect_changed(noah_lighting_facts_t *facts, bool local, bool synced) {
    slice_scratch_t scratch;
    bool changed = false;
    (void)scratch; (void)facts; (void)local; (void)synced;
#    define NOAH_LIGHTING_COLLECT_CHANGED(name, bytes, is_synced, gated) \
    if ((is_synced) ? synced : local) { \
        noah_lighting_slice_##name##_collect(scratch.name); \
        if (memcmp(facts->name, scratch.name, bytes)) { memcpy(facts->name, scratch.name, bytes); changed = true; } \
    }
    NOAH_LIGHTING_SLICE_ROWS(NOAH_LIGHTING_COLLECT_CHANGED)
#    undef NOAH_LIGHTING_COLLECT_CHANGED
    return changed;
}

void noah_lighting_facts_init_synced(noah_lighting_facts_t *facts) {
    (void)facts;
#    define NOAH_LIGHTING_INIT_SYNCED(name, bytes, is_synced, gated) if (is_synced) noah_lighting_slice_##name##_idle(facts->name);
    NOAH_LIGHTING_SLICE_ROWS(NOAH_LIGHTING_INIT_SYNCED)
#    undef NOAH_LIGHTING_INIT_SYNCED
}

bool noah_lighting_facts_unpack_synced_changed(noah_lighting_facts_t *facts, const uint8_t *frame) {
    size_t offset = 0u;
    bool changed = false;
    (void)facts; (void)frame; (void)offset;
#    define NOAH_LIGHTING_UNPACK_CHANGED(name, bytes, is_synced, gated) \
    if (is_synced) { \
        if (memcmp(facts->name, frame + offset, bytes)) { memcpy(facts->name, frame + offset, bytes); changed = true; } \
        offset += bytes; \
    }
    NOAH_LIGHTING_SLICE_ROWS(NOAH_LIGHTING_UNPACK_CHANGED)
#    undef NOAH_LIGHTING_UNPACK_CHANGED
    return changed;
}

bool __attribute__((noinline)) noah_lighting_facts_active(const uint8_t *frame, bool gated_only) {
    slice_scratch_t scratch;
    size_t offset = 0u;
    (void)scratch; (void)offset; (void)frame; (void)gated_only;
#    define NOAH_LIGHTING_CHECK_IDLE(name, bytes, is_synced, gated) \
    if (is_synced) { \
        if (!gated_only || (gated)) { \
            noah_lighting_slice_##name##_idle(scratch.name); \
            if (memcmp(frame + offset, scratch.name, bytes)) return true; \
        } \
        offset += bytes; \
    }
    NOAH_LIGHTING_SLICE_ROWS(NOAH_LIGHTING_CHECK_IDLE)
#    undef NOAH_LIGHTING_CHECK_IDLE
    return false;
}

void noah_lighting_facts_collect(noah_lighting_facts_t *facts, bool local, bool synced) {
    (void)facts;
    (void)local;
    (void)synced;
#    define NOAH_LIGHTING_SLICE_COLLECT(name, bytes, is_synced, gated) \
        if ((is_synced) ? synced : local) noah_lighting_slice_##name##_collect(facts->name);
    NOAH_LIGHTING_SLICE_ROWS(NOAH_LIGHTING_SLICE_COLLECT)
#    undef NOAH_LIGHTING_SLICE_COLLECT
}

void noah_lighting_facts_pack_synced(const noah_lighting_facts_t *facts, uint8_t *frame) {
    size_t offset = 0u;
    (void)facts;
    (void)frame;
    (void)offset;
#    define NOAH_LIGHTING_SLICE_PACK(name, bytes, is_synced, gated) \
        if (is_synced) {                                           \
            memcpy(&frame[offset], facts->name, (bytes));          \
            offset += (bytes);                                     \
        }
    NOAH_LIGHTING_SLICE_ROWS(NOAH_LIGHTING_SLICE_PACK)
#    undef NOAH_LIGHTING_SLICE_PACK
}

void noah_lighting_facts_unpack_synced(noah_lighting_facts_t *facts, const uint8_t *frame) {
    size_t offset = 0u;
    (void)facts;
    (void)frame;
    (void)offset;
#    define NOAH_LIGHTING_SLICE_UNPACK(name, bytes, is_synced, gated) \
        if (is_synced) {                                             \
            memcpy(facts->name, &frame[offset], (bytes));            \
            offset += (bytes);                                       \
        }
    NOAH_LIGHTING_SLICE_ROWS(NOAH_LIGHTING_SLICE_UNPACK)
#    undef NOAH_LIGHTING_SLICE_UNPACK
}

void noah_lighting_facts_build_synced(uint8_t *frame, bool refresh_gated) {
    size_t offset = 0u;
    (void)frame;
    (void)refresh_gated;
    (void)offset;
#    define NOAH_LIGHTING_SLICE_BUILD(name, bytes, is_synced, gated)               \
        if (is_synced) {                                                           \
            if (!(gated) || refresh_gated) noah_lighting_slice_##name##_collect(&frame[offset]); \
            offset += (bytes);                                                     \
        }
    NOAH_LIGHTING_SLICE_ROWS(NOAH_LIGHTING_SLICE_BUILD)
#    undef NOAH_LIGHTING_SLICE_BUILD
}

void noah_lighting_facts_build_idle(uint8_t *frame) {
    size_t offset = 0u;
    (void)frame;
    (void)offset;
#    define NOAH_LIGHTING_SLICE_IDLE(name, bytes, is_synced, gated) \
        if (is_synced) {                                           \
            noah_lighting_slice_##name##_idle(&frame[offset]);     \
            offset += (bytes);                                     \
        }
    NOAH_LIGHTING_SLICE_ROWS(NOAH_LIGHTING_SLICE_IDLE)
#    undef NOAH_LIGHTING_SLICE_IDLE
}

#endif // RGB_MATRIX_ENABLE
