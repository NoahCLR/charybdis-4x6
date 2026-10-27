// ──────────────────────────────────────────────────────────────────────────
// Exact Other-Half Profile -> Shared Candidate/Store Backend
// ──────────────────────────────────────────────────────────────────────────

#include "profile_other_half_store_backend.h"

#include <string.h>

#include "profile_record_identity.h"

static bool record_same_tuple(const noah_profile_store_record_t *record, const noah_profile_split_descriptor_t *descriptor) {
    return record && descriptor && record->slot != NOAH_PROFILE_SLOT_NONE && record->generation == descriptor->generation && record->origin_half == descriptor->origin_half;
}

static bool validated_profile_matches_descriptor(const noah_profile_validator_v1_profile_t *profile, const noah_profile_split_descriptor_t *descriptor) {
    return profile && descriptor && profile->domain_mask == descriptor->domain_mask && profile->byte_length == descriptor->payload_length && profile->crc32 == descriptor->payload_crc32 && profile->digest == descriptor->payload_digest && profile->action_abi_digest == descriptor->action_abi_digest;
}

static bool correlation_matches(const noah_profile_other_half_store_backend_t *other_half, uint32_t generation, uint32_t payload_digest) {
    return other_half && other_half->descriptor.generation == generation && other_half->descriptor.payload_digest == payload_digest;
}

static noah_profile_other_half_store_result_t state_result(const noah_profile_other_half_store_backend_t *other_half) {
    if (!other_half) {
        return NOAH_PROFILE_OTHER_HALF_STORE_INVALID_METADATA;
    }
    switch (other_half->state) {
        case NOAH_PROFILE_OTHER_HALF_STORE_RECEIVING:
        case NOAH_PROFILE_OTHER_HALF_STORE_VALIDATING:
        case NOAH_PROFILE_OTHER_HALF_STORE_PREPARING:
        case NOAH_PROFILE_OTHER_HALF_STORE_COMMITTING:
            return NOAH_PROFILE_OTHER_HALF_STORE_IN_PROGRESS;
        case NOAH_PROFILE_OTHER_HALF_STORE_PREPARED:
            return NOAH_PROFILE_OTHER_HALF_STORE_OK;
        case NOAH_PROFILE_OTHER_HALF_STORE_COMMITTED:
            return NOAH_PROFILE_OTHER_HALF_STORE_ALREADY_COMMITTED;
        case NOAH_PROFILE_OTHER_HALF_STORE_RECONCILE_REQUIRED:
            return NOAH_PROFILE_OTHER_HALF_STORE_DURABILITY_UNKNOWN;
        case NOAH_PROFILE_OTHER_HALF_STORE_REJECTED:
            return other_half->result;
        case NOAH_PROFILE_OTHER_HALF_STORE_IDLE:
            return NOAH_PROFILE_OTHER_HALF_STORE_OK;
        case NOAH_PROFILE_OTHER_HALF_STORE_UNINITIALIZED:
        default:
            return NOAH_PROFILE_OTHER_HALF_STORE_INVALID_METADATA;
    }
}

static noah_profile_other_half_store_result_t set_terminal(noah_profile_other_half_store_backend_t *other_half, noah_profile_other_half_store_state_t state, noah_profile_other_half_store_result_t result) {
    other_half->state = state;
    other_half->result = result;
    return result;
}

// A copy that ends rejected gives its storage admission back. The store may
// already have closed the prepare itself on an I/O error, or closed it when
// an abort's own write failed; either way nothing is left to abort, but the
// admission was still held, so every later copy met "prepare in progress"
// until the half was power cycled.
static noah_profile_other_half_store_result_t abort_rejected(noah_profile_other_half_store_backend_t *other_half, noah_profile_other_half_store_result_t result) {
    const noah_profile_store_t *store = other_half->backend ? other_half->backend->store : NULL;

    if (noah_profile_store_prepare_active(store) && other_half->interface.abort(other_half->interface.context) != NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
        result = NOAH_PROFILE_OTHER_HALF_STORE_STORAGE_ERROR;
    }
    if (store && !noah_profile_store_prepare_active(store)) {
        (void)noah_profile_candidate_store_backend_release_admission(other_half->backend, NOAH_PROFILE_STORAGE_ADMISSION_PEER);
    }
    return set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_REJECTED, result);
}

