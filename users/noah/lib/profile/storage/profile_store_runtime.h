// ─────────────────────────────────────────────────────────────────────────
// Live-Profile Store Boot Owner
// ────────────────────────────────────────────────────────────────────────
#pragma once

#include "profile_store.h"
#include "profile_store_runtime_hooks.h"
#include "../runtime/profile_owner.h"

typedef enum {
    NOAH_PROFILE_STORE_RUNTIME_UNINITIALIZED = 0u,
    NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING,
    NOAH_PROFILE_STORE_RUNTIME_COMPILED_FALLBACK,
    NOAH_PROFILE_STORE_RUNTIME_COMMITTED_FOUND,
    NOAH_PROFILE_STORE_RUNTIME_GENERATION_CONFLICT,
    NOAH_PROFILE_STORE_RUNTIME_STORAGE_ERROR,
    NOAH_PROFILE_STORE_RUNTIME_INTEGRATION_ERROR,
} noah_profile_store_runtime_state_t;

noah_profile_store_runtime_state_t noah_profile_store_runtime_state(void);
noah_profile_store_result_t noah_profile_store_runtime_discovery_result(void);
const noah_profile_store_record_t *noah_profile_store_runtime_committed(void);

// Narrow engineering bridge. Ordinary/read-only builds return false; the
// static runtime owner is never exposed for direct mutation.
bool noah_profile_store_runtime_owner_status(noah_profile_owner_status_t *status);
bool noah_profile_store_runtime_candidate_status(noah_profile_candidate_v1_status_t *status);
bool noah_profile_store_runtime_candidate_receive(uint8_t *frame, size_t length);
