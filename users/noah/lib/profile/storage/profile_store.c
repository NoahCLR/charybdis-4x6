// ───────────────────────────────────────────────────────────────────────────
// Dual-Slot Persistent Live Profile Store
// ───────────────────────────────────────────────────────────────────────────

#include "profile_store.h"

#include <stddef.h>
#include <string.h>

#include "profile_checksum.h"

#ifdef VIA_ENABLE

enum {
    HEADER_MAGIC_0             = 0u,
    HEADER_MAGIC_1             = 1u,
    HEADER_FORMAT_VERSION      = 2u,
    HEADER_SCHEMA_VERSION      = 3u,
    HEADER_PAYLOAD_LENGTH      = 4u,
    HEADER_GENERATION          = 6u,
    HEADER_ORIGIN_HALF         = 10u,
    HEADER_FLAGS               = 11u,
    HEADER_PAYLOAD_CRC32       = 12u,
    HEADER_PAYLOAD_DIGEST      = 16u,
    HEADER_COMPILED_DIGEST     = 20u,
    HEADER_ACTION_ABI_DIGEST   = 24u,
    HEADER_CRC16               = 28u,
    HEADER_COMMIT_MARKER       = 30u,
    HEADER_CHECKSUMMED_LENGTH  = 28u,
    PROFILE_BLOB_HEADER_SIZE   = 8u,
    DOMAIN_ENVELOPE_SIZE       = 4u,
    PROFILE_BLOB_CANONICAL_BIT = 1u,
};

static const uint8_t header_magic[2]   = {'N', 'P'};
static const uint8_t profile_magic[4]  = {'N', 'L', 'P', '1'};
static const uint8_t commit_marker[2]  = {0xA5u, 0x5Au};
static const uint8_t invalid_marker[2] = {0u, 0u};

static uint16_t read_u16(const uint8_t *source) {
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8u);
}

static uint32_t read_u32(const uint8_t *source) {
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8u) | ((uint32_t)source[2] << 16u) | ((uint32_t)source[3] << 24u);
}

static void write_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
}

static void write_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
    target[2] = (uint8_t)(value >> 16u);
    target[3] = (uint8_t)(value >> 24u);
}

static bool slot_start(noah_profile_slot_t slot, uint16_t *address) {
    if (!address) {
        return false;
    }
    if (slot == NOAH_PROFILE_SLOT_A) {
        *address = NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR;
        return true;
    }
    if (slot == NOAH_PROFILE_SLOT_B) {
        *address = NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR;
        return true;
    }
    return false;
}

static bool io_read(noah_profile_store_t *store, uint16_t address, uint8_t *target, uint16_t length) {
    return store && store->io.read && target && length != 0u && ((uint32_t)address + length) <= NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE && store->io.read(store->io.context, address, target, length);
}

static bool io_write(noah_profile_store_t *store, uint16_t address, const uint8_t *source, uint16_t length) {
    return store && store->io.write && source && length != 0u && ((uint32_t)address + length) <= NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE && store->io.write(store->io.context, address, source, length);
}

static bool generation_is_newer(uint32_t candidate, uint32_t current) {
    return candidate != 0u && candidate > current;
}

static bool record_identity_equal(const noah_profile_store_record_t *lhs, const noah_profile_store_record_t *rhs) {
    return lhs->schema_major == rhs->schema_major && lhs->schema_minor == rhs->schema_minor && lhs->flags == rhs->flags && lhs->payload_length == rhs->payload_length && lhs->generation == rhs->generation && lhs->origin_half == rhs->origin_half && lhs->payload_crc32 == rhs->payload_crc32 && lhs->payload_digest == rhs->payload_digest && lhs->compiled_default_digest == rhs->compiled_default_digest && lhs->action_abi_digest == rhs->action_abi_digest;
}

