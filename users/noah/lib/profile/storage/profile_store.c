// ───────────────────────────────────────────────────────────────────────────
// Dual-Slot Persistent Live Profile Store
// ───────────────────────────────────────────────────────────────────────────

#include "profile_store.h"

#include <stddef.h>
#include <string.h>

#include "profile_checksum.h"

#ifdef VIA_ENABLE

enum {
    LEGACY_HEADER_FORMAT_VERSION      = 2u,
    LEGACY_HEADER_SCHEMA_VERSION      = 3u,
    LEGACY_HEADER_PAYLOAD_LENGTH      = 4u,
    LEGACY_HEADER_GENERATION          = 6u,
    LEGACY_HEADER_ORIGIN_HALF         = 10u,
    LEGACY_HEADER_FLAGS               = 11u,
    LEGACY_HEADER_PAYLOAD_CRC32       = 12u,
    LEGACY_HEADER_PAYLOAD_DIGEST      = 16u,
    LEGACY_HEADER_COMPILED_DIGEST     = 20u,
    LEGACY_HEADER_ACTION_ABI_DIGEST   = 24u,
    LEGACY_HEADER_CRC16               = 28u,
    LEGACY_HEADER_MARKER              = 30u,
    LEGACY_HEADER_CHECKSUMMED_LENGTH  = 28u,
    LOGICAL_HEADER_IDENTITY           = 2u,
    LOGICAL_HEADER_PAYLOAD_LENGTH     = 3u,
    LOGICAL_HEADER_GENERATION         = 5u,
    LOGICAL_HEADER_PAYLOAD_CRC32      = 9u,
    LOGICAL_HEADER_COMPILED_DIGEST    = 13u,
    LOGICAL_HEADER_ACTION_ABI_DIGEST  = 17u,
    LOGICAL_HEADER_VIA_GENERATION     = 21u,
    LOGICAL_HEADER_VIA_DIGEST         = 25u,
    LOGICAL_HEADER_CRC16              = 29u,
    LOGICAL_HEADER_MARKER             = 31u,
    LOGICAL_HEADER_CHECKSUMMED_LENGTH = 29u,
    PROFILE_BLOB_HEADER_SIZE   = 8u,
    DOMAIN_ENVELOPE_SIZE       = 4u,
    PROFILE_BLOB_CANONICAL_BIT = 1u,
    DOMAIN_ID_RGB              = 0x10u,
    DOMAIN_ID_SETTINGS = 0x40u,
    DOMAIN_MASK_SETTINGS = 1u << 3,
    DOMAIN_ID_COMBOS = 0x30u,
    DOMAIN_MASK_COMBOS = 1u << 2,
    DOMAIN_ID_KEY_BEHAVIORS    = 0x20u,
    DOMAIN_MASK_RGB            = 1u << 0,
    DOMAIN_MASK_KEY_BEHAVIORS  = 1u << 1,
    DOMAIN_MASK_ALL            = DOMAIN_MASK_RGB | DOMAIN_MASK_KEY_BEHAVIORS | DOMAIN_MASK_COMBOS | DOMAIN_MASK_SETTINGS,
};

static const uint8_t legacy_header_magic[2] = {'N', 'P'};
static const uint8_t logical_header_magic[2] = {'N', 'Q'};
static const uint8_t profile_magic[4]  = {'N', 'L', 'P', '1'};
static const uint8_t commit_marker[2]  = {0xA5u, 0x5Au};
static const uint8_t prepared_marker[2] = {0x5Au, 0xA5u};
static const uint8_t invalid_marker[2] = {0u, 0u};
static const uint8_t logical_commit_marker = 0xA5u;
static const uint8_t logical_prepared_marker = 0x5Au;

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
    return lhs->format_version == rhs->format_version && lhs->schema_major == rhs->schema_major && lhs->schema_minor == rhs->schema_minor && lhs->domain_mask == rhs->domain_mask && lhs->flags == rhs->flags && lhs->payload_length == rhs->payload_length && lhs->generation == rhs->generation && lhs->origin_half == rhs->origin_half && lhs->payload_crc32 == rhs->payload_crc32 && lhs->payload_digest == rhs->payload_digest && lhs->compiled_default_digest == rhs->compiled_default_digest && lhs->action_abi_digest == rhs->action_abi_digest && lhs->via_generation == rhs->via_generation && lhs->via_digest == rhs->via_digest;
}

static uint8_t domain_mask_for_id(uint8_t domain_id) {
    if (domain_id == DOMAIN_ID_RGB) {
        return DOMAIN_MASK_RGB;
    }
    if (domain_id == DOMAIN_ID_SETTINGS) return DOMAIN_MASK_SETTINGS;
    if (domain_id == DOMAIN_ID_COMBOS) return DOMAIN_MASK_COMBOS;
    if (domain_id == DOMAIN_ID_KEY_BEHAVIORS) {
        return DOMAIN_MASK_KEY_BEHAVIORS;
    }
    return 0u;
}

