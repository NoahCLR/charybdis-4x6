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
uint8_t             key_runtime_deferred_release_blocker_count(void);
active_key_state_t *key_runtime_deferred_release_blocker_slot_by_order(uint8_t order);
void                key_runtime_index_sync_slot(active_key_state_t *slot);
void                key_runtime_index_refresh_timed_deferred_release_blockers(void);
uint8_t             key_runtime_index_snapshot_active_slots(active_key_state_t **out_slots, uint8_t capacity);
uint8_t             key_runtime_index_snapshot_pending_multi_tap_slots(active_key_state_t **out_slots, uint8_t capacity);
bool                key_runtime_index_has_any_deferred_release_blocker(void);
bool                key_runtime_index_has_foreign_deferred_release_blocker_except(keypos_t key_pos);
