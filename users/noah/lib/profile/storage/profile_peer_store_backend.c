// ──────────────────────────────────────────────────────────────────────────
// Exact Peer Profile -> Shared Candidate/Store Backend
// ──────────────────────────────────────────────────────────────────────────

#include "profile_peer_store_backend.h"

#include <string.h>

#ifdef VIA_ENABLE

static bool descriptor_equal(const noah_profile_split_descriptor_t *left, const noah_profile_split_descriptor_t *right) {
    return left && right && left->generation == right->generation && left->payload_crc32 == right->payload_crc32 && left->payload_digest == right->payload_digest && left->compiled_default_digest == right->compiled_default_digest && left->action_abi_digest == right->action_abi_digest && left->payload_length == right->payload_length && left->schema_major == right->schema_major && left->schema_minor == right->schema_minor && left->domain_mask == right->domain_mask && left->profile_flags == right->profile_flags && left->origin_half == right->origin_half && left->readable == right->readable && left->has_profile == right->has_profile && left->logical == right->logical;
}

static bool record_matches_descriptor(const noah_profile_store_record_t *record, const noah_profile_split_descriptor_t *descriptor) {
    return record && descriptor && record->slot != NOAH_PROFILE_SLOT_NONE && record->schema_major == descriptor->schema_major && record->schema_minor == descriptor->schema_minor && record->domain_mask == descriptor->domain_mask && record->flags == descriptor->profile_flags && record->payload_length == descriptor->payload_length && record->generation == descriptor->generation && record->origin_half == descriptor->origin_half && record->payload_crc32 == descriptor->payload_crc32 && record->payload_digest == descriptor->payload_digest && record->compiled_default_digest == descriptor->compiled_default_digest && record->action_abi_digest == descriptor->action_abi_digest && (record->format_version == NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL) == descriptor->logical;
}

static bool record_same_tuple(const noah_profile_store_record_t *record, const noah_profile_split_descriptor_t *descriptor) {
    return record && descriptor && record->slot != NOAH_PROFILE_SLOT_NONE && record->generation == descriptor->generation && record->origin_half == descriptor->origin_half;
}

static bool validated_profile_matches_descriptor(const noah_profile_validator_v1_profile_t *profile, const noah_profile_split_descriptor_t *descriptor) {
    return profile && descriptor && profile->domain_mask == descriptor->domain_mask && profile->byte_length == descriptor->payload_length && profile->crc32 == descriptor->payload_crc32 && profile->digest == descriptor->payload_digest && profile->action_abi_digest == descriptor->action_abi_digest;
}

static bool correlation_matches(const noah_profile_peer_store_backend_t *peer, uint32_t generation, uint32_t payload_digest) {
    return peer && peer->descriptor.generation == generation && peer->descriptor.payload_digest == payload_digest;
}

static noah_profile_peer_store_result_t state_result(const noah_profile_peer_store_backend_t *peer) {
    if (!peer) {
        return NOAH_PROFILE_PEER_STORE_INVALID_METADATA;
    }
    switch (peer->state) {
        case NOAH_PROFILE_PEER_STORE_RECEIVING:
        case NOAH_PROFILE_PEER_STORE_VALIDATING:
        case NOAH_PROFILE_PEER_STORE_PREPARING:
        case NOAH_PROFILE_PEER_STORE_COMMITTING:
            return NOAH_PROFILE_PEER_STORE_IN_PROGRESS;
        case NOAH_PROFILE_PEER_STORE_PREPARED:
            return NOAH_PROFILE_PEER_STORE_OK;
        case NOAH_PROFILE_PEER_STORE_COMMITTED:
            return NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED;
        case NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED:
            return NOAH_PROFILE_PEER_STORE_DURABILITY_UNKNOWN;
        case NOAH_PROFILE_PEER_STORE_REJECTED:
            return peer->result;
        case NOAH_PROFILE_PEER_STORE_IDLE:
            return NOAH_PROFILE_PEER_STORE_OK;
        case NOAH_PROFILE_PEER_STORE_UNINITIALIZED:
        default:
            return NOAH_PROFILE_PEER_STORE_INVALID_METADATA;
    }
}

static noah_profile_peer_store_result_t set_terminal(noah_profile_peer_store_backend_t *peer, noah_profile_peer_store_state_t state, noah_profile_peer_store_result_t result) {
    peer->state  = state;
    peer->result = result;
    return result;
}

