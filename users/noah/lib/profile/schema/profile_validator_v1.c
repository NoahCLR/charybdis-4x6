// ──────────────────────────────────────────────────────────────────────────
// Incremental Whole-Profile Validator — Profile Wire v1.0
// ───────────────────────────────────────────────────────────────────────────

#include "profile_validator_v1.h"

#include <stdbool.h>
#include <string.h>

#include "../storage/profile_checksum.h"

enum {
    KEY_BEHAVIOR_TABLE_ROWS = 1u,
};

noah_profile_validator_v1_error_t noah_profile_validator_v1_no_error(void) {
    noah_profile_validator_v1_error_t error;

    memset(&error, 0, sizeof(error));
    error.domain_index = NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8;
    error.domain_id    = NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8;
    error.table_id     = NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8;
    error.row_index    = NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U16;
    error.step_index   = NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8;
    error.field_id     = NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8;
    return error;
}

noah_profile_validator_v1_compatibility_t noah_profile_validator_v1_default_compatibility(uint32_t action_abi_digest) {
    noah_profile_validator_v1_compatibility_t compatibility;

    memset(&compatibility, 0, sizeof(compatibility));
    compatibility.required_domain_mask              = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_PD;
    compatibility.allowed_domain_mask               = NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS;
    compatibility.max_blob_size                     = NOAH_PROFILE_BLOB_V1_MAX_SIZE;
    compatibility.action_abi_digest                 = action_abi_digest;
    compatibility.logical_layer_count               = NOAH_PROFILE_ACTION_V1_MAX_LOGICAL_LAYERS;
    compatibility.placement_positions               = NOAH_SETTINGS_PLACEMENT_MAX_POSITIONS;
    compatibility.supported_pd_mode_mask            = NOAH_PROFILE_RGB_V1_PD_MODE_MASK_ALL;
    compatibility.via_macro_slot_count              = NOAH_PROFILE_ACTION_V1_MAX_VIA_MACRO_SLOTS;
    compatibility.custom_key_count        = NOAH_PROFILE_ACTION_V1_MAX_CUSTOM_KEYS;
    compatibility.behavior_limits                   = noah_key_behavior_domain_v1_default_limits();
    compatibility.rgb_limits                        = noah_profile_rgb_v1_default_limits();
    compatibility.rgb_limits.logical_layer_count    = compatibility.logical_layer_count;
    compatibility.rgb_limits.supported_pd_mode_mask = compatibility.supported_pd_mode_mask;
    return compatibility;
}

static void copy_error(noah_profile_validator_v1_error_t *target, const noah_profile_validator_v1_error_t *source) {
    if (target) {
        *target = *source;
    }
}

static noah_profile_validator_v1_result_t reject(noah_profile_validator_v1_t *validator, noah_profile_validator_v1_result_t code, size_t byte_offset, uint8_t domain_index, uint8_t domain_id, uint8_t table_id, uint16_t row_index, uint8_t step_index, uint8_t field_id, noah_profile_validator_v1_detail_t detail_kind, uint16_t detail_code, noah_profile_validator_v1_error_t *error) {
    noah_profile_validator_v1_error_t rejection = noah_profile_validator_v1_no_error();

    rejection.code         = code;
    rejection.byte_offset  = byte_offset;
    rejection.domain_index = domain_index;
    rejection.domain_id    = domain_id;
    rejection.table_id     = table_id;
    rejection.row_index    = row_index;
    rejection.step_index   = step_index;
    rejection.field_id     = field_id;
    rejection.detail_kind  = detail_kind;
    rejection.detail_code  = detail_code;
    if (validator) {
        validator->phase           = NOAH_PROFILE_VALIDATOR_V1_PHASE_REJECTED;
        validator->terminal_result = code;
        validator->terminal_error  = rejection;
    }
    copy_error(error, &rejection);
    return code;
}

static noah_profile_validator_v1_result_t reject_simple(noah_profile_validator_v1_t *validator, noah_profile_validator_v1_result_t code, size_t byte_offset, noah_profile_validator_v1_error_t *error) {
    return reject(validator, code, byte_offset, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U16, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8, NOAH_PROFILE_VALIDATOR_V1_DETAIL_NONE, 0u, error);
}

static bool behavior_limits_are_valid(const noah_key_behavior_limits_v1_t *limits) {
    return limits->max_rows != 0u && limits->max_rows <= NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS && limits->max_populated_steps != 0u && limits->max_populated_steps <= NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_POPULATED_STEPS && limits->max_tap_steps_per_row != 0u && limits->max_tap_steps_per_row <= NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_TAP_STEPS_PER_ROW && limits->max_repeat_hz != 0u && limits->max_repeat_hz <= NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_REPEAT_HZ && limits->max_payload_size != 0u && limits->max_payload_size <= NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_PAYLOAD_SIZE;
}

