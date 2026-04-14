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

uint8_t                          key_runtime_active_slot_count(void);
uint8_t                          key_runtime_pending_multi_tap_slot_count(void);
active_key_state_t              *key_runtime_active_slot_by_order(uint8_t order);
active_key_state_t              *key_runtime_pending_multi_tap_slot_by_order(uint8_t order);
active_key_state_t              *key_runtime_preview_owner_slot(void);
active_key_state_t              *key_runtime_pending_fallback_slot(void);
