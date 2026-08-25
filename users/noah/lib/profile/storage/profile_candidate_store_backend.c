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

static bool staged_reader_read(void *context, size_t offset, uint8_t *target, size_t length) {
    noah_profile_store_t *store = context;

    if (!store || !store->io.read || !target || length == 0u || offset > UINT16_MAX || length > UINT16_MAX || offset + length > NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE) {
        return false;
    }
    return store->io.read(store->io.context, (uint16_t)offset, target, (uint16_t)length);
}

static noah_profile_candidate_backend_result_t map_store_result(noah_profile_store_result_t result) {
    return result == NOAH_PROFILE_STORE_OK ? NOAH_PROFILE_CANDIDATE_BACKEND_OK : NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
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

static noah_profile_candidate_backend_result_t begin_candidate(void *context, const noah_profile_candidate_v1_metadata_t *metadata) {
    noah_profile_candidate_store_backend_t *backend = context;
    noah_profile_store_candidate_t          candidate;
    uint32_t                                generation;

    if (!backend || !backend->store || !metadata || backend->origin_half > 1u || !noah_profile_store_next_generation(backend->store, &generation)) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    candidate = (noah_profile_store_candidate_t){
        .schema_major            = metadata->schema_major,
        .schema_minor            = metadata->schema_minor,
        .flags                   = NOAH_PROFILE_STORE_FLAG_OVERRIDE,
        .payload_length          = metadata->payload_length,
        .generation              = generation,
        .origin_half             = backend->origin_half,
        .payload_crc32           = metadata->crc32,
        .payload_digest          = metadata->digest,
        .compiled_default_digest = backend->compiled_default_digest,
        .action_abi_digest       = metadata->action_abi_digest,
    };
    if (noah_profile_store_prepare_begin(backend->store, &candidate) != NOAH_PROFILE_STORE_OK) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    backend->metadata            = *metadata;
    backend->validation_complete = false;
    memset(&backend->validator, 0, sizeof(backend->validator));
    memset(&backend->validated_profile, 0, sizeof(backend->validated_profile));
    backend->staged_reader = (noah_profile_reader_t){
        .read    = staged_reader_read,
        .context = backend->store,
        .length  = NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE,
    };
    return NOAH_PROFILE_CANDIDATE_BACKEND_OK;
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
    noah_profile_candidate_store_backend_t   *backend = context;
    noah_profile_validator_v1_declaration_t   declaration;
    noah_profile_validator_v1_error_t         validator_error = noah_profile_validator_v1_no_error();
    noah_profile_validator_v1_result_t        result;
    uint16_t                                  payload_start;

    if (!backend || !backend->store || !metadata || !backend->store->prepare_active || backend->store->candidate_written != metadata->payload_length || !metadata_equal(metadata, &backend->metadata) || !candidate_payload_start(backend, &payload_start)) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
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
    noah_profile_candidate_store_backend_t *backend = context;
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
        return backend->validation_complete ? NOAH_PROFILE_CANDIDATE_BACKEND_VALID : NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    map_validator_error(&validator_error, error);
    return result == NOAH_PROFILE_VALIDATOR_V1_READ_ERROR ? NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR : NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
}

static noah_profile_candidate_backend_result_t abort_candidate(void *context) {
    noah_profile_candidate_store_backend_t *backend = context;
    noah_profile_store_result_t             result;

    if (!backend || !backend->store) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    result = backend->store->prepare_active ? noah_profile_store_prepare_abort(backend->store) : NOAH_PROFILE_STORE_OK;
    backend->validation_complete = false;
    memset(&backend->metadata, 0, sizeof(backend->metadata));
    memset(&backend->validated_profile, 0, sizeof(backend->validated_profile));
    return map_store_result(result);
}

void noah_profile_candidate_store_backend_init(noah_profile_candidate_store_backend_t *backend, noah_profile_store_t *store, const noah_profile_validator_v1_compatibility_t *compatibility, uint32_t compiled_default_digest, uint8_t origin_half) {
    if (!backend) {
        return;
    }
    memset(backend, 0, sizeof(*backend));
    backend->store                   = store;
    backend->compiled_default_digest = compiled_default_digest;
    backend->origin_half             = origin_half;
    if (compatibility) {
        backend->compatibility = *compatibility;
    }
}

noah_profile_candidate_backend_t noah_profile_candidate_store_backend_interface(noah_profile_candidate_store_backend_t *backend) {
    return (noah_profile_candidate_backend_t){
        .context          = backend,
        .begin            = begin_candidate,
        .write            = write_candidate,
        .read             = read_candidate,
        .validation_begin = validation_begin,
        .validation_step  = validation_step,
        .abort            = abort_candidate,
    };
}

noah_profile_candidate_backend_result_t noah_profile_candidate_store_backend_commit(noah_profile_candidate_store_backend_t *backend, noah_profile_store_record_t *committed) {
    if (!backend || !backend->store || !backend->validation_complete) {
        return NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED;
    }
    if (noah_profile_store_prepare_commit(backend->store, committed) != NOAH_PROFILE_STORE_OK) {
        backend->validation_complete = false;
        return NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR;
    }
    return NOAH_PROFILE_CANDIDATE_BACKEND_OK;
}

const noah_profile_validator_v1_profile_t *noah_profile_candidate_store_backend_validated_profile(const noah_profile_candidate_store_backend_t *backend) {
    return backend && backend->validation_complete ? &backend->validated_profile : NULL;
}

_Static_assert((unsigned)NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX <= (unsigned)NOAH_PROFILE_STORE_IO_CHUNK_MAX, "candidate wire chunks must fit the store write bound");

#endif