static bool rgb_limits_are_valid(const noah_profile_validator_v1_compatibility_t *compatibility) {
    const noah_profile_rgb_v1_limits_t *limits = &compatibility->rgb_limits;

    return (limits->compiled_stage_mask & (uint16_t)~NOAH_PROFILE_RGB_V1_STAGE_MASK_ALL) == 0u && limits->max_payload_size != 0u && limits->max_payload_size <= NOAH_PROFILE_RGB_V1_MAX_PAYLOAD_SIZE && limits->max_logical_layers <= NOAH_PROFILE_RGB_V1_MAX_LOGICAL_LAYERS && limits->logical_layer_count == compatibility->logical_layer_count && limits->logical_layer_count <= limits->max_logical_layers && limits->tap_branch_color_count != 0u && limits->tap_branch_color_count <= NOAH_PROFILE_RGB_V1_MAX_TAP_BRANCH_COLORS && limits->supported_pd_mode_mask == compatibility->supported_pd_mode_mask && (limits->supported_pd_mode_mask & ~NOAH_PROFILE_RGB_V1_PD_MODE_MASK_ALL) == 0u;
}

static bool compatibility_is_valid(const noah_profile_validator_v1_compatibility_t *compatibility) {
    if (!compatibility || (compatibility->allowed_domain_mask & (uint8_t)~NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS) != 0u || (compatibility->required_domain_mask & (uint8_t)~compatibility->allowed_domain_mask) != 0u || compatibility->max_blob_size < NOAH_PROFILE_BLOB_V1_HEADER_SIZE || compatibility->max_blob_size > NOAH_PROFILE_BLOB_V1_MAX_SIZE || compatibility->logical_layer_count > NOAH_PROFILE_ACTION_V1_MAX_LOGICAL_LAYERS || compatibility->placement_positions > NOAH_SETTINGS_PLACEMENT_MAX_POSITIONS || (compatibility->supported_pd_mode_mask & ~NOAH_PROFILE_RGB_V1_PD_MODE_MASK_ALL) != 0u || compatibility->via_macro_slot_count > NOAH_PROFILE_ACTION_V1_MAX_VIA_MACRO_SLOTS || compatibility->custom_key_count > NOAH_PROFILE_ACTION_V1_MAX_CUSTOM_KEYS) {
        return false;
    }
    return behavior_limits_are_valid(&compatibility->behavior_limits) && rgb_limits_are_valid(compatibility);
}

noah_profile_validator_v1_result_t noah_profile_validator_v1_begin(noah_profile_validator_v1_t *validator, const noah_profile_reader_t *reader, size_t base_offset, const noah_profile_validator_v1_declaration_t *declaration, const noah_profile_validator_v1_compatibility_t *compatibility, noah_profile_validator_v1_error_t *error) {
    noah_profile_validator_v1_error_t clear = noah_profile_validator_v1_no_error();

    copy_error(error, &clear);
    if (!validator) {
        if (error) {
            error->code = NOAH_PROFILE_VALIDATOR_V1_INVALID_ARGUMENT;
        }
        return NOAH_PROFILE_VALIDATOR_V1_INVALID_ARGUMENT;
    }
    memset(validator, 0, sizeof(*validator));
    validator->terminal_error = clear;
    if (!reader || !reader->read || !declaration || !compatibility || !compatibility_is_valid(compatibility) || base_offset > reader->length || declaration->byte_length > reader->length - base_offset) {
        return reject_simple(validator, NOAH_PROFILE_VALIDATOR_V1_INVALID_ARGUMENT, 0u, error);
    }
    validator->reader        = *reader;
    validator->base_offset   = base_offset;
    validator->declaration   = *declaration;
    validator->compatibility = *compatibility;

    if (declaration->schema_major != NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR || declaration->schema_minor != NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR) {
        return reject_simple(validator, NOAH_PROFILE_VALIDATOR_V1_INCOMPATIBLE_SCHEMA, 4u, error);
    }
    if (declaration->flags != 0u) {
        return reject(validator, NOAH_PROFILE_VALIDATOR_V1_INVALID_BLOB, 0u, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U16, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8, NOAH_PROFILE_VALIDATOR_V1_DETAIL_BLOB, NOAH_PROFILE_CODEC_V1_RESERVED_FLAGS, error);
    }
    if ((declaration->domain_mask & (uint8_t)~NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS) != 0u || (declaration->domain_mask & (uint8_t)~compatibility->allowed_domain_mask) != 0u) {
        return reject_simple(validator, NOAH_PROFILE_VALIDATOR_V1_UNSUPPORTED_DOMAIN, 0u, error);
    }
    if (declaration->action_abi_digest != compatibility->action_abi_digest) {
        return reject_simple(validator, NOAH_PROFILE_VALIDATOR_V1_INCOMPATIBLE_ACTION_ABI, 0u, error);
    }
    if (declaration->byte_length < NOAH_PROFILE_BLOB_V1_HEADER_SIZE || declaration->byte_length > compatibility->max_blob_size) {
        return reject_simple(validator, NOAH_PROFILE_VALIDATOR_V1_CAPACITY_EXCEEDED, declaration->byte_length, error);
    }

    validator->phase        = NOAH_PROFILE_VALIDATOR_V1_PHASE_CHECKSUM;
    validator->crc32_state  = NOAH_PROFILE_CRC32_INITIAL;
    validator->digest_state = NOAH_PROFILE_FNV1A_INITIAL;
    return NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
}

