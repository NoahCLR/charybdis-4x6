#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/profile/runtime/profile_action_placement_v1.h"
#include "users/noah/lib/profile/runtime/profile_owner.h"
#include "users/noah/lib/profile/runtime/effective_keymap_macros_runtime.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "users/noah/lib/profile/schema/profile_settings_v1.h"
#include "users/noah/lib/profile/storage/profile_flash_store.h"

static const uint8_t compiled_blob[] = {'N', 'L', 'P', '1', 4u, 0u, 0u, 1u};

static const uint8_t long_profile[] = {
    0x4eu, 0x4cu, 0x50u, 0x31u, 0x04u, 0x00u, 0x01u, 0x01u, 0x10u, 0x04u, 0x4fu, 0x02u, 0x00u, 0x00u, 0x04u, 0x00u,
    0x1fu, 0x00u, 0x10u, 0x10u, 0x0cu, 0x20u, 0x0cu, 0x00u, 0x04u, 0x08u, 0x3au, 0x08u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x03u, 0x01u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x03u,
    0x02u, 0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x03u, 0x03u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x03u, 0x04u, 0x04u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x03u, 0x05u, 0x05u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x03u, 0x06u, 0x06u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x03u, 0x07u, 0x07u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x03u, 0x08u, 0x08u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x03u, 0x09u,
    0x09u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x03u, 0x0au, 0x0au, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x03u, 0x0bu, 0x0bu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x03u, 0x0cu, 0x0cu, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x03u, 0x0du, 0x0du, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x03u, 0x0eu, 0x0eu, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x03u, 0x0fu, 0x0fu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x00u,
    0xffu, 0xc8u, 0x00u, 0x01u, 0x10u, 0xffu, 0xc8u, 0x01u, 0x02u, 0x20u, 0xffu, 0xc8u, 0x00u, 0x03u, 0x30u, 0xffu,
    0xc8u, 0x01u, 0x04u, 0x40u, 0xffu, 0xc8u, 0x00u, 0x05u, 0x50u, 0xffu, 0xc8u, 0x01u, 0x06u, 0x60u, 0xffu, 0xc8u,
    0x00u, 0x07u, 0x70u, 0xffu, 0xc8u, 0x01u, 0x08u, 0x80u, 0xffu, 0xc8u, 0x00u, 0x09u, 0x90u, 0xffu, 0xc8u, 0x01u,
    0x0au, 0xa0u, 0xffu, 0xc8u, 0x00u, 0x0bu, 0xb0u, 0xffu, 0xc8u, 0x01u, 0x0cu, 0xc0u, 0xffu, 0xc8u, 0x00u, 0x0du,
    0xd0u, 0xffu, 0xc8u, 0x01u, 0x0eu, 0xe0u, 0xffu, 0xc8u, 0x00u, 0x0fu, 0xf0u, 0xffu, 0xc8u, 0x01u, 0x07u, 0x00u,
    0xffu, 0xc8u, 0x00u, 0x08u, 0x0au, 0xffu, 0xc8u, 0x01u, 0x0fu, 0x14u, 0xffu, 0xc8u, 0x02u, 0xffu, 0x1eu, 0xffu,
    0xc8u, 0x03u, 0x07u, 0x28u, 0xffu, 0xc8u, 0x04u, 0x08u, 0x32u, 0xffu, 0xc8u, 0x05u, 0x0fu, 0x3cu, 0xffu, 0xc8u,
    0x06u, 0xffu, 0x46u, 0xffu, 0xc8u, 0x07u, 0x07u, 0x50u, 0xffu, 0xc8u, 0x08u, 0x08u, 0x5au, 0xffu, 0xc8u, 0x09u,
    0x0fu, 0x64u, 0xffu, 0xc8u, 0x0au, 0xffu, 0x6eu, 0xffu, 0xc8u, 0x0bu, 0x00u, 0xa7u, 0xffu, 0xc7u, 0x00u, 0x00u,
    0xc8u, 0x96u, 0x00u, 0x01u, 0x08u, 0xc8u, 0x96u, 0x01u, 0x02u, 0x10u, 0xc8u, 0x96u, 0x02u, 0x03u, 0x18u, 0xc8u,
    0x96u, 0x03u, 0x04u, 0x20u, 0xc8u, 0x96u, 0x04u, 0x05u, 0x28u, 0xc8u, 0x96u, 0x00u, 0x06u, 0x30u, 0xc8u, 0x96u,
    0x01u, 0x07u, 0x38u, 0xc8u, 0x96u, 0x02u, 0x08u, 0x40u, 0xc8u, 0x96u, 0x03u, 0x09u, 0x48u, 0xc8u, 0x96u, 0x04u,
    0x0au, 0x50u, 0xc8u, 0x96u, 0x00u, 0x0bu, 0x58u, 0xc8u, 0x96u, 0x01u, 0x0cu, 0x60u, 0xc8u, 0x96u, 0x02u, 0x0du,
    0x68u, 0xc8u, 0x96u, 0x03u, 0x0eu, 0x70u, 0xc8u, 0x96u, 0x04u, 0x0fu, 0x78u, 0xc8u, 0x96u, 0x00u, 0x10u, 0x80u,
    0xc8u, 0x96u, 0x01u, 0x11u, 0x88u, 0xc8u, 0x96u, 0x02u, 0x12u, 0x90u, 0xc8u, 0x96u, 0x03u, 0x13u, 0x98u, 0xc8u,
    0x96u, 0x04u, 0x14u, 0xa0u, 0xc8u, 0x96u, 0x00u, 0x15u, 0xa8u, 0xc8u, 0x96u, 0x01u, 0x16u, 0xb0u, 0xc8u, 0x96u,
    0x02u, 0x17u, 0xb8u, 0xc8u, 0x96u, 0x03u, 0x18u, 0xc0u, 0xc8u, 0x96u, 0x04u, 0x19u, 0xc8u, 0xc8u, 0x96u, 0x00u,
    0x1au, 0xd0u, 0xc8u, 0x96u, 0x01u, 0x1bu, 0xd8u, 0xc8u, 0x96u, 0x02u, 0x1cu, 0xe0u, 0xc8u, 0x96u, 0x03u, 0x1du,
    0xe8u, 0xc8u, 0x96u, 0x04u, 0x1eu, 0xf0u, 0xc8u, 0x96u, 0x00u, 0x1fu, 0xf8u, 0xc8u, 0x96u, 0x01u, 0x00u, 0x00u,
    0xffu, 0xc8u, 0x0fu, 0x1fu, 0x14u, 0xffu, 0xc8u, 0x0eu, 0xffu, 0x28u, 0xffu, 0xc8u, 0x0du, 0x00u, 0x3cu, 0xffu,
    0xc8u, 0x0cu, 0x1fu, 0x50u, 0xffu, 0xc8u, 0x0bu, 0xffu, 0x64u, 0xffu, 0xc8u, 0x0au, 0x00u, 0x78u, 0xffu, 0xc8u,
    0x09u, 0x1fu, 0x8cu, 0xffu, 0xc8u, 0x08u, 0xffu, 0xa0u, 0xffu, 0xc8u, 0x07u, 0x00u, 0xb4u, 0xffu, 0xc8u, 0x06u,
    0x1fu, 0xc8u, 0xffu, 0xc8u, 0x05u, 0xffu, 0xdcu, 0xffu, 0xc8u, 0x04u, 0xbfu, 0xffu, 0xc8u, 0x03u, 0x00u, 0xffu,
    0xc8u, 0x28u, 0xffu, 0xc8u, 0x50u, 0xffu, 0xc8u, 0x78u, 0xffu, 0xc8u, 0x00u, 0x00u, 0x96u, 0x12u, 0xffu, 0xc8u,
    0x94u, 0xffu, 0xc8u, 0x01u, 0x03u, 0x00u, 0x00u, 0xffu, 0xc8u, 0x04u, 0x01u, 0x1eu, 0xffu, 0xc8u, 0x05u, 0x02u,
    0x3cu, 0xffu, 0xc8u, 0x06u, 0x03u, 0x5au, 0xffu, 0xc8u, 0x07u, 0xffu, 0x78u, 0xffu, 0xc8u, 0x08u, 0x00u, 0x96u,
    0xffu, 0xc8u, 0x09u, 0x01u, 0xb4u, 0xffu, 0xc8u, 0x0au, 0x02u, 0xd2u, 0xffu, 0xc8u, 0x0bu,
};

static const uint8_t *candidate_blob = compiled_blob;
static uint32_t candidate_length = sizeof(compiled_blob);
static uint8_t candidate_domains;

typedef struct {
    uint8_t  bytes[NOAH_PROFILE_STORAGE_REGION_SIZE];
    uint32_t reads;
    uint32_t writes;
    uint16_t largest_read;
    uint16_t largest_write;
    bool     fail_next_write;
} memory_t;

typedef struct {
    noah_profile_owner_t *peer;
    uint32_t chunk_frames;
    uint32_t reuse_frames;
    uint32_t metadata_frames;
    uint32_t exchanges;
    uint32_t source_unavailable;
    uint8_t largest_chunk;
    bool inject_busy;
    noah_profile_split_v1_frame_t injected;
} split_link_t;

static bool paced_copy;
static uint32_t fake_now;

static uint32_t compiled_reads;
static bool     key_installed;
static bool     rgb_installed;
static bool     rgb_install_allowed = true;
static bool     rgb_cache_valid = true;
static uint32_t activation_block_reason;

static noah_profile_split_descriptor_t peer_descriptor(const noah_profile_owner_t *owner, uint32_t generation);

static uint32_t crc_of(const uint8_t *bytes, size_t length) {
    return noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, length));
}

static uint32_t digest_of(const uint8_t *bytes, size_t length) {
    return noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, bytes, length);
}

static bool compiled_read(void *context, size_t offset, uint8_t *target, size_t length) {
    (void)context;
    compiled_reads++;
    if (!target || length == 0u || offset > sizeof(compiled_blob) || length > sizeof(compiled_blob) - offset) {
        return false;
    }
    memcpy(target, &compiled_blob[offset], length);
    return true;
}

noah_profile_compiled_v1_result_t noah_profile_compiled_v1_open(noah_profile_compiled_v1_t *profile, noah_profile_compiled_v1_error_t *error) {
    (void)error;
    if (!profile) return NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT;
    *profile = (noah_profile_compiled_v1_t){
        .metadata =
            {
                .crc32             = crc_of(compiled_blob, sizeof(compiled_blob)),
                .digest            = digest_of(compiled_blob, sizeof(compiled_blob)),
                .action_abi_digest = UINT32_C(0x12345678),
                .byte_length       = sizeof(compiled_blob),
                .domain_mask       = 0u,
            },
    };
    return NOAH_PROFILE_COMPILED_V1_OK;
}

noah_profile_reader_t noah_profile_compiled_v1_reader(const noah_profile_compiled_v1_t *profile) {
    return (noah_profile_reader_t){.read = compiled_read, .context = (void *)profile, .length = sizeof(compiled_blob)};
}

// The placement rules are tested with the real action classification in the
// action-dispatch runner; the owner only has to hand them to its candidates.
bool noah_profile_action_placement_v1_supported(const noah_profile_action_v1_t *action, noah_profile_validator_v1_placement_t placement) {
    (void)action;
    (void)placement;
    return true;
}

bool noah_profile_compiled_v1_compatibility(const noah_profile_compiled_v1_t *profile, noah_profile_validator_v1_compatibility_t *compatibility) {
    if (!profile || !compatibility) return false;
    *compatibility                      = noah_profile_validator_v1_default_compatibility(profile->metadata.action_abi_digest);
    compatibility->required_domain_mask = 0u;
    return true;
}

void noah_effective_key_behavior_runtime_init(noah_effective_key_behavior_runtime_t *runtime) {
    memset(runtime, 0, sizeof(*runtime));
    runtime->initialized = true;
}

bool noah_effective_key_behavior_runtime_install(noah_effective_key_behavior_runtime_t *runtime) {
    key_installed = runtime && runtime->initialized;
    return key_installed;
}

void noah_effective_key_behavior_runtime_uninstall(noah_effective_key_behavior_runtime_t *runtime) {
    (void)runtime;
    key_installed = false;
}

void noah_effective_key_behavior_runtime_invalidate(void *context, uint32_t publication_count, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *callback_view) {
    (void)context;
    (void)publication_count;
    (void)previous;
    (void)active;
    (void)callback_view;
}

void noah_effective_rgb_runtime_init(noah_effective_rgb_runtime_t *runtime) {
    memset(runtime, 0, sizeof(*runtime));
    runtime->banks[0].valid = runtime->banks[1].valid = true;
    runtime->initialized = true;
}

bool noah_effective_rgb_runtime_install(noah_effective_rgb_runtime_t *runtime) {
    rgb_installed = rgb_install_allowed && runtime && runtime->initialized;
    return rgb_installed;
}

void noah_effective_rgb_runtime_uninstall(noah_effective_rgb_runtime_t *runtime) {
    (void)runtime;
    rgb_installed = false;
}

void noah_effective_rgb_runtime_invalidate(void *context, uint32_t publication_count, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *callback_view) {
    noah_effective_rgb_runtime_t *runtime = context;
    runtime->banks[runtime->active_index].valid = rgb_cache_valid;
    (void)publication_count;
    (void)previous;
    (void)active;
    (void)callback_view;
}

void noah_profile_activation_policy_init(noah_profile_activation_policy_t *policy, noah_profile_activation_peer_observer_fn observer, void *observer_context) {
    memset(policy, 0, sizeof(*policy));
    policy->peer_observer = observer;
    policy->peer_context  = observer_context;
    policy->initialized   = true;
}

uint32_t noah_profile_activation_policy_safe_boundary(void *context) {
    noah_profile_activation_policy_t *policy     = context;
    uint8_t                           unresolved = 1u;
    return activation_block_reason | (policy && policy->initialized && policy->peer_observer && policy->peer_observer(policy->peer_context, &unresolved) && unresolved == 0u ? 0u : NOAH_PROFILE_ACTIVATION_REASON_PEER);
}

// Reads and writes are bounded as the flash adapter bounds them
// (qmk_profile_flash.c).
static bool memory_read(void *context, noah_profile_storage_address_t address, uint8_t *target, uint32_t length) {
    memory_t *memory = context;
    memory->reads++;
    if (length > memory->largest_read) memory->largest_read = length;
    if (!target || length == 0u || length > NOAH_PROFILE_STORE_IO_CHUNK_MAX || (uint32_t)address + length > sizeof(memory->bytes)) return false;
    memcpy(target, &memory->bytes[address], length);
    return true;
}

static bool memory_write(void *context, noah_profile_storage_address_t address, const uint8_t *source, uint32_t length) {
    memory_t *memory = context;
    memory->writes++;
    if (length > memory->largest_write) memory->largest_write = length;
    if (memory->fail_next_write) {
        memory->fail_next_write = false;
        return false;
    }
    if (!source || length == 0u || length > NOAH_PROFILE_STORE_IO_CHUNK_MAX || (uint32_t)address + length > sizeof(memory->bytes)) return false;
    memcpy(&memory->bytes[address], source, length);
    return true;
}