static void encode_header(uint8_t *header, const noah_profile_store_candidate_t *candidate) {
    memset(header, 0, NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
    if (candidate->format_version == NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL) {
        memcpy(header, logical_header_magic, sizeof(logical_header_magic));
        header[LOGICAL_HEADER_IDENTITY] = (uint8_t)((candidate->domain_mask & 0x0Fu) | (candidate->origin_half << 4u) | ((candidate->flags & 1u) << 5u));
        write_u16(&header[LOGICAL_HEADER_PAYLOAD_LENGTH], candidate->payload_length);
        write_u32(&header[LOGICAL_HEADER_GENERATION], candidate->generation);
        write_u32(&header[LOGICAL_HEADER_PAYLOAD_CRC32], candidate->payload_crc32);
        write_u32(&header[LOGICAL_HEADER_COMPILED_DIGEST], candidate->compiled_default_digest);
        write_u32(&header[LOGICAL_HEADER_ACTION_ABI_DIGEST], candidate->action_abi_digest);
        write_u32(&header[LOGICAL_HEADER_VIA_GENERATION], candidate->via_generation);
        write_u32(&header[LOGICAL_HEADER_VIA_DIGEST], candidate->via_digest);
        write_u16(&header[LOGICAL_HEADER_CRC16], noah_profile_crc16_ccitt_update(NOAH_PROFILE_CRC16_INITIAL, header, LOGICAL_HEADER_CHECKSUMMED_LENGTH));
        return;
    }
    memcpy(header, legacy_header_magic, sizeof(legacy_header_magic));
    header[LEGACY_HEADER_FORMAT_VERSION] = NOAH_PROFILE_STORE_FORMAT_VERSION_LEGACY;
    header[LEGACY_HEADER_SCHEMA_VERSION] = (uint8_t)((candidate->schema_major << 4u) | candidate->schema_minor);
    write_u16(&header[LEGACY_HEADER_PAYLOAD_LENGTH], candidate->payload_length);
    write_u32(&header[LEGACY_HEADER_GENERATION], candidate->generation);
    header[LEGACY_HEADER_ORIGIN_HALF] = candidate->origin_half;
    header[LEGACY_HEADER_FLAGS]       = candidate->flags;
    write_u32(&header[LEGACY_HEADER_PAYLOAD_CRC32], candidate->payload_crc32);
    write_u32(&header[LEGACY_HEADER_PAYLOAD_DIGEST], candidate->payload_digest);
    write_u32(&header[LEGACY_HEADER_COMPILED_DIGEST], candidate->compiled_default_digest);
    write_u32(&header[LEGACY_HEADER_ACTION_ABI_DIGEST], candidate->action_abi_digest);
    write_u16(&header[LEGACY_HEADER_CRC16], noah_profile_crc16_ccitt_update(NOAH_PROFILE_CRC16_INITIAL, header, LEGACY_HEADER_CHECKSUMMED_LENGTH));
}

static noah_profile_store_result_t decode_header(noah_profile_store_t *store, noah_profile_slot_t slot, const uint8_t *header, bool require_commit, noah_profile_store_record_t *record) {
    uint16_t expected_crc;
    uint8_t  format_version;

    if (memcmp(header, legacy_header_magic, sizeof(legacy_header_magic)) == 0 && header[LEGACY_HEADER_FORMAT_VERSION] == NOAH_PROFILE_STORE_FORMAT_VERSION_LEGACY) {
        format_version = NOAH_PROFILE_STORE_FORMAT_VERSION_LEGACY;
    } else if (memcmp(header, logical_header_magic, sizeof(logical_header_magic)) == 0) {
        format_version = NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL;
    } else {
        return NOAH_PROFILE_STORE_INVALID_HEADER;
    }
    if (require_commit && (format_version == NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL ? header[LOGICAL_HEADER_MARKER] != logical_commit_marker : memcmp(&header[LEGACY_HEADER_MARKER], commit_marker, sizeof(commit_marker)) != 0)) {
        return NOAH_PROFILE_STORE_INVALID_HEADER;
    }
    expected_crc = noah_profile_crc16_ccitt_update(NOAH_PROFILE_CRC16_INITIAL, header, format_version == NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL ? LOGICAL_HEADER_CHECKSUMMED_LENGTH : LEGACY_HEADER_CHECKSUMMED_LENGTH);
    if (read_u16(&header[format_version == NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL ? LOGICAL_HEADER_CRC16 : LEGACY_HEADER_CRC16]) != expected_crc) {
        return NOAH_PROFILE_STORE_CHECKSUM_MISMATCH;
    }

    memset(record, 0, sizeof(*record));
    record->slot                    = slot;
    record->format_version          = format_version;
    if (format_version == NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL) {
        uint8_t identity = header[LOGICAL_HEADER_IDENTITY];
        if ((identity & 0xC0u) != 0u) {
            return NOAH_PROFILE_STORE_INVALID_HEADER;
        }
        record->schema_major            = store->compatibility.schema_major;
        record->schema_minor            = store->compatibility.schema_minor;
        record->domain_mask             = identity & 0x0Fu;
        record->origin_half             = (identity >> 4u) & 1u;
        record->flags                   = (identity >> 5u) & 1u;
        record->payload_length          = read_u16(&header[LOGICAL_HEADER_PAYLOAD_LENGTH]);
        record->generation              = read_u32(&header[LOGICAL_HEADER_GENERATION]);
        record->payload_crc32           = read_u32(&header[LOGICAL_HEADER_PAYLOAD_CRC32]);
        record->compiled_default_digest = read_u32(&header[LOGICAL_HEADER_COMPILED_DIGEST]);
        record->action_abi_digest       = read_u32(&header[LOGICAL_HEADER_ACTION_ABI_DIGEST]);
        record->via_generation          = read_u32(&header[LOGICAL_HEADER_VIA_GENERATION]);
        record->via_digest              = read_u32(&header[LOGICAL_HEADER_VIA_DIGEST]);
    } else {
        record->schema_major            = header[LEGACY_HEADER_SCHEMA_VERSION] >> 4u;
        record->schema_minor            = header[LEGACY_HEADER_SCHEMA_VERSION] & 0x0Fu;
        record->payload_length          = read_u16(&header[LEGACY_HEADER_PAYLOAD_LENGTH]);
        record->generation              = read_u32(&header[LEGACY_HEADER_GENERATION]);
        record->origin_half             = header[LEGACY_HEADER_ORIGIN_HALF];
        record->flags                   = header[LEGACY_HEADER_FLAGS];
        record->payload_crc32           = read_u32(&header[LEGACY_HEADER_PAYLOAD_CRC32]);
        record->payload_digest          = read_u32(&header[LEGACY_HEADER_PAYLOAD_DIGEST]);
        record->compiled_default_digest = read_u32(&header[LEGACY_HEADER_COMPILED_DIGEST]);
        record->action_abi_digest       = read_u32(&header[LEGACY_HEADER_ACTION_ABI_DIGEST]);
        if (record->schema_major != store->compatibility.schema_major || record->schema_minor != store->compatibility.schema_minor) {
            return NOAH_PROFILE_STORE_INCOMPATIBLE_SCHEMA;
        }
    }

    if (record->payload_length < PROFILE_BLOB_HEADER_SIZE || record->payload_length > NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX || record->generation == 0u || record->origin_half > 1u || (record->flags & (uint8_t)~NOAH_PROFILE_STORE_ALLOWED_FLAGS) != 0u || (format_version == NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL && (record->via_generation == 0u || record->via_digest == 0u))) {
        return NOAH_PROFILE_STORE_INVALID_HEADER;
    }
    if (record->action_abi_digest != store->compatibility.action_abi_digest) {
        return NOAH_PROFILE_STORE_INCOMPATIBLE_ACTION_ABI;
    }
    if (record->compiled_default_digest != store->compatibility.compiled_default_digest) {
        return NOAH_PROFILE_STORE_INCOMPATIBLE_COMPILED_DEFAULT;
    }
    return NOAH_PROFILE_STORE_OK;
}

static noah_profile_store_result_t validate_blob_shape(noah_profile_store_t *store, uint16_t payload_start, noah_profile_store_record_t *record) {
    uint16_t offset;
    uint8_t  domain_count;
    uint8_t  expected_domain_mask = record->domain_mask;
    uint8_t  prior_domain = 0u;
    uint8_t  domain_index;

    if (!io_read(store, payload_start, store->scratch, PROFILE_BLOB_HEADER_SIZE)) {
        return NOAH_PROFILE_STORE_IO_ERROR;
    }
    if (memcmp(store->scratch, profile_magic, sizeof(profile_magic)) != 0 || store->scratch[4] != record->schema_major || store->scratch[5] != record->schema_minor || store->scratch[7] != PROFILE_BLOB_CANONICAL_BIT) {
        return NOAH_PROFILE_STORE_INVALID_PAYLOAD;
    }

    domain_count        = store->scratch[6];
    offset              = PROFILE_BLOB_HEADER_SIZE;
    record->domain_mask = 0u;
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
        if (domain_id <= prior_domain || domain_mask_for_id(domain_id) == 0u || domain_version != 1u || (uint32_t)offset + DOMAIN_ENVELOPE_SIZE + domain_length > record->payload_length) {
            return NOAH_PROFILE_STORE_INVALID_PAYLOAD;
        }
        record->domain_mask |= domain_mask_for_id(domain_id);
        prior_domain = domain_id;
        offset       = (uint16_t)(offset + DOMAIN_ENVELOPE_SIZE + domain_length);
    }
    if (offset != record->payload_length || (record->format_version == NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL && record->domain_mask != expected_domain_mask)) {
        return NOAH_PROFILE_STORE_INVALID_PAYLOAD;
    }
    return NOAH_PROFILE_STORE_OK;
}