static noah_profile_validator_v1_result_t read_blob(noah_profile_validator_v1_t *validator, size_t offset, uint8_t *target, size_t length, noah_profile_validator_v1_error_t *error) {
    if (!noah_profile_reader_read(&validator->reader, validator->base_offset + offset, target, length)) {
        return reject_simple(validator, NOAH_PROFILE_VALIDATOR_V1_READ_ERROR, offset, error);
    }
    return NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
}

static noah_profile_validator_v1_result_t checksum_step(noah_profile_validator_v1_t *validator, uint8_t byte_budget, noah_profile_validator_v1_error_t *error) {
    uint8_t bytes[NOAH_PROFILE_VALIDATOR_V1_CHECKSUM_CHUNK_MAX];
    size_t  remaining;
    size_t  chunk;

    if (byte_budget == 0u) {
        return reject_simple(validator, NOAH_PROFILE_VALIDATOR_V1_INVALID_ARGUMENT, validator->checksum_offset, error);
    }
    remaining = validator->declaration.byte_length - validator->checksum_offset;
    chunk     = remaining;
    if (chunk > byte_budget) chunk = byte_budget;
    if (chunk > sizeof(bytes)) chunk = sizeof(bytes);
    if (read_blob(validator, validator->checksum_offset, bytes, chunk, error) != NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
        return validator->terminal_result;
    }
    validator->crc32_state  = noah_profile_crc32_update(validator->crc32_state, bytes, chunk);
    validator->digest_state = noah_profile_fnv1a_update(validator->digest_state, bytes, chunk);
    validator->checksum_offset = (uint16_t)(validator->checksum_offset + chunk);
    if (validator->checksum_offset != validator->declaration.byte_length) {
        return NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
    }
    validator->crc32_state = noah_profile_crc32_finish(validator->crc32_state);
    if (validator->crc32_state != validator->declaration.crc32 || validator->digest_state != validator->declaration.digest) {
        return reject_simple(validator, NOAH_PROFILE_VALIDATOR_V1_CHECKSUM_MISMATCH, 0u, error);
    }
    validator->phase = NOAH_PROFILE_VALIDATOR_V1_PHASE_BLOB_HEADER;
    return NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
}

static noah_profile_validator_v1_result_t reject_envelope(noah_profile_validator_v1_t *validator, const noah_profile_codec_v1_error_t *source, noah_profile_validator_v1_error_t *error) {
    noah_profile_validator_v1_result_t code = NOAH_PROFILE_VALIDATOR_V1_INVALID_BLOB;
    if (source->code == NOAH_PROFILE_CODEC_V1_INCOMPATIBLE_SCHEMA) code = NOAH_PROFILE_VALIDATOR_V1_INCOMPATIBLE_SCHEMA;
    if (source->code == NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED) code = NOAH_PROFILE_VALIDATOR_V1_CAPACITY_EXCEEDED;
    if (source->code == NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN) code = NOAH_PROFILE_VALIDATOR_V1_UNSUPPORTED_DOMAIN;
    if (source->code == NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN_VERSION) code = NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN;
    return reject(validator, code, source->offset, source->domain_index, source->domain_index == UINT8_MAX ? NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8 : source->domain_id, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U16, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8, NOAH_PROFILE_VALIDATOR_V1_DETAIL_BLOB, source->code, error);
}

