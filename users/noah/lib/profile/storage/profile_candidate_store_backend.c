// ───────────────────────────────────────────────────────────────────────────
// Candidate Transaction -> Dual-Slot Store Backend
// ───────────────────────────────────────────────────────────────────────────

#include "profile_candidate_store_backend.h"

#include "profile_storage_layout.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#ifdef VIA_ENABLE

static bool metadata_equal(const noah_profile_candidate_v1_metadata_t *left, const noah_profile_candidate_v1_metadata_t *right) {
    return left && right && left->schema_major == right->schema_major && left->schema_minor == right->schema_minor && left->requested_domains == right->requested_domains && left->flags == right->flags && left->payload_length == right->payload_length && left->crc32 == right->crc32 && left->digest == right->digest && left->action_abi_digest == right->action_abi_digest;
}

static bool committed_record_equal(const noah_profile_store_record_t *left, const noah_profile_store_record_t *right) {
    return left && right && left->slot == right->slot && left->schema_major == right->schema_major && left->schema_minor == right->schema_minor && left->domain_mask == right->domain_mask && left->flags == right->flags && left->payload_length == right->payload_length && left->generation == right->generation && left->origin_half == right->origin_half && left->payload_crc32 == right->payload_crc32 && left->payload_digest == right->payload_digest && left->compiled_default_digest == right->compiled_default_digest && left->action_abi_digest == right->action_abi_digest;
}

static bool validated_profile_matches_record(const noah_profile_validator_v1_profile_t *profile, const noah_profile_store_record_t *record) {
    return profile && record && profile->domain_mask == record->domain_mask && profile->byte_length == record->payload_length && profile->crc32 == record->payload_crc32 && profile->digest == record->payload_digest && profile->action_abi_digest == record->action_abi_digest;
}

static bool metadata_matches_candidate(const noah_profile_candidate_v1_metadata_t *metadata, const noah_profile_store_candidate_t *candidate) {
    return metadata && candidate && metadata->schema_major == candidate->schema_major && metadata->schema_minor == candidate->schema_minor && metadata->requested_domains == candidate->domain_mask && metadata->flags == 0u && metadata->payload_length == candidate->payload_length && metadata->crc32 == candidate->payload_crc32 && metadata->digest == candidate->payload_digest && metadata->action_abi_digest == candidate->action_abi_digest;
}