static noah_profile_other_half_store_result_t map_begin_result(noah_profile_store_result_t result) {
    switch (result) {
        case NOAH_PROFILE_STORE_OK:
            return NOAH_PROFILE_OTHER_HALF_STORE_OK;
        case NOAH_PROFILE_STORE_INCOMPATIBLE_SCHEMA:
        case NOAH_PROFILE_STORE_INCOMPATIBLE_COMPILED_DEFAULT:
        case NOAH_PROFILE_STORE_INCOMPATIBLE_ACTION_ABI:
            return NOAH_PROFILE_OTHER_HALF_STORE_INCOMPATIBLE;
        case NOAH_PROFILE_STORE_GENERATION_NOT_NEWER:
            return NOAH_PROFILE_OTHER_HALF_STORE_STALE;
        case NOAH_PROFILE_STORE_GENERATION_CONFLICT:
            return NOAH_PROFILE_OTHER_HALF_STORE_CONFLICT;
        case NOAH_PROFILE_STORE_PREPARE_IN_PROGRESS:
        case NOAH_PROFILE_STORE_BACKING_REUSE_DENIED:
            return NOAH_PROFILE_OTHER_HALF_STORE_BUSY;
        case NOAH_PROFILE_STORE_DURABILITY_UNKNOWN:
            return NOAH_PROFILE_OTHER_HALF_STORE_DURABILITY_UNKNOWN;
        case NOAH_PROFILE_STORE_INVALID_ARGUMENT:
            return NOAH_PROFILE_OTHER_HALF_STORE_INVALID_METADATA;
        default:
            return NOAH_PROFILE_OTHER_HALF_STORE_STORAGE_ERROR;
    }
}

static noah_profile_other_half_store_result_t begin_commit(noah_profile_other_half_store_backend_t *other_half) {
    noah_profile_candidate_backend_result_t result = other_half->interface.commit_begin(other_half->interface.context);

    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        other_half->state = NOAH_PROFILE_OTHER_HALF_STORE_COMMITTING;
        other_half->result = NOAH_PROFILE_OTHER_HALF_STORE_IN_PROGRESS;
        return other_half->result;
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN) {
        return set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_RECONCILE_REQUIRED, NOAH_PROFILE_OTHER_HALF_STORE_DURABILITY_UNKNOWN);
    }
    return abort_rejected(other_half, NOAH_PROFILE_OTHER_HALF_STORE_STORAGE_ERROR);
}

static noah_profile_other_half_store_result_t begin_prepare_durable(noah_profile_other_half_store_backend_t *other_half) {
    noah_profile_candidate_backend_result_t result = noah_profile_candidate_store_backend_prepare_durable_begin(other_half->backend);

    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        other_half->state = NOAH_PROFILE_OTHER_HALF_STORE_PREPARING;
        other_half->result = NOAH_PROFILE_OTHER_HALF_STORE_IN_PROGRESS;
        return other_half->result;
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
        return set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_PREPARED, NOAH_PROFILE_OTHER_HALF_STORE_OK);
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN) {
        return set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_RECONCILE_REQUIRED, NOAH_PROFILE_OTHER_HALF_STORE_DURABILITY_UNKNOWN);
    }
    return abort_rejected(other_half, NOAH_PROFILE_OTHER_HALF_STORE_STORAGE_ERROR);
}

void noah_profile_other_half_store_backend_init(noah_profile_other_half_store_backend_t *other_half, noah_profile_candidate_store_backend_t *backend) {
    if (!other_half) {
        return;
    }
    memset(other_half, 0, sizeof(*other_half));
    other_half->backend    = backend;
    other_half->interface  = noah_profile_candidate_store_backend_interface(backend);
    other_half->state      = backend ? NOAH_PROFILE_OTHER_HALF_STORE_IDLE : NOAH_PROFILE_OTHER_HALF_STORE_UNINITIALIZED;
    other_half->result     = backend ? NOAH_PROFILE_OTHER_HALF_STORE_OK : NOAH_PROFILE_OTHER_HALF_STORE_INVALID_METADATA;
    other_half->validation_error = noah_profile_candidate_v1_no_error();
}