static bool split_exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    split_link_t                    *link = context;
    noah_profile_split_reconciler_t *peer_reconciler;

    if (link && link->inject_busy) {
        link->inject_busy = false;
        return noah_profile_split_v1_frame_encode(&link->injected, response, response_length) == response_length;
    }
    if (!link || !link->peer || !(peer_reconciler = noah_profile_owner_split_reconciler(link->peer))) {
        return false;
    }
    if (request_length > 1u && request[1] == NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK) {
        link->chunk_frames++;
        if (request[20] > link->largest_chunk) link->largest_chunk = request[20];
    }
    if (request_length > 1u && request[1] == NOAH_PROFILE_SPLIT_V1_PAYLOAD_REUSE) link->reuse_frames++;
    if (request_length > 1u && request[1] == NOAH_PROFILE_SPLIT_V1_METADATA) link->metadata_frames++;
    link->exchanges++;
    bool result = noah_profile_split_reconciler_receive(peer_reconciler, request, request_length, response, response_length);
    if (result && response[2] == NOAH_PROFILE_SPLIT_V1_STATUS_SOURCE_UNAVAILABLE) link->source_unavailable++;
    if (paced_copy) {
        // Model the peer continuing to scan during the master's blocking RPC.
        uint32_t rpc_ms = request_length > NOAH_PROFILE_SPLIT_V1_FRAME_SIZE || response_length > NOAH_PROFILE_SPLIT_V1_FRAME_SIZE ? 9u : 5u;
        for (uint32_t elapsed = 0u; elapsed < rpc_ms; elapsed++)
            (void)noah_profile_owner_scan(link->peer, false, ++fake_now);
    }
    return result;
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

static noah_profile_candidate_v1_metadata_t metadata(void) {
    return (noah_profile_candidate_v1_metadata_t){
        .store_format_version = NOAH_PROFILE_LOGICAL_STORE_VERSION,
        .schema_major         = NOAH_PROFILE_SCHEMA_MAJOR,
        .schema_minor         = 0u,
        .requested_domains    = candidate_domains,
        .payload_length       = candidate_length,
        .crc32                = crc_of(candidate_blob, candidate_length),
        .digest               = digest_of(candidate_blob, candidate_length),
        .action_abi_digest    = UINT32_C(0x12345678),
    };
}

static void begin_frame(uint8_t frame[32], uint16_t transaction_id) {
    noah_profile_candidate_v1_metadata_t declaration = metadata();
    memset(frame, 0, 32u);
    frame[0] = NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET;
    frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    frame[2] = NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN;
    write_u16(&frame[3], transaction_id);
    frame[5] = declaration.schema_major;
    frame[6] = declaration.schema_minor;
    frame[7] = declaration.requested_domains;
    write_u32(&frame[8], declaration.payload_length);
    write_u32(&frame[12], declaration.crc32);
    write_u32(&frame[16], declaration.digest);
    write_u32(&frame[20], declaration.action_abi_digest);
}

static void chunk_frame(uint8_t frame[32], uint16_t transaction_id) {
    memset(frame, 0, 32u);
    frame[0] = NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET;
    frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    frame[2] = NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK;
    write_u16(&frame[3], transaction_id);
    write_u32(&frame[5], 0u);
    frame[9] = sizeof(compiled_blob);
    memcpy(&frame[10], compiled_blob, sizeof(compiled_blob));
}

static void simple_frame(uint8_t frame[32], uint8_t value, uint16_t transaction_id) {
    memset(frame, 0, 32u);
    frame[0] = value == NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT ? NOAH_PROFILE_CANDIDATE_V1_COMMAND_SAVE : NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET;
    frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    frame[2] = value;
    write_u16(&frame[3], transaction_id);
}

static void boot_empty(noah_profile_owner_t *owner, memory_t *memory) {
    noah_profile_owner_config_t config = {
        .store_io    = {.read = memory_read, .write = memory_write, .context = memory},
        .origin_half = 0u,
    };
    uint32_t prior_eeprom_reads;
    uint32_t writes_before_init   = memory->writes;
    uint32_t compiled_before_init = compiled_reads;

    assert(noah_profile_owner_init(owner, &config));
    assert(owner->state == NOAH_PROFILE_OWNER_VALIDATING_COMPILED);
    assert(memory->reads == 0u && memory->writes == writes_before_init && compiled_reads == compiled_before_init);
    for (uint32_t scan = 0u; scan < 256u && owner->state != NOAH_PROFILE_OWNER_READY_COMPILED; scan++) {
        prior_eeprom_reads = memory->reads;
        assert(noah_profile_owner_scan(owner, true, scan));
        assert(memory->reads - prior_eeprom_reads <= 1u);
    }
    assert(owner->state == NOAH_PROFILE_OWNER_READY_COMPILED);
    assert(memory->writes == writes_before_init && memory->largest_read <= NOAH_PROFILE_STORE_IO_CHUNK_MAX);
    assert(compiled_reads > compiled_before_init && key_installed && rgb_installed);
    assert(owner->descriptor_readable && !owner->committed_descriptor.has_profile);
    // Candidates the host asks this owner to save are held to where their
    // actions are placed; its own compatibility, used for the compiled
    // defaults and committed records, is not.
    assert(owner->staging.candidate_backend.candidate_runtime == &owner->candidate_runtime);
    assert(owner->candidate_runtime.placement_supported == noah_profile_action_placement_v1_supported);
    assert(!(owner->compatibility.runtime && owner->compatibility.runtime->placement_supported));
}

static void scan_pair(noah_profile_owner_t *master, noah_profile_owner_t *peer, uint32_t *now) {
    (void)noah_profile_owner_scan(peer, false, *now);
    (void)noah_profile_owner_scan(master, true, *now);
    *now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
}

static void initialize_pair(noah_profile_owner_t *left, memory_t *left_memory, noah_profile_owner_t *right, memory_t *right_memory, split_link_t *left_link, split_link_t *right_link, uint32_t *now) {
    assert(noah_profile_owner_init(left, &(noah_profile_owner_config_t){
                                             .store_io                = {.read = memory_read, .write = memory_write, .context = left_memory},
                                             .split_exchange          = split_exchange,
                                             .split_transport_context = left_link,
                                             .origin_half             = 0u,
                                             .peer_required           = true,
                                         }));
    assert(noah_profile_owner_init(right, &(noah_profile_owner_config_t){
                                              .store_io                = {.read = memory_read, .write = memory_write, .context = right_memory},
                                              .split_exchange          = split_exchange,
                                              .split_transport_context = right_link,
                                              .origin_half             = 1u,
                                              .peer_required           = true,
                                          }));
}

static void boot_empty_pair(noah_profile_owner_t *left, memory_t *left_memory, noah_profile_owner_t *right, memory_t *right_memory, split_link_t *left_link, split_link_t *right_link, uint32_t *now) {
    initialize_pair(left, left_memory, right, right_memory, left_link, right_link, now);
    for (uint32_t guard = 0u; guard < 4096u && (left->state != NOAH_PROFILE_OWNER_READY_COMPILED || right->state != NOAH_PROFILE_OWNER_READY_COMPILED); guard++) {
        scan_pair(left, right, now);
    }
    assert(left->state == NOAH_PROFILE_OWNER_READY_COMPILED);
    assert(right->state == NOAH_PROFILE_OWNER_READY_COMPILED);
}

static void send_and_scan(noah_profile_owner_t *owner, uint8_t frame[32], uint32_t now) {
    assert(noah_profile_owner_receive(owner, frame, 32u));
    assert(noah_profile_owner_scan(owner, true, now));
}

static void commit_blob(noah_profile_owner_t *owner, uint16_t transaction_id, uint32_t *now) {
    uint8_t frame[32];

    begin_frame(frame, transaction_id);
    send_and_scan(owner, frame, (*now)++);
    chunk_frame(frame, transaction_id);
    send_and_scan(owner, frame, (*now)++);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, transaction_id);
    send_and_scan(owner, frame, (*now)++);
    while (owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING)
        assert(noah_profile_owner_scan(owner, true, (*now)++));
    assert(owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, transaction_id);
    send_and_scan(owner, frame, (*now)++);
    for (uint32_t guard = 0u; guard < 256u && owner->host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE; guard++)
        assert(noah_profile_owner_scan(owner, true, (*now)++));
    assert(owner->state == NOAH_PROFILE_OWNER_READY_VALIDATED);
}

static void send_and_scan_pair(noah_profile_owner_t *master, noah_profile_owner_t *peer, uint8_t frame[32], uint32_t *now) {
    assert(noah_profile_owner_receive(master, frame, 32u));
    for (uint32_t guard = 0u; guard < 32u && master->host_transaction.mailbox.pending; guard++) {
        scan_pair(master, peer, now);
    }
    assert(!master->host_transaction.mailbox.pending);
}

static void stage_valid_candidate_pair(noah_profile_owner_t *master, noah_profile_owner_t *peer, uint16_t transaction_id, uint32_t *now) {
    uint8_t frame[32];

    begin_frame(frame, transaction_id);
    send_and_scan_pair(master, peer, frame, now);
    for (uint32_t offset = 0u; offset < candidate_length;) {
        uint8_t piece = candidate_length - offset < NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX ? (uint8_t)(candidate_length - offset) : NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX;
        memset(frame, 0, sizeof(frame));
        frame[0] = NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET;
        frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
        frame[2] = NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK;
        write_u16(&frame[3], transaction_id);
        write_u32(&frame[5], offset);
        frame[9] = piece;
        memcpy(&frame[10], candidate_blob + offset, piece);
        send_and_scan_pair(master, peer, frame, now);
        offset += piece;
    }
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, transaction_id);
    send_and_scan_pair(master, peer, frame, now);
    for (uint32_t guard = 0u; guard < 4096u && master->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING; guard++) {
        scan_pair(master, peer, now);
    }
    assert(master->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);
}

static void assert_pair_profile(noah_profile_owner_t *left, noah_profile_owner_t *right, uint32_t generation) {
    noah_profile_owner_t *halves[] = {left, right};
    for (unsigned half = 0u; half < 2u; half++) {
        noah_effective_profile_snapshot_t active;
        uint8_t bytes[sizeof(long_profile)];
        assert(candidate_length <= sizeof(bytes));
        assert(noah_effective_profile_provider_copy_active(&halves[half]->provider, &active) == NOAH_EFFECTIVE_PROFILE_OK);
        assert(active.identity.kind == NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE);
        assert(active.identity.generation == generation && active.identity.payload_digest == digest_of(candidate_blob, candidate_length));
        for (uint32_t offset = 0u; offset < candidate_length;) {
            uint32_t piece = candidate_length - offset < NOAH_PROFILE_STORE_IO_CHUNK_MAX ? candidate_length - offset : NOAH_PROFILE_STORE_IO_CHUNK_MAX;
            assert(noah_effective_profile_snapshot_read(&active, offset, bytes + offset, piece));
            offset += piece;
        }
        assert(memcmp(bytes, candidate_blob, candidate_length) == 0);
    }
}

