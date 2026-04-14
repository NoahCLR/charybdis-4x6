// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Index Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Internal mutation hooks for keeping the runtime index in sync with slot
// state transitions. Public callers should only consume the read-only index
// accessors from key_runtime_index.h.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_state.h"

void key_runtime_index_sync_slot(active_key_state_t *slot);