static noah_profile_store_result_t validate_payload(noah_profile_store_t *store, uint16_t payload_start, noah_profile_store_record_t *record) {
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
    if (noah_profile_crc32_finish(crc) != record->payload_crc32 || (record->format_version != NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL && digest != record->payload_digest)) {
        return NOAH_PROFILE_STORE_CHECKSUM_MISMATCH;
    }
    record->payload_digest = digest;
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

    if (result == NOAH_PROFILE_STORE_DURABILITY_UNKNOWN) {
        store->reconciliation_required = true;
    }
    store->prepare_active = false;
    store->prepared_durable = false;
    store->auto_commit_prepared = false;
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
    noah_profile_store_result_t result;

    if (!selected) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }
    result = noah_profile_store_boot_select_begin(store);

    while (result == NOAH_PROFILE_STORE_IN_PROGRESS) {
        result = noah_profile_store_boot_select_step(store, NOAH_PROFILE_STORE_IO_CHUNK_MAX, selected);
    }
    return result;
}

static bool boot_phase_is_slot_a(noah_profile_store_boot_phase_t phase) {
    return phase >= NOAH_PROFILE_STORE_BOOT_SLOT_A_HEADER && phase <= NOAH_PROFILE_STORE_BOOT_SLOT_A_SHAPE_DOMAIN;
}

static bool boot_phase_is_header(noah_profile_store_boot_phase_t phase) {
    return phase == NOAH_PROFILE_STORE_BOOT_SLOT_A_HEADER || phase == NOAH_PROFILE_STORE_BOOT_SLOT_B_HEADER;
}

static bool boot_phase_is_payload(noah_profile_store_boot_phase_t phase) {
    return phase == NOAH_PROFILE_STORE_BOOT_SLOT_A_PAYLOAD || phase == NOAH_PROFILE_STORE_BOOT_SLOT_B_PAYLOAD;
}

static bool boot_phase_is_shape_header(noah_profile_store_boot_phase_t phase) {
    return phase == NOAH_PROFILE_STORE_BOOT_SLOT_A_SHAPE_HEADER || phase == NOAH_PROFILE_STORE_BOOT_SLOT_B_SHAPE_HEADER;
}

static noah_profile_store_boot_phase_t boot_payload_phase(bool slot_a) {
    return slot_a ? NOAH_PROFILE_STORE_BOOT_SLOT_A_PAYLOAD : NOAH_PROFILE_STORE_BOOT_SLOT_B_PAYLOAD;
}

