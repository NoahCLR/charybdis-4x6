// ─────────────────────────────────────────────────────────────────────────
// Compiled Authored Defaults — Profile Wire v1.0
// ─────────────────────────────────────────────────────────────────────────────

#include "profile_compiled_defaults_v1.h"

#include <string.h>

#include "key_behavior_domain_v1.h"
#include "profile_rgb_v1.h"
#include "profile_compiled_writer.h"
#include "../runtime/profile_action_runtime_v1.h"
#include "../storage/profile_checksum.h"
#include "noah_keymap_ids.h"
#include "lib/key/behavior/key_behavior.h"
#include "lib/pointing/defs/pd_modes.h"
#include "lib/rgb/core/rgb_helpers.h"

typedef struct {
    uint32_t crc32;
    uint32_t digest;
} checksum_sink_t;

typedef struct {
    size_t   start;
    size_t   end;
    uint8_t *target;
    size_t   copied;
    size_t   stream_offset;
} range_sink_t;

static bool checksum_write(void *context, const uint8_t *bytes, size_t length) {
    checksum_sink_t *sink = context;
    sink->crc32           = noah_profile_crc32_update(sink->crc32, bytes, length);
    sink->digest          = noah_profile_fnv1a_update(sink->digest, bytes, length);
    return true;
}

static bool range_write(void *context, const uint8_t *bytes, size_t length) {
    range_sink_t *sink        = context;
    size_t        chunk_start = sink->stream_offset;
    size_t        chunk_end   = chunk_start + length;
    size_t        copy_start  = chunk_start > sink->start ? chunk_start : sink->start;
    size_t        copy_end    = chunk_end < sink->end ? chunk_end : sink->end;

    if (copy_start < copy_end) {
        size_t amount = copy_end - copy_start;
        memcpy(&sink->target[sink->copied], &bytes[copy_start - chunk_start], amount);
        sink->copied += amount;
    }
    sink->stream_offset = chunk_end;
    return chunk_end < sink->end;
}

noah_profile_compiled_v1_result_t noah_profile_compiled_v1_action(uint16_t native_action, noah_profile_action_v1_t *action) {
    noah_profile_action_runtime_v1_result_t result = noah_profile_action_runtime_v1_from_native(native_action, action);

    if (result == NOAH_PROFILE_ACTION_RUNTIME_V1_OK) {
        return NOAH_PROFILE_COMPILED_V1_OK;
    }
    return result == NOAH_PROFILE_ACTION_RUNTIME_V1_INVALID_ARGUMENT ? NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT : NOAH_PROFILE_COMPILED_V1_INVALID_ACTION;
}

static noah_profile_compiled_v1_result_t action_abi_digest(uint32_t *digest, noah_profile_compiled_v1_error_t *error) {
    checksum_sink_t      sink     = {.crc32 = NOAH_PROFILE_CRC32_INITIAL, .digest = NOAH_PROFILE_FNV1A_INITIAL};
    compiled_writer_t    writer   = {.write = checksum_write, .context = &sink, .result = NOAH_PROFILE_COMPILED_V1_OK};
    static const uint8_t magic[4] = {'N', 'L', 'A', '1'};

    if (!digest) {
        return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT, NOAH_PROFILE_COMPILED_V1_SURFACE_ACTION_ABI, UINT8_MAX, UINT8_MAX);
    }
    if (key_behavior_count > NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS) {
        return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_ACTION_ABI, UINT8_MAX, UINT8_MAX);
    }
    *digest = 0u;

    emit(&writer, magic, sizeof(magic));
    emit_u8(&writer, NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR);
    emit_u8(&writer, NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR);
    emit_u8(&writer, NOAH_PROFILE_ACTION_V1_CUSTOM_KEY);
    emit_u8(&writer, LAYER_COUNT);
    emit_u8(&writer, PD_MODE_COUNT);
    emit_u8(&writer, VIA_MACRO_SLOT_COUNT);
    emit_u8(&writer, NOAH_PROFILE_ACTION_V1_MAX_CUSTOM_KEYS);