static void encode_header(uint8_t *header, const noah_profile_store_candidate_t *candidate) {
    memset(header, 0, NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
    header[HEADER_MAGIC_0]        = header_magic[0];
    header[HEADER_MAGIC_1]        = header_magic[1];
    header[HEADER_FORMAT_VERSION] = NOAH_PROFILE_STORE_FORMAT_VERSION;
    header[HEADER_SCHEMA_VERSION] = (uint8_t)((candidate->schema_major << 4u) | candidate->schema_minor);
    write_u16(&header[HEADER_PAYLOAD_LENGTH], candidate->payload_length);
    write_u32(&header[HEADER_GENERATION], candidate->generation);
    header[HEADER_ORIGIN_HALF] = candidate->origin_half;
    header[HEADER_FLAGS]       = candidate->flags;
    write_u32(&header[HEADER_PAYLOAD_CRC32], candidate->payload_crc32);
    write_u32(&header[HEADER_PAYLOAD_DIGEST], candidate->payload_digest);
    write_u32(&header[HEADER_COMPILED_DIGEST], candidate->compiled_default_digest);
    write_u32(&header[HEADER_ACTION_ABI_DIGEST], candidate->action_abi_digest);
    write_u16(&header[HEADER_CRC16], noah_profile_crc16_ccitt_update(NOAH_PROFILE_CRC16_INITIAL, header, HEADER_CHECKSUMMED_LENGTH));
}

static noah_profile_store_result_t decode_header(noah_profile_store_t *store, noah_profile_slot_t slot, const uint8_t *header, bool require_commit, noah_profile_store_record_t *record) {
    uint16_t expected_crc;

    if (header[HEADER_MAGIC_0] != header_magic[0] || header[HEADER_MAGIC_1] != header_magic[1] || header[HEADER_FORMAT_VERSION] != NOAH_PROFILE_STORE_FORMAT_VERSION) {
        return NOAH_PROFILE_STORE_INVALID_HEADER;
    }
    if (require_commit && memcmp(&header[HEADER_COMMIT_MARKER], commit_marker, sizeof(commit_marker)) != 0) {
        return NOAH_PROFILE_STORE_INVALID_HEADER;
    }
    if ((header[HEADER_SCHEMA_VERSION] >> 4u) != store->compatibility.schema_major || (header[HEADER_SCHEMA_VERSION] & 0x0Fu) != store->compatibility.schema_minor) {
        return NOAH_PROFILE_STORE_INCOMPATIBLE_SCHEMA;
    }
    expected_crc = noah_profile_crc16_ccitt_update(NOAH_PROFILE_CRC16_INITIAL, header, HEADER_CHECKSUMMED_LENGTH);
    if (read_u16(&header[HEADER_CRC16]) != expected_crc) {
        return NOAH_PROFILE_STORE_CHECKSUM_MISMATCH;
    }

    memset(record, 0, sizeof(*record));
    record->slot                    = slot;
    record->schema_major            = header[HEADER_SCHEMA_VERSION] >> 4u;
    record->schema_minor            = header[HEADER_SCHEMA_VERSION] & 0x0Fu;
    record->payload_length          = read_u16(&header[HEADER_PAYLOAD_LENGTH]);
    record->generation              = read_u32(&header[HEADER_GENERATION]);
    record->origin_half             = header[HEADER_ORIGIN_HALF];
    record->flags                   = header[HEADER_FLAGS];
    record->payload_crc32           = read_u32(&header[HEADER_PAYLOAD_CRC32]);
    record->payload_digest          = read_u32(&header[HEADER_PAYLOAD_DIGEST]);
    record->compiled_default_digest = read_u32(&header[HEADER_COMPILED_DIGEST]);
    record->action_abi_digest       = read_u32(&header[HEADER_ACTION_ABI_DIGEST]);

    if (record->payload_length < PROFILE_BLOB_HEADER_SIZE || record->payload_length > NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX || record->generation == 0u || record->origin_half > 1u || (record->flags & (uint8_t)~NOAH_PROFILE_STORE_ALLOWED_FLAGS) != 0u) {
        return NOAH_PROFILE_STORE_INVALID_HEADER;
    }
    if (record->action_abi_digest != store->compatibility.action_abi_digest) {
        return NOAH_PROFILE_STORE_INCOMPATIBLE_ACTION_ABI;
    }
    return NOAH_PROFILE_STORE_OK;
}

static noah_profile_store_result_t validate_blob_shape(noah_profile_store_t *store, uint16_t payload_start, const noah_profile_store_record_t *record) {
    uint16_t offset;
    uint8_t  domain_count;
    uint8_t  prior_domain = 0u;
    uint8_t  domain_index;

    if (!io_read(store, payload_start, store->scratch, PROFILE_BLOB_HEADER_SIZE)) {
        return NOAH_PROFILE_STORE_IO_ERROR;
    }
    if (memcmp(store->scratch, profile_magic, sizeof(profile_magic)) != 0 || store->scratch[4] != record->schema_major || store->scratch[5] != record->schema_minor || store->scratch[7] != PROFILE_BLOB_CANONICAL_BIT) {
        return NOAH_PROFILE_STORE_INVALID_PAYLOAD;
    }

    domain_count = store->scratch[6];
    offset       = PROFILE_BLOB_HEADER_SIZE;
    for (domain_index = 0u; domain_index < domain_count; domain_index++) {
        uint8_t  domain_id;
        uint8_t  domain_version;
        uint16_t domain_length;

        if ((uint32_t)offset + DOMAIN_ENVELOPE_SIZE > record->payload_length || !io_read(store, (uint16_t)(payload_start + offset), store->scratch, DOMAIN_ENVELOPE_SIZE)) {
            return (uint32_t)offset + DOMAIN_ENVELOPE_SIZE > record->payload_length ? NOAH_PROFILE_STORE_INVALID_PAYLOAD : NOAH_PROFILE_STORE_IO_ERROR;
        }
        domain_id      = store->scratch[0];
        domain_version = store->scratch[1];
        domain_length  = read_u16(&store->scratch[2]);
        if (domain_id <= prior_domain || (domain_id != 0x10u && domain_id != 0x20u) || domain_version != 1u || (uint32_t)offset + DOMAIN_ENVELOPE_SIZE + domain_length > record->payload_length) {
            return NOAH_PROFILE_STORE_INVALID_PAYLOAD;
        }
        prior_domain = domain_id;
        offset       = (uint16_t)(offset + DOMAIN_ENVELOPE_SIZE + domain_length);
    }
    return offset == record->payload_length ? NOAH_PROFILE_STORE_OK : NOAH_PROFILE_STORE_INVALID_PAYLOAD;
}

static noah_profile_store_result_t validate_payload(noah_profile_store_t *store, uint16_t payload_start, const noah_profile_store_record_t *record) {
    uint16_t offset = 0u;
    uint32_t crc    = NOAH_PROFILE_CRC32_INITIAL;
    uint32_t digest = NOAH_PROFILE_FNV1A_INITIAL;

    while (offset < record->payload_length) {
        uint16_t length = (uint16_t)(record->payload_length - offset);

        if (length > sizeof(store->scratch)) {
            length = sizeof(store->scratch);
        }
        if (!io_read(store, (uint16_t)(payload_start + offset), store->scratch, length)) {
            return NOAH_PROFILE_STORE_IO_ERROR;
        }
        crc    = noah_profile_crc32_update(crc, store->scratch, length);
        digest = noah_profile_fnv1a_update(digest, store->scratch, length);
        offset = (uint16_t)(offset + length);
    }
    if (noah_profile_crc32_finish(crc) != record->payload_crc32 || digest != record->payload_digest) {
        return NOAH_PROFILE_STORE_CHECKSUM_MISMATCH;
    }
    return validate_blob_shape(store, payload_start, record);
}

void noah_profile_store_init(noah_profile_store_t *store, noah_profile_store_io_t io, noah_profile_store_compatibility_t compatibility) {
    if (!store) {
        return;
    }
    memset(store, 0, sizeof(*store));
    store->io            = io;
    store->compatibility = compatibility;
}

bool noah_profile_store_set_reuse_guard(noah_profile_store_t *store, const noah_profile_store_reuse_guard_t *guard) {
    if (!store || store->prepare_active || store->reuse_active || (guard && (!guard->begin || !guard->end))) {
        return false;
    }
    memset(&store->reuse_guard, 0, sizeof(store->reuse_guard));
    if (guard) {
        store->reuse_guard = *guard;
    }
    return true;
}

static noah_profile_store_result_t begin_reuse(noah_profile_store_t *store, noah_profile_slot_t slot) {
    if (!store->reuse_guard.begin) {
        return NOAH_PROFILE_STORE_OK;
    }
    if (!store->reuse_guard.begin(store->reuse_guard.context, slot)) {
        return NOAH_PROFILE_STORE_BACKING_REUSE_DENIED;
    }
    store->reuse_active = true;
    return NOAH_PROFILE_STORE_OK;
}

static noah_profile_store_result_t end_reuse(noah_profile_store_t *store) {
    noah_profile_slot_t slot;

    if (!store->reuse_active) {
        return NOAH_PROFILE_STORE_OK;
    }
    slot = store->candidate_slot;
    if (!store->reuse_guard.end(store->reuse_guard.context, slot)) {
        return NOAH_PROFILE_STORE_BACKING_REUSE_RELEASE_FAILED;
    }
    store->reuse_active = false;
    return NOAH_PROFILE_STORE_OK;
}

static noah_profile_store_result_t finish_prepare(noah_profile_store_t *store, noah_profile_store_result_t result) {
    noah_profile_store_result_t release_result;

    store->prepare_active = false;
    store->commit_phase   = NOAH_PROFILE_STORE_COMMIT_IDLE;
    store->commit_offset  = 0u;
    release_result        = end_reuse(store);
    return release_result == NOAH_PROFILE_STORE_OK ? result : release_result;
}

noah_profile_store_result_t noah_profile_store_validate_slot(noah_profile_store_t *store, noah_profile_slot_t slot, bool require_commit, noah_profile_store_record_t *record) {
    noah_profile_store_result_t result;
    uint16_t                    start;

    if (!store || !record || !store->io.read || !slot_start(slot, &start)) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }
    if (!io_read(store, start, store->scratch, NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE)) {
        return NOAH_PROFILE_STORE_IO_ERROR;
    }
    result = decode_header(store, slot, store->scratch, require_commit, record);
    if (result != NOAH_PROFILE_STORE_OK) {
        return result;
    }
    return validate_payload(store, (uint16_t)(start + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE), record);
}