noah_profile_other_half_store_result_t noah_profile_other_half_store_backend_begin(noah_profile_other_half_store_backend_t *other_half, const noah_profile_split_descriptor_t *descriptor) {
    const noah_profile_store_t        *store;
    const noah_profile_store_record_t *saved;
    noah_profile_store_candidate_t     candidate;
    noah_profile_store_result_t    store_result;

    if (!other_half || !other_half->backend || !other_half->backend->store || !descriptor) {
        return NOAH_PROFILE_OTHER_HALF_STORE_INVALID_METADATA;
    }
    if (noah_profile_other_half_store_backend_holds_copy(other_half)) {
        if (!noah_profile_descriptor_equal(&other_half->descriptor, descriptor)) return NOAH_PROFILE_OTHER_HALF_STORE_BUSY;
        return state_result(other_half);
    }
    if (other_half->state == NOAH_PROFILE_OTHER_HALF_STORE_COMMITTED && noah_profile_descriptor_equal(&other_half->descriptor, descriptor)) {
        return NOAH_PROFILE_OTHER_HALF_STORE_ALREADY_COMMITTED;
    }
    if (!descriptor->readable || !descriptor->has_profile || !noah_profile_split_descriptor_valid(descriptor)) {
        return set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_REJECTED, NOAH_PROFILE_OTHER_HALF_STORE_INVALID_METADATA);
    }

    store = other_half->backend->store;
    if (descriptor->schema_major != store->compatibility.schema_major || descriptor->schema_minor != store->compatibility.schema_minor || descriptor->compiled_default_digest != other_half->backend->compiled_default_digest || descriptor->compiled_default_digest != store->compatibility.compiled_default_digest || descriptor->action_abi_digest != store->compatibility.action_abi_digest || descriptor->action_abi_digest != other_half->backend->compatibility.action_abi_digest || descriptor->payload_length > other_half->backend->compatibility.max_blob_size || (descriptor->domain_mask & (uint8_t)~other_half->backend->compatibility.allowed_domain_mask) != 0u || (descriptor->domain_mask & other_half->backend->compatibility.required_domain_mask) != other_half->backend->compatibility.required_domain_mask) {
        return set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_REJECTED, NOAH_PROFILE_OTHER_HALF_STORE_INCOMPATIBLE);
    }
    saved = noah_profile_store_saved(store);
    if (saved) {
        if (noah_profile_record_matches_descriptor(saved, descriptor)) {
            if (other_half->backend->validation_complete && other_half->backend->saved_available && noah_profile_record_matches_descriptor(&other_half->backend->saved_record, descriptor) && validated_profile_matches_descriptor(&other_half->backend->validated_profile, descriptor)) {
                other_half->descriptor = *descriptor;
                other_half->metadata.store_format_version = NOAH_PROFILE_STORE_FORMAT_VERSION;
                other_half->next_offset = descriptor->payload_length;
                return set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_COMMITTED, NOAH_PROFILE_OTHER_HALF_STORE_ALREADY_COMMITTED);
            }
            return set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_REJECTED, NOAH_PROFILE_OTHER_HALF_STORE_BUSY);
        }
        if (descriptor->generation < saved->generation) {
            return set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_REJECTED, NOAH_PROFILE_OTHER_HALF_STORE_STALE);
        }
        if (descriptor->generation == saved->generation) {
            return set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_REJECTED, record_same_tuple(saved, descriptor) ? NOAH_PROFILE_OTHER_HALF_STORE_CORRUPT : NOAH_PROFILE_OTHER_HALF_STORE_CONFLICT);
        }
    }

    other_half->metadata = (noah_profile_candidate_v1_metadata_t){
        .schema_major         = descriptor->schema_major,
        .schema_minor         = descriptor->schema_minor,
        .requested_domains    = descriptor->domain_mask,
        .flags                = 0u,
        .payload_length       = descriptor->payload_length,
        .crc32                = descriptor->payload_crc32,
        .digest               = descriptor->payload_digest,
        .action_abi_digest    = descriptor->action_abi_digest,
        .store_format_version = NOAH_PROFILE_STORE_FORMAT_VERSION,
    };
    candidate = (noah_profile_store_candidate_t){
        .format_version          = NOAH_PROFILE_STORE_FORMAT_VERSION,
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
    };
    store_result = noah_profile_candidate_store_backend_begin_exact(other_half->backend, &other_half->metadata, &candidate);
    if (store_result != NOAH_PROFILE_STORE_OK) {
        noah_profile_other_half_store_result_t result = map_begin_result(store_result);
        return set_terminal(other_half, result == NOAH_PROFILE_OTHER_HALF_STORE_DURABILITY_UNKNOWN ? NOAH_PROFILE_OTHER_HALF_STORE_RECONCILE_REQUIRED : NOAH_PROFILE_OTHER_HALF_STORE_REJECTED, result);
    }

    other_half->descriptor = *descriptor;
    other_half->next_offset = 0u;
    other_half->validation_error = noah_profile_candidate_v1_no_error();
    other_half->state      = NOAH_PROFILE_OTHER_HALF_STORE_RECEIVING;
    other_half->result     = NOAH_PROFILE_OTHER_HALF_STORE_IN_PROGRESS;
    return NOAH_PROFILE_OTHER_HALF_STORE_OK;
}