#ifdef VIA_FIRMWARE_VERSION
    emit_u32(&writer, VIA_FIRMWARE_VERSION);
#else
    emit_u32(&writer, 0u);
#endif
    emit_u16(&writer, KC_NO);
    emit_u16(&writer, KC_TRNS);
    emit_u16(&writer, MO(0));
    emit_u16(&writer, LT(0, KC_NO));
    emit_u16(&writer, OSM(0));
    emit_u16(&writer, MT(0, KC_NO));
    emit_u16(&writer, QK_MACRO_0);
    emit_u16(&writer, CUSTOM_KEY_0);
    emit_u16(&writer, LAYER_LOCK_BASE);
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        emit_u8(&writer, layer);
        emit_u16(&writer, MO(layer));
        emit_u16(&writer, LOCK_LAYER(layer));
    }
    for (uint8_t id = 0u; id < PD_MODE_COUNT; id++) {
        emit_u8(&writer, id);
        // The whole 32-bit flag: slots 16..31 have no bits in a u16.
        emit_u32(&writer, pd_modes[id].mode_flag);
        emit_u16(&writer, pd_modes[id].keycode);
        emit_u16(&writer, pd_modes[id].lock_action);
    }
    emit_u16(&writer, NOAH_KEYCODE_USERSPACE_END);
    emit_u16(&writer, UINT16_MAX);
    if (writer.result != NOAH_PROFILE_COMPILED_V1_OK) {
        return fail(error, writer.result, NOAH_PROFILE_COMPILED_V1_SURFACE_ACTION_ABI, UINT8_MAX, UINT8_MAX);
    }
    *digest = sink.digest;
    return NOAH_PROFILE_COMPILED_V1_OK;
}

static bool emit_domain_header(compiled_writer_t *writer, uint8_t id, uint16_t length) {
    const noah_profile_domain_shape_t *shape = noah_profile_domain_find(id);
    return shape && emit_u8(writer, shape->id) && emit_u8(writer, shape->version) && emit_u16(writer, length);
}

// Each registry row's encoder, called directly so the linked stack contexts
// keep a direct edge to every domain.
static noah_profile_compiled_v1_result_t write_domain(size_t index, compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
#ifdef NOAH_COMPILED_DEFAULTS_TEST
    extern void noah_compiled_defaults_test_domain_write(uint8_t id);
    noah_compiled_defaults_test_domain_write(noah_profile_domain_at(index)->id);
#endif
    switch (index) {
#define NOAH_DOMAIN_WRITE(NAME, module, id, version) \
    case NOAH_PROFILE_DOMAIN_INDEX_##NAME:           \
        return noah_profile_##module##_compiled_v1_write(writer, error);
        NOAH_PROFILE_DOMAIN_ROWS(NOAH_DOMAIN_WRITE)
#undef NOAH_DOMAIN_WRITE
        default:
            return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
    }
}

static bool count_write(void *context, const uint8_t *bytes, size_t length) {
    (void)bytes;
    *(size_t *)context += length;
    return true;
}

// A domain's length is what its encoder emits; one that emits nothing is
// absent from this build's compiled profile.
static noah_profile_compiled_v1_result_t layout(noah_profile_compiled_v1_t *profile, noah_profile_compiled_v1_error_t *error) {
    size_t offset = NOAH_PROFILE_BLOB_V1_HEADER_SIZE;
    for (size_t i = 0; i < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT; i++) {
        size_t                            length = 0;
        compiled_writer_t                 counter = {.write = count_write, .context = &length, .result = NOAH_PROFILE_COMPILED_V1_OK};
        noah_profile_compiled_v1_result_t result  = write_domain(i, &counter, error);
        if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
        if (!length) continue;
        offset += NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE;
        if (offset > NOAH_PROFILE_BLOB_V1_MAX_SIZE || length > NOAH_PROFILE_BLOB_V1_MAX_SIZE - offset) return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
        profile->domains[i].offset = (uint16_t)offset;
        profile->domains[i].length = (uint16_t)length;
        profile->metadata.domain_mask |= noah_profile_domain_at(i)->mask;
        offset += length;
    }
    profile->metadata.byte_length = (uint16_t)offset;
    return NOAH_PROFILE_COMPILED_V1_OK;
}

