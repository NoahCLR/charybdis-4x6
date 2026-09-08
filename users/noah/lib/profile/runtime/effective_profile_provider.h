// ───────────────────────────────────────────────────────────────────────────
// Generation-Owned Effective Profile Provider
// ─────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../schema/profile_validator_v1.h"
#include "../../state/shared/runtime_publication.h"

enum {
    NOAH_EFFECTIVE_PROFILE_MAX_INVALIDATORS    = 8u,
    NOAH_EFFECTIVE_PROFILE_ORIGIN_COMPILED     = UINT8_MAX,
    // This is a firmware-state regression policy, not a physical-RAM claim.
    NOAH_EFFECTIVE_PROFILE_PROVIDER_STATE_BUDGET_32BIT = 784u,
};

typedef enum {
    NOAH_EFFECTIVE_PROFILE_KIND_NONE = 0u,
    NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS,
    NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE,
} noah_effective_profile_kind_t;

typedef enum {
    NOAH_EFFECTIVE_PROFILE_OK = 0u,
    NOAH_EFFECTIVE_PROFILE_NO_CHANGE,
    NOAH_EFFECTIVE_PROFILE_WAITING,
    NOAH_EFFECTIVE_PROFILE_PUBLISHED,
    NOAH_EFFECTIVE_PROFILE_NO_PENDING,
    NOAH_EFFECTIVE_PROFILE_BUSY,
    NOAH_EFFECTIVE_PROFILE_INVALID_ARGUMENT,
    NOAH_EFFECTIVE_PROFILE_INVALID_SNAPSHOT,
    NOAH_EFFECTIVE_PROFILE_STALE_GENERATION,
    NOAH_EFFECTIVE_PROFILE_NO_ROLLBACK,
    NOAH_EFFECTIVE_PROFILE_REENTRANT,
    NOAH_EFFECTIVE_PROFILE_SAFE_BOUNDARY_REQUIRED,
    NOAH_EFFECTIVE_PROFILE_ACTIVE_BACKING_PINNED,
    NOAH_EFFECTIVE_PROFILE_BACKING_REUSE_IN_PROGRESS,
    NOAH_EFFECTIVE_PROFILE_NO_BACKING_REUSE,
} noah_effective_profile_result_t;

typedef struct {
    uint32_t generation;
    uint32_t payload_crc32;
    uint32_t payload_digest;
    uint32_t compiled_default_digest;
    uint32_t action_abi_digest;
    uint8_t  origin;
    uint8_t  kind;
} noah_effective_profile_identity_t;

// A snapshot is copied out of the provider; no API returns a pointer into the
// active bank. Its reader and decoded domain views borrow one immutable backing
// generation. The backing owner must keep that generation readable while a
// copied snapshot is in use. Consumers that span an activation boundary must
// retain copied semantic records, not pointers into reader-backed payloads.
typedef struct {
    noah_effective_profile_identity_t  identity;
    noah_profile_reader_t              reader;
    size_t                             base_offset;
    noah_profile_validator_v1_profile_t profile;
} noah_effective_profile_snapshot_t;

// Describes a generation's immutable backing range. Store integrations use
// the complete slot payload range here, not merely the current blob length.
typedef struct {
    noah_profile_reader_t reader;
    size_t                base_offset;
    size_t                byte_length;
} noah_effective_profile_backing_t;

typedef uint32_t (*noah_effective_profile_safe_boundary_fn)(void *context);

// Invalidators run in the exact array order supplied to provider_init. The
// active bank has already switched, pending state is already clear, and both
// identities are passed by value before the first callback runs. Ordinary
// copy_active/status calls remain fail-closed until all invalidators finish.
// The callback_view is the only supported view of the new reader during this
// interval and is valid only for the duration of the callback.
typedef void (*noah_effective_profile_invalidate_fn)(void *context, uint32_t publication_count, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *callback_view);

typedef struct {
    noah_effective_profile_invalidate_fn callback;
    void                                *context;
} noah_effective_profile_invalidator_t;

typedef struct {
    noah_effective_profile_identity_t active;
    noah_effective_profile_identity_t pending;
    uint32_t                          safe_boundary_reason_mask;
    uint32_t                          publication_count;
    bool                              has_pending;
    bool                              rollback_available;
    bool                              publication_in_progress;
    bool                              safe_boundary_evaluation_in_progress;
    bool                              backing_reuse_in_progress;
} noah_effective_profile_status_t;

