#pragma once
#include "lighting_retained.h"
#include "lighting_event_protocol.h"

// One arena owns full-equality payloads and immutable factored assignment
// blocks. Each rule/thing keeps exact independent order, lifetime and delivery;
// columns share constants/deltas without requiring whole-cell equality. Current
// and frozen references use the existing registry bound128 identities + board.
// No production bank allocation or accepted-profile fit is selected here.
enum { NOAH_LIGHTING_HISTORY_ADDRESSES = 129u };
typedef struct {
    noah_lighting_batch_arena_t *arena;
    noah_lighting_batch_ref_t    current[NOAH_LIGHTING_HISTORY_ADDRESSES], frame[NOAH_LIGHTING_HISTORY_ADDRESSES];
} noah_lighting_history_store_t;
void noah_lighting_history_store_init(noah_lighting_history_store_t *store, noah_lighting_batch_arena_t *arena);
bool noah_lighting_history_store_read(const noah_lighting_history_store_t *store, uint8_t type, uint8_t rule, uint8_t thing, noah_lighting_retained_t *cell);
// Consumes cell's one owned batch reference ONLY on success; caller discards
// it on failure. Existing metadata/payload references remain coherent on failure.
bool noah_lighting_history_store_write(noah_lighting_history_store_t *store, const noah_lighting_retained_t *cell);
void noah_lighting_history_store_remove(noah_lighting_history_store_t *store, uint8_t rule, uint8_t thing);
// Iteration is by logical (rule,thing), independent of reference wrap or
// compaction. Between steps a main caller may update/remove only the returned
// key; any newly materialized metadata for that key is not visited twice.
typedef struct {
    uint32_t after;
} noah_lighting_history_cursor_t;
void noah_lighting_history_store_cursor(const noah_lighting_history_store_t *store, noah_lighting_history_cursor_t *cursor);
bool noah_lighting_history_store_next(const noah_lighting_history_store_t *store, uint8_t type, noah_lighting_history_cursor_t *cursor, noah_lighting_retained_t *cell);
// Frozen metadata/batches stay owned across capture, ACK, expiry and
// compaction. Fully consumable immutable blocks are pinned; mixed blocks are
// filtered into owned render-only assignments. Expired delivery-only markers
// never enter the frame. Failed reserve releases partial pins and keeps current
// state. Current replacement also accounts complete old/new block overlap.
bool noah_lighting_history_store_freeze(noah_lighting_history_store_t *store);
void noah_lighting_history_store_clear(noah_lighting_history_store_t *store, uint8_t type);
// Atomic same-occurrence group install. Consumes one owned payload reference
// only on success. All members receive this template; nonmembers keep their
// independent previous histories. Caller removes duplicate versions first.
bool noah_lighting_history_store_group(noah_lighting_history_store_t *store, const noah_lighting_retained_t *cell, const uint8_t rules[32]);
// Rule-local scan avoids walking other rules' memberships during painting.
// Set after to UINT16_MAX before the first thing; reads return ascending ids.
bool noah_lighting_history_store_rule_next(const noah_lighting_history_store_t *store, uint8_t type, uint8_t rule, uint16_t *after, noah_lighting_retained_t *cell);
