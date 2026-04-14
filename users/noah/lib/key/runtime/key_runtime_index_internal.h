// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Index Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Internal read and mutation hooks for key-runtime owner modules and the
// allowlisted white-box host suites. This surface exposes storage-shaped slot
// access and is not part of the public cross-module runtime seam.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_internal.h"

uint8_t             key_runtime_active_slot_count(void);
uint8_t             key_runtime_pending_multi_tap_slot_count(void);
active_key_state_t *key_runtime_active_slot_by_order(uint8_t order);
active_key_state_t *key_runtime_pending_multi_tap_slot_by_order(uint8_t order);
active_key_state_t *key_runtime_preview_owner_slot(void);
active_key_state_t *key_runtime_pending_fallback_slot(void);
void                key_runtime_index_sync_slot(active_key_state_t *slot);
uint8_t             key_runtime_index_snapshot_active_slots(active_key_state_t **out_slots, uint8_t capacity);
uint8_t             key_runtime_index_snapshot_pending_multi_tap_slots(active_key_state_t **out_slots, uint8_t capacity);