noah_profile_other_half_store_result_t noah_profile_other_half_store_backend_write(noah_profile_other_half_store_backend_t *other_half, uint32_t generation, uint32_t payload_digest, uint32_t offset, const uint8_t *bytes, uint8_t length) {
    uint8_t staged[NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX];

    if (!other_half || other_half->state != NOAH_PROFILE_OTHER_HALF_STORE_RECEIVING) {
        return other_half && other_half->state == NOAH_PROFILE_OTHER_HALF_STORE_IDLE ? NOAH_PROFILE_OTHER_HALF_STORE_BUSY : state_result(other_half);
    }
    if (!correlation_matches(other_half, generation, payload_digest)) {
        return abort_rejected(other_half, NOAH_PROFILE_OTHER_HALF_STORE_CONFLICT);
    }
    if (!bytes || length == 0u || length > NOAH_PROFILE_SPLIT_V1_CHUNK_MAX || offset > other_half->descriptor.payload_length || length > other_half->descriptor.payload_length - offset) {
        return abort_rejected(other_half, NOAH_PROFILE_OTHER_HALF_STORE_RANGE_ERROR);
    }
    if (offset < other_half->next_offset) {
        if (length > other_half->next_offset - offset) {
            return abort_rejected(other_half, NOAH_PROFILE_OTHER_HALF_STORE_RANGE_ERROR);
        }
        for (uint8_t done = 0u; done < length;) {
            uint8_t piece = (uint8_t)(length - done) < NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX ? (uint8_t)(length - done) : NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX;

            if (other_half->interface.read(other_half->interface.context, offset + done, staged, piece) != NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
                return abort_rejected(other_half, NOAH_PROFILE_OTHER_HALF_STORE_STORAGE_ERROR);
            }
            if (memcmp(staged, &bytes[done], piece) != 0) {
                return abort_rejected(other_half, NOAH_PROFILE_OTHER_HALF_STORE_CHUNK_CONFLICT);
            }
            done = (uint8_t)(done + piece);
        }
        return NOAH_PROFILE_OTHER_HALF_STORE_OK;
    }
    if (offset != other_half->next_offset) {
        return abort_rejected(other_half, NOAH_PROFILE_OTHER_HALF_STORE_RANGE_ERROR);
    }
    for (uint8_t done = 0u; done < length;) {
        uint8_t piece = (uint8_t)(length - done) < NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX ? (uint8_t)(length - done) : NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX;

        if (other_half->interface.write(other_half->interface.context, other_half->next_offset, &bytes[done], piece) != NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
            return abort_rejected(other_half, NOAH_PROFILE_OTHER_HALF_STORE_STORAGE_ERROR);
        }
        other_half->next_offset = other_half->next_offset + piece;
        done              = (uint8_t)(done + piece);
    }
    return NOAH_PROFILE_OTHER_HALF_STORE_OK;
}

