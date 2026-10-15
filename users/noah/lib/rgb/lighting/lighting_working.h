#pragma once
#include "lighting.h"
#include "lighting_event_protocol.h"
#include "lighting_lifetime.h"

// Direct, profile-sized reservations inside the one advertised fixed pool.
// Current/split snapshot banks and replacement workspace are all charged by Q.
// No heap, interning, reserve failure or history eviction after admission.
// Every admitted retained identity reserves at least one aligned member slot
// in BOTH banks. This is a derived metadata bound, never another admission cap.
enum { NOAH_LIGHTING_WORKING_MIN_SLOT = (12u + 5u + KEY_ORIGIN_BITMAP_SIZE + 3u) & ~3u,
       NOAH_LIGHTING_WORKING_CELL_BOUND = NOAH_LIGHTING_WORKING_CAPACITY / (2u * NOAH_LIGHTING_WORKING_MIN_SLOT),
       NOAH_LIGHTING_WORKING_DIRTY_BYTES = (NOAH_LIGHTING_WORKING_CELL_BOUND + 7u) / 8u };
typedef struct noah_lighting_working {
    const noah_profile_lighting_v1_program_t *program;
    uint8_t                                  *bytes;
    uint32_t                                  last_time, generation, revision;
    uint16_t                                  bank_bytes, scratch_bytes;
    uint8_t                                   rules, current;
    bool                                      clock_ready, snapshot_ready, dirty, render_dirty, staging_delta;
    uint8_t                                   changed_cells[NOAH_LIGHTING_WORKING_DIRTY_BYTES], changed_orders[NOAH_LIGHTING_WORKING_DIRTY_BYTES];
    uint8_t                                   snapshot_cells[NOAH_LIGHTING_WORKING_DIRTY_BYTES], snapshot_orders[NOAH_LIGHTING_WORKING_DIRTY_BYTES];
} noah_lighting_working_t;
typedef struct {
    uint32_t offset;
    uint16_t stride, count;
    uint8_t  rule, selected, members, flags;
} noah_lighting_working_rule_t;
_Static_assert(sizeof(noah_lighting_working_rule_t) == 12u, "cost model descriptor");

enum { NOAH_LIGHTING_WORKING_EMPTY, NOAH_LIGHTING_WORKING_MATCH, NOAH_LIGHTING_WORKING_TAIL, NOAH_LIGHTING_WORKING_FLASH };
enum { NOAH_LIGHTING_WORKING_ELIGIBLE = 1u, NOAH_LIGHTING_WORKING_NEW_ENTRY = 2u };
bool           noah_lighting_working_begin(noah_lighting_working_t *, const noah_profile_lighting_v1_program_t *, uint8_t *, uint32_t capacity, uint32_t generation);
bool           noah_lighting_working_rule(const noah_lighting_working_t *, uint8_t ordinal, noah_lighting_working_rule_t *);
const uint8_t *noah_lighting_working_slot(const noah_lighting_working_t *, bool snapshot, const noah_lighting_working_rule_t *, uint16_t thing);
void           noah_lighting_working_advance(noah_lighting_working_t *, uint32_t now);
bool           noah_lighting_working_event(noah_lighting_working_t *, const noah_lighting_event_group_t *);
bool           noah_lighting_working_observe(noah_lighting_working_t *, const noah_lighting_facts_t *);
void           noah_lighting_working_snapshot(noah_lighting_working_t *);
void           noah_lighting_working_release_snapshot(noah_lighting_working_t *);
// A false result leaves current bank/generation untouched. Successful commit
// only swaps already-owned storage after complete validation/age adjustment.
bool noah_lighting_working_commit(noah_lighting_working_t *, uint16_t age_lower, uint16_t age_upper);
bool noah_lighting_working_visible(const uint8_t *, const noah_lighting_lifetime_parameters_t *);
void noah_lighting_working_member(const uint8_t *, uint8_t kind, uint8_t thing, noah_lighting_subject_t *, uint16_t *natural);

// Highest order key paints last. Streaming owns one subject, not a placement
// array; every admitted member is considered, including distinct owners.
typedef void (*noah_lighting_working_paint_fn)(void *, const noah_lighting_subject_t *);
bool     noah_lighting_working_paint(const noah_lighting_working_t *, uint8_t rule, noah_lighting_working_paint_fn, void *);
uint32_t noah_lighting_working_wake(const noah_lighting_working_t *);
// Explicit little-endian descriptor encoding; slot encoding already consists
// solely of specified bytes. Receiver compares descriptors before staging.
bool noah_lighting_working_read_snapshot(const noah_lighting_working_t *, uint16_t offset, uint8_t *, uint8_t length);
bool noah_lighting_working_stage(noah_lighting_working_t *, uint16_t offset, const uint8_t *, uint8_t length);

// Re-observe the published coherent remote matching scene without requalifying
// or replacing its captured subjects. Local show-after clocks may mature.
void noah_lighting_working_seen(noah_lighting_working_t *);

// Compact complete snapshot ranges contain only occupied slot headers/members.
// Unsent reserved space is canonically zero; native descriptors stay local.
bool noah_lighting_working_snapshot_span(const noah_lighting_working_t *, uint16_t from, uint16_t *offset, uint8_t *, uint8_t *length, bool delta);
void noah_lighting_working_stage_empty(noah_lighting_working_t *);

void noah_lighting_working_stage_delta(noah_lighting_working_t *);

bool noah_lighting_working_snapshot_orders(const noah_lighting_working_t *, uint16_t from, uint16_t *first, uint8_t *out, uint8_t *length);
bool noah_lighting_working_stage_orders(noah_lighting_working_t *, uint16_t first, const uint8_t *, uint8_t length);

#if UINTPTR_MAX == UINT32_MAX && NOAH_LIGHTING_MATRIX_KEYS >= 57u && NOAH_LIGHTING_MATRIX_KEYS <= 64u && NOAH_LIGHTING_WORKING_CAPACITY == 8220u
_Static_assert(NOAH_LIGHTING_WORKING_CELL_BOUND == 146u && NOAH_LIGHTING_WORKING_DIRTY_BYTES == 19u, "admission-derived per-cell metadata bound");
_Static_assert(sizeof(noah_lighting_working_t) == 108u, "recorded owned control includes all four dirty masks");
#endif