static noah_profile_store_boot_phase_t boot_shape_header_phase(bool slot_a) {
    return slot_a ? NOAH_PROFILE_STORE_BOOT_SLOT_A_SHAPE_HEADER : NOAH_PROFILE_STORE_BOOT_SLOT_B_SHAPE_HEADER;
}

static noah_profile_store_boot_phase_t boot_shape_domain_phase(bool slot_a) {
    return slot_a ? NOAH_PROFILE_STORE_BOOT_SLOT_A_SHAPE_DOMAIN : NOAH_PROFILE_STORE_BOOT_SLOT_B_SHAPE_DOMAIN;
}

static noah_profile_store_result_t boot_finish_selection(noah_profile_store_t *store, noah_profile_store_record_t *selected) {
    const noah_profile_store_record_t *slot_b = &store->boot_current;

    memset(selected, 0, sizeof(*selected));
    memset(&store->committed, 0, sizeof(store->committed));
    store->boot_scanned            = false;
    store->reconciliation_required = false;
    store->boot_phase              = NOAH_PROFILE_STORE_BOOT_DONE;
    if (store->boot_slot_a_result == NOAH_PROFILE_STORE_IO_ERROR || store->boot_slot_b_result == NOAH_PROFILE_STORE_IO_ERROR) {
        store->boot_result = NOAH_PROFILE_STORE_IO_ERROR;
    } else if (store->boot_slot_a_result != NOAH_PROFILE_STORE_OK && store->boot_slot_b_result != NOAH_PROFILE_STORE_OK) {
        store->boot_scanned = true;
        store->boot_result = NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE;
    } else if (store->boot_slot_a_result == NOAH_PROFILE_STORE_OK && store->boot_slot_b_result != NOAH_PROFILE_STORE_OK) {
        store->boot_scanned = true;
        *selected = store->boot_slot_a;
        store->boot_result = NOAH_PROFILE_STORE_OK;
    } else if (store->boot_slot_b_result == NOAH_PROFILE_STORE_OK && store->boot_slot_a_result != NOAH_PROFILE_STORE_OK) {
        store->boot_scanned = true;
        *selected = *slot_b;
        store->boot_result = NOAH_PROFILE_STORE_OK;
    } else if (store->boot_slot_a.generation > slot_b->generation) {
        store->boot_scanned = true;
        *selected = store->boot_slot_a;
        store->boot_result = NOAH_PROFILE_STORE_OK;
    } else if (slot_b->generation > store->boot_slot_a.generation || record_identity_equal(&store->boot_slot_a, slot_b)) {
        store->boot_scanned = true;
        *selected = *slot_b;
        store->boot_result = NOAH_PROFILE_STORE_OK;
    } else {
        store->boot_scanned = true;
        store->conflict    = true;
        store->boot_result = NOAH_PROFILE_STORE_GENERATION_CONFLICT;
    }
    if (store->boot_result == NOAH_PROFILE_STORE_OK) {
        store->committed = *selected;
    }
    return store->boot_result;
}

static noah_profile_store_result_t boot_finish_slot(noah_profile_store_t *store, noah_profile_store_result_t result, noah_profile_store_record_t *selected) {
    if (boot_phase_is_slot_a(store->boot_phase)) {
        store->boot_slot_a_result = result;
        if (result == NOAH_PROFILE_STORE_OK) {
            store->boot_slot_a = store->boot_current;
        }
        memset(&store->boot_current, 0, sizeof(store->boot_current));
        store->boot_phase = NOAH_PROFILE_STORE_BOOT_SLOT_B_HEADER;
        return NOAH_PROFILE_STORE_IN_PROGRESS;
    }
    store->boot_slot_b_result = result;
    return boot_finish_selection(store, selected);
}

noah_profile_store_result_t noah_profile_store_boot_select_begin(noah_profile_store_t *store) {
    if (!store || !store->io.read || store->prepare_active || store->reuse_active) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }
    memset(&store->committed, 0, sizeof(store->committed));
    memset(&store->boot_slot_a, 0, sizeof(store->boot_slot_a));
    memset(&store->boot_current, 0, sizeof(store->boot_current));
    store->boot_slot_a_result = NOAH_PROFILE_STORE_INVALID_HEADER;
    store->boot_slot_b_result = NOAH_PROFILE_STORE_INVALID_HEADER;
    store->boot_result        = NOAH_PROFILE_STORE_IN_PROGRESS;
    store->boot_phase         = NOAH_PROFILE_STORE_BOOT_SLOT_A_HEADER;
    store->boot_payload_start = 0u;
    store->boot_offset        = 0u;
    store->boot_crc32_state   = NOAH_PROFILE_CRC32_INITIAL;
    store->boot_digest_state  = NOAH_PROFILE_FNV1A_INITIAL;
    store->boot_domain_count  = 0u;
    store->boot_domain_index  = 0u;
    store->boot_prior_domain  = 0u;
    store->boot_scanned       = false;
    store->conflict           = false;
    store->prepare_active     = false;
    return NOAH_PROFILE_STORE_IN_PROGRESS;
}