static noah_profile_validator_v1_result_t blob_header_step(noah_profile_validator_v1_t *validator, noah_profile_validator_v1_error_t *error) {
    uint8_t header[NOAH_PROFILE_BLOB_V1_HEADER_SIZE];
    noah_profile_codec_v1_error_t detail;
    if (read_blob(validator, 0u, header, sizeof(header), error) != NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) return validator->terminal_result;
    if (noah_profile_envelope_begin(&validator->envelope, header, validator->declaration.byte_length, &detail) != NOAH_PROFILE_CODEC_V1_OK) return reject_envelope(validator, &detail, error);
    validator->phase = NOAH_PROFILE_VALIDATOR_V1_PHASE_DOMAIN_HEADER;
    return NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
}

static noah_profile_validator_v1_result_t finish_domains(noah_profile_validator_v1_t *validator, noah_profile_validator_v1_error_t *error) {
    noah_profile_codec_v1_error_t detail;
    if (noah_profile_envelope_finish(&validator->envelope, &detail) != NOAH_PROFILE_CODEC_V1_OK) return reject_envelope(validator, &detail, error);
    if ((validator->compatibility.required_domain_mask & (uint8_t)~validator->envelope.mask) != 0u) {
        return reject_simple(validator, NOAH_PROFILE_VALIDATOR_V1_MISSING_DOMAIN, validator->envelope.offset, error);
    }
    if (validator->envelope.mask != validator->declaration.domain_mask) {
        return reject_simple(validator, NOAH_PROFILE_VALIDATOR_V1_DOMAIN_MASK_MISMATCH, validator->envelope.offset, error);
    }

    validator->profile.domain_mask       = validator->envelope.mask;
    validator->profile.domain_count      = validator->envelope.count;
    validator->profile.byte_length       = validator->declaration.byte_length;
    validator->profile.crc32             = validator->crc32_state;
    validator->profile.digest            = validator->digest_state;
    validator->profile.action_abi_digest = validator->declaration.action_abi_digest;
    if (validator->has_reference_error) {
        return reject(validator, NOAH_PROFILE_VALIDATOR_V1_INVALID_REFERENCE, validator->reference_error_offset, noah_profile_domain_count(validator->envelope.mask & (uint8_t)(NOAH_PROFILE_DOMAIN_MASK_KEY_BEHAVIORS - 1u)), NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS, KEY_BEHAVIOR_TABLE_ROWS, validator->reference_error_row, validator->reference_error_step, validator->reference_error_field, NOAH_PROFILE_VALIDATOR_V1_DETAIL_NONE, 0u, error);
    }
    validator->phase           = NOAH_PROFILE_VALIDATOR_V1_PHASE_VALID;
    validator->terminal_result = NOAH_PROFILE_VALIDATOR_V1_VALID;
    return NOAH_PROFILE_VALIDATOR_V1_VALID;
}

static noah_profile_validator_v1_result_t domain_header_step(noah_profile_validator_v1_t *validator, noah_profile_validator_v1_error_t *error) {
    uint8_t header[NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE];
    noah_profile_domain_record_t record;
    noah_profile_codec_v1_error_t detail;
    if (validator->envelope.index == validator->envelope.count) return finish_domains(validator, error);
    if ((size_t)(validator->envelope.byte_length - validator->envelope.offset) < sizeof(header)) {
        detail = (noah_profile_codec_v1_error_t){.code = NOAH_PROFILE_CODEC_V1_TRUNCATED, .offset = validator->envelope.offset, .domain_index = validator->envelope.index};
        return reject_envelope(validator, &detail, error);
    }
    if (read_blob(validator, validator->envelope.offset, header, sizeof(header), error) != NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) return validator->terminal_result;
    const noah_profile_domain_shape_t *shape = noah_profile_domain_find(header[0]);
    if (shape && !(shape->mask & validator->compatibility.allowed_domain_mask)) {
        detail = (noah_profile_codec_v1_error_t){.code = NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN, .offset = validator->envelope.offset, .domain_index = validator->envelope.index, .domain_id = header[0]};
        return reject_envelope(validator, &detail, error);
    }
    if (noah_profile_envelope_next(&validator->envelope, header, &record, &detail) != NOAH_PROFILE_CODEC_V1_OK) return reject_envelope(validator, &detail, error);
    validator->domain  = record.shape->index;
    validator->payload = (noah_profile_domain_range_t){record.offset, record.length};
    memset(&validator->domain_validation, 0, sizeof(validator->domain_validation));
    validator->phase = NOAH_PROFILE_VALIDATOR_V1_PHASE_DOMAIN_DECODE;
    return NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
}

