// ─────────────────────────────────────────────────────────────────────────
// Canonical Live-Profile Blob And Semantic Actions — Profile Wire v1.0
// ────────────────────────────────────────────────────────────────────────────

#include "profile_blob_v1.h"

#include <stdbool.h>
#include <string.h>

#include "../storage/profile_checksum.h"
#include "../storage/profile_storage_layout.h"

static const uint8_t profile_magic[4]      = {'N', 'L', 'P', '1'};
static const uint8_t ordered_domain_ids[4] = {NOAH_PROFILE_DOMAIN_V1_RGB, NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS, NOAH_PROFILE_DOMAIN_V1_COMBOS, NOAH_PROFILE_DOMAIN_V1_SETTINGS};

static void write_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
}

static uint16_t read_u16(const uint8_t *source) {
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8u);
}

static void clear_error(noah_profile_codec_v1_error_t *error) {
    if (!error) {
        return;
    }
    memset(error, 0, sizeof(*error));
    error->domain_index = UINT8_MAX;
    error->table_id     = UINT8_MAX;
    error->row_index    = UINT8_MAX;
    error->step_index   = UINT8_MAX;
    error->field_id     = UINT8_MAX;
}

static noah_profile_codec_v1_result_t fail(noah_profile_codec_v1_error_t *error, noah_profile_codec_v1_result_t code, size_t offset, uint8_t domain_index, uint8_t domain_id) {
    if (error) {
        error->code         = code;
        error->offset       = offset;
        error->domain_index = domain_index;
        error->domain_id    = domain_id;
    }
    return code;
}

static bool domain_version_is_known(uint8_t id, uint8_t version) {
    return (id == NOAH_PROFILE_DOMAIN_V1_SETTINGS && version == 1u) || (id == NOAH_PROFILE_DOMAIN_V1_COMBOS && version == 1u) || (id == NOAH_PROFILE_DOMAIN_V1_RGB && version == NOAH_PROFILE_DOMAIN_V1_RGB_VERSION) || (id == NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS && version == NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIOR_VERSION);
}

static bool domain_id_is_known(uint8_t id) {
    return id == NOAH_PROFILE_DOMAIN_V1_SETTINGS || id == NOAH_PROFILE_DOMAIN_V1_COMBOS || id == NOAH_PROFILE_DOMAIN_V1_RGB || id == NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS;
}

static noah_profile_codec_v1_result_t validate_domain(const noah_profile_domain_v1_t *domain, uint8_t domain_index, noah_profile_codec_v1_error_t *error) {
    if (!domain || (!domain->payload && domain->payload_length != 0u)) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, 0u, domain_index, domain ? domain->id : 0u);
    }
    if (!domain_id_is_known(domain->id)) {
        return fail(error, NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN, 0u, domain_index, domain->id);
    }
    if (!domain_version_is_known(domain->id, domain->version)) {
        return fail(error, NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN_VERSION, 1u, domain_index, domain->id);
    }
    if (domain->payload_length > UINT16_MAX) {
        return fail(error, NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED, 2u, domain_index, domain->id);
    }
    return NOAH_PROFILE_CODEC_V1_OK;
}

noah_profile_action_v1_limits_t noah_profile_action_v1_default_limits(void) {
    noah_profile_action_v1_limits_t limits = {
        .max_logical_layers        = NOAH_PROFILE_ACTION_V1_MAX_LOGICAL_LAYERS,
        .max_pd_modes              = NOAH_PROFILE_ACTION_V1_MAX_PD_MODES,
        .max_via_macro_slots       = NOAH_PROFILE_ACTION_V1_MAX_VIA_MACRO_SLOTS,
        .max_hardcoded_macro_slots = NOAH_PROFILE_ACTION_V1_MAX_HARDCODED_MACRO_SLOTS,
    };
    return limits;
}