noah_profile_store_result_t noah_profile_store_boot_select_step(noah_profile_store_t *store, uint8_t byte_budget, noah_profile_store_record_t *selected) {
    bool                        slot_a;
    noah_profile_slot_t         slot;
    uint16_t                    start;
    noah_profile_store_result_t result;

    if (!store || !selected || !store->io.read || byte_budget == 0u || byte_budget > NOAH_PROFILE_STORE_IO_CHUNK_MAX || store->prepare_active || store->reuse_active) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }
    if (store->boot_phase == NOAH_PROFILE_STORE_BOOT_DONE) {
        if (store->boot_result == NOAH_PROFILE_STORE_OK) {
            *selected = store->committed;
        } else {
            memset(selected, 0, sizeof(*selected));
        }
        return store->boot_result;
    }
    if (store->boot_phase == NOAH_PROFILE_STORE_BOOT_IDLE) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }

    slot_a = boot_phase_is_slot_a(store->boot_phase);
    slot   = slot_a ? NOAH_PROFILE_SLOT_A : NOAH_PROFILE_SLOT_B;
    if (!slot_start(slot, &start)) {
        return boot_finish_slot(store, NOAH_PROFILE_STORE_INVALID_ARGUMENT, selected);
    }

    if (boot_phase_is_header(store->boot_phase)) {
        if (!io_read(store, start, store->scratch, NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE)) {
            return boot_finish_slot(store, NOAH_PROFILE_STORE_IO_ERROR, selected);
        }
        result = decode_header(store, slot, store->scratch, true, &store->boot_current);
        if (result != NOAH_PROFILE_STORE_OK) {
            return boot_finish_slot(store, result, selected);
        }
        store->boot_payload_start = (uint16_t)(start + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
        store->boot_offset        = 0u;
        store->boot_crc32_state   = NOAH_PROFILE_CRC32_INITIAL;
        store->boot_digest_state  = NOAH_PROFILE_FNV1A_INITIAL;
        store->boot_phase         = boot_payload_phase(slot_a);
        return NOAH_PROFILE_STORE_IN_PROGRESS;
    }

    if (boot_phase_is_payload(store->boot_phase)) {
        uint16_t length = (uint16_t)(store->boot_current.payload_length - store->boot_offset);

        if (length > byte_budget) {
            length = byte_budget;
        }
        if (!io_read(store, (uint16_t)(store->boot_payload_start + store->boot_offset), store->scratch, length)) {
            return boot_finish_slot(store, NOAH_PROFILE_STORE_IO_ERROR, selected);
        }
        store->boot_crc32_state  = noah_profile_crc32_update(store->boot_crc32_state, store->scratch, length);
        store->boot_digest_state = noah_profile_fnv1a_update(store->boot_digest_state, store->scratch, length);
        store->boot_offset       = (uint16_t)(store->boot_offset + length);
        if (store->boot_offset == store->boot_current.payload_length) {
            if (noah_profile_crc32_finish(store->boot_crc32_state) != store->boot_current.payload_crc32 || (store->boot_current.format_version != NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL && store->boot_digest_state != store->boot_current.payload_digest)) {
                return boot_finish_slot(store, NOAH_PROFILE_STORE_CHECKSUM_MISMATCH, selected);
            }
            store->boot_current.payload_digest = store->boot_digest_state;
            store->boot_phase = boot_shape_header_phase(slot_a);
        }
        return NOAH_PROFILE_STORE_IN_PROGRESS;
    }

    if (boot_phase_is_shape_header(store->boot_phase)) {
        if (!io_read(store, store->boot_payload_start, store->scratch, PROFILE_BLOB_HEADER_SIZE)) {
            return boot_finish_slot(store, NOAH_PROFILE_STORE_IO_ERROR, selected);
        }
        if (memcmp(store->scratch, profile_magic, sizeof(profile_magic)) != 0 || store->scratch[4] != store->boot_current.schema_major || store->scratch[5] != store->boot_current.schema_minor || store->scratch[7] != PROFILE_BLOB_CANONICAL_BIT) {
            return boot_finish_slot(store, NOAH_PROFILE_STORE_INVALID_PAYLOAD, selected);
        }
        store->boot_domain_count        = store->scratch[6];
        store->boot_domain_index        = 0u;
        store->boot_prior_domain        = 0u;
        store->boot_expected_domain_mask = store->boot_current.domain_mask;
        store->boot_offset              = PROFILE_BLOB_HEADER_SIZE;
        store->boot_current.domain_mask = 0u;
        if (store->boot_domain_count == 0u) {
            bool shape_valid = store->boot_offset == store->boot_current.payload_length && (store->boot_current.format_version != NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL || store->boot_expected_domain_mask == 0u);
            return boot_finish_slot(store, shape_valid ? NOAH_PROFILE_STORE_OK : NOAH_PROFILE_STORE_INVALID_PAYLOAD, selected);
        }
        store->boot_phase = boot_shape_domain_phase(slot_a);
        return NOAH_PROFILE_STORE_IN_PROGRESS;
    }

    {
        uint8_t  domain_id;
        uint8_t  domain_version;
        uint16_t domain_length;

        if ((uint32_t)store->boot_offset + DOMAIN_ENVELOPE_SIZE > store->boot_current.payload_length) {
            return boot_finish_slot(store, NOAH_PROFILE_STORE_INVALID_PAYLOAD, selected);
        }
        if (!io_read(store, (uint16_t)(store->boot_payload_start + store->boot_offset), store->scratch, DOMAIN_ENVELOPE_SIZE)) {
            return boot_finish_slot(store, NOAH_PROFILE_STORE_IO_ERROR, selected);
        }
        domain_id      = store->scratch[0];
        domain_version = store->scratch[1];
        domain_length  = read_u16(&store->scratch[2]);
        if (domain_id <= store->boot_prior_domain || domain_mask_for_id(domain_id) == 0u || domain_version != 1u || (uint32_t)store->boot_offset + DOMAIN_ENVELOPE_SIZE + domain_length > store->boot_current.payload_length) {
            return boot_finish_slot(store, NOAH_PROFILE_STORE_INVALID_PAYLOAD, selected);
        }
        store->boot_current.domain_mask |= domain_mask_for_id(domain_id);
        store->boot_prior_domain = domain_id;
        store->boot_offset       = (uint16_t)(store->boot_offset + DOMAIN_ENVELOPE_SIZE + domain_length);
        store->boot_domain_index++;
        if (store->boot_domain_index == store->boot_domain_count) {
            bool shape_valid = store->boot_offset == store->boot_current.payload_length && (store->boot_current.format_version != NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL || store->boot_current.domain_mask == store->boot_expected_domain_mask);
            return boot_finish_slot(store, shape_valid ? NOAH_PROFILE_STORE_OK : NOAH_PROFILE_STORE_INVALID_PAYLOAD, selected);
        }
        return NOAH_PROFILE_STORE_IN_PROGRESS;
    }
}