static bool emit_blob_header(const noah_profile_compiled_v1_t *profile, compiled_writer_t *writer) {
    const uint8_t header[8] = {'N', 'L', 'P', '1', NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR, NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR, noah_profile_domain_count(profile->metadata.domain_mask), NOAH_PROFILE_BLOB_V1_CANONICAL_FLAG};
    return emit(writer, header, sizeof(header));
}

static noah_profile_compiled_v1_result_t write_blob(const noah_profile_compiled_v1_t *profile, compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
    if (!emit_blob_header(profile, writer)) return writer->result;
    for (size_t i = 0; i < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT; i++) {
        if (!profile->domains[i].length) continue;
        const noah_profile_domain_shape_t *shape = noah_profile_domain_at(i);
        if (!emit_domain_header(writer, shape->id, profile->domains[i].length)) return writer->result;
        noah_profile_compiled_v1_result_t result = write_domain(i, writer, error);
        if (result != NOAH_PROFILE_COMPILED_V1_OK || writer->stopped) return result;
    }
    return writer->result;
}

noah_profile_compiled_v1_result_t noah_profile_compiled_v1_open(noah_profile_compiled_v1_t *profile, noah_profile_compiled_v1_error_t *error) {
    checksum_sink_t                   sink   = {.crc32 = NOAH_PROFILE_CRC32_INITIAL, .digest = NOAH_PROFILE_FNV1A_INITIAL};
    compiled_writer_t                 writer = {.write = checksum_write, .context = &sink, .result = NOAH_PROFILE_COMPILED_V1_OK};
    noah_profile_compiled_v1_result_t result;
    uint32_t                          action_abi;

    if (error) *error = no_error();
    if (!profile) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
    memset(profile, 0, sizeof(*profile));
    result = action_abi_digest(&action_abi, error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    result = layout(profile, error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    result = write_blob(profile, &writer, error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    if (writer.offset > UINT16_MAX || writer.offset > NOAH_PROFILE_BLOB_V1_MAX_SIZE) return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
    profile->metadata = (noah_profile_compiled_v1_metadata_t){
        .byte_length       = (uint16_t)writer.offset,
        .crc32             = noah_profile_crc32_finish(sink.crc32),
        .digest            = sink.digest,
        .action_abi_digest = action_abi,
        .domain_mask       = profile->metadata.domain_mask,
    };
    return NOAH_PROFILE_COMPILED_V1_OK;
}

noah_profile_compiled_v1_result_t noah_profile_compiled_v1_write(const noah_profile_compiled_v1_t *profile, noah_profile_compiled_v1_write_fn write, void *context, noah_profile_compiled_v1_error_t *error) {
    compiled_writer_t                 writer = {.write = write, .context = context, .result = NOAH_PROFILE_COMPILED_V1_OK};
    noah_profile_compiled_v1_result_t result;
    if (error) *error = no_error();
    if (!profile || !write || profile->metadata.byte_length == 0u) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
    result = write_blob(profile, &writer, error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    if (writer.offset != profile->metadata.byte_length) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_BEHAVIOR, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
    return NOAH_PROFILE_COMPILED_V1_OK;
}

static bool compiled_reader_read(void *context, size_t offset, uint8_t *target, size_t length) {
    const noah_profile_compiled_v1_t *profile = context;
    range_sink_t                      sink    = {.start = offset, .end = offset + length, .target = target};
    noah_profile_compiled_v1_error_t  error;
    compiled_writer_t                 writer = {.write = range_write, .context = &sink, .result = NOAH_PROFILE_COMPILED_V1_OK, .allow_early_stop = true};
    if (!profile) return false;
    if (offset < NOAH_PROFILE_BLOB_V1_HEADER_SIZE) {
        if (!emit_blob_header(profile, &writer)) return writer.result == NOAH_PROFILE_COMPILED_V1_OK && sink.copied == length;
    }
    for (size_t i = 0; i < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT && sink.copied < length; i++) {
        size_t start = profile->domains[i].offset, size = profile->domains[i].length;
        if (!size || offset >= start + size || offset + length <= start - NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE) continue;
        const noah_profile_domain_shape_t *shape = noah_profile_domain_at(i);
        sink.stream_offset                       = start - NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE;
        if (offset < start && !emit_domain_header(&writer, shape->id, size)) return writer.result == NOAH_PROFILE_COMPILED_V1_OK && sink.copied == length;
        if (sink.copied == length) break;
        sink.stream_offset = start;
        if (write_domain(i, &writer, &error) != NOAH_PROFILE_COMPILED_V1_OK) return false;
        if (writer.stopped) break;
    }
    return writer.result == NOAH_PROFILE_COMPILED_V1_OK && sink.copied == length;
}

noah_profile_reader_t noah_profile_compiled_v1_reader(const noah_profile_compiled_v1_t *profile) {
    return (noah_profile_reader_t){
        .read    = profile && profile->metadata.byte_length != 0u ? compiled_reader_read : NULL,
        .context = (void *)profile,
        .length  = profile ? profile->metadata.byte_length : 0u,
    };
}

#ifdef COMBO_ENABLE
static bool combo_to_native(const noah_profile_action_v1_t *action, uint16_t *native) {
    return noah_profile_action_runtime_v1_to_native(action, native) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
}

static const noah_profile_validator_v1_runtime_t compiled_runtime = {
    .combo_to_native = combo_to_native,
};
#endif

bool noah_profile_compiled_v1_compatibility(const noah_profile_compiled_v1_t *profile, noah_profile_validator_v1_compatibility_t *compatibility) {
    noah_profile_validator_v1_compatibility_t result;

    if (!profile || !compatibility || profile->metadata.domain_mask == 0u || (profile->metadata.domain_mask & (uint8_t)~NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS) != 0u) {
        return false;
    }
    result                     = noah_profile_validator_v1_default_compatibility(profile->metadata.action_abi_digest);
    result.allowed_domain_mask = profile->metadata.domain_mask;
#ifdef COMBO_ENABLE
    result.allowed_domain_mask |= NOAH_PROFILE_VALIDATOR_V1_DOMAIN_COMBOS;
    result.runtime = &compiled_runtime;
#endif
    result.required_domain_mask              = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_PD;
    result.logical_layer_count               = LAYER_COUNT;
    result.supported_pd_mode_mask            = UINT32_MAX >> (32u - PD_MODE_COUNT);
    result.via_macro_slot_count              = VIA_MACRO_SLOT_COUNT;
    result.custom_key_count                  = NOAH_PROFILE_ACTION_V1_MAX_CUSTOM_KEYS;
    result.rgb_limits.logical_layer_count    = LAYER_COUNT;
    result.rgb_limits.supported_pd_mode_mask = result.supported_pd_mode_mask;
    result.rgb_limits.tap_branch_color_count = KEY_BEHAVIOR_MAX_TAP_COUNT - 1u;
#if defined(RGB_MATRIX_ENABLE)
    result.rgb_limits.maximum_brightness  = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
    result.rgb_limits.compiled_stage_mask = noah_profile_rgb_compiled_v1_stage_mask();
#else
    result.rgb_limits.compiled_stage_mask = 0u;
#endif
    *compatibility = result;
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
    compatibility->allowed_domain_mask |= NOAH_PROFILE_VALIDATOR_V1_DOMAIN_SETTINGS;
#endif
    return true;
}

_Static_assert(sizeof(noah_profile_compiled_v1_t) <= 40u, "compiled-profile handle retains metadata and five ranges only");