static noah_profile_peer_store_result_t abort_rejected(noah_profile_peer_store_backend_t *peer, noah_profile_peer_store_result_t result) {
    if (peer->backend && peer->backend->store && peer->backend->store->prepare_active && peer->interface.abort(peer->interface.context) != NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
        result = NOAH_PROFILE_PEER_STORE_STORAGE_ERROR;
    }
    return set_terminal(peer, NOAH_PROFILE_PEER_STORE_REJECTED, result);
}

static noah_profile_peer_store_result_t map_begin_result(noah_profile_store_result_t result) {
    switch (result) {
        case NOAH_PROFILE_STORE_OK:
            return NOAH_PROFILE_PEER_STORE_OK;
        case NOAH_PROFILE_STORE_INCOMPATIBLE_SCHEMA:
        case NOAH_PROFILE_STORE_INCOMPATIBLE_COMPILED_DEFAULT:
        case NOAH_PROFILE_STORE_INCOMPATIBLE_ACTION_ABI:
            return NOAH_PROFILE_PEER_STORE_INCOMPATIBLE;
        case NOAH_PROFILE_STORE_GENERATION_NOT_NEWER:
            return NOAH_PROFILE_PEER_STORE_STALE;
        case NOAH_PROFILE_STORE_GENERATION_CONFLICT:
            return NOAH_PROFILE_PEER_STORE_CONFLICT;
        case NOAH_PROFILE_STORE_PREPARE_IN_PROGRESS:
        case NOAH_PROFILE_STORE_BACKING_REUSE_DENIED:
            return NOAH_PROFILE_PEER_STORE_BUSY;
        case NOAH_PROFILE_STORE_DURABILITY_UNKNOWN:
            return NOAH_PROFILE_PEER_STORE_DURABILITY_UNKNOWN;
        case NOAH_PROFILE_STORE_INVALID_ARGUMENT:
            return NOAH_PROFILE_PEER_STORE_INVALID_METADATA;
        default:
            return NOAH_PROFILE_PEER_STORE_STORAGE_ERROR;
    }
}

static noah_profile_peer_store_result_t begin_commit(noah_profile_peer_store_backend_t *peer) {
    noah_profile_candidate_backend_result_t result = peer->interface.commit_begin(peer->interface.context);

    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        peer->state  = NOAH_PROFILE_PEER_STORE_COMMITTING;
        peer->result = NOAH_PROFILE_PEER_STORE_IN_PROGRESS;
        return peer->result;
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN) {
        return set_terminal(peer, NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED, NOAH_PROFILE_PEER_STORE_DURABILITY_UNKNOWN);
    }
    return abort_rejected(peer, NOAH_PROFILE_PEER_STORE_STORAGE_ERROR);
}

static noah_profile_peer_store_result_t begin_prepare_durable(noah_profile_peer_store_backend_t *peer) {
    noah_profile_candidate_backend_result_t result = noah_profile_candidate_store_backend_prepare_durable_begin(peer->backend);

    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        peer->state  = NOAH_PROFILE_PEER_STORE_PREPARING;
        peer->result = NOAH_PROFILE_PEER_STORE_IN_PROGRESS;
        return peer->result;
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
        return set_terminal(peer, NOAH_PROFILE_PEER_STORE_PREPARED, NOAH_PROFILE_PEER_STORE_OK);
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN) {
        return set_terminal(peer, NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED, NOAH_PROFILE_PEER_STORE_DURABILITY_UNKNOWN);
    }
    return abort_rejected(peer, NOAH_PROFILE_PEER_STORE_STORAGE_ERROR);
}

void noah_profile_peer_store_backend_init(noah_profile_peer_store_backend_t *peer, noah_profile_candidate_store_backend_t *backend) {
    if (!peer) {
        return;
    }
    memset(peer, 0, sizeof(*peer));
    peer->backend          = backend;
    peer->interface        = noah_profile_candidate_store_backend_interface(backend);
    peer->state            = backend ? NOAH_PROFILE_PEER_STORE_IDLE : NOAH_PROFILE_PEER_STORE_UNINITIALIZED;
    peer->result           = backend ? NOAH_PROFILE_PEER_STORE_OK : NOAH_PROFILE_PEER_STORE_INVALID_METADATA;
    peer->validation_error = noah_profile_candidate_v1_no_error();
}