static noah_profile_store_result_t validate_candidate(const noah_profile_store_t *store, const noah_profile_store_candidate_t *candidate) {
    if (!candidate || candidate->payload_length < PROFILE_BLOB_HEADER_SIZE || candidate->payload_length > NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX || candidate->generation == 0u || candidate->origin_half > 1u || candidate->schema_major > 15u || candidate->schema_minor > 15u || (candidate->flags & (uint8_t)~NOAH_PROFILE_STORE_ALLOWED_FLAGS) != 0u || (candidate->format_version != NOAH_PROFILE_STORE_FORMAT_VERSION_LEGACY && candidate->format_version != NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL) || (candidate->format_version == NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL && (candidate->via_generation == 0u || candidate->via_digest == 0u))) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }
    if (candidate->schema_major != store->compatibility.schema_major || candidate->schema_minor != store->compatibility.schema_minor) {
        return NOAH_PROFILE_STORE_INCOMPATIBLE_SCHEMA;
    }
    if (candidate->action_abi_digest != store->compatibility.action_abi_digest) {
        return NOAH_PROFILE_STORE_INCOMPATIBLE_ACTION_ABI;
    }
    if (candidate->compiled_default_digest != store->compatibility.compiled_default_digest) {
        return NOAH_PROFILE_STORE_INCOMPATIBLE_COMPILED_DEFAULT;
    }
    if ((candidate->domain_mask & (uint8_t)~DOMAIN_MASK_ALL) != 0u) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }
    if (store->committed.slot != NOAH_PROFILE_SLOT_NONE && !generation_is_newer(candidate->generation, store->committed.generation)) {
        return NOAH_PROFILE_STORE_GENERATION_NOT_NEWER;
    }
    return NOAH_PROFILE_STORE_OK;
}

noah_profile_store_result_t noah_profile_store_prepare_begin(noah_profile_store_t *store, const noah_profile_store_candidate_t *candidate) {
    noah_profile_store_result_t result;
    noah_profile_store_candidate_t normalized;
    uint16_t                    start;

    if (!store || !store->boot_scanned || !store->io.write) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }
    if (store->conflict) {
        return NOAH_PROFILE_STORE_GENERATION_CONFLICT;
    }
    if (store->reconciliation_required) {
        return NOAH_PROFILE_STORE_DURABILITY_UNKNOWN;
    }
    if (store->prepare_active) {
        return NOAH_PROFILE_STORE_PREPARE_IN_PROGRESS;
    }
    if (store->reuse_active) {
        return NOAH_PROFILE_STORE_BACKING_REUSE_RELEASE_FAILED;
    }
    if (!candidate) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }
    normalized = *candidate;
    if (normalized.format_version == 0u) {
        normalized.format_version = NOAH_PROFILE_STORE_FORMAT_VERSION_LEGACY;
    }
    result = validate_candidate(store, &normalized);
    if (result != NOAH_PROFILE_STORE_OK) {
        return result;
    }

    store->candidate_slot = store->committed.slot == NOAH_PROFILE_SLOT_A ? NOAH_PROFILE_SLOT_B : NOAH_PROFILE_SLOT_A;
    result                = begin_reuse(store, store->candidate_slot);
    if (result != NOAH_PROFILE_STORE_OK) {
        return result;
    }
    if (!slot_start(store->candidate_slot, &start) || !io_write(store, (uint16_t)(start + LEGACY_HEADER_MARKER), invalid_marker, sizeof(invalid_marker))) {
        return finish_prepare(store, NOAH_PROFILE_STORE_IO_ERROR);
    }
    store->candidate              = normalized;
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

static noah_profile_store_result_t prepare_durable_begin(noah_profile_store_t *store, bool auto_commit) {
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
    store->commit_domain_mask   = 0u;
    store->commit_record_offset = 0u;
    store->prepared_durable     = false;
    store->auto_commit_prepared = auto_commit;
    return NOAH_PROFILE_STORE_IN_PROGRESS;
}

noah_profile_store_result_t noah_profile_store_prepare_durable_begin(noah_profile_store_t *store) {
    return prepare_durable_begin(store, false);
}

noah_profile_store_result_t noah_profile_store_prepare_commit_begin(noah_profile_store_t *store) {
    return prepare_durable_begin(store, true);
}

static uint16_t bounded_step_length(uint16_t remaining, uint8_t byte_budget) {
    return remaining < byte_budget ? remaining : byte_budget;
}

static void record_from_candidate(const noah_profile_store_t *store, noah_profile_store_record_t *record) {
    *record = (noah_profile_store_record_t){
        .slot                    = store->candidate_slot,
        .format_version          = store->candidate.format_version,
        .schema_major            = store->candidate.schema_major,
        .schema_minor            = store->candidate.schema_minor,
        .domain_mask             = store->candidate.domain_mask,
        .flags                   = store->candidate.flags,
        .payload_length          = store->candidate.payload_length,
        .generation              = store->candidate.generation,
        .origin_half             = store->candidate.origin_half,
        .payload_crc32           = store->candidate.payload_crc32,
        .payload_digest          = store->candidate.payload_digest,
        .compiled_default_digest = store->candidate.compiled_default_digest,
        .action_abi_digest       = store->candidate.action_abi_digest,
        .via_generation          = store->candidate.via_generation,
        .via_digest              = store->candidate.via_digest,
    };
}