noah_profile_other_half_store_result_t noah_profile_other_half_store_backend_reuse(noah_profile_other_half_store_backend_t *other_half, uint32_t generation, uint32_t payload_digest, uint32_t offset, const noah_profile_split_v1_source_t *source, uint32_t source_offset, uint32_t end, uint16_t byte_budget) {
    noah_profile_candidate_v1_source_t candidate_source;
    uint8_t                            bytes[NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX];
    uint16_t                           copied = 0u;

    if (!other_half || other_half->state != NOAH_PROFILE_OTHER_HALF_STORE_RECEIVING) {
        return other_half && other_half->state == NOAH_PROFILE_OTHER_HALF_STORE_IDLE ? NOAH_PROFILE_OTHER_HALF_STORE_BUSY : state_result(other_half);
    }
    if (!correlation_matches(other_half, generation, payload_digest)) {
        return abort_rejected(other_half, NOAH_PROFILE_OTHER_HALF_STORE_CONFLICT);
    }
    if (!source || byte_budget == 0u || end <= offset || end > other_half->descriptor.payload_length || offset > other_half->next_offset) {
        return abort_rejected(other_half, NOAH_PROFILE_OTHER_HALF_STORE_RANGE_ERROR);
    }
    candidate_source = (noah_profile_candidate_v1_source_t){
        .generation = source->generation,
        .digest     = source->digest,
        .crc32      = source->crc32,
        .kind       = source->kind,
        .origin     = source->origin,
    };
    while (other_half->next_offset < end && copied < byte_budget) {
        uint32_t left  = end - other_half->next_offset < (uint32_t)(byte_budget - copied) ? end - other_half->next_offset : (uint32_t)(byte_budget - copied);
        uint8_t  piece = left < NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX ? (uint8_t)left : NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX;
        uint32_t from  = source_offset + (other_half->next_offset - offset);

        if (from < source_offset || noah_profile_candidate_store_backend_read_active(other_half->backend, NOAH_PROFILE_STORAGE_ADMISSION_PEER, &candidate_source, from, bytes, piece) != NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
            return NOAH_PROFILE_OTHER_HALF_STORE_SOURCE_UNAVAILABLE;
        }
        if (other_half->interface.write(other_half->interface.context, other_half->next_offset, bytes, piece) != NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
            return abort_rejected(other_half, NOAH_PROFILE_OTHER_HALF_STORE_STORAGE_ERROR);
        }
        other_half->next_offset = other_half->next_offset + piece;
        copied            = (uint16_t)(copied + piece);
    }
    return NOAH_PROFILE_OTHER_HALF_STORE_OK;
}

noah_profile_other_half_store_result_t noah_profile_other_half_store_backend_commit_begin(noah_profile_other_half_store_backend_t *other_half, const noah_profile_split_descriptor_t *descriptor) {
    noah_profile_candidate_backend_result_t result;

    if (!other_half || !descriptor || !noah_profile_descriptor_equal(&other_half->descriptor, descriptor)) {
        return other_half && (other_half->state == NOAH_PROFILE_OTHER_HALF_STORE_COMMITTING || other_half->state == NOAH_PROFILE_OTHER_HALF_STORE_RECONCILE_REQUIRED) ? state_result(other_half) : NOAH_PROFILE_OTHER_HALF_STORE_CONFLICT;
    }
    if (other_half->state != NOAH_PROFILE_OTHER_HALF_STORE_RECEIVING) {
        return state_result(other_half);
    }
    if (other_half->next_offset != other_half->descriptor.payload_length) {
        return abort_rejected(other_half, NOAH_PROFILE_OTHER_HALF_STORE_RANGE_ERROR);
    }

    other_half->auto_commit = true;
    result            = other_half->interface.validation_begin(other_half->interface.context, &other_half->metadata, &other_half->validation_error);
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        other_half->state = NOAH_PROFILE_OTHER_HALF_STORE_VALIDATING;
        other_half->result = NOAH_PROFILE_OTHER_HALF_STORE_IN_PROGRESS;
        return other_half->result;
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_VALID) {
        return begin_commit(other_half);
    }
    return abort_rejected(other_half, result == NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED ? NOAH_PROFILE_OTHER_HALF_STORE_VALIDATION_ERROR : NOAH_PROFILE_OTHER_HALF_STORE_STORAGE_ERROR);
}

noah_profile_other_half_store_result_t noah_profile_other_half_store_backend_prepare_durable_begin(noah_profile_other_half_store_backend_t *other_half, const noah_profile_split_descriptor_t *descriptor) {
    noah_profile_candidate_backend_result_t result;

    if (!other_half || !descriptor || !noah_profile_descriptor_equal(&other_half->descriptor, descriptor)) {
        return NOAH_PROFILE_OTHER_HALF_STORE_CONFLICT;
    }
    if (other_half->state == NOAH_PROFILE_OTHER_HALF_STORE_PREPARED) {
        return NOAH_PROFILE_OTHER_HALF_STORE_OK;
    }
    if (other_half->state != NOAH_PROFILE_OTHER_HALF_STORE_RECEIVING || other_half->next_offset != other_half->descriptor.payload_length) {
        return state_result(other_half);
    }
    other_half->auto_commit = false;
    result            = other_half->interface.validation_begin(other_half->interface.context, &other_half->metadata, &other_half->validation_error);
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        other_half->state = NOAH_PROFILE_OTHER_HALF_STORE_VALIDATING;
        other_half->result = NOAH_PROFILE_OTHER_HALF_STORE_IN_PROGRESS;
        return other_half->result;
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_VALID) {
        return begin_prepare_durable(other_half);
    }
    return abort_rejected(other_half, result == NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED ? NOAH_PROFILE_OTHER_HALF_STORE_VALIDATION_ERROR : NOAH_PROFILE_OTHER_HALF_STORE_STORAGE_ERROR);
}