static void reboot_saved_pair(noah_profile_owner_t *left, memory_t *left_memory, noah_profile_owner_t *right, memory_t *right_memory, split_link_t *left_link, split_link_t *right_link, uint32_t *now, uint32_t generation) {
    // Only owners restart: both durable memories and the split links survive.
    initialize_pair(left, left_memory, right, right_memory, left_link, right_link, now);
    for (uint32_t guard = 0u; guard < 32768u && (left->state != NOAH_PROFILE_OWNER_READY_VALIDATED || right->state != NOAH_PROFILE_OWNER_READY_VALIDATED); guard++)
        scan_pair(left, right, now);
    assert(left->state == NOAH_PROFILE_OWNER_READY_VALIDATED && right->state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    assert_pair_profile(left, right, generation);
}

static void commit_valid_candidate_pair(noah_profile_owner_t *master, noah_profile_owner_t *peer, uint16_t transaction_id, uint32_t expected_generation, uint32_t *now) {
    noah_effective_profile_status_t master_provider;
    uint8_t                         frame[32];
    bool                            saw_peer_prepared_before_local = false;
    bool                            saw_local_durable_before_peer  = false;

    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, transaction_id);
    send_and_scan_pair(master, peer, frame, now);
    assert(master->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER);

    for (uint32_t guard = 0u; guard < 4096u && (master->host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE || master->state != NOAH_PROFILE_OWNER_READY_VALIDATED || peer->state != NOAH_PROFILE_OWNER_READY_VALIDATED || master->committed_descriptor.generation != expected_generation || peer->committed_descriptor.generation != expected_generation); guard++) {
        if (master->store.committed.generation != expected_generation && peer->store.prepare_active && peer->store.candidate_written == peer->store.candidate.payload_length && noah_profile_peer_store_backend_state(&peer->peer_store) == NOAH_PROFILE_PEER_STORE_RECEIVING) {
            saw_peer_prepared_before_local = true;
        }
        if (master->store.committed.generation == expected_generation && peer->store.committed.generation != expected_generation) {
            saw_local_durable_before_peer = true;
            assert(noah_effective_profile_provider_status(&master->provider, &master_provider) == NOAH_EFFECTIVE_PROFILE_OK);
            assert(master_provider.active.generation != expected_generation);
        }
        scan_pair(master, peer, now);
        assert(master->state != NOAH_PROFILE_OWNER_STORAGE_ERROR && master->state != NOAH_PROFILE_OWNER_INTEGRATION_ERROR);
        assert(peer->state != NOAH_PROFILE_OWNER_STORAGE_ERROR && peer->state != NOAH_PROFILE_OWNER_INTEGRATION_ERROR);
    }
    assert(saw_peer_prepared_before_local);
    assert(saw_local_durable_before_peer);
    assert(master->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(master->committed_descriptor.generation == expected_generation);
    assert(peer->committed_descriptor.generation == expected_generation);
    assert(master->committed_descriptor.origin_half == 0u && peer->committed_descriptor.origin_half == 0u);
    assert(noah_effective_profile_provider_status(&master->provider, &master_provider) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(master_provider.active.generation == expected_generation && master_provider.active.origin == 0u);
    assert_pair_profile(master, peer, expected_generation);
}

static void test_committed_reuse_saves_both_halves_and_reboots(bool reboot) {
    noah_profile_owner_t left, right;
    memory_t left_memory = {0}, right_memory = {0};
    split_link_t left_link = {.peer = &right}, right_link = {.peer = &left};
    uint32_t now = 0u;
    uint8_t frame[32];

    candidate_blob = long_profile;
    candidate_length = sizeof(long_profile);
    candidate_domains = NOAH_PROFILE_CANDIDATE_V1_DOMAIN_RGB;
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    boot_empty_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);
    stage_valid_candidate_pair(&left, &right, 311u, &now);
    commit_valid_candidate_pair(&left, &right, 311u, 1u, &now);
    if (reboot) reboot_saved_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now, 1u);

    noah_effective_profile_snapshot_t source;
    assert(noah_effective_profile_provider_copy_active(&left.provider, &source) == NOAH_EFFECTIVE_PROFILE_OK);
    begin_frame(frame, 312u);
    send_and_scan_pair(&left, &right, frame, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_REUSE, 312u);
    write_u32(frame + 13, candidate_length);
    write_u32(frame + 17, source.identity.generation);
    write_u32(frame + 21, source.identity.payload_digest);
    write_u32(frame + 25, source.identity.payload_crc32);
    frame[29] = 1u;
    frame[30] = source.identity.origin;
    assert(noah_profile_owner_receive(&left, frame, sizeof(frame)));
    for (uint32_t guard = 0u; guard < 4096u && left.host_transaction.mailbox.pending; guard++) scan_pair(&left, &right, &now);
    assert(!left.host_transaction.mailbox.pending && left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_COMPLETE);
    assert_pair_profile(&left, &right, 1u);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, 312u);
    send_and_scan_pair(&left, &right, frame, &now);
    for (uint32_t guard = 0u; guard < 4096u && left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED; guard++) scan_pair(&left, &right, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);

    left_link.chunk_frames = left_link.reuse_frames = left_link.metadata_frames = left_link.exchanges = left_link.source_unavailable = 0u;
    uint32_t peer_ready_at = 0u, master_ready_at = 0u, master_scans = 0u;
    paced_copy = true;
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 312u);
    assert(noah_profile_owner_receive(&left, frame, sizeof(frame)));
    for (uint32_t guard = 0u; guard < 8192u && !master_ready_at; guard++) {
        fake_now = now;
        (void)noah_profile_owner_scan(&right, false, now);
        (void)noah_profile_owner_scan(&left, true, now);
        master_scans++;
        now = fake_now + 1u;
        if (!peer_ready_at && right.peer_store.state == NOAH_PROFILE_PEER_STORE_PREPARED) peer_ready_at = now;
        if (noah_profile_split_reconciler_prepared_push_ready(&left.reconciler, NULL)) master_ready_at = now;
    }
    paced_copy = false;
    assert(peer_ready_at && master_ready_at);
    assert(master_ready_at - peer_ready_at <= NOAH_PROFILE_SPLIT_STORE_POLL_MS + 5u);
    noah_profile_candidate_v1_peer_status_t diagnostic;
    assert(noah_profile_owner_peer_transfer(&left, &diagnostic));
    assert(diagnostic.reused_bytes == candidate_length && !diagnostic.reuse_unavailable);
    assert(diagnostic.reuse_exchanges == left_link.reuse_frames && diagnostic.source_unavailable_replies == 0u);
    // Every exchange but the periodic metadata one, which may fall due
    // while the copy runs, is the prepared push's.
    assert(diagnostic.prepared_exchanges == left_link.exchanges - left_link.metadata_frames);
    assert(diagnostic.prepared_scans > 0u && diagnostic.prepared_scans <= master_scans);
    for (uint32_t guard = 0u; guard < 8192u && (left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE || left.state != NOAH_PROFILE_OWNER_READY_VALIDATED || right.state != NOAH_PROFILE_OWNER_READY_VALIDATED || right.committed_descriptor.generation != 2u); guard++) scan_pair(&left, &right, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert_pair_profile(&left, &right, 2u);
    noah_profile_split_reconciler_status_t split;
    assert(noah_profile_split_reconciler_status(&left.reconciler, &split));
    assert(left_link.reuse_frames >= 1u && left_link.chunk_frames == 0u && split.reused_bytes == candidate_length && left_link.source_unavailable == 0u);
    reboot_saved_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now, 2u);
    candidate_blob = compiled_blob;
    candidate_length = sizeof(compiled_blob);
    candidate_domains = 0u;
}

static void test_long_profile_copy_reads_within_the_store_bound(void) {
    noah_profile_owner_t left, right;
    memory_t left_memory = {0}, right_memory = {0};
    split_link_t left_link = {.peer = &right}, right_link = {.peer = &left};
    uint32_t now = 0u;
    candidate_blob = long_profile;
    candidate_length = sizeof(long_profile);
    candidate_domains = NOAH_PROFILE_CANDIDATE_V1_DOMAIN_RGB;
    _Static_assert(NOAH_PROFILE_SPLIT_V1_CHUNK_MAX == 106u && NOAH_PROFILE_SPLIT_V1_CHUNK_MAX > NOAH_PROFILE_STORE_IO_CHUNK_MAX, "copy chunks must exceed the bounded store read");
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    boot_empty_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);
    stage_valid_candidate_pair(&left, &right, 321u, &now);
    commit_valid_candidate_pair(&left, &right, 321u, 1u, &now);
    assert(right.committed_descriptor.payload_length == sizeof(long_profile));
    assert(left_link.chunk_frames >= 1u && left_link.largest_chunk == 106u);
    assert(left_memory.largest_read <= NOAH_PROFILE_STORE_IO_CHUNK_MAX && right_memory.largest_read <= NOAH_PROFILE_STORE_IO_CHUNK_MAX);
    candidate_blob = compiled_blob;
    candidate_length = sizeof(compiled_blob);
    candidate_domains = 0u;
}

static void test_two_consecutive_live_commits_use_distributed_prepare_barrier(void) {
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint32_t             now        = 20000u;

    memset(&left_memory, 0, sizeof(left_memory));
    memset(&right_memory, 0, sizeof(right_memory));
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    boot_empty_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);

    stage_valid_candidate_pair(&left, &right, 80u, &now);
    commit_valid_candidate_pair(&left, &right, 80u, 1u, &now);
    stage_valid_candidate_pair(&left, &right, 81u, &now);
    commit_valid_candidate_pair(&left, &right, 81u, 2u, &now);
}

static void scan_dual_master(noah_profile_owner_t *left, noah_profile_owner_t *right, uint32_t *now) {
    (void)noah_profile_owner_scan(left, true, *now);
    (void)noah_profile_owner_scan(right, true, *now);
    *now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
}

static void queue_candidate_operation_dual(noah_profile_owner_t *target, noah_profile_owner_t *left, noah_profile_owner_t *right, uint8_t frame[32], uint32_t *now) {
    assert(noah_profile_owner_receive(target, frame, 32u));
    for (uint32_t guard = 0u; guard < 64u && target->host_transaction.mailbox.pending; guard++) {
        scan_dual_master(left, right, now);
    }
    assert(!target->host_transaction.mailbox.pending);
}

static void test_simultaneous_hosts_choose_stable_physical_origin_before_durability(void) {
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint8_t              frame[32];
    uint32_t             now = 50000u;

    memset(&left_memory, 0, sizeof(left_memory));
    memset(&right_memory, 0, sizeof(right_memory));
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    boot_empty_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);

    begin_frame(frame, 90u);
    queue_candidate_operation_dual(&left, &left, &right, frame, &now);
    chunk_frame(frame, 90u);
    queue_candidate_operation_dual(&left, &left, &right, frame, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, 90u);
    queue_candidate_operation_dual(&left, &left, &right, frame, &now);
    while (left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING)
        scan_dual_master(&left, &right, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);

    begin_frame(frame, 91u);
    queue_candidate_operation_dual(&right, &left, &right, frame, &now);
    chunk_frame(frame, 91u);
    queue_candidate_operation_dual(&right, &left, &right, frame, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, 91u);
    queue_candidate_operation_dual(&right, &left, &right, frame, &now);
    while (right.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING)
        scan_dual_master(&left, &right, &now);
    assert(right.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);

    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 90u);
    assert(noah_profile_owner_receive(&left, frame, sizeof(frame)));
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 91u);
    assert(noah_profile_owner_receive(&right, frame, sizeof(frame)));

    for (uint32_t guard = 0u; guard < 8192u && (left.committed_descriptor.generation != 1u || right.committed_descriptor.generation != 1u || left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE); guard++) {
        scan_dual_master(&left, &right, &now);
        assert(left.state != NOAH_PROFILE_OWNER_STORAGE_ERROR && left.state != NOAH_PROFILE_OWNER_INTEGRATION_ERROR && left.state != NOAH_PROFILE_OWNER_CONCURRENT_COMMIT);
        assert(right.state != NOAH_PROFILE_OWNER_STORAGE_ERROR && right.state != NOAH_PROFILE_OWNER_INTEGRATION_ERROR && right.state != NOAH_PROFILE_OWNER_CONCURRENT_COMMIT);
    }
    assert(left.committed_descriptor.generation == 1u && left.committed_descriptor.origin_half == 0u);
    assert(right.committed_descriptor.generation == 1u && right.committed_descriptor.origin_half == 0u);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(right.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(right.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_PREPARE_YIELDED);
    assert(left.store.committed.origin_half == 0u && right.store.committed.origin_half == 0u);
}

static void test_prepared_peer_is_aborted_before_host_timeout_releases_candidate(void) {
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint8_t              frame[32];
    uint32_t             now = 70000u;

    memset(&left_memory, 0, sizeof(left_memory));
    memset(&right_memory, 0, sizeof(right_memory));
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    boot_empty_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);
    stage_valid_candidate_pair(&left, &right, 100u, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 100u);
    send_and_scan_pair(&left, &right, frame, &now);
    for (uint32_t guard = 0u; guard < 1024u && !noah_profile_split_reconciler_prepared_push_ready(&left.reconciler, NULL); guard++) {
        scan_pair(&left, &right, &now);
    }
    assert(noah_profile_split_reconciler_prepared_push_ready(&left.reconciler, NULL));
    assert(right.store.prepare_active);
    assert(left.store.committed.slot == NOAH_PROFILE_SLOT_NONE && right.store.committed.slot == NOAH_PROFILE_SLOT_NONE);

    now = left.host_last_activity_at + NOAH_PROFILE_OWNER_HOST_BARRIER_NO_PROGRESS_MS;
    assert(noah_profile_owner_scan(&left, true, now));
    for (uint32_t guard = 0u; guard < 1024u && left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE; guard++) {
        scan_pair(&left, &right, &now);
    }
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT);
    assert(!left.store.prepare_active && !right.store.prepare_active);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    assert(noah_profile_candidate_store_backend_admission_owner(&right.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
}

static void test_host_abort_is_coordinated_at_each_prepare_boundary(void) {
    for (uint8_t phase = 0u; phase < 3u; phase++) {
        noah_profile_owner_t left;
        noah_profile_owner_t right;
        memory_t             left_memory;
        memory_t             right_memory;
        split_link_t         left_link  = {.peer = &right};
        split_link_t         right_link = {.peer = &left};
        uint8_t              frame[32];
        uint16_t             transaction_id = (uint16_t)(120u + phase);
        uint32_t             now            = (uint32_t)(120000u + phase * 10000u);

        memset(&left_memory, 0, sizeof(left_memory));
        memset(&right_memory, 0, sizeof(right_memory));
        memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
        memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
        boot_empty_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);
        stage_valid_candidate_pair(&left, &right, transaction_id, &now);
        simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, transaction_id);
        send_and_scan_pair(&left, &right, frame, &now);
        assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER);

        if (phase == 1u) {
            for (uint32_t guard = 0u; guard < 1024u && !(right.store.prepare_active && right.store.candidate_written < right.store.candidate.payload_length); guard++) {
                scan_pair(&left, &right, &now);
            }
            assert(right.store.prepare_active && right.store.candidate_written < right.store.candidate.payload_length);
        } else if (phase == 2u) {
            for (uint32_t guard = 0u; guard < 1024u && !noah_profile_split_reconciler_prepared_push_ready(&left.reconciler, NULL); guard++) {
                scan_pair(&left, &right, &now);
            }
            assert(noah_profile_split_reconciler_prepared_push_ready(&left.reconciler, NULL));
        }

        simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, transaction_id);
        assert(noah_profile_owner_receive(&left, frame, sizeof(frame)));
        for (uint32_t guard = 0u; guard < 2048u && left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE; guard++) {
            scan_pair(&left, &right, &now);
        }
        assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
        assert(left.host_transaction.status.last_operation == NOAH_PROFILE_CANDIDATE_V1_OPERATION_ABORT);
        assert(left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
        assert(left.store.committed.slot == NOAH_PROFILE_SLOT_NONE && right.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
        assert(!left.store.prepare_active && !right.store.prepare_active);
        assert(noah_profile_candidate_store_backend_admission_owner(&left.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
        assert(noah_profile_candidate_store_backend_admission_owner(&right.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    }
}

static bool owner_peer_cleanup_pending(const noah_profile_owner_t *owner) {
    noah_profile_owner_status_t status;

    assert(noah_profile_owner_status(owner, &status));
    return status.peer_cleanup_pending;
}

// The keyboard wedged this way on 2026-09-23: the peer answered every split
// ABORT with BUSY, so the host's own ABORT sat in the mailbox, the host
// timeout (which needs an empty mailbox) never ran, and only a power cycle
// released the transaction.
static void test_host_abort_ends_when_peer_never_acknowledges(void) {
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint8_t              frame[32];
    uint32_t             now = 200000u;
    uint32_t             aborted_at;

    memset(&left_memory, 0, sizeof(left_memory));
    memset(&right_memory, 0, sizeof(right_memory));
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    boot_empty_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);
    stage_valid_candidate_pair(&left, &right, 1u, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 1u);
    send_and_scan_pair(&left, &right, frame, &now);
    for (uint32_t guard = 0u; guard < 1024u && !noah_profile_split_reconciler_prepared_push_ready(&left.reconciler, NULL); guard++) {
        scan_pair(&left, &right, &now);
    }
    assert(noah_profile_split_reconciler_prepared_push_ready(&left.reconciler, NULL));
    assert(!owner_peer_cleanup_pending(&left));
    {
        noah_profile_candidate_v1_peer_status_t peer;

        assert(noah_profile_owner_peer_transfer(&left, &peer));
        assert(peer.phase == NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_PREPARED);
        assert(peer.transfer_length > 0u && peer.transfer_offset == peer.transfer_length);
        assert(peer.flags == NOAH_PROFILE_CANDIDATE_V1_PEER_FLAG_MASTER);
    }

    // Durability unknown on the peer: every ABORT is BUSY until it restarts.
    right.peer_store.state = NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED;
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, 1u);
    assert(noah_profile_owner_receive(&left, frame, sizeof(frame)));
    aborted_at = now;
    for (uint32_t guard = 0u; guard < 4096u && left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE; guard++) {
        scan_pair(&left, &right, &now);
        assert(left.state != NOAH_PROFILE_OWNER_STORAGE_ERROR && left.state != NOAH_PROFILE_OWNER_INTEGRATION_ERROR);
    }
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(left.host_transaction.status.last_operation == NOAH_PROFILE_CANDIDATE_V1_OPERATION_ABORT);
    assert(left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
    assert(!left.host_transaction.mailbox.pending);
    assert(now - aborted_at <= NOAH_PROFILE_SPLIT_PREPARED_ABORT_TIMEOUT_MS + 2u * NOAH_PROFILE_SPLIT_RETRY_MAX_MS);
    assert(!left.store.prepare_active);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    // Nothing became durable on either half.
    assert(left.store.committed.slot == NOAH_PROFILE_SLOT_NONE && right.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
    assert(owner_peer_cleanup_pending(&left));
    {
        noah_profile_candidate_v1_peer_status_t peer;

        // Released: no transfer is active, and page 1 says cleanup is owed.
        assert(noah_profile_owner_peer_transfer(&left, &peer));
        assert(peer.phase == NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_IDLE);
        assert(peer.flags & NOAH_PROFILE_CANDIDATE_V1_PEER_FLAG_CLEANUP_PENDING);
        assert(peer.retry_count > 0u);
    }

    // The next save cannot stack a second transfer onto the wedged peer. It
    // ends on the host's own timeout instead of waiting forever.
    stage_valid_candidate_pair(&left, &right, 2u, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 2u);
    send_and_scan_pair(&left, &right, frame, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER);
    aborted_at = now;
    for (uint32_t guard = 0u; guard < 4096u && left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE; guard++) {
        scan_pair(&left, &right, &now);
    }
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT);
    assert(now - aborted_at <= NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS + 2u * NOAH_PROFILE_SPLIT_RETRY_MAX_MS);
    assert(left.store.committed.slot == NOAH_PROFILE_SLOT_NONE && right.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
    assert(owner_peer_cleanup_pending(&left));
}

static void test_barrier_mailbox_retries_and_postcommit_abort_do_not_starve(void) {
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint8_t              frame[32];
    uint32_t             now = 160000u;
    uint32_t             writes_before;

    memset(&left_memory, 0, sizeof(left_memory));
    memset(&right_memory, 0, sizeof(right_memory));
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    boot_empty_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);
    stage_valid_candidate_pair(&left, &right, 130u, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 130u);
    send_and_scan_pair(&left, &right, frame, &now);
    for (uint32_t guard = 0u; guard < 2048u && left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_CONVERGING_PEER; guard++) {
        scan_pair(&left, &right, &now);
    }
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_CONVERGING_PEER);
    assert(left.store.committed.generation == 1u);

    writes_before = left_memory.writes;
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 130u);
    assert(noah_profile_owner_receive(&left, frame, sizeof(frame)));
    left.scheduler_cursor = 0u;
    assert(noah_profile_owner_scan(&left, true, now++));
    assert(!left.host_transaction.mailbox.pending);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_CONVERGING_PEER);
    assert(left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
    assert(left_memory.writes == writes_before);

    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, 130u);
    assert(noah_profile_owner_receive(&left, frame, sizeof(frame)));
    left.scheduler_cursor = 0u;
    assert(noah_profile_owner_scan(&left, true, now++));
    assert(!left.host_transaction.mailbox.pending);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_CONVERGING_PEER);
    assert(left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE);
    assert(left_memory.writes == writes_before);

    for (uint32_t guard = 0u; guard < 4096u && left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE; guard++) {
        scan_pair(&left, &right, &now);
    }
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(left.state == NOAH_PROFILE_OWNER_READY_VALIDATED && right.state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    assert(left.committed_descriptor.generation == 1u && right.committed_descriptor.generation == 1u);
}