static noah_profile_validator_v1_result_t map_behavior_error(noah_profile_validator_v1_t *validator, const noah_profile_codec_v1_error_t *source, noah_profile_validator_v1_error_t *error) {
    noah_profile_validator_v1_result_t result    = NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN;
    uint16_t                           row_index = source->row_index == UINT8_MAX ? NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U16 : source->row_index;

    if (source->code == NOAH_PROFILE_CODEC_V1_READ_ERROR) result = NOAH_PROFILE_VALIDATOR_V1_READ_ERROR;
    if (source->code == NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED) result = NOAH_PROFILE_VALIDATOR_V1_CAPACITY_EXCEEDED;
    return reject(validator, result, validator->payload.offset + source->offset, (uint8_t)(validator->envelope.index - 1u), NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS, source->table_id, row_index, source->step_index, source->field_id, NOAH_PROFILE_VALIDATOR_V1_DETAIL_KEY_BEHAVIOR, source->code, error);
}

static noah_profile_validator_v1_result_t map_rgb_error(noah_profile_validator_v1_t *validator, const noah_profile_rgb_v1_error_t *source, noah_profile_validator_v1_error_t *error) {
    noah_profile_validator_v1_result_t result    = NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN;
    uint16_t                           row_index = source->row == UINT8_MAX ? NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U16 : source->row;

    if (source->code == NOAH_PROFILE_RGB_V1_READ_ERROR) result = NOAH_PROFILE_VALIDATOR_V1_READ_ERROR;
    if (source->code == NOAH_PROFILE_RGB_V1_CAPACITY_EXCEEDED) result = NOAH_PROFILE_VALIDATOR_V1_CAPACITY_EXCEEDED;
    return reject(validator, result, validator->payload.offset + source->offset, (uint8_t)(validator->envelope.index - 1u), NOAH_PROFILE_DOMAIN_V1_RGB, source->table, row_index, NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8, source->field, NOAH_PROFILE_VALIDATOR_V1_DETAIL_RGB, source->code, error);
}

static bool action_reference_is_valid(const noah_profile_validator_v1_t *validator, const noah_profile_action_v1_t *action);

static noah_profile_validator_v1_placement_t behavior_placement(const noah_key_behavior_domain_v1_action_event_t *event) {
    switch (event->field_id) {
        case NOAH_KEY_BEHAVIOR_FIELD_V1_TARGET:
            return NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_KEY;
        case NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_ACTION:
            return NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_TAP;
        default:
            return event->hold_mode == NOAH_KEY_BEHAVIOR_HOLD_V1_PRESS_AND_HOLD_UNTIL_RELEASE ? NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_HOLD_PRESS_AND_HOLD : NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_HOLD_OTHER;
    }
}

static bool placement_is_supported(const noah_profile_validator_v1_t *validator, const noah_profile_action_v1_t *action, noah_profile_validator_v1_placement_t placement) {
    const noah_profile_validator_v1_runtime_t *runtime = validator->compatibility.runtime;

    return !(runtime && runtime->placement_supported) || runtime->placement_supported(action, placement);
}

static void record_action_reference(noah_profile_validator_v1_t *validator) {
    noah_key_behavior_domain_v1_action_event_t event;

    if (validator->has_reference_error || !noah_key_behavior_domain_v1_validation_action_event(&validator->domain_validation.key_behaviors, &event) || (action_reference_is_valid(validator, &event.action) && placement_is_supported(validator, &event.action, behavior_placement(&event)))) {
        return;
    }
    validator->has_reference_error    = true;
    validator->reference_error_offset = validator->payload.offset + event.offset;
    validator->reference_error_row    = event.row_index;
    validator->reference_error_step   = event.step_index;
    validator->reference_error_field  = event.field_id;
}