static noah_profile_peer_store_result_t begin_with_binding(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor, uint8_t format_version, uint32_t via_generation, uint32_t via_digest) {
    noah_profile_store_t          *store;
    noah_profile_store_candidate_t candidate;
    noah_profile_store_result_t    store_result;

    if (!peer || !peer->backend || !peer->backend->store) {
        return NOAH_PROFILE_PEER_STORE_INVALID_METADATA;
    }
    if (peer->state == NOAH_PROFILE_PEER_STORE_RECEIVING || peer->state == NOAH_PROFILE_PEER_STORE_VALIDATING || peer->state == NOAH_PROFILE_PEER_STORE_PREPARING || peer->state == NOAH_PROFILE_PEER_STORE_PREPARED || peer->state == NOAH_PROFILE_PEER_STORE_COMMITTING || peer->state == NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED) {
        return descriptor && descriptor_equal(&peer->descriptor, descriptor) ? state_result(peer) : NOAH_PROFILE_PEER_STORE_BUSY;
    }
    if (peer->state == NOAH_PROFILE_PEER_STORE_COMMITTED && descriptor && descriptor_equal(&peer->descriptor, descriptor)) {
        return NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED;
    }
    if (!descriptor || !descriptor->readable || !descriptor->has_profile || !noah_profile_split_descriptor_valid(descriptor)) {
        return set_terminal(peer, NOAH_PROFILE_PEER_STORE_REJECTED, NOAH_PROFILE_PEER_STORE_INVALID_METADATA);
    }

    store = peer->backend->store;
    if (descriptor->schema_major != store->compatibility.schema_major || descriptor->schema_minor != store->compatibility.schema_minor || descriptor->compiled_default_digest != peer->backend->compiled_default_digest || descriptor->compiled_default_digest != store->compatibility.compiled_default_digest || descriptor->action_abi_digest != store->compatibility.action_abi_digest || descriptor->action_abi_digest != peer->backend->compatibility.action_abi_digest || descriptor->payload_length > peer->backend->compatibility.max_blob_size || (descriptor->domain_mask & (uint8_t)~peer->backend->compatibility.allowed_domain_mask) != 0u || (descriptor->domain_mask & peer->backend->compatibility.required_domain_mask) != peer->backend->compatibility.required_domain_mask) {
        return set_terminal(peer, NOAH_PROFILE_PEER_STORE_REJECTED, NOAH_PROFILE_PEER_STORE_INCOMPATIBLE);
    }
    if (store->committed.slot != NOAH_PROFILE_SLOT_NONE) {
        if (record_matches_descriptor(&store->committed, descriptor)) {
            if (peer->backend->validation_complete && peer->backend->committed_available && record_matches_descriptor(&peer->backend->committed_record, descriptor) && validated_profile_matches_descriptor(&peer->backend->validated_profile, descriptor)) {
                peer->descriptor  = *descriptor;
                peer->next_offset = descriptor->payload_length;
                return set_terminal(peer, NOAH_PROFILE_PEER_STORE_COMMITTED, NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED);
            }
            return set_terminal(peer, NOAH_PROFILE_PEER_STORE_REJECTED, NOAH_PROFILE_PEER_STORE_BUSY);
        }
        if (descriptor->generation < store->committed.generation) {
            return set_terminal(peer, NOAH_PROFILE_PEER_STORE_REJECTED, NOAH_PROFILE_PEER_STORE_STALE);
        }
        if (descriptor->generation == store->committed.generation) {
            return set_terminal(peer, NOAH_PROFILE_PEER_STORE_REJECTED, record_same_tuple(&store->committed, descriptor) ? NOAH_PROFILE_PEER_STORE_CORRUPT : NOAH_PROFILE_PEER_STORE_CONFLICT);
        }
    }

    peer->metadata = (noah_profile_candidate_v1_metadata_t){
        .schema_major         = descriptor->schema_major,
        .schema_minor         = descriptor->schema_minor,
        .requested_domains    = descriptor->domain_mask,
        .flags                = 0u,
        .payload_length       = descriptor->payload_length,
        .crc32                = descriptor->payload_crc32,
        .digest               = descriptor->payload_digest,
        .action_abi_digest    = descriptor->action_abi_digest,
        .store_format_version = format_version,
        .via_generation       = via_generation,
        .via_digest           = via_digest,
    };
    candidate = (noah_profile_store_candidate_t){
        .format_version          = format_version,
        .schema_major            = descriptor->schema_major,
        .schema_minor            = descriptor->schema_minor,
        .domain_mask             = descriptor->domain_mask,
        .flags                   = descriptor->profile_flags,
        .payload_length          = descriptor->payload_length,
        .generation              = descriptor->generation,
        .origin_half             = descriptor->origin_half,
        .payload_crc32           = descriptor->payload_crc32,
        .payload_digest          = descriptor->payload_digest,
        .compiled_default_digest = descriptor->compiled_default_digest,
        .action_abi_digest       = descriptor->action_abi_digest,
        .via_generation          = via_generation,
        .via_digest              = via_digest,
    };
    store_result = noah_profile_candidate_store_backend_begin_exact(peer->backend, &peer->metadata, &candidate);
    if (store_result != NOAH_PROFILE_STORE_OK) {
        noah_profile_peer_store_result_t result = map_begin_result(store_result);
        return set_terminal(peer, result == NOAH_PROFILE_PEER_STORE_DURABILITY_UNKNOWN ? NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED : NOAH_PROFILE_PEER_STORE_REJECTED, result);
    }

    peer->descriptor       = *descriptor;
    peer->next_offset      = 0u;
    peer->validation_error = noah_profile_candidate_v1_no_error();
    peer->state            = NOAH_PROFILE_PEER_STORE_RECEIVING;
    peer->result           = NOAH_PROFILE_PEER_STORE_IN_PROGRESS;
    return NOAH_PROFILE_PEER_STORE_OK;
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor) {
    return begin_with_binding(peer, descriptor, NOAH_PROFILE_STORE_FORMAT_VERSION_LEGACY, 0u, 0u);
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_begin_logical(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor, uint32_t via_generation, uint32_t via_digest) {
    if (via_generation == 0u || via_digest == 0u) {
        return NOAH_PROFILE_PEER_STORE_INVALID_METADATA;
    }
    return begin_with_binding(peer, descriptor, NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL, via_generation, via_digest);
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_write(noah_profile_peer_store_backend_t *peer, uint32_t generation, uint32_t payload_digest, uint16_t offset, const uint8_t *bytes, uint8_t length) {
    uint8_t staged[NOAH_PROFILE_SPLIT_V1_CHUNK_MAX];

    if (!peer || peer->state != NOAH_PROFILE_PEER_STORE_RECEIVING) {
        return peer && peer->state == NOAH_PROFILE_PEER_STORE_IDLE ? NOAH_PROFILE_PEER_STORE_BUSY : state_result(peer);
    }
    if (!correlation_matches(peer, generation, payload_digest)) {
        return abort_rejected(peer, NOAH_PROFILE_PEER_STORE_CONFLICT);
    }
    if (!bytes || length == 0u || length > NOAH_PROFILE_SPLIT_V1_CHUNK_MAX || (uint32_t)offset + length > peer->descriptor.payload_length) {
        return abort_rejected(peer, NOAH_PROFILE_PEER_STORE_RANGE_ERROR);
    }
    if (offset < peer->next_offset) {
        if ((uint32_t)offset + length > peer->next_offset) {
            return abort_rejected(peer, NOAH_PROFILE_PEER_STORE_RANGE_ERROR);
        }
        if (peer->interface.read(peer->interface.context, offset, staged, length) != NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
            return abort_rejected(peer, NOAH_PROFILE_PEER_STORE_STORAGE_ERROR);
        }
        return memcmp(staged, bytes, length) == 0 ? NOAH_PROFILE_PEER_STORE_OK : abort_rejected(peer, NOAH_PROFILE_PEER_STORE_CHUNK_CONFLICT);
    }
    if (offset != peer->next_offset) {
        return abort_rejected(peer, NOAH_PROFILE_PEER_STORE_RANGE_ERROR);
    }
    if (peer->interface.write(peer->interface.context, offset, bytes, length) != NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
        return abort_rejected(peer, NOAH_PROFILE_PEER_STORE_STORAGE_ERROR);
    }
    peer->next_offset = (uint16_t)(peer->next_offset + length);
    return NOAH_PROFILE_PEER_STORE_OK;
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_commit_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor) {
    noah_profile_candidate_backend_result_t result;

    if (!peer || !descriptor || !descriptor_equal(&peer->descriptor, descriptor)) {
        return peer && (peer->state == NOAH_PROFILE_PEER_STORE_COMMITTING || peer->state == NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED) ? state_result(peer) : NOAH_PROFILE_PEER_STORE_CONFLICT;
    }
    if (peer->state != NOAH_PROFILE_PEER_STORE_RECEIVING) {
        return state_result(peer);
    }
    if (peer->next_offset != peer->descriptor.payload_length) {
        return abort_rejected(peer, NOAH_PROFILE_PEER_STORE_RANGE_ERROR);
    }

    peer->auto_commit = true;
    result            = peer->interface.validation_begin(peer->interface.context, &peer->metadata, &peer->validation_error);
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        peer->state  = NOAH_PROFILE_PEER_STORE_VALIDATING;
        peer->result = NOAH_PROFILE_PEER_STORE_IN_PROGRESS;
        return peer->result;
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_VALID) {
        return begin_commit(peer);
    }
    return abort_rejected(peer, result == NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED ? NOAH_PROFILE_PEER_STORE_VALIDATION_ERROR : NOAH_PROFILE_PEER_STORE_STORAGE_ERROR);
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_prepare_durable_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor) {
    noah_profile_candidate_backend_result_t result;

    if (!peer || !descriptor || !descriptor_equal(&peer->descriptor, descriptor)) {
        return NOAH_PROFILE_PEER_STORE_CONFLICT;
    }
    if (peer->state == NOAH_PROFILE_PEER_STORE_PREPARED) {
        return NOAH_PROFILE_PEER_STORE_OK;
    }
    if (peer->state != NOAH_PROFILE_PEER_STORE_RECEIVING || peer->next_offset != peer->descriptor.payload_length) {
        return state_result(peer);
    }
    peer->auto_commit = false;
    result            = peer->interface.validation_begin(peer->interface.context, &peer->metadata, &peer->validation_error);
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        peer->state  = NOAH_PROFILE_PEER_STORE_VALIDATING;
        peer->result = NOAH_PROFILE_PEER_STORE_IN_PROGRESS;
        return peer->result;
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_VALID) {
        return begin_prepare_durable(peer);
    }
    return abort_rejected(peer, result == NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED ? NOAH_PROFILE_PEER_STORE_VALIDATION_ERROR : NOAH_PROFILE_PEER_STORE_STORAGE_ERROR);
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_prepared_commit_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor) {
    noah_profile_candidate_backend_result_t result;

    if (!peer || !descriptor || !descriptor_equal(&peer->descriptor, descriptor)) {
        return NOAH_PROFILE_PEER_STORE_CONFLICT;
    }
    if (peer->state != NOAH_PROFILE_PEER_STORE_PREPARED) {
        return state_result(peer);
    }
    result = noah_profile_candidate_store_backend_prepared_commit_begin(peer->backend);
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        peer->state  = NOAH_PROFILE_PEER_STORE_COMMITTING;
        peer->result = NOAH_PROFILE_PEER_STORE_IN_PROGRESS;
        return peer->result;
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN) {
        return set_terminal(peer, NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED, NOAH_PROFILE_PEER_STORE_DURABILITY_UNKNOWN);
    }
    return result == NOAH_PROFILE_CANDIDATE_BACKEND_OK ? set_terminal(peer, NOAH_PROFILE_PEER_STORE_COMMITTED, NOAH_PROFILE_PEER_STORE_OK) : set_terminal(peer, NOAH_PROFILE_PEER_STORE_REJECTED, NOAH_PROFILE_PEER_STORE_STORAGE_ERROR);
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_step(noah_profile_peer_store_backend_t *peer, uint8_t byte_budget) {
    noah_profile_candidate_backend_result_t result;

    if (!peer || byte_budget == 0u || byte_budget > NOAH_PROFILE_VALIDATOR_V1_STEP_READ_MAX) {
        return NOAH_PROFILE_PEER_STORE_INVALID_METADATA;
    }
    if (peer->state == NOAH_PROFILE_PEER_STORE_VALIDATING) {
        result = peer->interface.validation_step(peer->interface.context, byte_budget, &peer->validation_error);
        if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
            return NOAH_PROFILE_PEER_STORE_IN_PROGRESS;
        }
        if (result == NOAH_PROFILE_CANDIDATE_BACKEND_VALID) {
            return peer->auto_commit ? begin_commit(peer) : begin_prepare_durable(peer);
        }
        return abort_rejected(peer, result == NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED ? NOAH_PROFILE_PEER_STORE_VALIDATION_ERROR : NOAH_PROFILE_PEER_STORE_STORAGE_ERROR);
    }
    if (peer->state == NOAH_PROFILE_PEER_STORE_PREPARING) {
        result = noah_profile_candidate_store_backend_prepare_durable_step(peer->backend, byte_budget);
        if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
            return NOAH_PROFILE_PEER_STORE_IN_PROGRESS;
        }
        if (result == NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
            return set_terminal(peer, NOAH_PROFILE_PEER_STORE_PREPARED, NOAH_PROFILE_PEER_STORE_OK);
        }
        return set_terminal(peer, result == NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN ? NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED : NOAH_PROFILE_PEER_STORE_REJECTED, result == NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN ? NOAH_PROFILE_PEER_STORE_DURABILITY_UNKNOWN : NOAH_PROFILE_PEER_STORE_STORAGE_ERROR);
    }
    if (peer->state != NOAH_PROFILE_PEER_STORE_COMMITTING) {
        return state_result(peer);
    }

    result = peer->auto_commit ? peer->interface.commit_step(peer->interface.context, byte_budget) : noah_profile_candidate_store_backend_prepared_commit_step(peer->backend, byte_budget);
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        return NOAH_PROFILE_PEER_STORE_IN_PROGRESS;
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN) {
        return set_terminal(peer, NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED, NOAH_PROFILE_PEER_STORE_DURABILITY_UNKNOWN);
    }
    if (result != NOAH_PROFILE_CANDIDATE_BACKEND_OK || !record_matches_descriptor(&peer->backend->committed_record, &peer->descriptor) || !validated_profile_matches_descriptor(&peer->backend->validated_profile, &peer->descriptor)) {
        return set_terminal(peer, NOAH_PROFILE_PEER_STORE_REJECTED, result == NOAH_PROFILE_CANDIDATE_BACKEND_OK ? NOAH_PROFILE_PEER_STORE_CORRUPT : NOAH_PROFILE_PEER_STORE_STORAGE_ERROR);
    }
    return set_terminal(peer, NOAH_PROFILE_PEER_STORE_COMMITTED, NOAH_PROFILE_PEER_STORE_OK);
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_abort(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor) {
    if (!peer || !descriptor || !descriptor_equal(&peer->descriptor, descriptor)) {
        return NOAH_PROFILE_PEER_STORE_CONFLICT;
    }
    if (peer->state == NOAH_PROFILE_PEER_STORE_PREPARING || peer->state == NOAH_PROFILE_PEER_STORE_COMMITTING) {
        return NOAH_PROFILE_PEER_STORE_BUSY;
    }
    if (peer->state == NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED) {
        return NOAH_PROFILE_PEER_STORE_DURABILITY_UNKNOWN;
    }
    if (peer->state == NOAH_PROFILE_PEER_STORE_COMMITTED) {
        return NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED;
    }
    if (peer->state != NOAH_PROFILE_PEER_STORE_RECEIVING && peer->state != NOAH_PROFILE_PEER_STORE_VALIDATING && peer->state != NOAH_PROFILE_PEER_STORE_PREPARED) {
        return state_result(peer);
    }
    if (peer->interface.abort(peer->interface.context) != NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
        return set_terminal(peer, NOAH_PROFILE_PEER_STORE_REJECTED, NOAH_PROFILE_PEER_STORE_STORAGE_ERROR);
    }
    memset(&peer->descriptor, 0, sizeof(peer->descriptor));
    memset(&peer->metadata, 0, sizeof(peer->metadata));
    peer->next_offset = 0u;
    peer->state       = NOAH_PROFILE_PEER_STORE_IDLE;
    peer->result      = NOAH_PROFILE_PEER_STORE_OK;
    return peer->result;
}

noah_profile_peer_store_state_t noah_profile_peer_store_backend_state(const noah_profile_peer_store_backend_t *peer) {
    return peer ? peer->state : NOAH_PROFILE_PEER_STORE_UNINITIALIZED;
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_result(const noah_profile_peer_store_backend_t *peer) {
    return peer ? peer->result : NOAH_PROFILE_PEER_STORE_INVALID_METADATA;
}

uint16_t noah_profile_peer_store_backend_next_offset(const noah_profile_peer_store_backend_t *peer) {
    return peer ? peer->next_offset : 0u;
}

const noah_profile_candidate_v1_error_t *noah_profile_peer_store_backend_validation_error(const noah_profile_peer_store_backend_t *peer) {
    return peer && peer->result == NOAH_PROFILE_PEER_STORE_VALIDATION_ERROR ? &peer->validation_error : NULL;
}

_Static_assert((unsigned)NOAH_PROFILE_SPLIT_V1_CHUNK_MAX <= (unsigned)NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX, "split chunks must fit the shared candidate backend");
_Static_assert(sizeof(noah_profile_peer_store_backend_t) <= NOAH_PROFILE_PEER_STORE_STATE_BUDGET_32BIT, "peer store state exceeded its payload-independent firmware regression policy");

#endif