noah_profile_store_result_t noah_profile_store_boot_select(noah_profile_store_t *store, noah_profile_store_record_t *selected) {
    noah_profile_store_record_t slot_a;
    noah_profile_store_record_t slot_b;
    noah_profile_store_result_t a_result;
    noah_profile_store_result_t b_result;

    if (!store || !selected || !store->io.read || store->prepare_active || store->reuse_active) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }

    a_result = noah_profile_store_validate_slot(store, NOAH_PROFILE_SLOT_A, true, &slot_a);
    b_result = noah_profile_store_validate_slot(store, NOAH_PROFILE_SLOT_B, true, &slot_b);
    memset(selected, 0, sizeof(*selected));
    memset(&store->committed, 0, sizeof(store->committed));
    store->boot_scanned   = false;
    store->conflict       = false;
    store->prepare_active = false;

    if (a_result == NOAH_PROFILE_STORE_IO_ERROR || b_result == NOAH_PROFILE_STORE_IO_ERROR) {
        return NOAH_PROFILE_STORE_IO_ERROR;
    }
    store->boot_scanned = true;
    if (a_result != NOAH_PROFILE_STORE_OK && b_result != NOAH_PROFILE_STORE_OK) {
        return NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE;
    }
    if (a_result == NOAH_PROFILE_STORE_OK && b_result != NOAH_PROFILE_STORE_OK) {
        *selected = slot_a;
    } else if (b_result == NOAH_PROFILE_STORE_OK && a_result != NOAH_PROFILE_STORE_OK) {
        *selected = slot_b;
    } else if (slot_a.generation > slot_b.generation) {
        *selected = slot_a;
    } else if (slot_b.generation > slot_a.generation) {
        *selected = slot_b;
    } else if (record_identity_equal(&slot_a, &slot_b)) {
        *selected = slot_b;
    } else {
        store->conflict = true;
        return NOAH_PROFILE_STORE_GENERATION_CONFLICT;
    }
    store->committed = *selected;
    return NOAH_PROFILE_STORE_OK;
}