// One function per registry row validates that domain's payload. Each returns
// IN_PROGRESS after a bounded step, VALID once its payload is complete, or a
// rejection.
static noah_profile_validator_v1_result_t validate_rgb(noah_profile_validator_v1_t *validator, noah_profile_validator_v1_error_t *error) {
    noah_profile_rgb_v1_error_t             error_rgb;
    noah_profile_rgb_v1_validation_result_t result;

    if (validator->domain_validation.rgb.phase == NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_UNINITIALIZED) {
        result = noah_profile_rgb_v1_validation_begin(&validator->domain_validation.rgb, &validator->reader, validator->base_offset + validator->payload.offset, validator->payload.length, &validator->compatibility.rgb_limits, &error_rgb);
        if (result == NOAH_PROFILE_RGB_V1_VALIDATION_REJECTED) return map_rgb_error(validator, &error_rgb, error);
    }
    result = noah_profile_rgb_v1_validation_step(&validator->domain_validation.rgb, &error_rgb);
    if (result == NOAH_PROFILE_RGB_V1_VALIDATION_REJECTED) return map_rgb_error(validator, &error_rgb, error);
    if (result == NOAH_PROFILE_RGB_V1_VALIDATION_IN_PROGRESS) return NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
    if (noah_profile_rgb_v1_validation_view(&validator->domain_validation.rgb, &validator->profile.rgb, &error_rgb) != NOAH_PROFILE_RGB_V1_VALIDATION_VALID) return map_rgb_error(validator, &error_rgb, error);
    return NOAH_PROFILE_VALIDATOR_V1_VALID;
}

static noah_profile_validator_v1_result_t validate_key_behaviors(noah_profile_validator_v1_t *validator, noah_profile_validator_v1_error_t *error) {
    noah_profile_codec_v1_error_t                   codec_error;
    noah_profile_action_v1_limits_t                 frozen_action_limits = noah_profile_action_v1_default_limits();
    noah_key_behavior_domain_v1_validation_result_t result;

    if (validator->domain_validation.key_behaviors.phase == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_PHASE_UNINITIALIZED) {
        result = noah_key_behavior_domain_v1_validation_begin(&validator->domain_validation.key_behaviors, &validator->reader, validator->base_offset + validator->payload.offset, validator->payload.length, &validator->compatibility.behavior_limits, &frozen_action_limits, &codec_error);
        if (result == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_REJECTED) return map_behavior_error(validator, &codec_error, error);
    }
    result = noah_key_behavior_domain_v1_validation_step(&validator->domain_validation.key_behaviors, &codec_error);
    if (result == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_REJECTED) return map_behavior_error(validator, &codec_error, error);
    record_action_reference(validator);
    if (result == NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_IN_PROGRESS) return NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
    if (noah_key_behavior_domain_v1_validation_view(&validator->domain_validation.key_behaviors, &validator->profile.key_behaviors, &codec_error) != NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_VALID) return map_behavior_error(validator, &codec_error, error);
    return NOAH_PROFILE_VALIDATOR_V1_VALID;
}

static noah_profile_validator_v1_result_t validate_combos(noah_profile_validator_v1_t *validator, noah_profile_validator_v1_error_t *error) {
    noah_profile_combo_v1_validation_t *state  = &validator->domain_validation.combos;
    size_t                              offset = validator->payload.offset;
    noah_profile_combo_v1_header_t header;
    noah_profile_combo_v1_row_t row;
    noah_profile_codec_v1_result_t detail;
    noah_profile_action_v1_limits_t limits = noah_profile_action_v1_default_limits();
    uint8_t row_index = state->row_index;
    uint8_t phase = state->phase;
    uint16_t read_offset = state->offset;
    noah_profile_combo_v1_iteration_t result = noah_profile_combo_v1_iteration_step(state, &validator->reader, validator->base_offset + offset, validator->payload.length, &limits, &header, &row, &detail);
    if (result == NOAH_PROFILE_COMBO_V1_HEADER || result == NOAH_PROFILE_COMBO_V1_ITERATING) return NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
    if (result == NOAH_PROFILE_COMBO_V1_COMPLETE) return NOAH_PROFILE_VALIDATOR_V1_VALID;
    if (result == NOAH_PROFILE_COMBO_V1_REJECTED) {
        if (detail == NOAH_PROFILE_CODEC_V1_READ_ERROR) return reject_simple(validator, NOAH_PROFILE_VALIDATOR_V1_READ_ERROR, offset + read_offset, error);
        offset += phase ? state->row_offset : 0u;
        goto invalid;
    }
    offset += state->row_offset;
    if (!action_reference_is_valid(validator, &row.output) || !placement_is_supported(validator, &row.output, NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_COMBO_OUTPUT)) goto invalid;
    for (uint8_t input = 0; input < row.input_count; input++)
        if (!action_reference_is_valid(validator, &row.inputs[input])) goto invalid;
    if (validator->compatibility.runtime && validator->compatibility.runtime->combo_to_native) {
        bool (*to_native)(const noah_profile_action_v1_t *, uint16_t *) = validator->compatibility.runtime->combo_to_native;
        uint16_t native[NOAH_PROFILE_COMBO_V1_MAX_INPUTS], output;
        if (!to_native(&row.output, &output) || !output) goto invalid;
        for (uint8_t input = 0; input < row.input_count; input++) {
            if (!to_native(&row.inputs[input], &native[input]) || native[input] <= 1u) goto invalid;
            for (uint8_t prior = 0; prior < input; prior++)
                if (native[prior] == native[input]) goto invalid;
        }
    }
    return NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
invalid:
    if (validator->phase == NOAH_PROFILE_VALIDATOR_V1_PHASE_REJECTED) return validator->terminal_result;
    return reject(validator, NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN, offset, (uint8_t)(validator->envelope.index - 1u), NOAH_PROFILE_DOMAIN_V1_COMBOS, 0u, row_index, 0u, 0u, NOAH_PROFILE_VALIDATOR_V1_DETAIL_BLOB, NOAH_PROFILE_CODEC_V1_INVALID_ACTION, error);
}

