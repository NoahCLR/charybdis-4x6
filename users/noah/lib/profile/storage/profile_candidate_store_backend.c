// ───────────────────────────────────────────────────────────────────────────
// Candidate Transaction -> Dual-Slot Store Backend
// ───────────────────────────────────────────────────────────────────────────

#include "profile_candidate_store_backend.h"

#include "profile_record_identity.h"
#include "profile_storage_layout.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static bool validated_profile_matches_record(const noah_profile_validator_v1_profile_t *profile, const noah_profile_store_record_t *record) {
    return profile && record && profile->domain_mask == record->domain_mask && profile->byte_length == record->payload_length && profile->crc32 == record->payload_crc32 && profile->digest == record->payload_digest && profile->action_abi_digest == record->action_abi_digest;
}

static bool metadata_matches_candidate(const noah_profile_candidate_v1_metadata_t *metadata, const noah_profile_store_candidate_t *candidate) {
    return metadata && candidate && metadata->schema_major == candidate->schema_major && metadata->schema_minor == candidate->schema_minor && metadata->requested_domains == candidate->domain_mask && metadata->flags == 0u && metadata->payload_length == candidate->payload_length && metadata->crc32 == candidate->payload_crc32 && metadata->digest == candidate->payload_digest && metadata->action_abi_digest == candidate->action_abi_digest && metadata->store_format_version == NOAH_PROFILE_STORE_FORMAT_VERSION && metadata->store_format_version == candidate->format_version;
}

static bool staged_reader_read(void *context, size_t offset, uint8_t *target, size_t length) {
    noah_profile_store_t *store = context;

    if (!store || !store->io.read || !target || length == 0u || offset >= NOAH_PROFILE_STORAGE_REGION_SIZE || length > NOAH_PROFILE_STORAGE_REGION_SIZE - offset) {
        return false;
    }
    return store->io.read(store->io.context, (noah_profile_storage_address_t)offset, target, (uint32_t)length);
}

static bool begin_slot_reuse(void *context, noah_profile_slot_t slot) {
    noah_profile_candidate_store_backend_t *backend = context;
    noah_effective_profile_backing_t        backing;
    noah_profile_storage_address_t  payload_start;

    if (!backend || !backend->provider || !noah_profile_store_slot_payload_start(slot, &payload_start)) {
        return false;
    }
    backing = (noah_effective_profile_backing_t){
        .reader      = backend->staged_reader,
        .base_offset = payload_start,
        .byte_length = NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX,
    };
    return noah_effective_profile_provider_begin_backing_reuse(backend->provider, &backing) == NOAH_EFFECTIVE_PROFILE_OK;
}

static bool end_slot_reuse(void *context, noah_profile_slot_t slot) {
    noah_profile_candidate_store_backend_t *backend = context;

    (void)slot;
    return backend && backend->provider && noah_effective_profile_provider_end_backing_reuse(backend->provider) == NOAH_EFFECTIVE_PROFILE_OK;
}

static noah_profile_candidate_backend_result_t map_store_result(noah_profile_store_result_t result) {
    if (result == NOAH_PROFILE_STORE_OK) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    }
    if (result == NOAH_PROFILE_STORE_PREPARE_IN_PROGRESS || result == NOAH_PROFILE_STORE_BACKING_REUSE_DENIED) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_BUSY;
    }
    return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
}

static void map_validator_error(const noah_profile_validator_v1_error_t *source, noah_profile_candidate_v1_error_t *target) {
    if (!target) {
        return;
    }
    *target = noah_profile_candidate_v1_no_error();
    if (!source) {
        target->code = NOAH_PROFILE_CANDIDATE_V1_ERROR_VALIDATION_REJECTED;
        return;
    }
    target->code        = source->code == NOAH_PROFILE_VALIDATOR_V1_CHECKSUM_MISMATCH ? NOAH_PROFILE_CANDIDATE_V1_ERROR_CHECKSUM_MISMATCH : NOAH_PROFILE_CANDIDATE_V1_ERROR_VALIDATION_REJECTED;
    target->domain_id   = source->domain_id;
    target->table_id    = source->table_id;
    target->row_index   = source->row_index;
    target->tap_index   = source->step_index;
    target->field_id    = source->field_id;
    target->byte_offset = source->byte_offset < UINT32_MAX ? (uint32_t)source->byte_offset : NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U32;
}