static void test_local_known_commit_failure_aborts_prepared_peer_first(void) {
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint8_t              frame[32];
    uint32_t             now = 180000u;

    memset(&left_memory, 0, sizeof(left_memory));
    memset(&right_memory, 0, sizeof(right_memory));
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    boot_empty_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);
    stage_valid_candidate_pair(&left, &right, 140u, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 140u);
    send_and_scan_pair(&left, &right, frame, &now);
    for (uint32_t guard = 0u; guard < 1024u && !noah_profile_split_reconciler_prepared_push_ready(&left.reconciler, NULL); guard++) {
        scan_pair(&left, &right, &now);
    }
    assert(noah_profile_split_reconciler_prepared_push_ready(&left.reconciler, NULL));
    assert(right.store.prepare_active && right.store.committed.slot == NOAH_PROFILE_SLOT_NONE);

    left_memory.fail_next_write = true;
    for (uint32_t guard = 0u; guard < 2048u && left.state != NOAH_PROFILE_OWNER_STORAGE_ERROR; guard++) {
        scan_pair(&left, &right, &now);
    }
    assert(left.state == NOAH_PROFILE_OWNER_STORAGE_ERROR);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED);
    assert(left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE);
    assert(!left.host_transaction.has_candidate);
    assert(left.store.committed.slot == NOAH_PROFILE_SLOT_NONE && right.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
    assert(!left.store.prepare_active && !right.store.prepare_active);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    assert(noah_profile_candidate_store_backend_admission_owner(&right.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    assert(!left.reconciler.prepared_push_active);
}

static void test_prepare_uses_extended_no_progress_timeout(void) {
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint8_t              frame[32];
    uint32_t             now = 200000u;
    uint32_t             started_at;

    memset(&left_memory, 0, sizeof(left_memory));
    memset(&right_memory, 0, sizeof(right_memory));
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    boot_empty_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);
    stage_valid_candidate_pair(&left, &right, 150u, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 150u);
    send_and_scan_pair(&left, &right, frame, &now);
    for (uint32_t guard = 0u; guard < 64u && !left.host_barrier_started; guard++) {
        scan_pair(&left, &right, &now);
    }
    assert(left.host_barrier_started && left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER);
    started_at                      = left.host_last_activity_at;
    left.reconciler.next_attempt_at = started_at + NOAH_PROFILE_OWNER_HOST_BARRIER_NO_PROGRESS_MS + 1000u;

    (void)noah_profile_owner_scan(&left, true, started_at + NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER);
    assert(!left.host_cancel_pending);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_HOST);

    assert(noah_profile_owner_scan(&left, true, started_at + NOAH_PROFILE_OWNER_HOST_BARRIER_NO_PROGRESS_MS));
    now = started_at + NOAH_PROFILE_OWNER_HOST_BARRIER_NO_PROGRESS_MS;
    for (uint32_t guard = 0u; guard < 1024u && left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE; guard++) {
        scan_pair(&left, &right, &now);
    }
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
}

// The other half waiting to erase until its keyboard is quiet is not a stall
// (D-F32): its storage step does not run, it answers BUSY_STORAGE_PAUSED,
// and neither half's deadlines count the wait, however long the typing goes on.
static bool right_storage_paused;
static bool storage_paused_flag(void *context) {
    return *(bool *)context;
}

static void test_waiting_for_a_quiet_keyboard_never_ends_a_save(void) {
    static noah_profile_owner_t left;
    static noah_profile_owner_t right;
    static memory_t             left_memory;
    static memory_t             right_memory;
    split_link_t                left_link  = {.peer = &right};
    split_link_t                right_link = {.peer = &left};
    uint8_t                     frame[32];
    uint32_t                    now = 300000u;
    uint32_t                    right_now;

    memset(&left_memory, 0, sizeof(left_memory));
    memset(&right_memory, 0, sizeof(right_memory));
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    right_storage_paused = false;
    assert(noah_profile_owner_init(&left, &(noah_profile_owner_config_t){.store_io = {.read = memory_read, .write = memory_write, .context = &left_memory}, .split_exchange = split_exchange, .split_transport_context = &left_link, .origin_half = 0u, .peer_required = true}));
    assert(noah_profile_owner_init(&right, &(noah_profile_owner_config_t){.store_io = {.read = memory_read, .write = memory_write, .context = &right_memory}, .split_exchange = split_exchange, .split_transport_context = &right_link, .origin_half = 1u, .peer_required = true, .storage_paused = storage_paused_flag, .storage_paused_context = &right_storage_paused}));
    for (uint32_t guard = 0u; guard < 4096u && (left.state != NOAH_PROFILE_OWNER_READY_COMPILED || right.state != NOAH_PROFILE_OWNER_READY_COMPILED); guard++) {
        scan_pair(&left, &right, &now);
    }
    stage_valid_candidate_pair(&left, &right, 160u, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 160u);
    send_and_scan_pair(&left, &right, frame, &now);
    for (uint32_t guard = 0u; guard < 64u && !left.host_barrier_started; guard++) {
        scan_pair(&left, &right, &now);
    }
    assert(left.host_barrier_started && left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER);
    // The other half's storage stops for 90 s (twice its own lease, half
    // again the copy's no-progress window): its owner gets no scans and its
    // clock stands still; the USB half keeps asking.
    right_storage_paused = true;
    right_now            = now;
    for (uint32_t elapsed = 0u; elapsed < 90000u; elapsed += 50u) {
        (void)noah_profile_owner_scan(&left, true, now);
        now += 50u;
        assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER && !left.host_cancel_pending);
    }
    assert(left.reconciler.last_busy_reason == NOAH_PROFILE_SPLIT_V1_BUSY_STORAGE_PAUSED);
    // Quiet again: the copy finishes and both halves commit.
    right_storage_paused = false;
    for (uint32_t guard = 0u; guard < 8192u && (left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE || right.committed_descriptor.generation != 1u || left.state != NOAH_PROFILE_OWNER_READY_VALIDATED || right.state != NOAH_PROFILE_OWNER_READY_VALIDATED); guard++) {
        (void)noah_profile_owner_scan(&right, false, right_now);
        (void)noah_profile_owner_scan(&left, true, now);
        right_now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
    }
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE && left.host_transaction.last_committed_transaction_id == 160u);
    assert(left.committed_descriptor.generation == 1u && right.committed_descriptor.generation == 1u);
}

// A matching peer may cross the old 60-second whole-prepare duration while
// advancing from validation to durable preparation. Only a new phase extends
// the no-progress deadline; repeated BUSY never keeps a stalled copy alive.
static void test_prepare_phase_progress_extends_only_once(void) {
    noah_profile_owner_t left, right;
    memory_t left_memory = {0}, right_memory = {0};
    split_link_t left_link = {.peer = &right}, right_link = {.peer = &left};
    uint32_t now = 300000;
    uint8_t frame[32];
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    boot_empty_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);
    stage_valid_candidate_pair(&left, &right, 151u, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 151u);
    send_and_scan_pair(&left, &right, frame, &now);
    for (unsigned guard = 0; guard < 64 && !left.host_barrier_started; guard++) scan_pair(&left, &right, &now);
    assert(left.host_barrier_started);
    uint32_t started = left.host_last_activity_at;
    left.reconciler.state = NOAH_PROFILE_SPLIT_RECONCILER_PUSH_DURABLE;
    left.reconciler.transfer_offset = left.reconciler.transfer_descriptor.payload_length;
    left.host_barrier_progress_offset = left.reconciler.transfer_offset;
    left.reconciler.prepare_progress_phase = NOAH_PROFILE_PEER_STORE_VALIDATING;
    left.host_barrier_progress_phase = NOAH_PROFILE_PEER_STORE_VALIDATING;
    left_link.injected = (noah_profile_split_v1_frame_t){
        .kind = NOAH_PROFILE_SPLIT_V1_ACK, .status = NOAH_PROFILE_SPLIT_V1_STATUS_BUSY,
        .generation = left.reconciler.transfer_descriptor.generation,
        .payload_digest = left.reconciler.transfer_descriptor.payload_digest,
        .payload_length = left.reconciler.transfer_descriptor.payload_length,
        .offset = left.reconciler.transfer_descriptor.payload_length,
        .busy_reason = NOAH_PROFILE_SPLIT_V1_BUSY_STORE_WORKING,
        .busy_store_state = NOAH_PROFILE_PEER_STORE_PREPARING,
        .busy_owner = NOAH_PROFILE_SPLIT_TRANSFER_REMOTE_PUSH,
        .busy_admission = NOAH_PROFILE_STORAGE_ADMISSION_PEER,
    };
    now = started + 50000;
    left_link.inject_busy = true;
    left.reconciler.attempt_immediate = true;
    left.scheduler_cursor = 1; // owner split scheduler
    noah_profile_owner_scan(&left, true, now);
    assert(left.host_last_activity_at == now && left.host_barrier_progress_phase == NOAH_PROFILE_PEER_STORE_PREPARING);
    uint32_t progressed = now;
    now = started + 70000; // beyond the old whole-copy deadline
    left_link.inject_busy = true;
    left.reconciler.attempt_immediate = true;
    left.scheduler_cursor = 1;
    noah_profile_owner_scan(&left, true, now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER);
    assert(left.host_last_activity_at == progressed && !left.host_cancel_pending);
    left.reconciler.next_attempt_at = progressed + NOAH_PROFILE_OWNER_HOST_BARRIER_NO_PROGRESS_MS + 1000;
    noah_profile_owner_scan(&left, true, progressed + NOAH_PROFILE_OWNER_HOST_BARRIER_NO_PROGRESS_MS);
    assert(left.host_cancel_pending || left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT);
}