noah_profile_codec_v1_result_t noah_profile_domain_v1_encode(const noah_profile_domain_v1_t *domain, uint8_t *output, size_t output_capacity, size_t *written, noah_profile_codec_v1_error_t *error) {
    noah_profile_codec_v1_result_t result;
    size_t                         required;

    clear_error(error);
    if (!output || !written) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, 0u, UINT8_MAX, domain ? domain->id : 0u);
    }
    *written = 0u;
    result   = validate_domain(domain, 0u, error);
    if (result != NOAH_PROFILE_CODEC_V1_OK) {
        return result;
    }
    required = NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE + domain->payload_length;
    if (required > output_capacity) {
        return fail(error, NOAH_PROFILE_CODEC_V1_OUTPUT_TOO_SMALL, 0u, 0u, domain->id);
    }

    output[0] = domain->id;
    output[1] = domain->version;
    write_u16(&output[2], (uint16_t)domain->payload_length);
    if (domain->payload_length != 0u) {
        memcpy(&output[NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE], domain->payload, domain->payload_length);
    }
    *written = required;
    return NOAH_PROFILE_CODEC_V1_OK;
}

noah_profile_codec_v1_result_t noah_profile_domain_v1_read(const uint8_t *bytes, size_t length, size_t offset, noah_profile_domain_v1_t *domain, size_t *next_offset, noah_profile_codec_v1_error_t *error) {
    size_t payload_length;
    size_t remaining;

    clear_error(error);
    if (!bytes || !domain || !next_offset || offset > length) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, offset, UINT8_MAX, 0u);
    }
    memset(domain, 0, sizeof(*domain));
    *next_offset = offset;
    remaining    = length - offset;
    if (remaining < NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE) {
        return fail(error, NOAH_PROFILE_CODEC_V1_TRUNCATED, offset, UINT8_MAX, 0u);
    }

    domain->id      = bytes[offset];
    domain->version = bytes[offset + 1u];
    if (!domain_id_is_known(domain->id)) {
        return fail(error, NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN, offset, UINT8_MAX, domain->id);
    }
    if (!domain_version_is_known(domain->id, domain->version)) {
        return fail(error, NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN_VERSION, offset + 1u, UINT8_MAX, domain->id);
    }

    payload_length = read_u16(&bytes[offset + 2u]);
    if (payload_length > remaining - NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE) {
        return fail(error, NOAH_PROFILE_CODEC_V1_TRUNCATED, offset + NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE, UINT8_MAX, domain->id);
    }
    domain->payload        = &bytes[offset + NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE];
    domain->payload_length = payload_length;
    *next_offset           = offset + NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE + payload_length;
    return NOAH_PROFILE_CODEC_V1_OK;
}

noah_profile_codec_v1_result_t noah_profile_domain_v1_decode(const uint8_t *bytes, size_t length, noah_profile_domain_v1_t *domain, noah_profile_codec_v1_error_t *error) {
    noah_profile_codec_v1_result_t result;
    size_t                         next_offset;

    result = noah_profile_domain_v1_read(bytes, length, 0u, domain, &next_offset, error);
    if (result != NOAH_PROFILE_CODEC_V1_OK) {
        return result;
    }
    if (next_offset != length) {
        return fail(error, NOAH_PROFILE_CODEC_V1_TRAILING_BYTES, next_offset, UINT8_MAX, domain->id);
    }
    return NOAH_PROFILE_CODEC_V1_OK;
}