noah_profile_other_half_store_result_t noah_profile_other_half_store_backend_prepared_commit_begin(noah_profile_other_half_store_backend_t *other_half, const noah_profile_split_descriptor_t *descriptor) {
    noah_profile_candidate_backend_result_t result;

    if (!other_half || !descriptor || !noah_profile_descriptor_equal(&other_half->descriptor, descriptor)) {
        return NOAH_PROFILE_OTHER_HALF_STORE_CONFLICT;
    }
    if (other_half->state != NOAH_PROFILE_OTHER_HALF_STORE_PREPARED) {
        return state_result(other_half);
    }
    result = noah_profile_candidate_store_backend_prepared_commit_begin(other_half->backend);
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        other_half->state = NOAH_PROFILE_OTHER_HALF_STORE_COMMITTING;
        other_half->result = NOAH_PROFILE_OTHER_HALF_STORE_IN_PROGRESS;
        return other_half->result;
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN) {
        return set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_RECONCILE_REQUIRED, NOAH_PROFILE_OTHER_HALF_STORE_DURABILITY_UNKNOWN);
    }
    return result == NOAH_PROFILE_CANDIDATE_BACKEND_OK ? set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_COMMITTED, NOAH_PROFILE_OTHER_HALF_STORE_OK) : set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_REJECTED, NOAH_PROFILE_OTHER_HALF_STORE_STORAGE_ERROR);
}

noah_profile_other_half_store_result_t noah_profile_other_half_store_backend_step(noah_profile_other_half_store_backend_t *other_half, uint8_t byte_budget) {
    noah_profile_candidate_backend_result_t result;

    if (!other_half || byte_budget == 0u || byte_budget > NOAH_PROFILE_VALIDATOR_V1_STEP_READ_MAX) {
        return NOAH_PROFILE_OTHER_HALF_STORE_INVALID_METADATA;
    }
    if (other_half->state == NOAH_PROFILE_OTHER_HALF_STORE_VALIDATING) {
        result = other_half->interface.validation_step(other_half->interface.context, byte_budget, &other_half->validation_error);
        if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
            return NOAH_PROFILE_OTHER_HALF_STORE_IN_PROGRESS;
        }
        if (result == NOAH_PROFILE_CANDIDATE_BACKEND_VALID) {
            return other_half->auto_commit ? begin_commit(other_half) : begin_prepare_durable(other_half);
        }
        return abort_rejected(other_half, result == NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED ? NOAH_PROFILE_OTHER_HALF_STORE_VALIDATION_ERROR : NOAH_PROFILE_OTHER_HALF_STORE_STORAGE_ERROR);
    }
    if (other_half->state == NOAH_PROFILE_OTHER_HALF_STORE_PREPARING) {
        result = noah_profile_candidate_store_backend_prepare_durable_step(other_half->backend, byte_budget);
        if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
            return NOAH_PROFILE_OTHER_HALF_STORE_IN_PROGRESS;
        }
        if (result == NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
            return set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_PREPARED, NOAH_PROFILE_OTHER_HALF_STORE_OK);
        }
        if (result == NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN) {
            return set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_RECONCILE_REQUIRED, NOAH_PROFILE_OTHER_HALF_STORE_DURABILITY_UNKNOWN);
        }
        return abort_rejected(other_half, NOAH_PROFILE_OTHER_HALF_STORE_STORAGE_ERROR);
    }
    if (other_half->state != NOAH_PROFILE_OTHER_HALF_STORE_COMMITTING) {
        return state_result(other_half);
    }

    result = other_half->auto_commit ? other_half->interface.commit_step(other_half->interface.context, byte_budget) : noah_profile_candidate_store_backend_prepared_commit_step(other_half->backend, byte_budget);
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        return NOAH_PROFILE_OTHER_HALF_STORE_IN_PROGRESS;
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN) {
        return set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_RECONCILE_REQUIRED, NOAH_PROFILE_OTHER_HALF_STORE_DURABILITY_UNKNOWN);
    }
    if (result != NOAH_PROFILE_CANDIDATE_BACKEND_OK || !noah_profile_record_matches_descriptor(&other_half->backend->saved_record, &other_half->descriptor) || !validated_profile_matches_descriptor(&other_half->backend->validated_profile, &other_half->descriptor)) {
        return abort_rejected(other_half, result == NOAH_PROFILE_CANDIDATE_BACKEND_OK ? NOAH_PROFILE_OTHER_HALF_STORE_CORRUPT : NOAH_PROFILE_OTHER_HALF_STORE_STORAGE_ERROR);
    }
    return set_terminal(other_half, NOAH_PROFILE_OTHER_HALF_STORE_COMMITTED, NOAH_PROFILE_OTHER_HALF_STORE_OK);
}