// The other half fails to store its copy (one flash write). Up to the split
// storage retries the copy is sent again and the Apply commits; past them the
// Apply ends at once with PEER_TRANSFER_FAILED, not after the no-progress
// timeout, and both halves stay ready for the next one.
static void run_peer_store_write_failures(uint8_t failures, uint16_t transaction_id) {
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint8_t              frame[32];
    uint32_t             now      = 300000u;
    uint32_t             started;
    uint8_t              armed    = 0u;
    bool                 in_phase = false;

    memset(&left_memory, 0, sizeof(left_memory));
    memset(&right_memory, 0, sizeof(right_memory));
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    boot_empty_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);
    stage_valid_candidate_pair(&left, &right, transaction_id, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, transaction_id);
    send_and_scan_pair(&left, &right, frame, &now);
    started = now;
    for (uint32_t guard = 0u; guard < 8192u && left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE; guard++) {
        bool preparing = noah_profile_peer_store_backend_state(&right.peer_store) == NOAH_PROFILE_PEER_STORE_PREPARING;
        if (preparing && !in_phase && armed < failures) {
            armed++;
            right_memory.fail_next_write = true;
        }
        in_phase = preparing;
        scan_pair(&left, &right, &now);
    }
    right_memory.fail_next_write = false;
    if (left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE) {
        noah_profile_split_reconciler_status_t split;
        (void)noah_profile_split_reconciler_status(&left.reconciler, &split);
        fprintf(stderr, "failures %u armed %u: host state %u error %u cancel %u | split state %u status %u retries %u active %u | peer store %u owner %u admission %u peer owner state %u\n", (unsigned)failures, (unsigned)armed, (unsigned)left.host_transaction.status.state, (unsigned)left.host_transaction.status.error.code, (unsigned)left.host_cancel_pending, (unsigned)split.state, (unsigned)split.last_status, (unsigned)left.reconciler.prepared_storage_retries, (unsigned)split.prepared_push_active, (unsigned)noah_profile_peer_store_backend_state(&right.peer_store), (unsigned)right.reconciler.transfer_owner, (unsigned)noah_profile_candidate_store_backend_admission_owner(&right.staging.candidate_backend), (unsigned)right.state);
    }
    assert(armed == failures);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(left.state == NOAH_PROFILE_OWNER_READY_COMPILED || left.state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    assert(right.state == NOAH_PROFILE_OWNER_READY_COMPILED || right.state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    if (failures <= NOAH_PROFILE_SPLIT_PREPARED_STORAGE_RETRIES) {
        assert(left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
        assert(left.committed_descriptor.generation == 1u && right.committed_descriptor.generation == 1u);
        return;
    }
    assert(left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_TRANSFER_FAILED);
    assert((uint32_t)(now - started) < NOAH_PROFILE_OWNER_HOST_BARRIER_NO_PROGRESS_MS / 4u);
    assert(!left.store.committed.generation && !right.store.committed.generation);
    assert(noah_profile_candidate_store_backend_admission_owner(&right.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    // Nothing was saved, and the next Apply goes through.
    stage_valid_candidate_pair(&left, &right, (uint16_t)(transaction_id + 1u), &now);
    commit_valid_candidate_pair(&left, &right, (uint16_t)(transaction_id + 1u), 1u, &now);
}

static void test_peer_store_write_failure_retries_then_fails_fast(void) {
    run_peer_store_write_failures(1u, 170u);
    run_peer_store_write_failures(NOAH_PROFILE_SPLIT_PREPARED_STORAGE_RETRIES + 1u, 180u);
}

static void test_unexpected_newer_postcommit_authority_never_activates(void) {
    noah_profile_owner_t            left;
    noah_profile_owner_t            right;
    noah_effective_profile_status_t provider;
    memory_t                        left_memory;
    memory_t                        right_memory;
    split_link_t                    left_link  = {.peer = &right};
    split_link_t                    right_link = {.peer = &left};
    noah_profile_split_descriptor_t newer;
    uint8_t                         frame[32];
    uint32_t                        now = 90000u;

    memset(&left_memory, 0, sizeof(left_memory));
    memset(&right_memory, 0, sizeof(right_memory));
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    boot_empty_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);
    stage_valid_candidate_pair(&left, &right, 110u, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 110u);
    send_and_scan_pair(&left, &right, frame, &now);
    for (uint32_t guard = 0u; guard < 2048u && left.store.committed.generation != 1u; guard++) {
        scan_pair(&left, &right, &now);
    }
    assert(left.store.committed.generation == 1u);
    assert(right.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
    for (uint32_t guard = 0u; guard < 32u && (!left.host_barrier_local_published || !left.host_barrier_peer_commit_authorized); guard++) {
        left.scheduler_cursor = 0u;
        assert(noah_profile_owner_scan(&left, true, now++));
    }
    assert(left.host_barrier_local_published && left.host_barrier_peer_commit_authorized);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_CONVERGING_PEER);

    newer = peer_descriptor(&left, 2u);
    assert(noah_profile_split_authority_publish(&left.reconciler.authority, left.committed_descriptor, newer, false));
    left.scheduler_cursor = 0u;
    assert(noah_profile_owner_scan(&left, true, now++));
    assert(left.state == NOAH_PROFILE_OWNER_POSTCOMMIT_AUTHORITY_LOST);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_AUTHORITY_FAILED);
    assert(left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_POSTCOMMIT_AUTHORITY_LOST);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_HOST);
    assert(noah_effective_profile_provider_status(&left.provider, &provider) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(provider.active.kind == NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS);
}

static noah_profile_split_descriptor_t peer_descriptor(const noah_profile_owner_t *owner, uint32_t generation) {
    return (noah_profile_split_descriptor_t){
        .generation              = generation,
        .payload_crc32           = crc_of(compiled_blob, sizeof(compiled_blob)),
        .payload_digest          = digest_of(compiled_blob, sizeof(compiled_blob)),
        .compiled_default_digest = owner->compiled.metadata.digest,
        .action_abi_digest       = owner->compiled.metadata.action_abi_digest,
        .payload_length          = sizeof(compiled_blob),
        .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
        .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
        .domain_mask             = owner->compiled.metadata.domain_mask,
        .profile_flags           = NOAH_PROFILE_STORE_FLAG_OVERRIDE,
        .origin_half             = 1u,
        .readable                = true,
        .has_profile             = true,
    };
}

static void publish_peer(noah_profile_owner_t *owner, uint32_t generation) {
    if (!owner->split_initialized) {
        noah_profile_split_authority_init(&owner->reconciler.authority);
        owner->split_initialized = true;
    }
    assert(noah_profile_split_authority_publish(&owner->reconciler.authority, owner->committed_descriptor, peer_descriptor(owner, generation), false));
}

static void test_empty_boot_live_commit_and_timeout(void) {
    noah_profile_owner_t owner;
    noah_profile_owner_t rebooted;
    memory_t             memory;
    uint8_t              frame[32];
    uint32_t             now = 100u;
    uint32_t             writes_after_commit;

    memset(&memory, 0, sizeof(memory));
    memset(memory.bytes, 0xff, sizeof(memory.bytes));
    compiled_reads = 0u;
    key_installed  = false;
    rgb_installed  = false;
    boot_empty(&owner, &memory);

    commit_blob(&owner, 41u, &now);
    assert(owner.committed_descriptor.has_profile && owner.committed_descriptor.generation == 1u);
    assert(memory.writes > 0u && memory.largest_write <= NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET);

    writes_after_commit = memory.writes;
    assert(noah_profile_owner_init(&rebooted, &(noah_profile_owner_config_t){
                                                  .store_io    = {.read = memory_read, .write = memory_write, .context = &memory},
                                                  .origin_half = 0u,
                                              }));
    for (uint32_t guard = 0u; guard < 512u && rebooted.state != NOAH_PROFILE_OWNER_READY_VALIDATED; guard++) {
        assert(noah_profile_owner_scan(&rebooted, true, now++));
        if (rebooted.state == NOAH_PROFILE_OWNER_ADOPTING_COMMITTED) {
            assert(!rebooted.descriptor_readable);
            assert(noah_profile_owner_committed(&rebooted) == NULL);
        }
    }
    assert(rebooted.state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    assert(noah_profile_owner_committed(&rebooted) != NULL);
    assert(rebooted.committed_descriptor.generation == 1u);
    assert(memory.writes == writes_after_commit);

    begin_frame(frame, 42u);
    send_and_scan(&rebooted, frame, now);
    assert(rebooted.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING);
    assert(noah_profile_owner_scan(&rebooted, true, now + NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS));
    assert(rebooted.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(rebooted.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT);
    assert(noah_profile_candidate_store_backend_admission_owner(&rebooted.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
}

static void test_boot_reconciles_different_generations_before_activation(void) {
    noah_profile_owner_t seeded_left;
    noah_profile_owner_t seeded_right;
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint32_t             now        = 1000u;
    uint32_t             left_writes_before_reconciliation;

    memset(&left_memory, 0, sizeof(left_memory));
    memset(&right_memory, 0, sizeof(right_memory));
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    boot_empty(&seeded_left, &left_memory);
    commit_blob(&seeded_left, 51u, &now);
    boot_empty(&seeded_right, &right_memory);
    commit_blob(&seeded_right, 61u, &now);
    commit_blob(&seeded_right, 62u, &now);
    assert(seeded_left.committed_descriptor.generation == 1u);
    assert(seeded_right.committed_descriptor.generation == 2u);
    left_writes_before_reconciliation = left_memory.writes;

    assert(noah_profile_owner_init(&left, &(noah_profile_owner_config_t){
                                              .store_io                = {.read = memory_read, .write = memory_write, .context = &left_memory},
                                              .split_exchange          = split_exchange,
                                              .split_transport_context = &left_link,
                                              .origin_half             = 0u,
                                              .peer_required           = true,
                                          }));
    assert(noah_profile_owner_init(&right, &(noah_profile_owner_config_t){
                                               .store_io                = {.read = memory_read, .write = memory_write, .context = &right_memory},
                                               .split_exchange          = split_exchange,
                                               .split_transport_context = &right_link,
                                               .origin_half             = 1u,
                                               .peer_required           = true,
                                           }));

    for (uint32_t guard = 0u; guard < 4096u && (left.state != NOAH_PROFILE_OWNER_READY_VALIDATED || right.state != NOAH_PROFILE_OWNER_READY_VALIDATED); guard++) {
        (void)noah_profile_owner_scan(&right, false, now);
        (void)noah_profile_owner_scan(&left, true, now);
        now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        assert(left.state != NOAH_PROFILE_OWNER_STORAGE_ERROR && left.state != NOAH_PROFILE_OWNER_INTEGRATION_ERROR);
        assert(right.state != NOAH_PROFILE_OWNER_STORAGE_ERROR && right.state != NOAH_PROFILE_OWNER_INTEGRATION_ERROR);
    }
    assert(left.state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    assert(right.state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    assert(left.committed_descriptor.generation == 2u);
    assert(right.committed_descriptor.generation == 2u);
    assert(left.committed_descriptor.payload_digest == right.committed_descriptor.payload_digest);
    assert(left_memory.writes > left_writes_before_reconciliation);
}

static void test_peer_authority_supersedes_only_precommit_host_generation(void) {
    noah_profile_owner_t        owner;
    noah_profile_owner_status_t status;
    memory_t                    memory;
    uint8_t                     frame[32];
    uint32_t                    now = 5000u;
    uint32_t                    writes_before_commit;
    uint16_t                    operation_sequence;

    memset(&memory, 0, sizeof(memory));
    memset(memory.bytes, 0xff, sizeof(memory.bytes));
    boot_empty(&owner, &memory);
    assert(noah_profile_owner_status(&owner, &status));
    assert(status.provider_known && status.active.kind == NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS);
    assert(status.compiled_default_digest == digest_of(compiled_blob, sizeof(compiled_blob)));
    assert(status.action_abi_digest == UINT32_C(0x12345678));
    assert(!status.has_committed && !status.candidate_pending);

    commit_blob(&owner, 70u, &now);
    assert(owner.committed_descriptor.generation == 1u);

    begin_frame(frame, 71u);
    send_and_scan(&owner, frame, now++);
    assert(owner.store.candidate.generation == 2u);
    publish_peer(&owner, 1u);
    (void)noah_profile_owner_scan(&owner, true, now++);
    assert(owner.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING);

    chunk_frame(frame, 71u);
    send_and_scan(&owner, frame, now++);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, 71u);
    send_and_scan(&owner, frame, now++);
    while (owner.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING)
        assert(noah_profile_owner_scan(&owner, true, now++));
    assert(owner.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);
    writes_before_commit = memory.writes;
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 71u);
    assert(noah_profile_owner_receive(&owner, frame, sizeof(frame)));
    publish_peer(&owner, 2u);
    assert(noah_profile_owner_scan(&owner, true, now++));
    // Supersession invalidates only the inactive prepared slot's marker; it
    // never starts marker-last commit or changes the prior durable record.
    assert(memory.writes == writes_before_commit + 1u);
    assert(owner.store.committed.generation == 1u);
    assert(noah_profile_candidate_store_backend_admission_owner(&owner.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    assert(owner.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(owner.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_SUPERSEDED);
    assert(owner.host_transaction.status.transaction_id == 71u);
    assert(owner.host_transaction.status.digest == digest_of(compiled_blob, sizeof(compiled_blob)));
    assert(owner.host_transaction.status.last_operation == NOAH_PROFILE_CANDIDATE_V1_OPERATION_COMMIT);

    assert(noah_profile_owner_status(&owner, &status));
    assert(status.has_committed && status.committed.generation == 1u);
    assert(status.peer_known && status.peer.generation == 2u);
    assert(status.candidate.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_SUPERSEDED);
    assert(!status.candidate_pending);

    begin_frame(frame, 72u);
    assert(noah_profile_owner_receive(&owner, frame, sizeof(frame)));
    assert(noah_profile_owner_scan(&owner, true, now++));
    assert(owner.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING);
    operation_sequence = owner.host_transaction.status.operation_sequence;
    publish_peer(&owner, 3u);
    assert(noah_profile_owner_scan(&owner, true, now++));
    assert(owner.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(owner.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_SUPERSEDED);
    assert(owner.host_transaction.status.operation_sequence == (uint16_t)(operation_sequence + 1u));
}

static void test_partial_runtime_install_rolls_back(void) {
    noah_profile_owner_t owner;
    memory_t             memory;

    memset(&memory, 0, sizeof(memory));
    memset(memory.bytes, 0xff, sizeof(memory.bytes));
    key_installed       = false;
    rgb_installed       = false;
    rgb_install_allowed = false;
    assert(noah_profile_owner_init(&owner, &(noah_profile_owner_config_t){
                                               .store_io    = {.read = memory_read, .write = memory_write, .context = &memory},
                                               .origin_half = 0u,
                                           }));
    for (uint32_t guard = 0u; guard < 32u && owner.state == NOAH_PROFILE_OWNER_VALIDATING_COMPILED; guard++) {
        assert(noah_profile_owner_scan(&owner, true, guard));
    }
    assert(owner.state == NOAH_PROFILE_OWNER_INTEGRATION_ERROR);
    assert(!key_installed && !rgb_installed);
    rgb_install_allowed = true;
}

static void test_failed_factory_rgb_cache_refuses_graph_install(void) {
    noah_profile_owner_t owner;
    memory_t memory;
    memset(&memory, 0, sizeof(memory));
    memset(memory.bytes, 0xff, sizeof(memory.bytes));
    rgb_cache_valid = false;
    assert(noah_profile_owner_init(&owner, &(noah_profile_owner_config_t){.store_io = {.read = memory_read, .write = memory_write, .context = &memory}}));
    for (uint32_t guard = 0; guard < 32 && owner.state == NOAH_PROFILE_OWNER_VALIDATING_COMPILED; guard++) noah_profile_owner_scan(&owner, true, guard);
    assert(owner.state == NOAH_PROFILE_OWNER_INTEGRATION_ERROR);
    assert(!key_installed && !rgb_installed);
    rgb_cache_valid = true;
}

static void test_clean_idle_scans_do_not_repeat_host_session_cleanup(void) {
    noah_profile_owner_t owner;
    memory_t             memory;
    uint32_t             refreshes;

    memset(&memory, 0, sizeof(memory));
    memset(memory.bytes, 0xff, sizeof(memory.bytes));
    boot_empty(&owner, &memory);
    refreshes = noah_profile_owner_test_ready_refresh_count();

    for (uint32_t scan = 0u; scan < 4096u; scan++) {
        assert(!noah_profile_owner_scan(&owner, true, 500u));
    }
    assert(noah_profile_owner_test_ready_refresh_count() == refreshes);

    owner.host_activity_known = true;
    assert(!noah_profile_owner_scan(&owner, true, 500u));
    assert(noah_profile_owner_test_ready_refresh_count() == refreshes + 1u);
    for (uint32_t scan = 0u; scan < 4096u; scan++) {
        assert(!noah_profile_owner_scan(&owner, true, 500u));
    }
    assert(noah_profile_owner_test_ready_refresh_count() == refreshes + 1u);
}

// One store (D-F31): a validated copy waits on the other half before
// anything is durable, the commit decision waits for a safe boundary, and
// once both halves hold the commit the new profile activates.
static void test_commit_waits_for_a_safe_boundary_then_activates(void) {
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint8_t              frame[32];
    uint32_t             now = 250000u;

    memset(&left_memory, 0, sizeof(left_memory));
    memset(&right_memory, 0, sizeof(right_memory));
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    activation_block_reason = 0u;
    boot_empty_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);

    begin_frame(frame, 201u);
    send_and_scan_pair(&left, &right, frame, &now);
    chunk_frame(frame, 201u);
    send_and_scan_pair(&left, &right, frame, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, 201u);
    send_and_scan_pair(&left, &right, frame, &now);
    for (uint32_t guard = 0u; guard < 64u && left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED; guard++)
        scan_pair(&left, &right, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);

    activation_block_reason = NOAH_PROFILE_ACTIVATION_REASON_PHYSICAL_PRESS;
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 201u);
    send_and_scan_pair(&left, &right, frame, &now);
    for (uint32_t guard = 0u; guard < 1024u && !noah_profile_split_reconciler_prepared_push_ready(&left.reconciler, NULL); guard++)
        scan_pair(&left, &right, &now);
    assert(noah_profile_split_reconciler_prepared_push_ready(&left.reconciler, NULL));
    for (uint32_t guard = 0u; guard < 10u; guard++)
        scan_pair(&left, &right, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER);
    assert(left.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
    {
        noah_profile_candidate_v1_peer_status_t peer;

        assert(noah_profile_owner_peer_transfer(&left, &peer));
        assert(peer.flags & NOAH_PROFILE_CANDIDATE_V1_PEER_FLAG_WAITING_SAFE_BOUNDARY);
        // The phase that bound a copy to the VIA store is never reported.
        assert(peer.phase != NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_BINDING);
    }
    activation_block_reason = 0u;
    for (uint32_t guard = 0u; guard < 4096u && left.store.committed.slot == NOAH_PROFILE_SLOT_NONE; guard++)
        scan_pair(&left, &right, &now);
    assert(left.store.committed.slot != NOAH_PROFILE_SLOT_NONE);
    assert(right.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
    assert(right.store.prepared_durable);
    assert(right.store.candidate.format_version == NOAH_PROFILE_STORE_FORMAT_VERSION);
    assert(left.state != NOAH_PROFILE_OWNER_READY_VALIDATED);

    for (uint32_t guard = 0u; guard < 4096u && (left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE || left.state != NOAH_PROFILE_OWNER_READY_VALIDATED); guard++)
        scan_pair(&left, &right, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(left.state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    assert(left.store.committed.format_version == NOAH_PROFILE_STORE_FORMAT_VERSION);
    assert(right.store.committed.format_version == NOAH_PROFILE_STORE_FORMAT_VERSION);
    assert(right.store.committed.generation == left.store.committed.generation);
}

static void init_right_half(noah_profile_owner_t *right, memory_t *right_memory, split_link_t *right_link) {
    assert(noah_profile_owner_init(right, &(noah_profile_owner_config_t){
                                              .store_io                = {.read = memory_read, .write = memory_write, .context = right_memory},
                                              .split_exchange          = split_exchange,
                                              .split_transport_context = right_link,
                                              .origin_half             = 1u,
                                              .peer_required           = true,
                                          }));
}

static uint32_t active_generation(const noah_profile_owner_t *owner) {
    noah_effective_profile_status_t status;

    assert(noah_effective_profile_provider_status(&owner->provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    return status.active.generation;
}

// Runs an Apply to the USB half's durable commit marker and stops there: the
// peer holds only its prepared copy and the old profile is still active.
static void decide_commit(noah_profile_owner_t *left, memory_t *left_memory, noah_profile_owner_t *right, memory_t *right_memory, split_link_t *left_link, split_link_t *right_link, uint16_t transaction_id, uint32_t *now) {
    uint8_t frame[32];

    memset(left_memory, 0, sizeof(*left_memory));
    memset(right_memory, 0, sizeof(*right_memory));
    memset(left_memory->bytes, 0xff, sizeof(left_memory->bytes));
    memset(right_memory->bytes, 0xff, sizeof(right_memory->bytes));
    activation_block_reason = 0u;
    boot_empty_pair(left, left_memory, right, right_memory, left_link, right_link, now);

    begin_frame(frame, transaction_id);
    send_and_scan_pair(left, right, frame, now);
    chunk_frame(frame, transaction_id);
    send_and_scan_pair(left, right, frame, now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, transaction_id);
    send_and_scan_pair(left, right, frame, now);
    for (uint32_t guard = 0u; guard < 64u && left->host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED; guard++)
        scan_pair(left, right, now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, transaction_id);
    send_and_scan_pair(left, right, frame, now);
    for (uint32_t guard = 0u; guard < 4096u && left->store.committed.slot == NOAH_PROFILE_SLOT_NONE; guard++)
        scan_pair(left, right, now);
    assert(left->store.committed.slot != NOAH_PROFILE_SLOT_NONE);
    assert(right->store.committed.slot == NOAH_PROFILE_SLOT_NONE && right->store.prepared_durable);
    assert(active_generation(left) != left->store.committed.generation);
}

static void finish_commit(noah_profile_owner_t *left, noah_profile_owner_t *right, uint32_t *now) {
    for (uint32_t guard = 0u; guard < 8192u && (left->host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE || left->state != NOAH_PROFILE_OWNER_READY_VALIDATED); guard++)
        scan_pair(left, right, now);
    assert(left->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(left->state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    assert(right->store.committed.format_version == NOAH_PROFILE_STORE_FORMAT_VERSION);
    assert(right->store.committed.generation == left->store.committed.generation);
    assert(active_generation(left) == left->store.committed.generation);
}

static void test_lost_link_after_decision_keeps_typing_and_resumes(void) {
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint32_t             now        = 300000u;

    decide_commit(&left, &left_memory, &right, &right_memory, &left_link, &right_link, 211u, &now);

    // The link drops for well beyond every host and barrier timeout. The old
    // profile stays active on the USB half, which keeps typing it, never
    // activates a commit the peer has not been seen holding, and never fails
    // for good.
    left_link.peer  = NULL;
    right_link.peer = NULL;
    for (uint32_t guard = 0u; guard < 4096u; guard++) {
        scan_pair(&left, &right, &now);
        assert(active_generation(&left) != left.store.committed.generation);
        assert(left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_AUTHORITY_FAILED);
        assert(left.state != NOAH_PROFILE_OWNER_STORAGE_ERROR && left.state != NOAH_PROFILE_OWNER_INTEGRATION_ERROR);
    }

    left_link.peer  = &right;
    right_link.peer = &left;
    finish_commit(&left, &right, &now);
}

static void test_peer_rebooted_while_unlinked_after_decision_resumes(void) {
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint32_t             now        = 400000u;

    decide_commit(&left, &left_memory, &right, &right_memory, &left_link, &right_link, 212u, &now);

    // Unplugging the link cable also powers the peer off: it boots again from
    // its durable prepared copy when the cable returns.
    left_link.peer  = NULL;
    right_link.peer = NULL;
    for (uint32_t guard = 0u; guard < 256u; guard++) {
        scan_pair(&left, &right, &now);
        assert(active_generation(&left) != left.store.committed.generation);
    }
    init_right_half(&right, &right_memory, &right_link);
    left_link.peer  = &right;
    right_link.peer = &left;
    finish_commit(&left, &right, &now);
}

static void test_activation_waits_for_a_safe_boundary_while_typing(void) {
    noah_profile_owner_t        left;
    noah_profile_owner_t        right;
    memory_t                    left_memory;
    memory_t                    right_memory;
    split_link_t                left_link  = {.peer = &right};
    split_link_t                right_link = {.peer = &left};
    noah_profile_owner_status_t status;
    uint32_t                    now        = 500000u;

    decide_commit(&left, &left_memory, &right, &right_memory, &left_link, &right_link, 213u, &now);

    // A layer locked after the decision holds activation back. Both halves
    // commit meanwhile, and the old profile keeps typing until it is released.
    activation_block_reason = NOAH_PROFILE_ACTIVATION_REASON_PERSISTENT_INTENT;
    for (uint32_t guard = 0u; guard < 4096u; guard++)
        scan_pair(&left, &right, &now);
    assert(right.store.committed.generation == left.store.committed.generation);
    assert(active_generation(&left) != left.store.committed.generation);
    assert(left.state != NOAH_PROFILE_OWNER_READY_VALIDATED || left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(noah_profile_owner_status(&left, &status));
    assert(status.has_committed && status.committed.generation == left.store.committed.generation);

    activation_block_reason = 0u;
    finish_commit(&left, &right, &now);
}

// D-F25: the half on USB may restart it to take a new keyboard name only
// once a save has fully settled on both halves and nothing is held.
static void test_usb_restart_waits_for_a_settled_save(void) {
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint32_t             now        = 600000u;

    assert(!noah_profile_owner_usb_restart_safe(NULL));
    decide_commit(&left, &left_memory, &right, &right_memory, &left_link, &right_link, 214u, &now);
    // Decided but not yet on the other half or activated.
    assert(!noah_profile_owner_usb_restart_safe(&left));
    finish_commit(&left, &right, &now);
    for (uint32_t guard = 0u; guard < 256u && !noah_profile_owner_usb_restart_safe(&left); guard++)
        scan_pair(&left, &right, &now);
    assert(noah_profile_owner_usb_restart_safe(&left));
    // Anything held or running on either half holds it back.
    activation_block_reason = NOAH_PROFILE_ACTIVATION_REASON_PHYSICAL_PRESS;
    assert(!noah_profile_owner_usb_restart_safe(&left));
    activation_block_reason = NOAH_PROFILE_ACTIVATION_REASON_MACRO;
    assert(!noah_profile_owner_usb_restart_safe(&left));
    activation_block_reason = 0u;
    assert(noah_profile_owner_usb_restart_safe(&left));
    // A new save in flight holds it back again.
    {
        uint8_t frame[32];

        begin_frame(frame, 215u);
        send_and_scan_pair(&left, &right, frame, &now);
        assert(!noah_profile_owner_usb_restart_safe(&left));
    }
}

static bool peer_in_sight(const noah_profile_owner_t *owner) {
    noah_profile_split_authority_status_t authority;

    return noah_profile_split_authority_status(&owner->reconciler.authority, &authority) && authority.state == NOAH_PROFILE_SPLIT_AUTHORITY_COMMITTED_CONVERGED;
}

static void test_link_lost_after_the_peer_commit_was_seen_still_activates(void) {
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint32_t             now        = 700000u;

    decide_commit(&left, &left_memory, &right, &right_memory, &left_link, &right_link, 215u, &now);
    // Hold activation until the peer's commit has been seen, then lose the
    // link before the boundary clears. The Apply is confirmed, so this half
    // activates without seeing the peer again.
    activation_block_reason = NOAH_PROFILE_ACTIVATION_REASON_PERSISTENT_INTENT;
    for (uint32_t guard = 0u; guard < 4096u && !left.host_barrier_peer_confirmed; guard++)
        scan_pair(&left, &right, &now);
    assert(left.host_barrier_peer_confirmed);
    assert(right.store.committed.generation == left.store.committed.generation);
    left_link.peer  = NULL;
    right_link.peer = NULL;
    for (uint32_t guard = 0u; guard < 64u && peer_in_sight(&left); guard++)
        scan_pair(&left, &right, &now);
    assert(!peer_in_sight(&left));
    assert(active_generation(&left) != left.store.committed.generation);
    activation_block_reason = 0u;
    for (uint32_t guard = 0u; guard < 1024u && (left.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE || left.state != NOAH_PROFILE_OWNER_READY_VALIDATED); guard++)
        scan_pair(&left, &right, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(left.state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    assert(active_generation(&left) == left.store.committed.generation);

    // Reconnecting finds both halves on the same record: nothing to repair.
    left_link.peer  = &right;
    right_link.peer = &left;
    for (uint32_t guard = 0u; guard < 1024u; guard++) {
        scan_pair(&left, &right, &now);
        assert(left.state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    }
    assert(left.committed_descriptor.generation == right.committed_descriptor.generation);
}

// Boots an empty pair and validates candidate 201 on the USB half.
static void validate_candidate(noah_profile_owner_t *left, memory_t *left_memory, noah_profile_owner_t *right, memory_t *right_memory, split_link_t *left_link, split_link_t *right_link, uint32_t *now) {
    uint8_t frame[32];

    memset(left_memory, 0, sizeof(*left_memory));
    memset(right_memory, 0, sizeof(*right_memory));
    memset(left_memory->bytes, 0xff, sizeof(left_memory->bytes));
    memset(right_memory->bytes, 0xff, sizeof(right_memory->bytes));
    activation_block_reason = 0u;
    boot_empty_pair(left, left_memory, right, right_memory, left_link, right_link, now);
    begin_frame(frame, 201u);
    send_and_scan_pair(left, right, frame, now);
    chunk_frame(frame, 201u);
    send_and_scan_pair(left, right, frame, now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, 201u);
    send_and_scan_pair(left, right, frame, now);
    for (uint32_t guard = 0u; guard < 64u && left->host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED; guard++)
        scan_pair(left, right, now);
    assert(left->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);
}

// Whatever ends a candidate before the decision releases it: the host's
// lease, its own cancel (repeated as if the acknowledgement was lost), or a
// cancel while it is still being received.
static void test_every_precommit_cancel_releases_the_candidate(void) {
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint8_t              frame[32];
    uint32_t             now = 250000u;

    // The host went away: the lease expires.
    validate_candidate(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);
    now = left.host_last_activity_at + NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS;
    for (uint32_t guard = 0u; guard < 8u; guard++)
        scan_pair(&left, &right, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);

    // The host cancels its own candidate before COMMIT, twice.
    validate_candidate(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, 201u);
    send_and_scan_pair(&left, &right, frame, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, 201u);
    send_and_scan_pair(&left, &right, frame, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
    assert(left.state == NOAH_PROFILE_OWNER_READY_COMPILED);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);

    // Still receiving.
    begin_frame(frame, 202u);
    send_and_scan_pair(&left, &right, frame, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, 202u);
    send_and_scan_pair(&left, &right, frame, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);

    // A BEGIN still carrying the retired VIA binding in its tail is refused
    // and acquires no candidate.
    begin_frame(frame, 203u);
    write_u32(&frame[24], 6u);
    write_u32(&frame[28], UINT32_C(0xabcdef01));
    assert(!noah_profile_owner_receive(&left, frame, sizeof(frame)) || !left.host_transaction.mailbox.pending);
    for (uint32_t guard = 0u; guard < 8u; guard++)
        scan_pair(&left, &right, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(!left.host_transaction.has_candidate);
}

// Once the USB half's commit marker is durable the peer holds the target
// recovery copy. Neither a host cancel nor an expired lease may end it, even
// when the host never observed the decision.
static void test_decided_commit_is_never_cancelled(void) {
    noah_profile_owner_t left;
    noah_profile_owner_t right;
    memory_t             left_memory;
    memory_t             right_memory;
    split_link_t         left_link  = {.peer = &right};
    split_link_t         right_link = {.peer = &left};
    uint8_t              frame[32];
    uint32_t             now = 250000u;

    decide_commit(&left, &left_memory, &right, &right_memory, &left_link, &right_link, 205u, &now);
    // Hold activation so the abort meets the decided transaction.
    activation_block_reason = NOAH_PROFILE_ACTIVATION_REASON_PERSISTENT_INTENT;
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, 205u);
    send_and_scan_pair(&left, &right, frame, &now);
    assert(left.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_CONVERGING_PEER || left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING);
    now += NOAH_PROFILE_OWNER_HOST_BARRIER_NO_PROGRESS_MS;
    scan_pair(&left, &right, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_CONVERGING_PEER || left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING);
    activation_block_reason = 0u;
    finish_commit(&left, &right, &now);
}

// ── Live edits and Save (D-F32) ─────────────────────────────────────────────

typedef struct {
    noah_profile_owner_t *peer;
    uint32_t              exchanges;
} working_link_t;

static uint8_t left_working[NOAH_PROFILE_PAYLOAD_MAX];
static uint8_t right_working[NOAH_PROFILE_PAYLOAD_MAX];
enum { KEYMAP_LENGTH = 16u * 64u * 2u };

static bool working_exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    working_link_t *link = context;

    link->exchanges++;
    return link->peer && noah_profile_owner_working_receive(link->peer, request, request_length, response, response_length);
}

static void erased_memory(memory_t *memory) {
    memset(memory, 0, sizeof(*memory));
    memset(memory->bytes, 0xFF, sizeof(memory->bytes));
}

static void boot_live_pair(noah_profile_owner_t *left, memory_t *left_memory, noah_profile_owner_t *right, memory_t *right_memory, split_link_t *left_link, split_link_t *right_link, working_link_t *left_working_link, working_link_t *right_working_link, uint32_t session, uint32_t *now) {
    assert(noah_profile_owner_init(left, &(noah_profile_owner_config_t){
                                             .store_io                  = {.read = memory_read, .write = memory_write, .context = left_memory},
                                             .split_exchange            = split_exchange,
                                             .split_transport_context   = left_link,
                                             .origin_half               = 0u,
                                             .peer_required             = true,
                                             .working_bytes             = left_working,
                                             .working_capacity          = sizeof(left_working),
                                             .working_session           = session,
                                             .working_exchange          = working_exchange,
                                             .working_transport_context = left_working_link,
                                         }));
    assert(noah_profile_owner_init(right, &(noah_profile_owner_config_t){
                                              .store_io                  = {.read = memory_read, .write = memory_write, .context = right_memory},
                                              .split_exchange            = split_exchange,
                                              .split_transport_context   = right_link,
                                              .origin_half               = 1u,
                                              .peer_required             = true,
                                              .working_bytes             = right_working,
                                              .working_capacity          = sizeof(right_working),
                                              .working_session           = session ^ 0xFFFFu,
                                              .working_exchange          = working_exchange,
                                              .working_transport_context = right_working_link,
                                          }));
    for (uint32_t guard = 0u; guard < 8192u && (left->state < NOAH_PROFILE_OWNER_READY_COMPILED || left->state > NOAH_PROFILE_OWNER_READY_VALIDATED || right->state < NOAH_PROFILE_OWNER_READY_COMPILED || right->state > NOAH_PROFILE_OWNER_READY_VALIDATED); guard++) {
        scan_pair(left, right, now);
    }
    assert(left->working_ready && right->working_ready);
}

static bool both_halves_converged(noah_profile_owner_t *left) {
    noah_profile_owner_working_status_t status;

    return noah_profile_owner_working_status(left, &status) && status.peer_converged && !status.edits_blocked;
}

static void scan_until_converged(noah_profile_owner_t *left, noah_profile_owner_t *right, uint32_t *now) {
    for (uint32_t guard = 0u; guard < 20000u && !both_halves_converged(left); guard++) {
        scan_pair(left, right, now);
    }
    assert(both_halves_converged(left));
}

static noah_profile_working_result_t live_edit(noah_profile_owner_t *left, noah_profile_owner_t *right, uint16_t transaction, const noah_profile_working_splice_t *splice, const uint8_t *bytes, uint32_t length, uint32_t *now) {
    noah_profile_working_result_t result = noah_profile_owner_edit_begin(left, transaction, left->working.session, left->working.revision, 1u, length, 0u);

    if (result != NOAH_PROFILE_WORKING_OK) return result;
    assert(noah_profile_owner_edit_splice(left, transaction, 0u, splice) == NOAH_PROFILE_WORKING_OK);
    for (uint32_t offset = 0u; offset < length; offset += 22u) {
        uint32_t count = length - offset < 22u ? length - offset : 22u;
        assert(noah_profile_owner_edit_data(left, transaction, offset, bytes + offset, count) == NOAH_PROFILE_WORKING_OK);
    }
    assert(noah_profile_owner_edit_commit(left, transaction, crc_of(bytes, length)) == NOAH_PROFILE_WORKING_QUEUED);
    for (uint32_t guard = 0u; guard < 4096u && noah_profile_working_copy_busy(&left->working); guard++) {
        scan_pair(left, right, now);
    }
    return (noah_profile_working_result_t)left->working.last_result;
}

// Reads the active profile 20 bytes at a time, as the store's I/O allows.
static void read_active(noah_profile_owner_t *owner, uint8_t *bytes, uint32_t length) {
    for (uint32_t offset = 0u; offset < length; offset += 20u) {
        uint32_t count = length - offset < 20u ? length - offset : 20u;
        assert(noah_profile_owner_working_read(owner, offset, bytes + offset, count));
    }
}

static void assert_same_active(noah_profile_owner_t *left, noah_profile_owner_t *right, const uint8_t *expected, uint32_t length) {
    static uint8_t                      bytes[NOAH_PROFILE_PAYLOAD_MAX];
    noah_profile_owner_working_status_t status;

    assert(noah_profile_owner_working_status(left, &status) && status.local.length == length);
    read_active(left, bytes, length);
    assert(memcmp(bytes, expected, length) == 0);
    assert(noah_profile_owner_working_status(right, &status) && status.local.length == length);
    read_active(right, bytes, length);
    assert(memcmp(bytes, expected, length) == 0);
}

static void test_live_edit_reaches_both_halves_and_save_writes_both(void) {
    static noah_profile_owner_t left;
    static noah_profile_owner_t right;
    static memory_t             left_memory;
    static memory_t             right_memory;
    static uint8_t              keymap[KEYMAP_LENGTH];
    static uint8_t              expected[NOAH_PROFILE_PAYLOAD_MAX];
    split_link_t                left_link          = {.peer = &right};
    split_link_t                right_link         = {.peer = &left};
    working_link_t              left_working_link  = {.peer = &right};
    working_link_t              right_working_link = {.peer = &left};
    noah_profile_working_splice_t create = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .insert = KEYMAP_LENGTH, .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};
    noah_profile_working_splice_t key    = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = 130u, .remove = 2u, .insert = 2u};
    const uint8_t                 kc_b[2] = {0x05u, 0x00u};
    noah_profile_candidate_v1_peer_status_t transfer;
    noah_profile_owner_working_status_t     status;
    noah_effective_profile_status_t         provider;
    uint32_t                                now    = 1000u;
    uint32_t                                length = sizeof(compiled_blob) + NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE + KEYMAP_LENGTH;

    erased_memory(&left_memory);
    erased_memory(&right_memory);
    boot_live_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &left_working_link, &right_working_link, 0xA1B2C3D4u, &now);
    scan_until_converged(&left, &right, &now);
    assert(right.working.session == 0xA1B2C3D4u);

    // A first edit creates the keymap domain: in RAM on both halves, nothing
    // written to flash on either.
    for (uint32_t i = 0u; i < KEYMAP_LENGTH; i += 2u) keymap[i] = (uint8_t)(4u + i % 20u);
    {
        uint32_t left_writes = left_memory.writes, right_writes = right_memory.writes;
        assert(live_edit(&left, &right, 1u, &create, keymap, KEYMAP_LENGTH, &now) == NOAH_PROFILE_WORKING_OK);
        scan_until_converged(&left, &right, &now);
        assert(left_memory.writes == left_writes && right_memory.writes == right_writes);
    }
    memcpy(expected, compiled_blob, sizeof(compiled_blob));
    expected[6] = 1u;
    expected[8] = NOAH_PROFILE_DOMAIN_V1_KEYMAP;
    expected[9] = NOAH_PROFILE_DOMAIN_VERSION_KEYMAP;
    write_u32(&expected[10], KEYMAP_LENGTH);
    memcpy(&expected[14], keymap, KEYMAP_LENGTH);
    assert_same_active(&left, &right, expected, length);
    assert(left.working.active && right.working.active && right.working.revision == left.working.revision);
    // The keyboard reads keys from the working copy.
    {
        uint8_t cell[2];
        assert(noah_effective_keymap_is_live() && noah_effective_keymap_read(130u, cell) && cell[0] == keymap[130]);
    }
    // A stale edit is refused and says where the working copy is.
    assert(noah_profile_owner_edit_begin(&left, 2u, left.working.session, left.working.revision - 1u, 1u, 2u, 0u) == NOAH_PROFILE_WORKING_STALE);

    // Save: the ordinary candidate path, every byte REUSE from the working
    // copy, so the other half copies its own working copy too.
    assert(noah_profile_owner_save(&left, 40u, left.working.session, left.working.revision - 1u) == NOAH_PROFILE_WORKING_STALE);
    assert(noah_profile_owner_save(&left, 40u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
    assert(noah_profile_owner_edit_begin(&left, 41u, left.working.session, left.working.revision, 1u, 2u, 0u) == NOAH_PROFILE_WORKING_BUSY);
    {
        uint32_t revision = left.working.revision;
        for (uint32_t guard = 0u; guard < 20000u && left.save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE; guard++) scan_pair(&left, &right, &now);
        assert(left.save.phase == NOAH_PROFILE_OWNER_SAVE_IDLE && left.save.last_result == NOAH_PROFILE_WORKING_OK && left.save.last_transaction_id == 40u);
        assert(left.host_transaction.last_committed_transaction_id == 40u);
        assert(left.committed_descriptor.generation == 1u && right.committed_descriptor.generation == 1u);
        assert(left.committed_descriptor.payload_length == length && left.committed_descriptor.payload_digest == digest_of(expected, length));
        assert(noah_profile_owner_peer_transfer(&left, &transfer) && transfer.reused_bytes == length);
        scan_until_converged(&left, &right, &now);
        // Same content: no unsaved edits, and the revision did not move.
        assert(!left.working.active && !right.working.active && left.working.revision == revision && right.working.revision == revision);
        assert(noah_effective_profile_provider_status(&left.provider, &provider) == NOAH_EFFECTIVE_PROFILE_OK && provider.active.kind == NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE && provider.active.generation == 1u);
    }
    assert_same_active(&left, &right, expected, length);
    // Nothing unsaved: Save answers at once and writes nothing.
    {
        uint32_t writes = left_memory.writes;
        assert(noah_profile_owner_save(&left, 42u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_OK && left_memory.writes == writes);
    }

    // An unsaved edit, then a replug: both halves boot the saved profile.
    assert(live_edit(&left, &right, 43u, &key, kc_b, sizeof(kc_b), &now) == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&left, &right, &now);
    assert(noah_profile_owner_working_status(&left, &status) && status.local.active);
    left_working_link.exchanges = 0u;
    boot_live_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &left_working_link, &right_working_link, 0x0BADF00Du, &now);
    scan_until_converged(&left, &right, &now);
    assert(!left.working.active && !right.working.active && left.working.session == 0x0BADF00Du && right.working.session == 0x0BADF00Du);
    assert_same_active(&left, &right, expected, length);

    // Revert on the USB half: the other half follows; flash untouched.
    assert(live_edit(&left, &right, 44u, &key, kc_b, sizeof(kc_b), &now) == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&left, &right, &now);
    {
        uint32_t writes = left_memory.writes + right_memory.writes;
        assert(noah_profile_owner_revert(&left, 45u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
        scan_until_converged(&left, &right, &now);
        assert(left_memory.writes + right_memory.writes == writes && !left.working.active && !right.working.active);
    }
    assert_same_active(&left, &right, expected, length);

    // The edit mode is stored outside the profile and refused with unsaved edits.
    assert(noah_profile_owner_set_edit_mode(&left, NOAH_PROFILE_EDIT_MODE_APPLY) == NOAH_PROFILE_WORKING_READ_ERROR); // no storage in this harness
    assert(live_edit(&left, &right, 46u, &key, kc_b, sizeof(kc_b), &now) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_owner_set_edit_mode(&left, NOAH_PROFILE_EDIT_MODE_APPLY) == NOAH_PROFILE_WORKING_NOT_CLEAN);
    puts("a live edit reaches both halves without flash; Save writes both by reuse; a replug boots the saved profile; Revert follows on both");
}


// One durable-I/O grant as the scheduler gives it (qmk_durable_io.c, D-F29
// "Pacing"): a step, then more of the subsystem whose step worked (the owner
// visits it alone) while the half has storage work in progress,
// until a step finds nothing to do, uses the split link or writes a page's
// worth (the flash store programs at most one page per grant), or 32 steps.
static unsigned grant(noah_profile_owner_t *owner, bool master, uint32_t now, const memory_t *memory, const split_link_t *link, const working_link_t *working_link, bool repeat) {
    unsigned steps = 0u;
    uint32_t writes = memory->writes;

    do {
        uint32_t exchanges = link->exchanges + working_link->exchanges;

        noah_profile_owner_scan_again(owner, steps != 0u);
        steps++;
        if (!noah_profile_owner_scan(owner, master, now) || !repeat || link->exchanges + working_link->exchanges != exchanges) break;
    } while (steps < 32u && noah_profile_owner_storage_busy(owner) && memory->writes - writes < NOAH_PROFILE_STORAGE_PAGE_SIZE / NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX);
    return steps;
}

// A save took seconds of scans on the keyboard though its flash took 0.1 s
// (keyboard check, 2026-10-19): one small step per scan. With local steps
// repeated within a grant it lands in far fewer grants, and the same as
// before on both halves; neither half reports work in progress before or
// after it.
static unsigned save_in_grants(bool repeat) {
    static noah_profile_owner_t left;
    static noah_profile_owner_t right;
    static memory_t             left_memory;
    static memory_t             right_memory;
    static uint8_t              keymap[KEYMAP_LENGTH];
    split_link_t                left_link          = {.peer = &right};
    split_link_t                right_link         = {.peer = &left};
    working_link_t              left_working_link  = {.peer = &right};
    working_link_t              right_working_link = {.peer = &left};
    noah_profile_working_splice_t create = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .insert = KEYMAP_LENGTH, .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};
    uint32_t                      now    = 1000u;
    unsigned                      grants = 0u;
    bool                          right_busy = false;

    erased_memory(&left_memory);
    erased_memory(&right_memory);
    boot_live_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &left_working_link, &right_working_link, 0x5AFE5AFEu, &now);
    scan_until_converged(&left, &right, &now);
    for (uint32_t i = 0u; i < KEYMAP_LENGTH; i += 2u) keymap[i] = (uint8_t)(4u + i % 20u);
    assert(live_edit(&left, &right, 1u, &create, keymap, KEYMAP_LENGTH, &now) == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&left, &right, &now);
    assert(!noah_profile_owner_storage_busy(&left) && !noah_profile_owner_storage_busy(&right));
    assert(noah_profile_owner_save(&left, 40u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
    assert(noah_profile_owner_storage_busy(&left));
    for (; grants < 100000u && left.save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE; grants++) {
        (void)grant(&right, false, now, &right_memory, &right_link, &right_working_link, repeat);
        right_busy = right_busy || noah_profile_owner_storage_busy(&right);
        (void)grant(&left, true, now, &left_memory, &left_link, &left_working_link, repeat);
        now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
    }
    assert(left.save.phase == NOAH_PROFILE_OWNER_SAVE_IDLE && left.save.last_result == NOAH_PROFILE_WORKING_OK && left.save.last_transaction_id == 40u);
    assert(right_busy);
    scan_until_converged(&left, &right, &now);
    assert(left.committed_descriptor.generation == 1u && right.committed_descriptor.generation == 1u);
    assert(left.committed_descriptor.payload_digest == right.committed_descriptor.payload_digest);
    assert(!left.working.active && !right.working.active);
    assert(!noah_profile_owner_storage_busy(&left) && !noah_profile_owner_storage_busy(&right));
    return grants;
}

static void test_repeated_local_steps_save_in_far_fewer_grants(void) {
    unsigned one_step = save_in_grants(false);
    unsigned repeated = save_in_grants(true);

    printf("a save of a %u-byte profile: %u grants with one step each, %u with repeated local steps\n", (unsigned)(sizeof(compiled_blob) + NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE + KEYMAP_LENGTH), one_step, repeated);
    assert(repeated * 4u <= one_step);
}

static void test_apply_while_editing_waits_and_a_save_refuses_apart_halves(void) {
    static noah_profile_owner_t left;
    static noah_profile_owner_t right;
    static memory_t             left_memory;
    static memory_t             right_memory;
    static uint8_t              keymap[KEYMAP_LENGTH];
    split_link_t                left_link          = {.peer = &right};
    split_link_t                right_link         = {.peer = &left};
    working_link_t              left_working_link  = {.peer = &right};
    working_link_t              right_working_link = {.peer = &left};
    noah_profile_working_splice_t create = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .insert = KEYMAP_LENGTH, .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};
    uint8_t                       frame[32];
    uint32_t                      now = 5000u;

    erased_memory(&left_memory);
    erased_memory(&right_memory);
    boot_live_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &left_working_link, &right_working_link, 7u, &now);
    // Before the other half is heard, edits and Save wait.
    assert(noah_profile_owner_edit_begin(&left, 1u, left.working.session, left.working.revision, 1u, KEYMAP_LENGTH, 0u) == NOAH_PROFILE_WORKING_BUSY);
    scan_until_converged(&left, &right, &now);
    // An Apply's BEGIN while an edit is being received is answered BUSY.
    assert(noah_profile_owner_edit_begin(&left, 2u, left.working.session, left.working.revision, 1u, KEYMAP_LENGTH, 0u) == NOAH_PROFILE_WORKING_OK);
    begin_frame(frame, 9u);
    assert(noah_profile_owner_receive(&left, frame, sizeof(frame)) && frame[5] == NOAH_PROFILE_CANDIDATE_V1_ADMISSION_BUSY);
    assert(noah_profile_owner_edit_abort(&left, 2u) == NOAH_PROFILE_WORKING_OK);
    // An Apply in progress holds edits off; once it lands it replaced the
    // working copy and moved the revision.
    assert(live_edit(&left, &right, 3u, &create, keymap, KEYMAP_LENGTH, &now) == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&left, &right, &now);
    {
        uint32_t revision = left.working.revision;
        stage_valid_candidate_pair(&left, &right, 10u, &now);
        assert(noah_profile_owner_edit_begin(&left, 4u, left.working.session, left.working.revision, 1u, 2u, 0u) == NOAH_PROFILE_WORKING_BUSY);
        commit_valid_candidate_pair(&left, &right, 10u, 1u, &now);
        scan_until_converged(&left, &right, &now);
        assert(!left.working.active && !right.working.active && left.working.revision == revision + 1u && right.working.revision == left.working.revision);
    }
    puts("Apply and live edits never overlap; an Apply replaces the working copy");
}

static void test_an_abandoned_edit_is_aborted_and_revert_ends_one(void) {
    static noah_profile_owner_t left;
    static noah_profile_owner_t right;
    static memory_t             left_memory;
    static memory_t             right_memory;
    split_link_t                left_link          = {.peer = &right};
    split_link_t                right_link         = {.peer = &left};
    working_link_t              left_working_link  = {.peer = &right};
    working_link_t              right_working_link = {.peer = &left};
    uint8_t                     frame[32];
    uint32_t                    now = 5000u;
    uint32_t                    started;

    erased_memory(&left_memory);
    erased_memory(&right_memory);
    boot_live_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &left_working_link, &right_working_link, 7u, &now);
    scan_until_converged(&left, &right, &now);
    // Revert is the way out of an edit the host began and never finished.
    assert(noah_profile_owner_edit_begin(&left, 1u, left.working.session, left.working.revision, 1u, 2u, 0u) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_owner_revert(&left, 2u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_OK);
    assert(left.working.phase == NOAH_PROFILE_WORKING_PHASE_IDLE);
    scan_until_converged(&left, &right, &now);
    // A host that quits mid-edit: the edit is aborted after the host
    // timeout on the owner's clock, and Apply is admitted again.
    assert(noah_profile_owner_edit_begin(&left, 3u, left.working.session, left.working.revision, 1u, 2u, 0u) == NOAH_PROFILE_WORKING_OK);
    started = now;
    for (uint32_t guard = 0u; guard < 20000u && left.working.phase == NOAH_PROFILE_WORKING_PHASE_RECEIVING; guard++) {
        scan_pair(&left, &right, &now);
    }
    assert(left.working.phase == NOAH_PROFILE_WORKING_PHASE_IDLE && left.working.last_result == NOAH_PROFILE_WORKING_ABORTED);
    assert(now - started >= NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS && now - started < NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS + 1000u);
    begin_frame(frame, 9u);
    assert(noah_profile_owner_receive(&left, frame, sizeof(frame)) && frame[5] != NOAH_PROFILE_CANDIDATE_V1_ADMISSION_BUSY);
    puts("an abandoned edit ends at the host timeout, and Revert ends one at once");
}
// A half whose own saved profile is gone (an EEPROM clear erases its slot
// headers) takes the other half's copy when it starts. Its peer store stays
// COMMITTED afterwards, to answer a sender's retry of the finished copy. The
// next Apply or Save holds the storage for its own candidate; publishing the
// taken copy again then failed and stopped the owner with STORAGE_ERROR: the
// save froze, the other half was no longer polled, and Save, Revert and edits
// answered INVALID_STATE until a restart (keyboard check, 2026-10-20).
static void assert_took_the_other_halfs_copy(const noah_profile_owner_t *master, uint32_t generation) {
    assert(master->state == NOAH_PROFILE_OWNER_READY_VALIDATED && master->committed_descriptor.generation == generation);
    assert(noah_profile_peer_store_backend_state(&master->peer_store) == NOAH_PROFILE_PEER_STORE_COMMITTED);
}

static void test_an_apply_after_taking_the_other_halfs_copy_lands(void) {
    static noah_profile_owner_t left, right;
    static memory_t             left_memory, right_memory;
    split_link_t                left_link  = {.peer = &right};
    split_link_t                right_link = {.peer = &left};
    uint32_t                    now        = 0u;

    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    memset(right_memory.bytes, 0xff, sizeof(right_memory.bytes));
    boot_empty_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now);
    stage_valid_candidate_pair(&left, &right, 311u, &now);
    commit_valid_candidate_pair(&left, &right, 311u, 1u, &now);
    memset(left_memory.bytes, 0xff, sizeof(left_memory.bytes));
    reboot_saved_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &now, 1u);
    assert_took_the_other_halfs_copy(&left, 1u);
    stage_valid_candidate_pair(&left, &right, 312u, &now);
    commit_valid_candidate_pair(&left, &right, 312u, 2u, &now);
    assert(left.state == NOAH_PROFILE_OWNER_READY_VALIDATED && right.state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    puts("an Apply after taking the other half's copy at start lands on both halves");
}

static void test_a_save_after_taking_the_other_halfs_copy_lands(void) {
    static noah_profile_owner_t left, right;
    static memory_t             left_memory, right_memory;
    static uint8_t              keymap[KEYMAP_LENGTH];
    split_link_t                left_link          = {.peer = &right};
    split_link_t                right_link         = {.peer = &left};
    working_link_t              left_working_link  = {.peer = &right};
    working_link_t              right_working_link = {.peer = &left};
    noah_profile_working_splice_t create  = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .insert = KEYMAP_LENGTH, .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};
    noah_profile_working_splice_t replace = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .insert = KEYMAP_LENGTH, .remove = KEYMAP_LENGTH};
    uint32_t                      now     = 1000u;
    unsigned                      grants  = 0u;

    erased_memory(&left_memory);
    erased_memory(&right_memory);
    boot_live_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &left_working_link, &right_working_link, 0x5AFE5AFEu, &now);
    scan_until_converged(&left, &right, &now);
    for (uint32_t i = 0u; i < KEYMAP_LENGTH; i += 2u) keymap[i] = (uint8_t)(4u + i % 20u);
    assert(live_edit(&left, &right, 1u, &create, keymap, KEYMAP_LENGTH, &now) == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&left, &right, &now);
    assert(noah_profile_owner_save(&left, 40u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
    for (uint32_t guard = 0u; guard < 20000u && left.save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE; guard++) scan_pair(&left, &right, &now);
    scan_until_converged(&left, &right, &now);
    assert(left.committed_descriptor.generation == 1u && right.committed_descriptor.generation == 1u);
    // Only the master's slots are erased; both halves restart.
    erased_memory(&left_memory);
    boot_live_pair(&left, &left_memory, &right, &right_memory, &left_link, &right_link, &left_working_link, &right_working_link, 0x5AFE5AFFu, &now);
    for (uint32_t guard = 0u; guard < 40000u && left.committed_descriptor.generation != 1u; guard++) scan_pair(&left, &right, &now);
    scan_until_converged(&left, &right, &now);
    assert_took_the_other_halfs_copy(&left, 1u);
    keymap[2] ^= 1u;
    assert(live_edit(&left, &right, 2u, &replace, keymap, KEYMAP_LENGTH, &now) == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&left, &right, &now);
    assert(noah_profile_owner_save(&left, 41u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
    // Grants as the scheduler gives them, the other half's included.
    for (; grants < 20000u && left.save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE; grants++) {
        (void)grant(&right, false, now, &right_memory, &right_link, &right_working_link, true);
        (void)grant(&left, true, now, &left_memory, &left_link, &left_working_link, true);
        assert(left.state == NOAH_PROFILE_OWNER_READY_VALIDATED);
        now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
    }
    assert(left.save.last_transaction_id == 41u && left.save.last_result == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&left, &right, &now);
    assert(left.committed_descriptor.generation == 2u && right.committed_descriptor.generation == 2u);
    assert(left.state == NOAH_PROFILE_OWNER_READY_VALIDATED && right.state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    puts("a Save after taking the other half's copy at start lands on both halves");
}

#include "profile_owner_auto_save_test.inc"

int main(void) {
    test_auto_save_modes_and_adjustments();
    test_saving_the_saved_bytes_writes_nothing();
    test_auto_save_hour_bound();
    test_auto_save_power_cuts();
    test_apply_waits_behind_a_queued_adjustment();
    test_adjustments_without_a_settings_domain();
    test_a_failed_automatic_save_is_reported_and_not_repeated();
    test_live_edit_reaches_both_halves_and_save_writes_both();
    test_repeated_local_steps_save_in_far_fewer_grants();
    test_a_save_after_taking_the_other_halfs_copy_lands();
    test_an_apply_after_taking_the_other_halfs_copy_lands();
    test_apply_while_editing_waits_and_a_save_refuses_apart_halves();
    test_an_abandoned_edit_is_aborted_and_revert_ends_one();
    test_partial_runtime_install_rolls_back();
    test_failed_factory_rgb_cache_refuses_graph_install();
    test_clean_idle_scans_do_not_repeat_host_session_cleanup();
    test_commit_waits_for_a_safe_boundary_then_activates();
    test_lost_link_after_decision_keeps_typing_and_resumes();
    test_peer_rebooted_while_unlinked_after_decision_resumes();
    test_activation_waits_for_a_safe_boundary_while_typing();
    test_usb_restart_waits_for_a_settled_save();
    test_link_lost_after_the_peer_commit_was_seen_still_activates();
    test_empty_boot_live_commit_and_timeout();
    test_committed_reuse_saves_both_halves_and_reboots(false);
    test_committed_reuse_saves_both_halves_and_reboots(true);
    test_long_profile_copy_reads_within_the_store_bound();
    test_two_consecutive_live_commits_use_distributed_prepare_barrier();
    test_simultaneous_hosts_choose_stable_physical_origin_before_durability();
    test_prepared_peer_is_aborted_before_host_timeout_releases_candidate();
    test_host_abort_is_coordinated_at_each_prepare_boundary();
    test_host_abort_ends_when_peer_never_acknowledges();
    test_barrier_mailbox_retries_and_postcommit_abort_do_not_starve();
    test_local_known_commit_failure_aborts_prepared_peer_first();
    test_prepare_uses_extended_no_progress_timeout();
    test_waiting_for_a_quiet_keyboard_never_ends_a_save();
    test_prepare_phase_progress_extends_only_once();
    test_peer_store_write_failure_retries_then_fails_fast();
    test_unexpected_newer_postcommit_authority_never_activates();
    test_boot_reconciles_different_generations_before_activation();
    test_peer_authority_supersedes_only_precommit_host_generation();
    test_every_precommit_cancel_releases_the_candidate();
    test_decided_commit_is_never_cancelled();
    puts("profile owner host tests passed");
    return 0;
}
