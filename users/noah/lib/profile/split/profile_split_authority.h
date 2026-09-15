// ──────────────────────────────────────────────────────────────────────────
// Live-Profile Split Authority
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../../state/shared/runtime_publication.h"

enum {
    NOAH_PROFILE_SPLIT_AUTHORITY_STATE_BUDGET_32BIT = 128u,
};

typedef enum {
    NOAH_PROFILE_SPLIT_AUTHORITY_UNINITIALIZED = 0u,
    NOAH_PROFILE_SPLIT_AUTHORITY_LOCAL_UNREADABLE,
    NOAH_PROFILE_SPLIT_AUTHORITY_PEER_UNREADABLE,
    NOAH_PROFILE_SPLIT_AUTHORITY_INVALID_METADATA,
    NOAH_PROFILE_SPLIT_AUTHORITY_INCOMPATIBLE,
    NOAH_PROFILE_SPLIT_AUTHORITY_COMPILED_CONVERGED,
    NOAH_PROFILE_SPLIT_AUTHORITY_COMMITTED_CONVERGED,
    NOAH_PROFILE_SPLIT_AUTHORITY_LOCAL_NEWER,
    NOAH_PROFILE_SPLIT_AUTHORITY_PEER_NEWER,
    NOAH_PROFILE_SPLIT_AUTHORITY_CONCURRENT_COMMIT,
    NOAH_PROFILE_SPLIT_AUTHORITY_CORRUPT_SAME_TUPLE,
} noah_profile_split_authority_state_t;

// Immutable durable metadata exchanged by the sibling profile reconciler.
// compiled_default_digest and action_abi_digest describe the firmware even
// when has_profile is false.
typedef struct {
    uint32_t generation;
    uint32_t payload_crc32;
    uint32_t payload_digest;
    uint32_t compiled_default_digest;
    uint32_t action_abi_digest;
    uint16_t payload_length;
    uint8_t  schema_major;
    uint8_t  schema_minor;
    uint8_t  domain_mask;
    uint8_t  profile_flags;
    uint8_t  origin_half;
    bool     readable : 1;
    bool     has_profile : 1;
    bool     logical : 1;
} noah_profile_split_descriptor_t;

typedef struct {
    noah_profile_split_descriptor_t      local;
    noah_profile_split_descriptor_t      peer;
    noah_profile_split_authority_state_t state;
    uint32_t                             publication_count;
    bool                                 transfer_pending;
} noah_profile_split_authority_status_t;

// Caller-owned state. A future QMK split owner publishes one complete local /
// peer observation after each metadata exchange or transfer transition.
typedef struct {
    noah_runtime_publication_generation_t publication_sequence;
    noah_profile_split_authority_status_t status;
    bool                                  initialized;
} noah_profile_split_authority_t;

bool                                 noah_profile_split_descriptor_valid(const noah_profile_split_descriptor_t *descriptor);
noah_profile_split_authority_state_t noah_profile_split_authority_compare(const noah_profile_split_descriptor_t *local, const noah_profile_split_descriptor_t *peer);

void noah_profile_split_authority_init(noah_profile_split_authority_t *authority);
// Invalid metadata is itself published as a fail-closed state so a stale
// converged result can never survive a malformed observation.
bool noah_profile_split_authority_publish(noah_profile_split_authority_t *authority, noah_profile_split_descriptor_t local, noah_profile_split_descriptor_t peer, bool transfer_pending);
bool noah_profile_split_authority_status(const noah_profile_split_authority_t *authority, noah_profile_split_authority_status_t *status);

// profile_activation_policy peer observer. It returns a coherent exact count
// for the single peer: zero only after durable convergence and no transfer.
bool noah_profile_split_authority_peer_observer(void *context, uint8_t *unresolved_count);