static noah_profile_store_result_t validate_candidate(const noah_profile_store_t *store, const noah_profile_store_candidate_t *candidate) {
    if (!candidate || candidate->payload_length < PROFILE_BLOB_HEADER_SIZE || candidate->payload_length > NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX || candidate->generation == 0u || candidate->origin_half > 1u || candidate->schema_major > 15u || candidate->schema_minor > 15u || (candidate->flags & (uint8_t)~NOAH_PROFILE_STORE_ALLOWED_FLAGS) != 0u) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }
    if (candidate->schema_major != store->compatibility.schema_major || candidate->schema_minor != store->compatibility.schema_minor) {
        return NOAH_PROFILE_STORE_INCOMPATIBLE_SCHEMA;
    }
    if (candidate->action_abi_digest != store->compatibility.action_abi_digest) {
        return NOAH_PROFILE_STORE_INCOMPATIBLE_ACTION_ABI;
    }
    if (store->committed.slot != NOAH_PROFILE_SLOT_NONE && !generation_is_newer(candidate->generation, store->committed.generation)) {
        return NOAH_PROFILE_STORE_GENERATION_NOT_NEWER;
    }
    return NOAH_PROFILE_STORE_OK;
}

noah_profile_store_result_t noah_profile_store_prepare_begin(noah_profile_store_t *store, const noah_profile_store_candidate_t *candidate) {
    noah_profile_store_result_t result;
    uint16_t                    start;

    if (!store || !store->boot_scanned || !store->io.write) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }
    if (store->conflict) {
        return NOAH_PROFILE_STORE_GENERATION_CONFLICT;
    }
    if (store->prepare_active) {
        return NOAH_PROFILE_STORE_PREPARE_IN_PROGRESS;
    }
    if (store->reuse_active) {
        return NOAH_PROFILE_STORE_BACKING_REUSE_RELEASE_FAILED;
    }
    result = validate_candidate(store, candidate);
    if (result != NOAH_PROFILE_STORE_OK) {
        return result;
    }

    store->candidate_slot = store->committed.slot == NOAH_PROFILE_SLOT_A ? NOAH_PROFILE_SLOT_B : NOAH_PROFILE_SLOT_A;
    result                = begin_reuse(store, store->candidate_slot);
    if (result != NOAH_PROFILE_STORE_OK) {
        return result;
    }
    if (!slot_start(store->candidate_slot, &start) || !io_write(store, (uint16_t)(start + HEADER_COMMIT_MARKER), invalid_marker, sizeof(invalid_marker))) {
        return finish_prepare(store, NOAH_PROFILE_STORE_IO_ERROR);
    }
    store->candidate              = *candidate;
    store->candidate_written      = 0u;
    store->candidate_crc32_state  = NOAH_PROFILE_CRC32_INITIAL;
    store->candidate_digest_state = NOAH_PROFILE_FNV1A_INITIAL;
    store->commit_phase           = NOAH_PROFILE_STORE_COMMIT_IDLE;
    store->commit_offset          = 0u;
    store->prepare_active         = true;
    return NOAH_PROFILE_STORE_OK;
}