noah_profile_other_half_store_result_t noah_profile_other_half_store_backend_abort(noah_profile_other_half_store_backend_t *other_half, const noah_profile_split_descriptor_t *descriptor) {
    if (!other_half || !descriptor || !noah_profile_descriptor_equal(&other_half->descriptor, descriptor)) {
        return NOAH_PROFILE_OTHER_HALF_STORE_CONFLICT;
    }
    if (other_half->state == NOAH_PROFILE_OTHER_HALF_STORE_PREPARING || other_half->state == NOAH_PROFILE_OTHER_HALF_STORE_COMMITTING) {
        return NOAH_PROFILE_OTHER_HALF_STORE_BUSY;
    }
    if (other_half->state == NOAH_PROFILE_OTHER_HALF_STORE_RECONCILE_REQUIRED) {
        return NOAH_PROFILE_OTHER_HALF_STORE_DURABILITY_UNKNOWN;
    }
    if (other_half->state == NOAH_PROFILE_OTHER_HALF_STORE_COMMITTED) {
        return NOAH_PROFILE_OTHER_HALF_STORE_ALREADY_COMMITTED;
    }
    if (!noah_profile_other_half_store_backend_abortable(other_half)) {
        return state_result(other_half);
    }
    if (other_half->interface.abort(other_half->interface.context) != NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
        return abort_rejected(other_half, NOAH_PROFILE_OTHER_HALF_STORE_STORAGE_ERROR);
    }
    memset(&other_half->descriptor, 0, sizeof(other_half->descriptor));
    memset(&other_half->metadata, 0, sizeof(other_half->metadata));
    other_half->next_offset = 0u;
    other_half->state = NOAH_PROFILE_OTHER_HALF_STORE_IDLE;
    other_half->result = NOAH_PROFILE_OTHER_HALF_STORE_OK;
    return other_half->result;
}

noah_profile_other_half_store_state_t noah_profile_other_half_store_backend_state(const noah_profile_other_half_store_backend_t *other_half) {
    return other_half ? other_half->state : NOAH_PROFILE_OTHER_HALF_STORE_UNINITIALIZED;
}

noah_profile_other_half_store_result_t noah_profile_other_half_store_backend_result(const noah_profile_other_half_store_backend_t *other_half) {
    return other_half ? other_half->result : NOAH_PROFILE_OTHER_HALF_STORE_INVALID_METADATA;
}

uint32_t noah_profile_other_half_store_backend_next_offset(const noah_profile_other_half_store_backend_t *other_half) {
    return other_half ? other_half->next_offset : 0u;
}

const noah_profile_candidate_v1_error_t *noah_profile_other_half_store_backend_validation_error(const noah_profile_other_half_store_backend_t *other_half) {
    return other_half && other_half->result == NOAH_PROFILE_OTHER_HALF_STORE_VALIDATION_ERROR ? &other_half->validation_error : NULL;
}

_Static_assert((unsigned)NOAH_PROFILE_SPLIT_V1_CHUNK_MAX <= UINT8_MAX, "a split chunk is written in candidate-store pieces counted in a byte");
_Static_assert(sizeof(noah_profile_other_half_store_backend_t) <= NOAH_PROFILE_OTHER_HALF_STORE_STATE_BUDGET_32BIT, "other-half store state exceeded its payload-independent firmware regression policy");
