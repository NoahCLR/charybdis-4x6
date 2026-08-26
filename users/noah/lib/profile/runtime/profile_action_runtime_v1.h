// ────────────────────────────────────────────────────────────────────────────
// Profile-Wire v1 Semantic Action Runtime Translation
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>

#include "../schema/profile_blob_v1.h"

typedef enum {
    NOAH_PROFILE_ACTION_RUNTIME_V1_OK = 0u,
    NOAH_PROFILE_ACTION_RUNTIME_V1_INVALID_ARGUMENT,
    NOAH_PROFILE_ACTION_RUNTIME_V1_UNSUPPORTED,
} noah_profile_action_runtime_v1_result_t;

// These are the only native/semantic translations used by live-profile
// consumers. Whole-profile validation and the action-ABI digest must already
// have accepted a persisted profile before to_native() is called.
noah_profile_action_runtime_v1_result_t noah_profile_action_runtime_v1_from_native(uint16_t native_action, noah_profile_action_v1_t *action);
noah_profile_action_runtime_v1_result_t noah_profile_action_runtime_v1_to_native(const noah_profile_action_v1_t *action, uint16_t *native_action);