static noah_profile_validator_v1_result_t validate_settings(noah_profile_validator_v1_t *v, noah_profile_validator_v1_error_t *error) {
    noah_profile_settings_v1_validation_t *state = &v->domain_validation.settings;
    uint8_t                                bytes[NOAH_PROFILE_VALIDATOR_V1_STEP_READ_MAX];
    size_t                                 offset = v->payload.offset + state->offset;
    uint16_t                               count  = v->payload.length - state->offset;
    // One read of up to a step's bytes, consumed one at a time: a maximum
    // settings domain takes about 470 steps rather than 9,380.
    if (count > sizeof(bytes)) count = sizeof(bytes);
    if (read_blob(v, offset, bytes, count, error) != NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) return v->terminal_result;
    for (uint16_t i = 0; i < count; i++)
        if (!noah_profile_settings_v1_consume(state, bytes[i], v->payload.length, v->compatibility.logical_layer_count, v->compatibility.placement_positions)) return reject_simple(v, NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN, offset + i, error);
    offset += count - 1u;
    if (state->offset < v->payload.length) return NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
    if (!noah_profile_settings_v1_complete(state, v->payload.length)) return reject_simple(v, NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN, offset, error);
    return NOAH_PROFILE_VALIDATOR_V1_VALID;
}

static noah_profile_validator_v1_result_t validate_pd(noah_profile_validator_v1_t *v, noah_profile_validator_v1_error_t *error) {
#ifdef NOAH_PD_PROFILE_ENABLE
    noah_profile_pd_v1_error_t detail;
    noah_profile_pd_v1_iteration_t result = noah_profile_pd_v1_iterator_step(&v->domain_validation.pd, &v->reader, v->base_offset + v->payload.offset, v->payload.length, &detail);
    if (result == NOAH_PROFILE_PD_V1_REJECTED) return reject_simple(v, detail.code == NOAH_PROFILE_PD_V1_READ_ERROR ? NOAH_PROFILE_VALIDATOR_V1_READ_ERROR : NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN, v->payload.offset + detail.offset, error);
    return noah_profile_pd_v1_iterator_complete(&v->domain_validation.pd) ? NOAH_PROFILE_VALIDATOR_V1_VALID : NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
#else
    // A build without pointing profiles never allows the domain's header.
    return reject_simple(v, NOAH_PROFILE_VALIDATOR_V1_UNSUPPORTED_DOMAIN, v->payload.offset, error);
#endif
}

// Each registry row's validator, called directly so the linked stack contexts
// keep a direct edge to every domain.
static noah_profile_validator_v1_result_t validate_domain(noah_profile_validator_v1_t *validator, noah_profile_validator_v1_error_t *error) {
    switch (validator->domain) {
#define NOAH_DOMAIN_VALIDATE(NAME, module, id, version) \
    case NOAH_PROFILE_DOMAIN_INDEX_##NAME:              \
        return validate_##module(validator, error);
        NOAH_PROFILE_DOMAIN_ROWS(NOAH_DOMAIN_VALIDATE)
#undef NOAH_DOMAIN_VALIDATE
        default:
            return reject_simple(validator, NOAH_PROFILE_VALIDATOR_V1_INVALID_ARGUMENT, validator->payload.offset, error);
    }
}

static noah_profile_validator_v1_result_t domain_decode_step(noah_profile_validator_v1_t *validator, noah_profile_validator_v1_error_t *error) {
    noah_profile_validator_v1_result_t result = validate_domain(validator, error);

    if (result != NOAH_PROFILE_VALIDATOR_V1_VALID) return result;
    validator->phase = NOAH_PROFILE_VALIDATOR_V1_PHASE_DOMAIN_HEADER;
    return NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
}