noah_profile_codec_v1_result_t noah_profile_blob_v1_encode(const noah_profile_domain_v1_t *domains, size_t domain_count, uint8_t *output, size_t output_capacity, size_t *written, noah_profile_codec_v1_error_t *error) {
    size_t total_length = NOAH_PROFILE_BLOB_V1_HEADER_SIZE;
    size_t output_offset;
    size_t index;

    clear_error(error);
    if ((!domains && domain_count != 0u) || !output || !written) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, 0u, UINT8_MAX, 0u);
    }
    *written = 0u;
    if (domain_count > UINT8_MAX) {
        return fail(error, NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED, 6u, UINT8_MAX, 0u);
    }

    for (index = 0u; index < domain_count; index++) {
        noah_profile_codec_v1_result_t result = validate_domain(&domains[index], (uint8_t)index, error);
        size_t                         prior;

        if (result != NOAH_PROFILE_CODEC_V1_OK) {
            return result;
        }
        for (prior = 0u; prior < index; prior++) {
            if (domains[prior].id == domains[index].id) {
                return fail(error, NOAH_PROFILE_CODEC_V1_DUPLICATE_DOMAIN, 0u, (uint8_t)index, domains[index].id);
            }
        }
        if (domains[index].payload_length > NOAH_PROFILE_BLOB_V1_MAX_SIZE - NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE || total_length > NOAH_PROFILE_BLOB_V1_MAX_SIZE - NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE - domains[index].payload_length) {
            return fail(error, NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED, total_length, (uint8_t)index, domains[index].id);
        }
        total_length += NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE + domains[index].payload_length;
    }
    if (domain_count > NOAH_PROFILE_BLOB_V1_MAX_DOMAINS || total_length > NOAH_PROFILE_BLOB_V1_MAX_SIZE) {
        return fail(error, NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED, total_length, UINT8_MAX, 0u);
    }
    if (total_length > output_capacity) {
        return fail(error, NOAH_PROFILE_CODEC_V1_OUTPUT_TOO_SMALL, 0u, UINT8_MAX, 0u);
    }

    memcpy(output, profile_magic, sizeof(profile_magic));
    output[4] = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR;
    output[5] = NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR;
    output[6] = (uint8_t)domain_count;
    output[7] = NOAH_PROFILE_BLOB_V1_CANONICAL_FLAG;

    output_offset = NOAH_PROFILE_BLOB_V1_HEADER_SIZE;
    for (index = 0u; index < sizeof(ordered_domain_ids) / sizeof(ordered_domain_ids[0]); index++) {
        size_t input_index;

        for (input_index = 0u; input_index < domain_count; input_index++) {
            size_t encoded_length;

            if (domains[input_index].id != ordered_domain_ids[index]) {
                continue;
            }
            output[output_offset]      = domains[input_index].id;
            output[output_offset + 1u] = domains[input_index].version;
            write_u16(&output[output_offset + 2u], (uint16_t)domains[input_index].payload_length);
            encoded_length = NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE + domains[input_index].payload_length;
            if (domains[input_index].payload_length != 0u) {
                memcpy(&output[output_offset + NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE], domains[input_index].payload, domains[input_index].payload_length);
            }
            output_offset += encoded_length;
            break;
        }
    }
    *written = output_offset;
    return NOAH_PROFILE_CODEC_V1_OK;
}

noah_profile_codec_v1_result_t noah_profile_blob_v1_decode(const uint8_t *bytes, size_t length, noah_profile_blob_v1_t *blob, noah_profile_codec_v1_error_t *error) {
    size_t  offset;
    uint8_t index;
    uint8_t prior_domain = 0u;

    clear_error(error);
    if (!bytes || !blob) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, 0u, UINT8_MAX, 0u);
    }
    memset(blob, 0, sizeof(*blob));
    if (length < NOAH_PROFILE_BLOB_V1_HEADER_SIZE) {
        return fail(error, NOAH_PROFILE_CODEC_V1_TRUNCATED, length, UINT8_MAX, 0u);
    }
    if (length > NOAH_PROFILE_BLOB_V1_MAX_SIZE) {
        return fail(error, NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED, length, UINT8_MAX, 0u);
    }
    if (memcmp(bytes, profile_magic, sizeof(profile_magic)) != 0) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_MAGIC, 0u, UINT8_MAX, 0u);
    }
    if (bytes[4] != NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR || bytes[5] != NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INCOMPATIBLE_SCHEMA, 4u, UINT8_MAX, 0u);
    }
    if ((bytes[7] & (uint8_t)~NOAH_PROFILE_BLOB_V1_KNOWN_FLAGS) != 0u) {
        return fail(error, NOAH_PROFILE_CODEC_V1_RESERVED_FLAGS, 7u, UINT8_MAX, 0u);
    }
    if (bytes[7] != NOAH_PROFILE_BLOB_V1_CANONICAL_FLAG) {
        return fail(error, NOAH_PROFILE_CODEC_V1_NONCANONICAL, 7u, UINT8_MAX, 0u);
    }

    blob->schema_major = bytes[4];
    blob->schema_minor = bytes[5];
    blob->flags        = bytes[7];
    blob->domain_count = bytes[6];
    offset             = NOAH_PROFILE_BLOB_V1_HEADER_SIZE;

    for (index = 0u; index < blob->domain_count; index++) {
        noah_profile_domain_v1_t      domain;
        noah_profile_codec_v1_error_t domain_error;
        noah_profile_codec_v1_result_t result;
        size_t                         next_offset;

        result = noah_profile_domain_v1_read(bytes, length, offset, &domain, &next_offset, &domain_error);
        if (result != NOAH_PROFILE_CODEC_V1_OK) {
            return fail(error, result, domain_error.offset, index, domain_error.domain_id);
        }
        if (index != 0u && domain.id == prior_domain) {
            return fail(error, NOAH_PROFILE_CODEC_V1_DUPLICATE_DOMAIN, offset, index, domain.id);
        }
        if (index != 0u && domain.id < prior_domain) {
            return fail(error, NOAH_PROFILE_CODEC_V1_DOMAIN_ORDER, offset, index, domain.id);
        }
        if (index >= NOAH_PROFILE_BLOB_V1_MAX_DOMAINS) {
            return fail(error, NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED, offset, index, domain.id);
        }
        blob->domains[index] = domain;
        prior_domain         = domain.id;
        offset               = next_offset;
    }
    if (offset != length) {
        return fail(error, NOAH_PROFILE_CODEC_V1_TRAILING_BYTES, offset, UINT8_MAX, 0u);
    }

    blob->byte_length = length;
    blob->digest      = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, bytes, length);
    blob->crc32       = noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, length));
    return NOAH_PROFILE_CODEC_V1_OK;
}