static bool candidate_payload_start(const noah_profile_candidate_store_backend_t *backend, uint16_t *address) {
    uint16_t slot_start;

    if (!backend || !backend->store || !address) {
        return false;
    }
    if (backend->store->candidate_slot == NOAH_PROFILE_SLOT_A) {
        slot_start = NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR;
    } else if (backend->store->candidate_slot == NOAH_PROFILE_SLOT_B) {
        slot_start = NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR;
    } else {
        return false;
    }
    *address = (uint16_t)(slot_start + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
    return true;
}

static bool slot_payload_start(noah_profile_slot_t slot, uint16_t *address) {
    if (!address) {
        return false;
    }
    if (slot == NOAH_PROFILE_SLOT_A) {
        *address = (uint16_t)(NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
        return true;
    }
    if (slot == NOAH_PROFILE_SLOT_B) {
        *address = (uint16_t)(NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
        return true;
    }
    return false;
}

static bool staged_reader_read(void *context, size_t offset, uint8_t *target, size_t length) {
    noah_profile_store_t *store = context;

    if (!store || !store->io.read || !target || length == 0u || offset > UINT16_MAX || length > UINT16_MAX || offset + length > NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE) {
        return false;
    }
    return store->io.read(store->io.context, (uint16_t)offset, target, (uint16_t)length);
}

static bool begin_slot_reuse(void *context, noah_profile_slot_t slot) {
    noah_profile_candidate_store_backend_t *backend = context;
    noah_effective_profile_backing_t        backing;
    uint16_t                                payload_start;

    if (!backend || !backend->provider || !slot_payload_start(slot, &payload_start)) {
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
    target->byte_offset = source->byte_offset <= UINT16_MAX ? (uint16_t)source->byte_offset : NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16;
}

static noah_profile_store_result_t begin_exact_with_owner(noah_profile_candidate_store_backend_t *backend, const noah_profile_candidate_v1_metadata_t *metadata, const noah_profile_store_candidate_t *candidate, noah_profile_storage_admission_owner_t owner) {
    noah_profile_store_result_t result;

    if (!backend || !backend->store || !backend->provider || !backend->reuse_guard_installed || owner == NOAH_PROFILE_STORAGE_ADMISSION_NONE || !metadata_matches_candidate(metadata, candidate) || candidate->compiled_default_digest != backend->compiled_default_digest) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }
    if (backend->admission_owner != NOAH_PROFILE_STORAGE_ADMISSION_NONE) {
        return backend->store->reconciliation_required ? NOAH_PROFILE_STORE_DURABILITY_UNKNOWN : NOAH_PROFILE_STORE_PREPARE_IN_PROGRESS;
    }
    backend->admission_owner = owner;
    result = noah_profile_store_prepare_begin(backend->store, candidate);
    if (result != NOAH_PROFILE_STORE_OK) {
        backend->admission_owner = NOAH_PROFILE_STORAGE_ADMISSION_NONE;
        return result;
    }
    backend->metadata             = *metadata;
    backend->validation_complete  = false;
    backend->committed_available  = false;
    backend->activation_requested = false;
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

static noah_profile_candidate_backend_result_t write_candidate(void *context, uint16_t offset, const uint8_t *bytes, uint8_t length) {
    noah_profile_candidate_store_backend_t *backend = context;

    if (!backend || !backend->store) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    return map_store_result(noah_profile_store_prepare_write(backend->store, offset, bytes, length));
}

static noah_profile_candidate_backend_result_t read_candidate(void *context, uint16_t offset, uint8_t *bytes, uint8_t length) {
    noah_profile_candidate_store_backend_t *backend = context;
    uint16_t                                payload_start;

    return candidate_payload_start(backend, &payload_start) && staged_reader_read(backend->store, (size_t)payload_start + offset, bytes, length) ? NOAH_PROFILE_CANDIDATE_BACKEND_OK : NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
}

static noah_profile_candidate_backend_result_t validation_begin(void *context, const noah_profile_candidate_v1_metadata_t *metadata, noah_profile_candidate_v1_error_t *error) {
    noah_profile_candidate_store_backend_t *backend = context;
    noah_profile_validator_v1_declaration_t declaration;
    noah_profile_validator_v1_error_t       validator_error = noah_profile_validator_v1_no_error();
    noah_profile_validator_v1_result_t      result;
    uint16_t                                payload_start;

    if (!backend || !backend->store || !metadata || !backend->store->prepare_active || backend->store->candidate_written != metadata->payload_length || !metadata_equal(metadata, &backend->metadata) || !candidate_payload_start(backend, &payload_start)) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    backend->validating_committed_record = false;
    declaration = (noah_profile_validator_v1_declaration_t){
        .schema_major      = metadata->schema_major,
        .schema_minor      = metadata->schema_minor,
        .domain_mask       = metadata->requested_domains,
        .flags             = metadata->flags,
        .byte_length       = metadata->payload_length,
        .crc32             = metadata->crc32,
        .digest            = metadata->digest,
        .action_abi_digest = metadata->action_abi_digest,
    };
    result = noah_profile_validator_v1_begin(&backend->validator, &backend->staged_reader, payload_start, &declaration, &backend->compatibility, &validator_error);
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

static noah_profile_candidate_backend_result_t validation_step(void *context, uint8_t byte_budget, noah_profile_candidate_v1_error_t *error) {
    noah_profile_candidate_store_backend_t *backend         = context;
    noah_profile_validator_v1_error_t       validator_error = noah_profile_validator_v1_no_error();
    noah_profile_validator_v1_result_t      result;

    if (!backend) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    result = noah_profile_validator_v1_step(&backend->validator, byte_budget, &validator_error);
    if (result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS;
    }
    if (result == NOAH_PROFILE_VALIDATOR_V1_VALID) {
        backend->validation_complete = noah_profile_validator_v1_profile(&backend->validator, &backend->validated_profile, &validator_error) == NOAH_PROFILE_VALIDATOR_V1_VALID;
        if (backend->validation_complete && backend->validating_committed_record) {
            backend->committed_available = validated_profile_matches_record(&backend->validated_profile, &backend->committed_record);
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
    result                        = backend->store->prepare_active ? noah_profile_store_prepare_abort(backend->store) : NOAH_PROFILE_STORE_OK;
    backend->validation_complete  = false;
    backend->activation_requested = false;
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
    if (!backend || owner == NOAH_PROFILE_STORAGE_ADMISSION_NONE || backend->admission_owner != owner || !backend->store || backend->store->prepare_active || backend->store->reuse_active) {
        return false;
    }
    backend->admission_owner = NOAH_PROFILE_STORAGE_ADMISSION_NONE;
    return true;
}

noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_adopt_committed_begin(noah_profile_candidate_store_backend_t *backend, const noah_profile_store_record_t *record, noah_profile_candidate_v1_error_t *error) {
    noah_profile_validator_v1_declaration_t declaration;
    noah_profile_validator_v1_error_t       validator_error = noah_profile_validator_v1_no_error();
    noah_profile_validator_v1_result_t      result;
    uint16_t                                payload_start;

    if (!backend || !backend->store || !backend->provider || !record || record->slot == NOAH_PROFILE_SLOT_NONE || backend->store->prepare_active || backend->store->reuse_active || !committed_record_equal(&backend->store->committed, record) || !slot_payload_start(record->slot, &payload_start)) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    if (backend->validation_complete && backend->committed_available && committed_record_equal(&backend->committed_record, record) && validated_profile_matches_record(&backend->validated_profile, record)) {
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
    backend->validation_complete          = false;
    backend->committed_available          = false;
    backend->activation_requested         = false;
    backend->validating_committed_record  = true;
    backend->committed_record             = *record;
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
    if (backend->committed_available && !backend->store->prepare_active && committed_record_equal(&backend->store->committed, &backend->committed_record)) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    }
    result = noah_profile_store_prepare_commit_begin(backend->store);
    if (result == NOAH_PROFILE_STORE_IN_PROGRESS) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS;
    }
    backend->validation_complete = false;
    return result == NOAH_PROFILE_STORE_OK ? NOAH_PROFILE_CANDIDATE_BACKEND_OK : NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
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
    noah_profile_candidate_backend_result_t result  = noah_profile_candidate_store_backend_request_activation(backend);

    if (backend) {
        backend->activation_requested = result == NOAH_PROFILE_CANDIDATE_BACKEND_OK;
    }
    return result;
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
    noah_effective_profile_status_t         status;
    noah_effective_profile_result_t         result;

    if (!backend || !backend->provider || !backend->committed_available) {
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
        .length  = NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE,
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

noah_profile_candidate_backend_t noah_profile_candidate_store_backend_interface(noah_profile_candidate_store_backend_t *backend) {
    return (noah_profile_candidate_backend_t){
        .context          = backend,
        .begin            = begin_candidate,
        .write            = write_candidate,
        .read             = read_candidate,
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
    uint16_t                        payload_start;

    if (!backend || !backend->store || !backend->provider || !backend->validation_complete || !backend->committed_available || backend->store->prepare_active || backend->store->reuse_active || !committed_record_equal(&backend->store->committed, &backend->committed_record) || backend->validated_profile.domain_mask != backend->committed_record.domain_mask || backend->validated_profile.byte_length != backend->committed_record.payload_length || backend->validated_profile.crc32 != backend->committed_record.payload_crc32 || backend->validated_profile.digest != backend->committed_record.payload_digest || backend->validated_profile.action_abi_digest != backend->committed_record.action_abi_digest || !slot_payload_start(backend->committed_record.slot, &payload_start)) {
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

_Static_assert((unsigned)NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX <= (unsigned)NOAH_PROFILE_STORE_IO_CHUNK_MAX, "candidate wire chunks must fit the store write bound");

#endif
