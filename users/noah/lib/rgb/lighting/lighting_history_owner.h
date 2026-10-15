#pragma once
#include "lighting_history_index.h"
#include "lighting_retained_group.h"

// Main-only owner over explicitly caller-owned index, cells and payload arena.
// Cells are dense occupied records, not a full rule×thing reservation.
// No storage is allocated here. The production caller must prove its complete
// accepted-profile capacity; begin failure is not a new profile admission rule.
typedef struct {
    const noah_profile_lighting_v1_program_t *program;
    const noah_lighting_history_index_t      *index;
    noah_lighting_retained_t                 *cells;
    noah_lighting_batch_arena_t              *arena;
    uint16_t                                  cursor, count, capacity;
} noah_lighting_history_owner_t;
bool noah_lighting_history_owner_begin(noah_lighting_history_owner_t *owner, const noah_profile_lighting_v1_program_t *program, const noah_lighting_history_index_t *index, noah_lighting_retained_t *cells, uint16_t capacity, noah_lighting_batch_arena_t *arena);
// Borrowed until the next owner mutation; pin/copy before crossing that boundary.
const noah_lighting_retained_t *noah_lighting_history_owner_find(const noah_lighting_history_owner_t *owner, uint8_t rule, uint8_t thing);
void                            noah_lighting_history_owner_discard(noah_lighting_history_owner_t *owner);
// The caller supplies a guarded forward local-clock delta once, before either
// observation or main transport consumption. This never resamples filters.
void noah_lighting_history_owner_advance(noah_lighting_history_owner_t *owner, uint32_t elapsed);
bool noah_lighting_history_owner_event(noah_lighting_history_owner_t *owner, const noah_lighting_event_group_t *group, bool source_delivery, uint8_t encoded[NOAH_LIGHTING_GROUP_BATCH_BYTES]);
// State observation follows one guarded advance. Batch is the actual complete
// ordered encoder output for this thing; filters and departures are observed by
// the coherent-frame caller. Full-match refresh never renews entry recency.
bool     noah_lighting_history_owner_state(noah_lighting_history_owner_t *owner, uint8_t rule, uint8_t thing, uint8_t observation, const uint8_t *batch, uint16_t size, uint64_t entry_order);
uint32_t noah_lighting_history_owner_wake(const noah_lighting_history_owner_t *owner);
enum { NOAH_LIGHTING_HISTORY_INVALID = -1, NOAH_LIGHTING_HISTORY_EMPTY = 0, NOAH_LIGHTING_HISTORY_READY = 1 };
// Fair dirty-cell selection; immutable RPC bytes own the complete single-event
// snapshot after offer. No payload reference is borrowed by the packet.
int  noah_lighting_history_owner_next(noah_lighting_history_owner_t *owner, noah_lighting_event_group_t *group);
void noah_lighting_history_owner_ack(noah_lighting_history_owner_t *owner, const noah_lighting_event_group_t *group);
// Fresh same-profile transport session: seed only live source events, or clear
// receiver events. Ordinary link failure must not call this. Profile/role reset
// discards the whole owner and all independent render pins before arena reset.
void noah_lighting_history_owner_session(noah_lighting_history_owner_t *owner, bool source);