noah_profile_store_result_t noah_profile_store_prepare_write(noah_profile_store_t *store, uint16_t offset, const uint8_t *bytes, uint16_t length) {
    uint16_t start;

    if (!store || !store->prepare_active) {
        return NOAH_PROFILE_STORE_NO_PREPARE;
    }
    if (store->commit_phase != NOAH_PROFILE_STORE_COMMIT_IDLE) {
        return NOAH_PROFILE_STORE_PREPARE_IN_PROGRESS;
    }
    if (!bytes || length == 0u) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }
    if (length > NOAH_PROFILE_STORE_IO_CHUNK_MAX) {
        return NOAH_PROFILE_STORE_CHUNK_TOO_LARGE;
    }
    if (offset != store->candidate_written || (uint32_t)offset + length > store->candidate.payload_length) {
        return NOAH_PROFILE_STORE_CHUNK_OUT_OF_ORDER;
    }
    if (!slot_start(store->candidate_slot, &start) || !io_write(store, (uint16_t)(start + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE + offset), bytes, length)) {
        return finish_prepare(store, NOAH_PROFILE_STORE_IO_ERROR);
    }
    store->candidate_crc32_state  = noah_profile_crc32_update(store->candidate_crc32_state, bytes, length);
    store->candidate_digest_state = noah_profile_fnv1a_update(store->candidate_digest_state, bytes, length);
    store->candidate_written      = (uint16_t)(store->candidate_written + length);
    return NOAH_PROFILE_STORE_OK;
}

noah_profile_store_result_t noah_profile_store_prepare_commit_begin(noah_profile_store_t *store) {
    if (!store || !store->prepare_active) {
        return NOAH_PROFILE_STORE_NO_PREPARE;
    }
    if (store->commit_phase != NOAH_PROFILE_STORE_COMMIT_IDLE) {
        return NOAH_PROFILE_STORE_IN_PROGRESS;
    }
    if (store->candidate_written != store->candidate.payload_length) {
        return NOAH_PROFILE_STORE_PAYLOAD_INCOMPLETE;
    }
    if (noah_profile_crc32_finish(store->candidate_crc32_state) != store->candidate.payload_crc32 || store->candidate_digest_state != store->candidate.payload_digest) {
        return finish_prepare(store, NOAH_PROFILE_STORE_CHECKSUM_MISMATCH);
    }
    store->commit_phase         = NOAH_PROFILE_STORE_COMMIT_HEADER_WRITE;
    store->commit_offset        = 0u;
    store->commit_crc32_state   = NOAH_PROFILE_CRC32_INITIAL;
    store->commit_digest_state  = NOAH_PROFILE_FNV1A_INITIAL;
    store->commit_domain_count  = 0u;
    store->commit_domain_index  = 0u;
    store->commit_prior_domain  = 0u;
    store->commit_record_offset = 0u;
    return NOAH_PROFILE_STORE_IN_PROGRESS;
}

