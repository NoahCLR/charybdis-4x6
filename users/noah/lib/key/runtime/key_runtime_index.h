// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Index
// ────────────────────────────────────────────────────────────────────────────
//
// Explicit registry helpers for cross-slot coordination. The slot table
// remains the source of truth for storage; this layer keeps the active/pending
// working sets and common single-owner answers cached in shared state.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_state.h"

void                             key_runtime_index_sync_slot(active_key_state_t *slot);
const key_runtime_index_state_t *key_runtime_index_state_snapshot(void);
active_key_state_t              *key_runtime_active_slot_by_order(uint8_t order);
active_key_state_t              *key_runtime_pending_multi_tap_slot_by_order(uint8_t order);
active_key_state_t              *key_runtime_preview_owner_slot(void);
active_key_state_t              *key_runtime_pending_fallback_slot(void);