static bool action_reference_is_valid(const noah_profile_validator_v1_t *validator, const noah_profile_action_v1_t *action) {
    switch (action->kind) {
        case NOAH_PROFILE_ACTION_V1_QMK_KEYCODE:
            return true;
        case NOAH_PROFILE_ACTION_V1_LAYER_MOMENTARY:
        case NOAH_PROFILE_ACTION_V1_LAYER_LOCK:
            return action->operand < validator->compatibility.logical_layer_count;
        case NOAH_PROFILE_ACTION_V1_PD_MODE_MOMENTARY:
        case NOAH_PROFILE_ACTION_V1_PD_MODE_LOCK:
            return action->operand < NOAH_PROFILE_RGB_V1_MAX_PD_MODES && (validator->compatibility.supported_pd_mode_mask & (UINT32_C(1) << action->operand)) != 0u;
        case NOAH_PROFILE_ACTION_V1_VIA_MACRO:
            return action->operand < validator->compatibility.via_macro_slot_count;
        case NOAH_PROFILE_ACTION_V1_CUSTOM_KEY:
            return action->operand < validator->compatibility.custom_key_count;
        default:
            return false;
    }
}

noah_profile_validator_v1_result_t noah_profile_validator_v1_step(noah_profile_validator_v1_t *validator, uint8_t checksum_byte_budget, noah_profile_validator_v1_error_t *error) {
    noah_profile_validator_v1_error_t clear = noah_profile_validator_v1_no_error();

    copy_error(error, &clear);
    if (!validator) {
        if (error) error->code = NOAH_PROFILE_VALIDATOR_V1_INVALID_ARGUMENT;
        return NOAH_PROFILE_VALIDATOR_V1_INVALID_ARGUMENT;
    }
    if (validator->phase == NOAH_PROFILE_VALIDATOR_V1_PHASE_REJECTED) {
        copy_error(error, &validator->terminal_error);
        return validator->terminal_result;
    }
    if (validator->phase == NOAH_PROFILE_VALIDATOR_V1_PHASE_VALID) {
        return NOAH_PROFILE_VALIDATOR_V1_VALID;
    }
    switch (validator->phase) {
        case NOAH_PROFILE_VALIDATOR_V1_PHASE_CHECKSUM:
            return checksum_step(validator, checksum_byte_budget, error);
        case NOAH_PROFILE_VALIDATOR_V1_PHASE_BLOB_HEADER:
            return blob_header_step(validator, error);
        case NOAH_PROFILE_VALIDATOR_V1_PHASE_DOMAIN_HEADER:
            return domain_header_step(validator, error);
        case NOAH_PROFILE_VALIDATOR_V1_PHASE_DOMAIN_DECODE:
            return domain_decode_step(validator, error);
        default:
            return reject_simple(validator, NOAH_PROFILE_VALIDATOR_V1_INVALID_ARGUMENT, 0u, error);
    }
}

noah_profile_validator_v1_result_t noah_profile_validator_v1_profile(const noah_profile_validator_v1_t *validator, noah_profile_validator_v1_profile_t *profile, noah_profile_validator_v1_error_t *error) {
    noah_profile_validator_v1_error_t clear = noah_profile_validator_v1_no_error();

    copy_error(error, &clear);
    if (!validator || !profile) {
        if (error) error->code = NOAH_PROFILE_VALIDATOR_V1_INVALID_ARGUMENT;
        return NOAH_PROFILE_VALIDATOR_V1_INVALID_ARGUMENT;
    }
    if (validator->phase == NOAH_PROFILE_VALIDATOR_V1_PHASE_REJECTED) {
        copy_error(error, &validator->terminal_error);
        return validator->terminal_result;
    }
    if (validator->phase != NOAH_PROFILE_VALIDATOR_V1_PHASE_VALID) {
        return NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS;
    }
    *profile = validator->profile;
    return NOAH_PROFILE_VALIDATOR_V1_VALID;
}

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(noah_profile_validator_v1_t) <= NOAH_PROFILE_VALIDATOR_V1_EMBEDDED_STATE_BUDGET, "whole-profile validator state exceeded its payload-independent firmware regression policy");
#endif

_Static_assert((unsigned)NOAH_PROFILE_PD_V1_STEP_READ_MAX <= (unsigned)NOAH_PROFILE_VALIDATOR_V1_STEP_READ_MAX, "PD iterator exceeds the scan read budget");