static noah_profile_store_result_t begin_exact_with_owner(noah_profile_candidate_store_backend_t *backend, const noah_profile_candidate_v1_metadata_t *metadata, const noah_profile_store_candidate_t *candidate, noah_profile_storage_admission_owner_t owner) {
    noah_profile_store_result_t result;

    if (!backend || !backend->store || !backend->provider || !backend->reuse_guard_installed || owner == NOAH_PROFILE_STORAGE_ADMISSION_NONE || !metadata_matches_candidate(metadata, candidate) || candidate->compiled_default_digest != backend->compiled_default_digest) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }
    if (backend->admission_owner != NOAH_PROFILE_STORAGE_ADMISSION_NONE) {
        return noah_profile_store_reconciliation_required(backend->store) ? NOAH_PROFILE_STORE_DURABILITY_UNKNOWN : NOAH_PROFILE_STORE_PREPARE_IN_PROGRESS;
    }
    backend->admission_owner = owner;
    result                   = noah_profile_store_prepare_begin(backend->store, candidate);
    if (result != NOAH_PROFILE_STORE_OK) {
        backend->admission_owner = NOAH_PROFILE_STORAGE_ADMISSION_NONE;
        return result;
    }
    backend->metadata                    = *metadata;
    backend->reuse_range_count           = 0u;
    backend->validation_complete         = false;
    backend->committed_available         = false;
    backend->activation_requested        = false;
    backend->validating_committed_record = false;
    memset(&backend->validator, 0, sizeof(backend->validator));
    memset(&backend->validated_profile, 0, sizeof(backend->validated_profile));
    memset(&backend->activation_snapshot, 0, sizeof(backend->activation_snapshot));
    memset(&backend->committed_record, 0, sizeof(backend->committed_record));
    return NOAH_PROFILE_STORE_OK;
}

noah_profile_store_result_t noah_profile_candidate_store_backend_begin_exact(noah_profile_candidate_store_backend_t *backend, const noah_profile_candidate_v1_metadata_t *metadata, const noah_profile_store_candidate_t *candidate) {
    return begin_exact_with_owner(backend, metadata, candidate, NOAH_PROFILE_STORAGE_ADMISSION_PEER);
}

static noah_profile_candidate_backend_result_t begin_candidate(void *context, const noah_profile_candidate_v1_metadata_t *metadata) {
    noah_profile_candidate_store_backend_t *backend = context;
    noah_profile_store_candidate_t          candidate;
    uint32_t                                generation;

    if (!backend || !metadata || backend->origin_half > 1u || !noah_profile_store_next_generation(backend->store, &generation)) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    candidate = (noah_profile_store_candidate_t){
        .format_version          = metadata->store_format_version,
        .schema_major            = metadata->schema_major,
        .schema_minor            = metadata->schema_minor,
        .domain_mask             = metadata->requested_domains,
        .flags                   = NOAH_PROFILE_STORE_FLAG_OVERRIDE,
        .payload_length          = metadata->payload_length,
        .generation              = generation,
        .origin_half             = backend->origin_half,
        .payload_crc32           = metadata->crc32,
        .payload_digest          = metadata->digest,
        .compiled_default_digest = backend->compiled_default_digest,
        .action_abi_digest       = metadata->action_abi_digest,
    };
    return map_store_result(begin_exact_with_owner(backend, metadata, &candidate, NOAH_PROFILE_STORAGE_ADMISSION_HOST));
}

static noah_profile_candidate_backend_result_t write_candidate(void *context, uint32_t offset, const uint8_t *bytes, uint8_t length) {
    noah_profile_candidate_store_backend_t *backend = context;

    if (!backend || !backend->store) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    return map_store_result(noah_profile_store_prepare_write(backend->store, offset, bytes, length));
}

static noah_profile_candidate_backend_result_t read_candidate(void *context, uint32_t offset, uint8_t *bytes, uint8_t length) {
    noah_profile_candidate_store_backend_t *backend = context;
    noah_profile_storage_address_t  payload_start;

    return noah_profile_store_candidate_payload_start(backend->store, &payload_start) && staged_reader_read(backend->store, (size_t)payload_start + offset, bytes, length) ? NOAH_PROFILE_CANDIDATE_BACKEND_OK : NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
}

noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_read_active(noah_profile_candidate_store_backend_t *backend, noah_profile_storage_admission_owner_t owner, const noah_profile_candidate_v1_source_t *source, uint32_t offset, uint8_t *bytes, uint8_t length) {
    noah_effective_profile_snapshot_t active;
    if (!backend || !source || !bytes || length == 0u || length > NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX || owner == NOAH_PROFILE_STORAGE_ADMISSION_NONE || backend->admission_owner != owner || noah_effective_profile_provider_copy_active(backend->provider, &active) != NOAH_EFFECTIVE_PROFILE_OK) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    if (active.identity.kind != source->kind + 1u || active.identity.origin != source->origin || active.identity.generation != source->generation || active.identity.payload_digest != source->digest || active.identity.payload_crc32 != source->crc32 || active.identity.compiled_default_digest != backend->compiled_default_digest || active.identity.action_abi_digest != backend->metadata.action_abi_digest || offset > active.profile.byte_length || length > active.profile.byte_length - offset) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    return noah_profile_reader_read(&active.reader, active.base_offset + offset, bytes, length) ? NOAH_PROFILE_CANDIDATE_BACKEND_OK : NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
}

static noah_profile_candidate_backend_result_t read_source(void *context, const noah_profile_candidate_v1_source_t *source, uint32_t offset, uint8_t *bytes, uint8_t length) {
    return noah_profile_candidate_store_backend_read_active(context, NOAH_PROFILE_STORAGE_ADMISSION_HOST, source, offset, bytes, length);
}

static bool source_equal(const noah_profile_candidate_v1_source_t *left, const noah_profile_candidate_v1_source_t *right) {
    return left->generation == right->generation && left->digest == right->digest && left->crc32 == right->crc32 && left->kind == right->kind && left->origin == right->origin;
}

// Remembers a host REUSE step for the peer copy. A step that continues the
// last range in both candidate and source extends it; one from a different
// source, or past a full table, is left out and later sent as bytes.
static void note_reuse(void *context, const noah_profile_candidate_v1_source_t *source, uint32_t offset, uint32_t source_offset, uint8_t length) {
    noah_profile_candidate_store_backend_t     *backend = context;
    noah_profile_candidate_store_reuse_range_t *last;

    if (!backend || !source || length == 0u || backend->admission_owner != NOAH_PROFILE_STORAGE_ADMISSION_HOST) {
        return;
    }
    if (backend->reuse_range_count == 0u) {
        backend->reuse_source = *source;
    } else if (!source_equal(&backend->reuse_source, source)) {
        return;
    } else {
        last = &backend->reuse_ranges[backend->reuse_range_count - 1u];
        if (last->offset + last->length == offset && last->source_offset + last->length == source_offset) {
            last->length += length;
            return;
        }
    }
    if (backend->reuse_range_count < NOAH_PROFILE_CANDIDATE_STORE_REUSE_RANGES) {
        backend->reuse_ranges[backend->reuse_range_count++] = (noah_profile_candidate_store_reuse_range_t){.offset = offset, .source_offset = source_offset, .length = length};
    }
}

