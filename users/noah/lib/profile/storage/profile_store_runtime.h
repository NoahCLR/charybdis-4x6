// ─────────────────────────────────────────────────────────────────────────
// Live-Profile Store Boot Owner
// ────────────────────────────────────────────────────────────────────────
#pragma once

#include "profile_store.h"
#include "profile_store_runtime_hooks.h"

typedef enum {
    NOAH_PROFILE_STORE_RUNTIME_UNINITIALIZED = 0u,
    NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING,
    NOAH_PROFILE_STORE_RUNTIME_COMPILED_FALLBACK,
    NOAH_PROFILE_STORE_RUNTIME_COMMITTED_FOUND,
    NOAH_PROFILE_STORE_RUNTIME_GENERATION_CONFLICT,
    NOAH_PROFILE_STORE_RUNTIME_STORAGE_ERROR,
} noah_profile_store_runtime_state_t;

noah_profile_store_runtime_state_t noah_profile_store_runtime_state(void);
noah_profile_store_result_t noah_profile_store_runtime_discovery_result(void);
const noah_profile_store_record_t *noah_profile_store_runtime_committed(void);