// Caller-owned, payload-independent state. Fields are public only so firmware
// can allocate the provider statically. Mutations are scan-owner operations;
// the publication sequence makes copied observations fail closed instead of
// exposing a partially updated provider state.
typedef struct {
    noah_runtime_publication_generation_t publication_sequence;
    noah_effective_profile_snapshot_t    active_banks[2];
    noah_effective_profile_snapshot_t    compiled_defaults;
    noah_effective_profile_snapshot_t    pending;
    noah_effective_profile_backing_t     reuse_backing;
    noah_effective_profile_safe_boundary_fn safe_boundary;
    void                                *safe_boundary_context;
    noah_effective_profile_invalidator_t invalidators[NOAH_EFFECTIVE_PROFILE_MAX_INVALIDATORS];
    uint32_t                             safe_boundary_reason_mask;
    uint32_t                             publication_count;
    uint8_t                              active_index;
    uint8_t                              invalidator_count;
    bool                                 has_pending;
    bool                                 rollback_available;
    bool                                 publication_in_progress;
    bool                                 safe_boundary_evaluation_in_progress;
    bool                                 backing_reuse_in_progress;
    bool                                 initialized;
} noah_effective_profile_provider_t;

noah_effective_profile_result_t noah_effective_profile_snapshot_make_compiled(const noah_profile_validator_v1_profile_t *profile, const noah_profile_reader_t *reader, size_t base_offset, noah_effective_profile_snapshot_t *snapshot);
noah_effective_profile_result_t noah_effective_profile_snapshot_make_validated(const noah_profile_validator_v1_profile_t *profile, const noah_profile_reader_t *reader, size_t base_offset, uint32_t generation, uint8_t origin, uint32_t compiled_default_digest, noah_effective_profile_snapshot_t *snapshot);
bool noah_effective_profile_snapshot_read(const noah_effective_profile_snapshot_t *snapshot, size_t offset, uint8_t *target, size_t length);

noah_effective_profile_result_t noah_effective_profile_provider_init(noah_effective_profile_provider_t *provider, const noah_effective_profile_snapshot_t *compiled_defaults, noah_effective_profile_safe_boundary_fn safe_boundary, void *safe_boundary_context, const noah_effective_profile_invalidator_t *invalidators, size_t invalidator_count);

// request_validated accepts only a strictly newer validated generation whose
// compiled-default and action-ABI identities match this firmware instance.
noah_effective_profile_result_t noah_effective_profile_provider_request_validated(noah_effective_profile_provider_t *provider, const noah_effective_profile_snapshot_t *snapshot);
noah_effective_profile_result_t noah_effective_profile_provider_request_rollback(noah_effective_profile_provider_t *provider);
noah_effective_profile_result_t noah_effective_profile_provider_request_compiled_fallback(noah_effective_profile_provider_t *provider);
noah_effective_profile_result_t noah_effective_profile_provider_cancel_pending(noah_effective_profile_provider_t *provider);

// Discarding only removes the provider's rollback option. It is never storage
// authorization: the active profile may itself borrow a slot that the store's
// committed-record bookkeeping calls inactive after runtime rollback.
noah_effective_profile_result_t noah_effective_profile_provider_discard_rollback(noah_effective_profile_provider_t *provider);

// Persistent staging is forbidden until its scan owner is active-backing
// aware. It must wrap every destructive slot-write interval with begin/end,
// passing that slot's complete payload backing. begin fails if the range is
// active, compiled-default-owned, pending, or already reserved; it atomically
// discards an overlapping rollback snapshot. While reserved, provider
// publication is blocked. The owner must end the reservation before requesting
// publication of a validated snapshot backed by the newly written slot.
noah_effective_profile_result_t noah_effective_profile_provider_begin_backing_reuse(noah_effective_profile_provider_t *provider, const noah_effective_profile_backing_t *backing);
noah_effective_profile_result_t noah_effective_profile_provider_end_backing_reuse(noah_effective_profile_provider_t *provider);

// poll evaluates the injected safe-boundary predicate once. It publishes a
// complete pending snapshot exactly once only when the returned reason mask is
// zero; otherwise the prior generation remains active and WAITING is returned.
// A behavior-bearing pending snapshot cannot publish without a predicate;
// SAFE_BOUNDARY_REQUIRED leaves it pending and the prior generation active.
noah_effective_profile_result_t noah_effective_profile_provider_poll(noah_effective_profile_provider_t *provider);

noah_effective_profile_result_t noah_effective_profile_provider_copy_active(const noah_effective_profile_provider_t *provider, noah_effective_profile_snapshot_t *snapshot);
noah_effective_profile_result_t noah_effective_profile_provider_status(const noah_effective_profile_provider_t *provider, noah_effective_profile_status_t *status);
