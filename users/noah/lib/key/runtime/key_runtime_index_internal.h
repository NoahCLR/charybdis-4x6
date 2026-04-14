// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Index Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Internal mutation hooks for keeping the runtime index in sync with slot
// state transitions. Public callers should only consume the read-only index
// accessors from key_runtime_index.h.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_internal.h"

void key_runtime_index_sync_slot(active_key_state_t *slot);
uint8_t key_runtime_index_snapshot_active_slots(active_key_state_t **out_slots, uint8_t capacity);
uint8_t key_runtime_index_snapshot_pending_multi_tap_slots(active_key_state_t **out_slots, uint8_t capacity);
