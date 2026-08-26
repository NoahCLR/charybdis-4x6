// ───────────────────────────────────────────────────────────────────────────
// Live-Profile Activation Safety Policy
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../../state/shared/runtime_publication.h"

enum {
    NOAH_PROFILE_ACTIVATION_REASON_PHYSICAL_PRESS       = 1u << 0,
    NOAH_PROFILE_ACTIVATION_REASON_TAP_SERIES           = 1u << 1,
    NOAH_PROFILE_ACTIVATION_REASON_RUNTIME_LEASE        = 1u << 2,
    NOAH_PROFILE_ACTIVATION_REASON_DEFERRED_RELEASE     = 1u << 3,
    NOAH_PROFILE_ACTIVATION_REASON_PERSISTENT_INTENT    = 1u << 4,
    NOAH_PROFILE_ACTIVATION_REASON_OWNED_OUTPUT         = 1u << 5,
    NOAH_PROFILE_ACTIVATION_REASON_MODIFIER_OR_ONESHOT  = 1u << 6,
    NOAH_PROFILE_ACTIVATION_REASON_ONESHOT_LAYER        = 1u << 7,
    NOAH_PROFILE_ACTIVATION_REASON_MACRO                = 1u << 8,
    NOAH_PROFILE_ACTIVATION_REASON_COMBO                = 1u << 9,
    NOAH_PROFILE_ACTIVATION_REASON_PEER                 = 1u << 10,
    NOAH_PROFILE_ACTIVATION_REASON_INTERNAL             = 1u << 30,
};

typedef struct {
    uint8_t  physical_press_count;
    uint8_t  tap_series_count;
    uint8_t  runtime_lease_count;
    uint8_t  deferred_release_count;
    uint8_t  persistent_intent_count;
    uint16_t owned_output_count;
    uint8_t  real_mods;
    uint8_t  weak_mods;
    uint8_t  oneshot_mods;
    uint8_t  oneshot_locked_mods;
    uint8_t  oneshot_layer_active;
    uint8_t  macro_engine_state;
    uint8_t  macro_hold_count;
    uint8_t  combo_pending_count;
    uint8_t  combo_active_count;
    uint8_t  unresolved_peer_count;
} noah_profile_activation_snapshot_t;

// A split owner returns true only after it wrote an exact unresolved count.
// A missing/failed observer is deliberately treated as one unresolved peer.
typedef bool (*noah_profile_activation_peer_observer_fn)(void *context, uint8_t *unresolved_count);

typedef struct {
    noah_runtime_publication_generation_t publication_sequence;
    noah_profile_activation_peer_observer_fn peer_observer;
    void                                    *peer_context;
    noah_profile_activation_snapshot_t       last_snapshot;
    uint32_t                                 last_reason_mask;
    uint32_t                                 evaluation_count;
    bool                                     initialized;
} noah_profile_activation_policy_t;

void     noah_profile_activation_policy_init(noah_profile_activation_policy_t *policy, noah_profile_activation_peer_observer_fn peer_observer, void *peer_context);
uint32_t noah_profile_activation_reason_mask(const noah_profile_activation_snapshot_t *snapshot);
uint32_t noah_profile_activation_policy_safe_boundary(void *context);
bool     noah_profile_activation_policy_status(const noah_profile_activation_policy_t *policy, noah_profile_activation_snapshot_t *snapshot, uint32_t *reason_mask, uint32_t *evaluation_count);