static uint16_t bounded_step_length(uint16_t remaining, uint8_t byte_budget) {
    return remaining < byte_budget ? remaining : byte_budget;
}

static void record_from_candidate(const noah_profile_store_t *store, noah_profile_store_record_t *record) {
    *record = (noah_profile_store_record_t){
        .slot                    = store->candidate_slot,
        .schema_major            = store->candidate.schema_major,
        .schema_minor            = store->candidate.schema_minor,
        .flags                   = store->candidate.flags,
        .payload_length          = store->candidate.payload_length,
        .generation              = store->candidate.generation,
        .origin_half             = store->candidate.origin_half,
        .payload_crc32           = store->candidate.payload_crc32,
        .payload_digest          = store->candidate.payload_digest,
        .compiled_default_digest = store->candidate.compiled_default_digest,
        .action_abi_digest       = store->candidate.action_abi_digest,
    };
}

noah_profile_store_result_t noah_profile_store_prepare_commit_step(noah_profile_store_t *store, uint8_t byte_budget, noah_profile_store_record_t *committed) {
    uint8_t                     expected_header[NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE];
    noah_profile_store_record_t record;
    uint16_t                    start;
    uint16_t                    length;

    if (!store || !store->prepare_active || store->commit_phase == NOAH_PROFILE_STORE_COMMIT_IDLE) {
        return NOAH_PROFILE_STORE_NO_PREPARE;
    }
    if (byte_budget == 0u || byte_budget > 20u || !slot_start(store->candidate_slot, &start)) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }

    switch (store->commit_phase) {
        case NOAH_PROFILE_STORE_COMMIT_HEADER_WRITE:
            encode_header(expected_header, &store->candidate);
            length = bounded_step_length((uint16_t)(HEADER_COMMIT_MARKER - store->commit_offset), byte_budget);
            if (!io_write(store, (uint16_t)(start + store->commit_offset), &expected_header[store->commit_offset], length)) {
                return finish_prepare(store, NOAH_PROFILE_STORE_IO_ERROR);
            }
            store->commit_offset = (uint16_t)(store->commit_offset + length);
            if (store->commit_offset == HEADER_COMMIT_MARKER) {
                store->commit_phase  = NOAH_PROFILE_STORE_COMMIT_HEADER_READBACK;
                store->commit_offset = 0u;
            }
            return NOAH_PROFILE_STORE_IN_PROGRESS;

        case NOAH_PROFILE_STORE_COMMIT_HEADER_READBACK:
            encode_header(expected_header, &store->candidate);
            length = bounded_step_length((uint16_t)(HEADER_COMMIT_MARKER - store->commit_offset), byte_budget);
            if (!io_read(store, (uint16_t)(start + store->commit_offset), store->scratch, length)) {
                return finish_prepare(store, NOAH_PROFILE_STORE_IO_ERROR);
            }
            if (memcmp(store->scratch, &expected_header[store->commit_offset], length) != 0) {
                return finish_prepare(store, NOAH_PROFILE_STORE_CHECKSUM_MISMATCH);
            }
            store->commit_offset = (uint16_t)(store->commit_offset + length);
            if (store->commit_offset == HEADER_COMMIT_MARKER) {
                store->commit_phase        = NOAH_PROFILE_STORE_COMMIT_PAYLOAD_READBACK;
                store->commit_offset       = 0u;
                store->commit_crc32_state  = NOAH_PROFILE_CRC32_INITIAL;
                store->commit_digest_state = NOAH_PROFILE_FNV1A_INITIAL;
            }
            return NOAH_PROFILE_STORE_IN_PROGRESS;

        case NOAH_PROFILE_STORE_COMMIT_PAYLOAD_READBACK:
            length = bounded_step_length((uint16_t)(store->candidate.payload_length - store->commit_offset), byte_budget);
            if (!io_read(store, (uint16_t)(start + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE + store->commit_offset), store->scratch, length)) {
                return finish_prepare(store, NOAH_PROFILE_STORE_IO_ERROR);
            }
            store->commit_crc32_state  = noah_profile_crc32_update(store->commit_crc32_state, store->scratch, length);
            store->commit_digest_state = noah_profile_fnv1a_update(store->commit_digest_state, store->scratch, length);
            store->commit_offset       = (uint16_t)(store->commit_offset + length);
            if (store->commit_offset == store->candidate.payload_length) {
                if (noah_profile_crc32_finish(store->commit_crc32_state) != store->candidate.payload_crc32 || store->commit_digest_state != store->candidate.payload_digest) {
                    return finish_prepare(store, NOAH_PROFILE_STORE_CHECKSUM_MISMATCH);
                }
                store->commit_phase  = NOAH_PROFILE_STORE_COMMIT_SHAPE_HEADER;
                store->commit_offset = 0u;
            }
            return NOAH_PROFILE_STORE_IN_PROGRESS;

        case NOAH_PROFILE_STORE_COMMIT_SHAPE_HEADER:
            length = bounded_step_length((uint16_t)(PROFILE_BLOB_HEADER_SIZE - store->commit_record_offset), byte_budget);
            if (!io_read(store, (uint16_t)(start + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE + store->commit_record_offset), &store->scratch[store->commit_record_offset], length)) {
                return finish_prepare(store, NOAH_PROFILE_STORE_IO_ERROR);
            }
            store->commit_record_offset = (uint8_t)(store->commit_record_offset + length);
            if (store->commit_record_offset != PROFILE_BLOB_HEADER_SIZE) {
                return NOAH_PROFILE_STORE_IN_PROGRESS;
            }
            if (memcmp(store->scratch, profile_magic, sizeof(profile_magic)) != 0 || store->scratch[4] != store->candidate.schema_major || store->scratch[5] != store->candidate.schema_minor || store->scratch[7] != PROFILE_BLOB_CANONICAL_BIT) {
                return finish_prepare(store, NOAH_PROFILE_STORE_INVALID_PAYLOAD);
            }
            store->commit_domain_count = store->scratch[6];
            store->commit_domain_index = 0u;
            store->commit_prior_domain = 0u;
            store->commit_offset       = PROFILE_BLOB_HEADER_SIZE;
            store->commit_record_offset = 0u;
            if (store->commit_domain_count == 0u) {
                if (store->commit_offset != store->candidate.payload_length) {
                    return finish_prepare(store, NOAH_PROFILE_STORE_INVALID_PAYLOAD);
                }
                store->commit_phase = NOAH_PROFILE_STORE_COMMIT_MARKER_WRITE;
            } else {
                store->commit_phase = NOAH_PROFILE_STORE_COMMIT_SHAPE_DOMAIN;
            }
            return NOAH_PROFILE_STORE_IN_PROGRESS;

        case NOAH_PROFILE_STORE_COMMIT_SHAPE_DOMAIN: {
            uint8_t  domain_id;
            uint8_t  domain_version;
            uint16_t domain_length;

            if ((uint32_t)store->commit_offset + DOMAIN_ENVELOPE_SIZE > store->candidate.payload_length) {
                return finish_prepare(store, NOAH_PROFILE_STORE_INVALID_PAYLOAD);
            }
            length = bounded_step_length((uint16_t)(DOMAIN_ENVELOPE_SIZE - store->commit_record_offset), byte_budget);
            if (!io_read(store, (uint16_t)(start + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE + store->commit_offset + store->commit_record_offset), &store->scratch[store->commit_record_offset], length)) {
                return finish_prepare(store, NOAH_PROFILE_STORE_IO_ERROR);
            }
            store->commit_record_offset = (uint8_t)(store->commit_record_offset + length);
            if (store->commit_record_offset != DOMAIN_ENVELOPE_SIZE) {
                return NOAH_PROFILE_STORE_IN_PROGRESS;
            }
            domain_id      = store->scratch[0];
            domain_version = store->scratch[1];
            domain_length  = read_u16(&store->scratch[2]);
            if (domain_id <= store->commit_prior_domain || (domain_id != 0x10u && domain_id != 0x20u) || domain_version != 1u || (uint32_t)store->commit_offset + DOMAIN_ENVELOPE_SIZE + domain_length > store->candidate.payload_length) {
                return finish_prepare(store, NOAH_PROFILE_STORE_INVALID_PAYLOAD);
            }
            store->commit_prior_domain = domain_id;
            store->commit_offset       = (uint16_t)(store->commit_offset + DOMAIN_ENVELOPE_SIZE + domain_length);
            store->commit_domain_index++;
            store->commit_record_offset = 0u;
            if (store->commit_domain_index == store->commit_domain_count) {
                if (store->commit_offset != store->candidate.payload_length) {
                    return finish_prepare(store, NOAH_PROFILE_STORE_INVALID_PAYLOAD);
                }
                store->commit_phase = NOAH_PROFILE_STORE_COMMIT_MARKER_WRITE;
            }
            return NOAH_PROFILE_STORE_IN_PROGRESS;
        }

        case NOAH_PROFILE_STORE_COMMIT_MARKER_WRITE:
            length = bounded_step_length((uint16_t)(sizeof(commit_marker) - store->commit_record_offset), byte_budget);
            if (!io_write(store, (uint16_t)(start + HEADER_COMMIT_MARKER + store->commit_record_offset), &commit_marker[store->commit_record_offset], length)) {
                return finish_prepare(store, store->commit_record_offset + length == sizeof(commit_marker) ? NOAH_PROFILE_STORE_DURABILITY_UNKNOWN : NOAH_PROFILE_STORE_IO_ERROR);
            }
            store->commit_record_offset = (uint8_t)(store->commit_record_offset + length);
            if (store->commit_record_offset == sizeof(commit_marker)) {
                store->commit_phase         = NOAH_PROFILE_STORE_COMMIT_MARKER_READBACK;
                store->commit_record_offset = 0u;
            }
            return NOAH_PROFILE_STORE_IN_PROGRESS;

        case NOAH_PROFILE_STORE_COMMIT_MARKER_READBACK:
            length = bounded_step_length((uint16_t)(sizeof(commit_marker) - store->commit_record_offset), byte_budget);
            if (!io_read(store, (uint16_t)(start + HEADER_COMMIT_MARKER + store->commit_record_offset), &store->scratch[store->commit_record_offset], length)) {
                return finish_prepare(store, NOAH_PROFILE_STORE_DURABILITY_UNKNOWN);
            }
            store->commit_record_offset = (uint8_t)(store->commit_record_offset + length);
            if (store->commit_record_offset != sizeof(commit_marker)) {
                return NOAH_PROFILE_STORE_IN_PROGRESS;
            }
            if (memcmp(store->scratch, commit_marker, sizeof(commit_marker)) != 0) {
                return finish_prepare(store, NOAH_PROFILE_STORE_DURABILITY_UNKNOWN);
            }
            record_from_candidate(store, &record);
            store->committed = record;
            if (committed) {
                *committed = record;
            }
            return finish_prepare(store, NOAH_PROFILE_STORE_OK);

        case NOAH_PROFILE_STORE_COMMIT_IDLE:
        default:
            return finish_prepare(store, NOAH_PROFILE_STORE_INVALID_ARGUMENT);
    }
}