noah_profile_store_result_t noah_profile_store_prepare_commit_step(noah_profile_store_t *store, uint8_t byte_budget, noah_profile_store_record_t *committed) {
    uint8_t                     expected_header[NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE];
    const uint8_t              *commit_value;
    const uint8_t              *prepared_value;
    noah_profile_store_record_t record;
    uint16_t                    start;
    uint16_t                    length;
    uint8_t                     marker_length;
    uint8_t                     marker_offset;

    if (!store || !store->prepare_active || store->commit_phase == NOAH_PROFILE_STORE_COMMIT_IDLE) {
        return NOAH_PROFILE_STORE_NO_PREPARE;
    }
    if (byte_budget == 0u || byte_budget > 20u || !slot_start(store->candidate_slot, &start)) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }
    if (store->candidate.format_version == NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL) {
        marker_offset  = LOGICAL_HEADER_MARKER;
        marker_length  = 1u;
        prepared_value = &logical_prepared_marker;
        commit_value   = &logical_commit_marker;
    } else {
        marker_offset  = LEGACY_HEADER_MARKER;
        marker_length  = sizeof(prepared_marker);
        prepared_value = prepared_marker;
        commit_value   = commit_marker;
    }

    switch (store->commit_phase) {
        case NOAH_PROFILE_STORE_COMMIT_HEADER_WRITE:
            encode_header(expected_header, &store->candidate);
            length = bounded_step_length((uint16_t)(marker_offset - store->commit_offset), byte_budget);
            if (!io_write(store, (uint16_t)(start + store->commit_offset), &expected_header[store->commit_offset], length)) {
                return finish_prepare(store, NOAH_PROFILE_STORE_IO_ERROR);
            }
            store->commit_offset = (uint16_t)(store->commit_offset + length);
            if (store->commit_offset == marker_offset) {
                store->commit_phase  = NOAH_PROFILE_STORE_COMMIT_HEADER_READBACK;
                store->commit_offset = 0u;
            }
            return NOAH_PROFILE_STORE_IN_PROGRESS;

        case NOAH_PROFILE_STORE_COMMIT_HEADER_READBACK:
            encode_header(expected_header, &store->candidate);
            length = bounded_step_length((uint16_t)(marker_offset - store->commit_offset), byte_budget);
            if (!io_read(store, (uint16_t)(start + store->commit_offset), store->scratch, length)) {
                return finish_prepare(store, NOAH_PROFILE_STORE_IO_ERROR);
            }
            if (memcmp(store->scratch, &expected_header[store->commit_offset], length) != 0) {
                return finish_prepare(store, NOAH_PROFILE_STORE_CHECKSUM_MISMATCH);
            }
            store->commit_offset = (uint16_t)(store->commit_offset + length);
            if (store->commit_offset == marker_offset) {
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
            store->commit_domain_count  = store->scratch[6];
            store->commit_domain_index  = 0u;
            store->commit_prior_domain  = 0u;
            store->commit_domain_mask   = 0u;
            store->commit_offset        = PROFILE_BLOB_HEADER_SIZE;
            store->commit_record_offset = 0u;
            if (store->commit_domain_count == 0u) {
                if (store->commit_offset != store->candidate.payload_length || store->candidate.domain_mask != 0u) {
                    return finish_prepare(store, NOAH_PROFILE_STORE_INVALID_PAYLOAD);
                }
                store->commit_phase = NOAH_PROFILE_STORE_COMMIT_PREPARED_MARKER_WRITE;
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
            if (domain_id <= store->commit_prior_domain || domain_mask_for_id(domain_id) == 0u || domain_version != 1u || (uint32_t)store->commit_offset + DOMAIN_ENVELOPE_SIZE + domain_length > store->candidate.payload_length) {
                return finish_prepare(store, NOAH_PROFILE_STORE_INVALID_PAYLOAD);
            }
            store->commit_prior_domain = domain_id;
            store->commit_domain_mask |= domain_mask_for_id(domain_id);
            store->commit_offset = (uint16_t)(store->commit_offset + DOMAIN_ENVELOPE_SIZE + domain_length);
            store->commit_domain_index++;
            store->commit_record_offset = 0u;
            if (store->commit_domain_index == store->commit_domain_count) {
                if (store->commit_offset != store->candidate.payload_length || store->commit_domain_mask != store->candidate.domain_mask) {
                    return finish_prepare(store, NOAH_PROFILE_STORE_INVALID_PAYLOAD);
                }
                store->commit_phase = NOAH_PROFILE_STORE_COMMIT_PREPARED_MARKER_WRITE;
            }
            return NOAH_PROFILE_STORE_IN_PROGRESS;
        }

        case NOAH_PROFILE_STORE_COMMIT_PREPARED_MARKER_WRITE:
            length = bounded_step_length((uint16_t)(marker_length - store->commit_record_offset), byte_budget);
            if (!io_write(store, (uint16_t)(start + marker_offset + store->commit_record_offset), &prepared_value[store->commit_record_offset], length)) {
                return finish_prepare(store, NOAH_PROFILE_STORE_IO_ERROR);
            }
            store->commit_record_offset = (uint8_t)(store->commit_record_offset + length);
            if (store->commit_record_offset == marker_length) {
                store->commit_phase         = NOAH_PROFILE_STORE_COMMIT_PREPARED_MARKER_READBACK;
                store->commit_record_offset = 0u;
            }
            return NOAH_PROFILE_STORE_IN_PROGRESS;

        case NOAH_PROFILE_STORE_COMMIT_PREPARED_MARKER_READBACK:
            length = bounded_step_length((uint16_t)(marker_length - store->commit_record_offset), byte_budget);
            if (!io_read(store, (uint16_t)(start + marker_offset + store->commit_record_offset), &store->scratch[store->commit_record_offset], length)) {
                return finish_prepare(store, NOAH_PROFILE_STORE_IO_ERROR);
            }
            store->commit_record_offset = (uint8_t)(store->commit_record_offset + length);
            if (store->commit_record_offset != marker_length) {
                return NOAH_PROFILE_STORE_IN_PROGRESS;
            }
            if (memcmp(store->scratch, prepared_value, marker_length) != 0) {
                return finish_prepare(store, NOAH_PROFILE_STORE_CHECKSUM_MISMATCH);
            }
            store->prepared_durable    = true;
            store->commit_record_offset = 0u;
            record_from_candidate(store, &record);
            if (committed) {
                *committed = record;
            }
            if (!store->auto_commit_prepared) {
                store->commit_phase = NOAH_PROFILE_STORE_COMMIT_IDLE;
                return NOAH_PROFILE_STORE_OK;
            }
            store->commit_phase = NOAH_PROFILE_STORE_COMMIT_MARKER_WRITE;
            return NOAH_PROFILE_STORE_IN_PROGRESS;

        case NOAH_PROFILE_STORE_COMMIT_MARKER_WRITE:
            length = bounded_step_length((uint16_t)(marker_length - store->commit_record_offset), byte_budget);
            if (!io_write(store, (uint16_t)(start + marker_offset + store->commit_record_offset), &commit_value[store->commit_record_offset], length)) {
                return finish_prepare(store, store->commit_record_offset + length == marker_length ? NOAH_PROFILE_STORE_DURABILITY_UNKNOWN : NOAH_PROFILE_STORE_IO_ERROR);
            }
            store->commit_record_offset = (uint8_t)(store->commit_record_offset + length);
            if (store->commit_record_offset == marker_length) {
                store->commit_phase         = NOAH_PROFILE_STORE_COMMIT_MARKER_READBACK;
                store->commit_record_offset = 0u;
            }
            return NOAH_PROFILE_STORE_IN_PROGRESS;

        case NOAH_PROFILE_STORE_COMMIT_MARKER_READBACK:
            length = bounded_step_length((uint16_t)(marker_length - store->commit_record_offset), byte_budget);
            if (!io_read(store, (uint16_t)(start + marker_offset + store->commit_record_offset), &store->scratch[store->commit_record_offset], length)) {
                return finish_prepare(store, NOAH_PROFILE_STORE_DURABILITY_UNKNOWN);
            }
            store->commit_record_offset = (uint8_t)(store->commit_record_offset + length);
            if (store->commit_record_offset != marker_length) {
                return NOAH_PROFILE_STORE_IN_PROGRESS;
            }
            if (memcmp(store->scratch, commit_value, marker_length) != 0) {
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

noah_profile_store_result_t noah_profile_store_prepare_durable_step(noah_profile_store_t *store, uint8_t byte_budget, noah_profile_store_record_t *prepared) {
    if (!store || store->auto_commit_prepared) {
        return NOAH_PROFILE_STORE_INVALID_ARGUMENT;
    }
    return noah_profile_store_prepare_commit_step(store, byte_budget, prepared);
}

noah_profile_store_result_t noah_profile_store_prepared_commit_begin(noah_profile_store_t *store) {
    if (!store || !store->prepare_active || !store->prepared_durable || store->commit_phase != NOAH_PROFILE_STORE_COMMIT_IDLE) {
        return NOAH_PROFILE_STORE_NO_PREPARE;
    }
    store->auto_commit_prepared = false;
    store->commit_phase         = NOAH_PROFILE_STORE_COMMIT_MARKER_WRITE;
    store->commit_record_offset = 0u;
    return NOAH_PROFILE_STORE_IN_PROGRESS;
}

noah_profile_store_result_t noah_profile_store_prepared_commit_step(noah_profile_store_t *store, uint8_t byte_budget, noah_profile_store_record_t *committed) {
    if (!store || !store->prepared_durable || (store->commit_phase != NOAH_PROFILE_STORE_COMMIT_MARKER_WRITE && store->commit_phase != NOAH_PROFILE_STORE_COMMIT_MARKER_READBACK)) {
        return NOAH_PROFILE_STORE_NO_PREPARE;
    }
    return noah_profile_store_prepare_commit_step(store, byte_budget, committed);
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
    if (store->commit_phase == NOAH_PROFILE_STORE_COMMIT_MARKER_WRITE || store->commit_phase == NOAH_PROFILE_STORE_COMMIT_MARKER_READBACK) {
        return NOAH_PROFILE_STORE_DURABILITY_UNKNOWN;
    }
    if (!slot_start(store->candidate_slot, &start) || !io_write(store, (uint16_t)(start + LEGACY_HEADER_MARKER), invalid_marker, sizeof(invalid_marker))) {
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
_Static_assert(LEGACY_HEADER_MARKER + NOAH_PROFILE_STORAGE_COMMIT_MARKER_SIZE == NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE, "legacy commit marker must be the final header field");
_Static_assert(LOGICAL_HEADER_MARKER + 1u == NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE, "logical commit marker must be the final header field");
_Static_assert(NOAH_PROFILE_STORE_IO_CHUNK_MAX <= NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX, "bounded write chunk must fit a slot payload");
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(noah_profile_store_t) <= NOAH_PROFILE_STORE_STATE_BUDGET_32BIT, "persistent profile store exceeded its reviewed 32-bit state budget");
#endif

#endif