static noah_profile_candidate_backend_result_t validation_begin(void *context, const noah_profile_candidate_v1_metadata_t *metadata, noah_profile_candidate_v1_error_t *error) {
    noah_profile_candidate_store_backend_t   *backend = context;
    noah_profile_validator_v1_compatibility_t compatibility;
    noah_profile_validator_v1_declaration_t   declaration;
    noah_profile_validator_v1_error_t       validator_error = noah_profile_validator_v1_no_error();
    noah_profile_validator_v1_result_t      result;
    noah_profile_storage_address_t  payload_start;

    if (!backend || !metadata || !noah_profile_store_fully_staged(backend->store) || !noah_profile_candidate_v1_metadata_equal(metadata, &backend->metadata) || !noah_profile_store_candidate_payload_start(backend->store, &payload_start)) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    backend->validating_committed_record = false;
    declaration                          = (noah_profile_validator_v1_declaration_t){
        .schema_major      = metadata->schema_major,
        .schema_minor      = metadata->schema_minor,
        .domain_mask       = metadata->requested_domains,
        .flags             = metadata->flags,
        .byte_length       = metadata->payload_length,
        .crc32             = metadata->crc32,
        .digest            = metadata->digest,
        .action_abi_digest = metadata->action_abi_digest,
    };
    // Only a candidate the host uploads is held to the candidate runtime. A
    // peer candidate is the other half's committed record, checked when it was
    // saved there, so a profile saved before a rule existed still syncs.
    compatibility = backend->compatibility;
    if (backend->candidate_runtime && backend->admission_owner == NOAH_PROFILE_STORAGE_ADMISSION_HOST) {
        compatibility.runtime = backend->candidate_runtime;
    }
    result = noah_profile_validator_v1_begin(&backend->validator, &backend->staged_reader, payload_start, &declaration, &compatibility, &validator_error);
    if (result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS;
    }
    if (result == NOAH_PROFILE_VALIDATOR_V1_VALID) {
        backend->validation_complete = noah_profile_validator_v1_profile(&backend->validator, &backend->validated_profile, &validator_error) == NOAH_PROFILE_VALIDATOR_V1_VALID;
        return backend->validation_complete ? NOAH_PROFILE_CANDIDATE_BACKEND_VALID : NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    map_validator_error(&validator_error, error);
    return NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
}

// Validation only reads, so a step takes a flash page's worth of validator
// calls rather than one: each call's fixed cost, not its bytes, was what a
// save waited on (D-F29, "Pacing").
static noah_profile_candidate_backend_result_t validation_step(void *context, uint8_t byte_budget, noah_profile_candidate_v1_error_t *error) {
    noah_profile_candidate_store_backend_t *backend         = context;
    noah_profile_validator_v1_error_t       validator_error = noah_profile_validator_v1_no_error();
    noah_profile_validator_v1_result_t      result;
    uint8_t                                 calls = 0u;

    if (!backend) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    do {
        result = noah_profile_validator_v1_step(&backend->validator, byte_budget, &validator_error);
    } while (result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS && ++calls < NOAH_PROFILE_CANDIDATE_VALIDATION_CALLS);
    if (result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS;
    }
    if (result == NOAH_PROFILE_VALIDATOR_V1_VALID) {
        backend->validation_complete = noah_profile_validator_v1_profile(&backend->validator, &backend->validated_profile, &validator_error) == NOAH_PROFILE_VALIDATOR_V1_VALID;
        if (backend->validation_complete && backend->validating_committed_record) {
            backend->committed_available         = validated_profile_matches_record(&backend->validated_profile, &backend->committed_record);
            backend->validating_committed_record = false;
            if (!backend->committed_available) {
                backend->validation_complete = false;
            }
        }
        return backend->validation_complete ? NOAH_PROFILE_CANDIDATE_BACKEND_VALID : NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    backend->validating_committed_record = false;
    map_validator_error(&validator_error, error);
    return result == NOAH_PROFILE_VALIDATOR_V1_READ_ERROR ? NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR : NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
}

static noah_profile_candidate_backend_result_t abort_candidate(void *context) {
    noah_profile_candidate_store_backend_t *backend = context;
    noah_profile_store_result_t             result;

    if (!backend || !backend->store) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    result                               = noah_profile_store_prepare_active(backend->store) ? noah_profile_store_prepare_abort(backend->store) : NOAH_PROFILE_STORE_OK;
    backend->validation_complete         = false;
    backend->activation_requested        = false;
    backend->validating_committed_record = false;
    memset(&backend->metadata, 0, sizeof(backend->metadata));
    memset(&backend->validated_profile, 0, sizeof(backend->validated_profile));
    if (result == NOAH_PROFILE_STORE_OK) {
        backend->admission_owner = NOAH_PROFILE_STORAGE_ADMISSION_NONE;
    }
    return map_store_result(result);
}

noah_profile_storage_admission_owner_t noah_profile_candidate_store_backend_admission_owner(const noah_profile_candidate_store_backend_t *backend) {
    return backend ? backend->admission_owner : NOAH_PROFILE_STORAGE_ADMISSION_NONE;
}

bool noah_profile_candidate_store_backend_release_admission(noah_profile_candidate_store_backend_t *backend, noah_profile_storage_admission_owner_t owner) {
    if (!backend || owner == NOAH_PROFILE_STORAGE_ADMISSION_NONE || backend->admission_owner != owner || !noah_profile_store_settled(backend->store)) {
        return false;
    }
    backend->admission_owner = NOAH_PROFILE_STORAGE_ADMISSION_NONE;
    return true;
}

static bool validated_profile_matches_candidate(const noah_profile_validator_v1_profile_t *profile, const noah_profile_store_candidate_t *candidate) {
    return profile && candidate && profile->domain_mask == candidate->domain_mask && profile->byte_length == candidate->payload_length && profile->crc32 == candidate->payload_crc32 && profile->digest == candidate->payload_digest && profile->action_abi_digest == candidate->action_abi_digest;
}

bool noah_profile_candidate_store_backend_staged_candidate(const noah_profile_candidate_store_backend_t *backend, noah_profile_store_candidate_t *candidate) {
    const noah_profile_store_candidate_t *staged = backend && noah_profile_store_fully_staged(backend->store) ? noah_profile_store_candidate(backend->store) : NULL;

    if (!staged || !candidate || backend->admission_owner != NOAH_PROFILE_STORAGE_ADMISSION_HOST || !backend->validation_complete || backend->committed_available || !validated_profile_matches_candidate(&backend->validated_profile, staged)) {
        return false;
    }
    *candidate = *staged;
    return true;
}

bool noah_profile_candidate_store_backend_staged_read(void *context, const noah_profile_store_candidate_t *candidate, uint32_t offset, uint8_t *bytes, uint8_t length) {
    noah_profile_candidate_store_backend_t *backend = context;
    noah_profile_store_candidate_t          staged;
    noah_profile_storage_address_t  payload_start;

    if (!candidate || !bytes || length == 0u || !noah_profile_candidate_store_backend_staged_candidate(backend, &staged) || !noah_profile_candidate_equal(&staged, candidate) || !noah_profile_store_candidate_payload_start(backend->store, &payload_start) || offset > staged.payload_length || length > staged.payload_length - offset) {
        return false;
    }
    // A peer-copy chunk is wider than one store read; read it in pieces.
    for (uint8_t done = 0u; done < length;) {
        uint8_t piece = (uint8_t)(length - done) < NOAH_PROFILE_STORE_IO_CHUNK_MAX ? (uint8_t)(length - done) : NOAH_PROFILE_STORE_IO_CHUNK_MAX;
        if (!staged_reader_read(backend->store, (size_t)payload_start + offset + done, bytes + done, piece)) {
            return false;
        }
        done = (uint8_t)(done + piece);
    }
    return true;
}

bool noah_profile_candidate_store_backend_staged_reuse(const noah_profile_candidate_store_backend_t *backend, const noah_profile_store_candidate_t *candidate, uint32_t offset, noah_profile_candidate_v1_source_t *source, noah_profile_candidate_store_reuse_range_t *range) {
    noah_profile_store_candidate_t staged;

    if (!candidate || !source || !range || !noah_profile_candidate_store_backend_staged_candidate(backend, &staged) || !noah_profile_candidate_equal(&staged, candidate)) {
        return false;
    }
    for (uint8_t index = 0u; index < backend->reuse_range_count; index++) {
        if (backend->reuse_ranges[index].offset + backend->reuse_ranges[index].length > offset) {
            *source = backend->reuse_source;
            *range  = backend->reuse_ranges[index];
            return true;
        }
    }
    return false;
}

noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_adopt_committed_begin(noah_profile_candidate_store_backend_t *backend, const noah_profile_store_record_t *record, noah_profile_candidate_v1_error_t *error) {
    noah_profile_validator_v1_declaration_t declaration;
    noah_profile_validator_v1_error_t       validator_error = noah_profile_validator_v1_no_error();
    noah_profile_validator_v1_result_t      result;
    noah_profile_storage_address_t  payload_start;

    if (!backend || !backend->provider || !record || !noah_profile_store_settled(backend->store) || !noah_profile_record_equal(noah_profile_store_committed(backend->store), record) || !noah_profile_store_slot_payload_start(record->slot, &payload_start)) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    if (backend->validation_complete && backend->committed_available && noah_profile_record_equal(&backend->committed_record, record) && validated_profile_matches_record(&backend->validated_profile, record)) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_VALID;
    }

    backend->metadata = (noah_profile_candidate_v1_metadata_t){
        .schema_major      = record->schema_major,
        .schema_minor      = record->schema_minor,
        .requested_domains = record->domain_mask,
        .flags             = 0u,
        .payload_length    = record->payload_length,
        .crc32             = record->payload_crc32,
        .digest            = record->payload_digest,
        .action_abi_digest = record->action_abi_digest,
    };
    declaration = (noah_profile_validator_v1_declaration_t){
        .schema_major      = record->schema_major,
        .schema_minor      = record->schema_minor,
        .domain_mask       = record->domain_mask,
        .flags             = 0u,
        .byte_length       = record->payload_length,
        .crc32             = record->payload_crc32,
        .digest            = record->payload_digest,
        .action_abi_digest = record->action_abi_digest,
    };
    backend->validation_complete         = false;
    backend->committed_available         = false;
    backend->activation_requested        = false;
    backend->validating_committed_record = true;
    backend->committed_record            = *record;
    memset(&backend->validator, 0, sizeof(backend->validator));
    memset(&backend->validated_profile, 0, sizeof(backend->validated_profile));
    memset(&backend->activation_snapshot, 0, sizeof(backend->activation_snapshot));

    result = noah_profile_validator_v1_begin(&backend->validator, &backend->staged_reader, payload_start, &declaration, &backend->compatibility, &validator_error);
    if (result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS;
    }
    backend->validating_committed_record = false;
    map_validator_error(&validator_error, error);
    return result == NOAH_PROFILE_VALIDATOR_V1_VALID ? NOAH_PROFILE_CANDIDATE_BACKEND_VALID : (result == NOAH_PROFILE_VALIDATOR_V1_READ_ERROR ? NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR : NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED);
}

noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_adopt_committed_step(noah_profile_candidate_store_backend_t *backend, uint8_t byte_budget, noah_profile_candidate_v1_error_t *error) {
    if (!backend || !backend->validating_committed_record) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    return validation_step(backend, byte_budget, error);
}

static noah_profile_candidate_backend_result_t commit_begin_candidate(void *context) {
    noah_profile_candidate_store_backend_t *backend = context;
    noah_profile_store_result_t             result;

    if (!backend || !backend->store || !backend->validation_complete) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
    }
    if (backend->committed_available && !noah_profile_store_prepare_active(backend->store) && noah_profile_record_equal(noah_profile_store_committed(backend->store), &backend->committed_record)) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    }
    result = noah_profile_store_prepare_commit_begin(backend->store);
    if (result == NOAH_PROFILE_STORE_IN_PROGRESS) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS;
    }
    backend->validation_complete = false;
    return result == NOAH_PROFILE_STORE_OK ? NOAH_PROFILE_CANDIDATE_BACKEND_OK : NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
}

noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_prepare_durable_begin(noah_profile_candidate_store_backend_t *backend) {
    noah_profile_store_result_t result;

    if (!backend || !backend->store || !backend->validation_complete || backend->committed_available) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
    }
    if (noah_profile_store_durably_prepared(backend->store)) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    }
    result = noah_profile_store_prepare_durable_begin(backend->store);
    return result == NOAH_PROFILE_STORE_IN_PROGRESS ? NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS : (result == NOAH_PROFILE_STORE_OK ? NOAH_PROFILE_CANDIDATE_BACKEND_OK : NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR);
}

noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_prepare_durable_step(noah_profile_candidate_store_backend_t *backend, uint8_t byte_budget) {
    noah_profile_store_record_t prepared;
    noah_profile_store_result_t result;

    if (!backend || !backend->store || !backend->validation_complete) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
    }
    result = noah_profile_store_prepare_durable_step(backend->store, byte_budget, &prepared);
    if (result == NOAH_PROFILE_STORE_IN_PROGRESS) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS;
    }
    return result == NOAH_PROFILE_STORE_OK ? NOAH_PROFILE_CANDIDATE_BACKEND_OK : (result == NOAH_PROFILE_STORE_DURABILITY_UNKNOWN ? NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN : NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR);
}

noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_prepared_commit_begin(noah_profile_candidate_store_backend_t *backend) {
    noah_profile_store_result_t result;

    if (!backend || !backend->validation_complete || !noah_profile_store_durably_prepared(backend->store)) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
    }
    result = noah_profile_store_prepared_commit_begin(backend->store);
    return result == NOAH_PROFILE_STORE_IN_PROGRESS ? NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS : (result == NOAH_PROFILE_STORE_OK ? NOAH_PROFILE_CANDIDATE_BACKEND_OK : NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR);
}

noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_prepared_commit_step(noah_profile_candidate_store_backend_t *backend, uint8_t byte_budget) {
    noah_profile_store_record_t record;
    noah_profile_store_result_t result;

    if (!backend || !backend->store || !backend->validation_complete) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
    }
    result = noah_profile_store_prepared_commit_step(backend->store, byte_budget, &record);
    if (result == NOAH_PROFILE_STORE_IN_PROGRESS) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS;
    }
    if (result != NOAH_PROFILE_STORE_OK) {
        backend->validation_complete = false;
        return result == NOAH_PROFILE_STORE_DURABILITY_UNKNOWN ? NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN : NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    backend->committed_record    = record;
    backend->committed_available = true;
    return NOAH_PROFILE_CANDIDATE_BACKEND_OK;
}

static noah_profile_candidate_backend_result_t commit_step_candidate(void *context, uint8_t byte_budget) {
    noah_profile_candidate_store_backend_t *backend = context;
    noah_profile_store_record_t             record;
    noah_profile_store_result_t             result;

    if (!backend || !backend->store || !backend->validation_complete) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
    }
    result = noah_profile_store_prepare_commit_step(backend->store, byte_budget, &record);
    if (result == NOAH_PROFILE_STORE_IN_PROGRESS) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS;
    }
    if (result != NOAH_PROFILE_STORE_OK) {
        backend->validation_complete = false;
        return result == NOAH_PROFILE_STORE_DURABILITY_UNKNOWN ? NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN : NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    backend->committed_record    = record;
    backend->committed_available = true;
    return NOAH_PROFILE_CANDIDATE_BACKEND_OK;
}