noah_profile_store_result_t noah_profile_store_prepare_commit(noah_profile_store_t *store, noah_profile_store_record_t *committed) {
    noah_profile_store_result_t result = noah_profile_store_prepare_commit_begin(store);

    while (result == NOAH_PROFILE_STORE_IN_PROGRESS) {
        result = noah_profile_store_prepare_commit_step(store, 20u, committed);
    }
    return result;
}

noah_profile_store_result_t noah_profile_store_prepare_abort(noah_profile_store_t *store) {
    uint16_t start;

    if (!store || !store->prepare_active) {
        return NOAH_PROFILE_STORE_NO_PREPARE;
    }
    if (!slot_start(store->candidate_slot, &start) || !io_write(store, (uint16_t)(start + HEADER_COMMIT_MARKER), invalid_marker, sizeof(invalid_marker))) {
        return finish_prepare(store, NOAH_PROFILE_STORE_IO_ERROR);
    }
    return finish_prepare(store, NOAH_PROFILE_STORE_OK);
}

bool noah_profile_store_next_generation(const noah_profile_store_t *store, uint32_t *generation) {
    if (!store || !generation || !store->boot_scanned || store->conflict || store->committed.generation == UINT32_MAX) {
        return false;
    }
    *generation = store->committed.slot == NOAH_PROFILE_SLOT_NONE ? 1u : store->committed.generation + 1u;
    return true;
}

_Static_assert(NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE == 32u, "persistent profile header layout requires exactly 32 bytes");
_Static_assert(HEADER_COMMIT_MARKER + NOAH_PROFILE_STORAGE_COMMIT_MARKER_SIZE == NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE, "commit marker must be the final header field");
_Static_assert(NOAH_PROFILE_STORE_IO_CHUNK_MAX <= NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX, "bounded write chunk must fit a slot payload");

#endif
