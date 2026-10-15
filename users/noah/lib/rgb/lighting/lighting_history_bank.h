#pragma once
#include "lighting_history_store.h"
#include "lighting_history_index.h"
#include "lighting_batch.h"

// Fully accounted caller-owned arena, no heap or rule×thing arrays. Runtime
// bank allocation must still prove accepted-profile capacity; this module does
// not turn a failed reserve into profile rejection, eviction or dropped input.
typedef struct {
    noah_lighting_history_store_t             store;
    const noah_profile_lighting_v1_program_t *program;
    uint16_t                                  dirty_after;
    bool                                      dirty_started;
} noah_lighting_history_bank_t;
void     noah_lighting_history_bank_begin(noah_lighting_history_bank_t *bank, const noah_profile_lighting_v1_program_t *program, noah_lighting_batch_arena_t *arena);
bool     noah_lighting_history_bank_event(noah_lighting_history_bank_t *bank, const noah_lighting_event_group_t *group, bool source, uint8_t encoded[1u + 3u + KEY_ORIGIN_BITMAP_SIZE]);
bool     noah_lighting_history_bank_state(noah_lighting_history_bank_t *bank, uint8_t rule, uint8_t thing, uint8_t observation, const uint8_t *batch, uint16_t size, uint64_t order);
// Calendar/ACK/session walks commit one independent cell at a time. A false
// result exposes an allocation/invariant failure, not a retry/drop policy; the
// production owner must prove full workspace capacity before enabling input.
bool     noah_lighting_history_bank_advance(noah_lighting_history_bank_t *bank, uint32_t elapsed);
uint32_t noah_lighting_history_bank_wake(const noah_lighting_history_bank_t *bank);
bool     noah_lighting_history_bank_next(noah_lighting_history_bank_t *bank, noah_lighting_event_group_t *group);
bool     noah_lighting_history_bank_ack(noah_lighting_history_bank_t *bank, const noah_lighting_event_group_t *group);
bool     noah_lighting_history_bank_session(noah_lighting_history_bank_t *bank, bool source);