static noah_profile_codec_v1_result_t validate_action(const noah_profile_action_v1_t *action, const noah_profile_action_v1_limits_t *supplied_limits, size_t offset, noah_profile_codec_v1_error_t *error) {
    noah_profile_action_v1_limits_t limits;
    uint32_t                        capacity;

    if (!action) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, offset, UINT8_MAX, 0u);
    }
    limits = supplied_limits ? *supplied_limits : noah_profile_action_v1_default_limits();
    if (limits.max_logical_layers > UINT32_C(0x10000) || limits.max_pd_modes > UINT32_C(0x10000) || limits.max_via_macro_slots > UINT32_C(0x10000) || limits.max_hardcoded_macro_slots > UINT32_C(0x10000)) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, offset, UINT8_MAX, 0u);
    }
    if (action->flags != 0u) {
        return fail(error, NOAH_PROFILE_CODEC_V1_RESERVED_FLAGS, offset + 1u, UINT8_MAX, 0u);
    }

    switch (action->kind) {
        case NOAH_PROFILE_ACTION_V1_NONE:
            if (action->operand != 0u) {
                return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_OPERAND, offset + 2u, UINT8_MAX, 0u);
            }
            return NOAH_PROFILE_CODEC_V1_OK;
        case NOAH_PROFILE_ACTION_V1_QMK_KEYCODE:
            return NOAH_PROFILE_CODEC_V1_OK;
        case NOAH_PROFILE_ACTION_V1_LAYER_MOMENTARY:
        case NOAH_PROFILE_ACTION_V1_LAYER_LOCK:
            capacity = limits.max_logical_layers;
            break;
        case NOAH_PROFILE_ACTION_V1_PD_MODE_MOMENTARY:
        case NOAH_PROFILE_ACTION_V1_PD_MODE_LOCK:
            capacity = limits.max_pd_modes;
            break;
        case NOAH_PROFILE_ACTION_V1_VIA_MACRO:
            capacity = limits.max_via_macro_slots;
            break;
        case NOAH_PROFILE_ACTION_V1_HARDCODED_MACRO:
            capacity = limits.max_hardcoded_macro_slots;
            break;
        default:
            return fail(error, NOAH_PROFILE_CODEC_V1_UNKNOWN_ACTION_KIND, offset, UINT8_MAX, 0u);
    }
    if ((uint32_t)action->operand >= capacity) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_OPERAND, offset + 2u, UINT8_MAX, 0u);
    }
    return NOAH_PROFILE_CODEC_V1_OK;
}