static noah_profile_candidate_backend_result_t activation_begin_candidate(void *context) {
    noah_profile_candidate_store_backend_t *backend = context;
    return noah_profile_candidate_store_backend_activation_begin(backend, NOAH_PROFILE_STORAGE_ADMISSION_HOST);
}

static bool active_identity_matches_commit(const noah_effective_profile_identity_t *active, const noah_profile_store_record_t *record) {
    if (!active || !record) {
        return false;
    }
    if ((record->flags & NOAH_PROFILE_STORE_FLAG_OVERRIDE) == 0u) {
        return active->kind == NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS && active->compiled_default_digest == record->compiled_default_digest && active->action_abi_digest == record->action_abi_digest;
    }
    return active->kind == NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE && active->generation == record->generation && active->origin == record->origin_half && active->payload_crc32 == record->payload_crc32 && active->payload_digest == record->payload_digest && active->compiled_default_digest == record->compiled_default_digest && active->action_abi_digest == record->action_abi_digest;
}

static noah_profile_candidate_backend_result_t activation_step_candidate(void *context) {
    noah_profile_candidate_store_backend_t *backend = context;
    return noah_profile_candidate_store_backend_activation_step(backend, NOAH_PROFILE_STORAGE_ADMISSION_HOST);
}

static bool activation_owner_matches(const noah_profile_candidate_store_backend_t *backend, noah_profile_storage_admission_owner_t expected_owner) {
    return backend && backend->admission_owner == expected_owner && (expected_owner != NOAH_PROFILE_STORAGE_ADMISSION_NONE || backend->validating_committed_record || backend->committed_available);
}

noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_activation_begin(noah_profile_candidate_store_backend_t *backend, noah_profile_storage_admission_owner_t expected_owner) {
    noah_profile_candidate_backend_result_t result;

    if (!activation_owner_matches(backend, expected_owner)) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_BUSY;
    }
    result = noah_profile_candidate_store_backend_request_activation(backend);
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
        backend->activation_requested = true;
    }
    return result;
}

noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_activation_step(noah_profile_candidate_store_backend_t *backend, noah_profile_storage_admission_owner_t expected_owner) {
    noah_effective_profile_status_t status;
    noah_effective_profile_result_t result;

    if (!activation_owner_matches(backend, expected_owner)) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_BUSY;
    }
    if (!backend->provider || !backend->committed_available) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
    }
    if (!backend->activation_requested) {
        noah_profile_candidate_backend_result_t request_result = noah_profile_candidate_store_backend_request_activation(backend);

        if (request_result != NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
            return request_result;
        }
        backend->activation_requested = true;
        return NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS;
    }
    result = noah_effective_profile_provider_poll(backend->provider);
    if (result == NOAH_EFFECTIVE_PROFILE_PUBLISHED) {
        backend->admission_owner = NOAH_PROFILE_STORAGE_ADMISSION_NONE;
        return NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    }
    if (result == NOAH_EFFECTIVE_PROFILE_WAITING || result == NOAH_EFFECTIVE_PROFILE_BUSY || result == NOAH_EFFECTIVE_PROFILE_BACKING_REUSE_IN_PROGRESS) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS;
    }
    if (result == NOAH_EFFECTIVE_PROFILE_NO_PENDING && noah_effective_profile_provider_status(backend->provider, &status) == NOAH_EFFECTIVE_PROFILE_OK && active_identity_matches_commit(&status.active, &backend->committed_record)) {
        backend->admission_owner = NOAH_PROFILE_STORAGE_ADMISSION_NONE;
        return NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    }
    return NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
}

void noah_profile_candidate_store_backend_init(noah_profile_candidate_store_backend_t *backend, noah_profile_store_t *store, noah_effective_profile_provider_t *provider, const noah_profile_validator_v1_compatibility_t *compatibility, uint32_t compiled_default_digest, uint8_t origin_half) {
    noah_profile_store_reuse_guard_t guard;

    if (!backend) {
        return;
    }
    memset(backend, 0, sizeof(*backend));
    backend->store                   = store;
    backend->provider                = provider;
    backend->compiled_default_digest = compiled_default_digest;
    backend->origin_half             = origin_half;
    backend->staged_reader           = (noah_profile_reader_t){
        .read    = staged_reader_read,
        .context = store,
        .length  = NOAH_PROFILE_STORAGE_REGION_SIZE,
    };
    if (compatibility) {
        backend->compatibility = *compatibility;
    }
    guard = (noah_profile_store_reuse_guard_t){
        .begin   = begin_slot_reuse,
        .end     = end_slot_reuse,
        .context = backend,
    };
    backend->reuse_guard_installed = store && provider && noah_profile_store_set_reuse_guard(store, &guard);
}

