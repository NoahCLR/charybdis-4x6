// ─────────────────────────────────────────────────────────────────────────
// Live-Profile Split Authority
// ──────────────────────────────────────────────────────────────────────────

#include "profile_split_authority.h"

#include <limits.h>
#include <string.h>

#include "../schema/profile_blob_v1.h"
#include "../schema/profile_validator_v1.h"
#include "../storage/profile_store.h"

static bool descriptor_empty_profile_fields(const noah_profile_split_descriptor_t *descriptor) {
    return descriptor && descriptor->generation == 0u && descriptor->payload_crc32 == 0u && descriptor->payload_digest == 0u && descriptor->payload_length == 0u && descriptor->domain_mask == 0u && descriptor->profile_flags == 0u && descriptor->origin_half == 0u;
}

bool noah_profile_split_descriptor_valid(const noah_profile_split_descriptor_t *descriptor) {
    if (!descriptor) {
        return false;
    }
    if (!descriptor->readable) {
        return !descriptor->has_profile && descriptor->generation == 0u && descriptor->payload_crc32 == 0u && descriptor->payload_digest == 0u && descriptor->compiled_default_digest == 0u && descriptor->action_abi_digest == 0u && descriptor->payload_length == 0u && descriptor->schema_major == 0u && descriptor->schema_minor == 0u && descriptor->domain_mask == 0u && descriptor->profile_flags == 0u && descriptor->origin_half == 0u;
    }
    if (descriptor->schema_major != NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR || descriptor->schema_minor != NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR) {
        return false;
    }
    if (!descriptor->has_profile) {
        return descriptor_empty_profile_fields(descriptor);
    }
    return descriptor->generation != 0u && descriptor->origin_half <= 1u && descriptor->payload_length >= NOAH_PROFILE_BLOB_V1_HEADER_SIZE && descriptor->payload_length <= NOAH_PROFILE_BLOB_V1_MAX_SIZE && (descriptor->domain_mask & (uint8_t)~NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS) == 0u && (descriptor->profile_flags & (uint8_t)~NOAH_PROFILE_STORE_ALLOWED_FLAGS) == 0u;
}

static bool compatible(const noah_profile_split_descriptor_t *local, const noah_profile_split_descriptor_t *peer) {
    return local->schema_major == peer->schema_major && local->schema_minor == peer->schema_minor && local->compiled_default_digest == peer->compiled_default_digest && local->action_abi_digest == peer->action_abi_digest;
}

static bool same_record(const noah_profile_split_descriptor_t *local, const noah_profile_split_descriptor_t *peer) {
    return local->generation == peer->generation && local->origin_half == peer->origin_half && local->payload_crc32 == peer->payload_crc32 && local->payload_digest == peer->payload_digest && local->payload_length == peer->payload_length && local->domain_mask == peer->domain_mask && local->profile_flags == peer->profile_flags;
}

noah_profile_split_authority_state_t noah_profile_split_authority_compare(const noah_profile_split_descriptor_t *local, const noah_profile_split_descriptor_t *peer) {
    if (!(noah_profile_split_descriptor_valid(local) && noah_profile_split_descriptor_valid(peer))) {
        return NOAH_PROFILE_SPLIT_AUTHORITY_INVALID_METADATA;
    }
    if (!local->readable) {
        return NOAH_PROFILE_SPLIT_AUTHORITY_LOCAL_UNREADABLE;
    }
    if (!peer->readable) {
        return NOAH_PROFILE_SPLIT_AUTHORITY_PEER_UNREADABLE;
    }
    if (!compatible(local, peer)) {
        return NOAH_PROFILE_SPLIT_AUTHORITY_INCOMPATIBLE;
    }
    if (!local->has_profile && !peer->has_profile) {
        return NOAH_PROFILE_SPLIT_AUTHORITY_COMPILED_CONVERGED;
    }
    if (local->has_profile && !peer->has_profile) {
        return NOAH_PROFILE_SPLIT_AUTHORITY_LOCAL_NEWER;
    }
    if (!local->has_profile && peer->has_profile) {
        return NOAH_PROFILE_SPLIT_AUTHORITY_PEER_NEWER;
    }
    if (local->generation > peer->generation) {
        return NOAH_PROFILE_SPLIT_AUTHORITY_LOCAL_NEWER;
    }
    if (peer->generation > local->generation) {
        return NOAH_PROFILE_SPLIT_AUTHORITY_PEER_NEWER;
    }
    if (local->origin_half != peer->origin_half) {
        return NOAH_PROFILE_SPLIT_AUTHORITY_CONCURRENT_COMMIT;
    }
    return same_record(local, peer) ? NOAH_PROFILE_SPLIT_AUTHORITY_COMMITTED_CONVERGED : NOAH_PROFILE_SPLIT_AUTHORITY_CORRUPT_SAME_TUPLE;
}

void noah_profile_split_authority_init(noah_profile_split_authority_t *authority) {
    if (!authority) {
        return;
    }
    memset(authority, 0, sizeof(*authority));
    authority->status.state = NOAH_PROFILE_SPLIT_AUTHORITY_UNINITIALIZED;
    authority->initialized  = true;
}

bool noah_profile_split_authority_publish(noah_profile_split_authority_t *authority, noah_profile_split_descriptor_t local, noah_profile_split_descriptor_t peer, bool transfer_pending) {
    noah_profile_split_authority_status_t next;
    bool                                  valid;

    if (!(authority && authority->initialized)) {
        return false;
    }
    valid = noah_profile_split_descriptor_valid(&local) && noah_profile_split_descriptor_valid(&peer);
    next  = (noah_profile_split_authority_status_t){
        .local             = local,
        .peer              = peer,
        .state             = valid ? noah_profile_split_authority_compare(&local, &peer) : NOAH_PROFILE_SPLIT_AUTHORITY_INVALID_METADATA,
        .publication_count = authority->status.publication_count,
        .transfer_pending  = transfer_pending,
    };
    if (next.publication_count != UINT32_MAX) {
        next.publication_count++;
    }

    noah_runtime_publication_begin(&authority->publication_sequence);
    authority->status = next;
    noah_runtime_publication_end(&authority->publication_sequence);
    return valid;
}

bool noah_profile_split_authority_status(const noah_profile_split_authority_t *authority, noah_profile_split_authority_status_t *status) {
    if (!(authority && authority->initialized && status)) {
        return false;
    }
    for (uint8_t attempt = 0u; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t                               observed = noah_runtime_publication_observe(&authority->publication_sequence);
        noah_profile_split_authority_status_t copied;

        if (noah_runtime_publication_in_flight(observed)) {
            continue;
        }
        copied = authority->status;
        if (noah_runtime_publication_settled(&authority->publication_sequence, observed)) {
            *status = copied;
            return true;
        }
    }
    return false;
}

bool noah_profile_split_authority_peer_observer(void *context, uint8_t *unresolved_count) {
    noah_profile_split_authority_status_t status;

    if (!unresolved_count || !noah_profile_split_authority_status(context, &status)) {
        return false;
    }
    *unresolved_count = status.transfer_pending || (status.state != NOAH_PROFILE_SPLIT_AUTHORITY_COMPILED_CONVERGED && status.state != NOAH_PROFILE_SPLIT_AUTHORITY_COMMITTED_CONVERGED) ? 1u : 0u;
    return true;
}

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(noah_profile_split_authority_t) <= NOAH_PROFILE_SPLIT_AUTHORITY_STATE_BUDGET_32BIT, "profile split authority state exceeded its 32-bit regression policy");
#endif