noah_profile_codec_v1_result_t noah_profile_action_v1_encode(const noah_profile_action_v1_t *action, const noah_profile_action_v1_limits_t *limits, uint8_t output[NOAH_PROFILE_BLOB_V1_ACTION_SIZE], noah_profile_codec_v1_error_t *error) {
    noah_profile_codec_v1_result_t result;

    clear_error(error);
    if (!output) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, 0u, UINT8_MAX, 0u);
    }
    result = validate_action(action, limits, 0u, error);
    if (result != NOAH_PROFILE_CODEC_V1_OK) {
        return result;
    }
    output[0] = action->kind;
    output[1] = action->flags;
    write_u16(&output[2], action->operand);
    return NOAH_PROFILE_CODEC_V1_OK;
}

noah_profile_codec_v1_result_t noah_profile_action_v1_read(const uint8_t *bytes, size_t length, size_t offset, const noah_profile_action_v1_limits_t *limits, noah_profile_action_v1_t *action, size_t *next_offset, noah_profile_codec_v1_error_t *error) {
    noah_profile_codec_v1_result_t result;

    clear_error(error);
    if (!bytes || !action || !next_offset || offset > length) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT, offset, UINT8_MAX, 0u);
    }
    memset(action, 0, sizeof(*action));
    *next_offset = offset;
    if (length - offset < NOAH_PROFILE_BLOB_V1_ACTION_SIZE) {
        return fail(error, NOAH_PROFILE_CODEC_V1_TRUNCATED, offset, UINT8_MAX, 0u);
    }
    action->kind    = bytes[offset];
    action->flags   = bytes[offset + 1u];
    action->operand = read_u16(&bytes[offset + 2u]);
    result          = validate_action(action, limits, offset, error);
    if (result != NOAH_PROFILE_CODEC_V1_OK) {
        return result;
    }
    *next_offset = offset + NOAH_PROFILE_BLOB_V1_ACTION_SIZE;
    return NOAH_PROFILE_CODEC_V1_OK;
}

noah_profile_codec_v1_result_t noah_profile_action_v1_decode(const uint8_t *bytes, size_t length, const noah_profile_action_v1_limits_t *limits, noah_profile_action_v1_t *action, noah_profile_codec_v1_error_t *error) {
    noah_profile_codec_v1_result_t result;
    size_t                         next_offset;

    clear_error(error);
    if (length != NOAH_PROFILE_BLOB_V1_ACTION_SIZE) {
        return fail(error, NOAH_PROFILE_CODEC_V1_INVALID_LENGTH, length, UINT8_MAX, 0u);
    }
    result = noah_profile_action_v1_read(bytes, length, 0u, limits, action, &next_offset, error);
    if (result != NOAH_PROFILE_CODEC_V1_OK) {
        return result;
    }
    return NOAH_PROFILE_CODEC_V1_OK;
}

_Static_assert(NOAH_PROFILE_BLOB_V1_HEADER_SIZE + NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE <= NOAH_PROFILE_BLOB_V1_MAX_SIZE, "Profile blob envelope must fit the payload ceiling");
_Static_assert(NOAH_PROFILE_BLOB_V1_MAX_DOMAINS == sizeof(ordered_domain_ids) / sizeof(ordered_domain_ids[0]), "Known domain table and decoded view capacity drifted");
_Static_assert(NOAH_PROFILE_ACTION_V1_MAX_LOGICAL_LAYERS == NOAH_PROFILE_WIRE_V1_MAX_LOGICAL_LAYERS, "action and storage layer ceilings drifted");
_Static_assert(NOAH_PROFILE_ACTION_V1_MAX_HARDCODED_MACRO_SLOTS == NOAH_PROFILE_WIRE_V1_MAX_HARDCODED_MACRO_SLOTS, "action and storage macro ceilings drifted");
#ifdef VIA_ENABLE
_Static_assert(NOAH_PROFILE_BLOB_V1_MAX_SIZE == NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX, "canonical blob must fit exactly within a profile slot payload");
#endif