void noah_profile_candidate_store_backend_hold_candidates_to(noah_profile_candidate_store_backend_t *backend, const noah_profile_validator_v1_runtime_t *runtime) {
    if (backend) {
        backend->candidate_runtime = runtime;
    }
}

noah_profile_candidate_backend_t noah_profile_candidate_store_backend_interface(noah_profile_candidate_store_backend_t *backend) {
    return (noah_profile_candidate_backend_t){
        .context          = backend,
        .begin            = begin_candidate,
        .write            = write_candidate,
        .read             = read_candidate,
        .read_source      = read_source,
        .note_reuse       = note_reuse,
        .validation_begin = validation_begin,
        .validation_step  = validation_step,
        .commit_begin     = commit_begin_candidate,
        .commit_step      = commit_step_candidate,
        .activation_begin = activation_begin_candidate,
        .activation_step  = activation_step_candidate,
        .abort            = abort_candidate,
    };
}

noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_commit(noah_profile_candidate_store_backend_t *backend, noah_profile_store_record_t *committed) {
    noah_profile_candidate_backend_result_t result = commit_begin_candidate(backend);

    while (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        result = commit_step_candidate(backend, NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET);
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_OK && committed) {
        *committed = backend->committed_record;
    }
    return result;
}

noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_request_activation(noah_profile_candidate_store_backend_t *backend) {
    noah_effective_profile_result_t result;
    noah_profile_storage_address_t  payload_start;

    if (!backend || !backend->provider || !backend->validation_complete || !backend->committed_available || !noah_profile_store_settled(backend->store) || !noah_profile_record_equal(noah_profile_store_committed(backend->store), &backend->committed_record) || backend->validated_profile.domain_mask != backend->committed_record.domain_mask || backend->validated_profile.byte_length != backend->committed_record.payload_length || backend->validated_profile.crc32 != backend->committed_record.payload_crc32 || backend->validated_profile.digest != backend->committed_record.payload_digest || backend->validated_profile.action_abi_digest != backend->committed_record.action_abi_digest || !noah_profile_store_slot_payload_start(backend->committed_record.slot, &payload_start)) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
    }
    if ((backend->committed_record.flags & NOAH_PROFILE_STORE_FLAG_OVERRIDE) == 0u) {
        result = noah_effective_profile_provider_request_compiled_fallback(backend->provider);
    } else {
        result = noah_effective_profile_snapshot_make_validated(&backend->validated_profile, &backend->staged_reader, payload_start, backend->committed_record.generation, backend->committed_record.origin_half, backend->committed_record.compiled_default_digest, &backend->activation_snapshot);
        if (result != NOAH_EFFECTIVE_PROFILE_OK) {
            return NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
        }
        result = noah_effective_profile_provider_request_validated(backend->provider, &backend->activation_snapshot);
    }
    if (result == NOAH_EFFECTIVE_PROFILE_OK || result == NOAH_EFFECTIVE_PROFILE_NO_CHANGE) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    }
    return result == NOAH_EFFECTIVE_PROFILE_BUSY || result == NOAH_EFFECTIVE_PROFILE_BACKING_REUSE_IN_PROGRESS ? NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS : NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
}

const noah_profile_validator_v1_profile_t *noah_profile_candidate_store_backend_validated_profile(const noah_profile_candidate_store_backend_t *backend) {
    return backend && backend->validation_complete ? &backend->validated_profile : NULL;
}

bool noah_profile_candidate_store_backend_committed(const noah_profile_candidate_store_backend_t *backend, noah_profile_store_record_t *record) {
    if (!backend || !record || !backend->validation_complete || !backend->committed_available || !noah_profile_record_equal(&backend->committed_record, noah_profile_store_committed(backend->store)) || !validated_profile_matches_record(&backend->validated_profile, &backend->committed_record)) {
        return false;
    }
    *record = backend->committed_record;
    return true;
}

_Static_assert((unsigned)NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX <= (unsigned)NOAH_PROFILE_STORE_IO_CHUNK_MAX, "candidate wire chunks must fit the store write bound");
