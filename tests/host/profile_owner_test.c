#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/profile/runtime/profile_owner.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"

static const uint8_t compiled_blob[] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};

typedef struct {
    uint8_t  bytes[NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE];
    uint32_t reads;
    uint32_t writes;
    uint16_t largest_read;
    uint16_t largest_write;
} memory_t;

typedef struct {
    noah_profile_owner_t *peer;
} split_link_t;

static uint32_t compiled_reads;
static bool     key_installed;
static bool     rgb_installed;
static bool     rgb_install_allowed = true;

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
        .metadata = {
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

bool noah_profile_compiled_v1_compatibility(const noah_profile_compiled_v1_t *profile, noah_profile_validator_v1_compatibility_t *compatibility) {
    if (!profile || !compatibility) return false;
    *compatibility = noah_profile_validator_v1_default_compatibility(profile->metadata.action_abi_digest);
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
    (void)context; (void)publication_count; (void)previous; (void)active; (void)callback_view;
}

void noah_effective_rgb_runtime_init(noah_effective_rgb_runtime_t *runtime) {
    memset(runtime, 0, sizeof(*runtime));
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
    (void)context; (void)publication_count; (void)previous; (void)active; (void)callback_view;
}

void noah_profile_activation_policy_init(noah_profile_activation_policy_t *policy, noah_profile_activation_peer_observer_fn observer, void *observer_context) {
    memset(policy, 0, sizeof(*policy));
    policy->peer_observer = observer;
    policy->peer_context  = observer_context;
    policy->initialized   = true;
}

uint32_t noah_profile_activation_policy_safe_boundary(void *context) {
    noah_profile_activation_policy_t *policy = context;
    uint8_t unresolved = 1u;
    return policy && policy->initialized && policy->peer_observer && policy->peer_observer(policy->peer_context, &unresolved) && unresolved == 0u ? 0u : NOAH_PROFILE_ACTIVATION_REASON_PEER;
}

static bool memory_read(void *context, uint16_t address, uint8_t *target, uint16_t length) {
    memory_t *memory = context;
    memory->reads++;
    if (length > memory->largest_read) memory->largest_read = length;
    if (!target || length == 0u || (uint32_t)address + length > sizeof(memory->bytes)) return false;
    memcpy(target, &memory->bytes[address], length);
    return true;
}

static bool memory_write(void *context, uint16_t address, const uint8_t *source, uint16_t length) {
    memory_t *memory = context;
    memory->writes++;
    if (length > memory->largest_write) memory->largest_write = length;
    if (!source || length == 0u || (uint32_t)address + length > sizeof(memory->bytes)) return false;
    memcpy(&memory->bytes[address], source, length);
    return true;
}

static bool split_exchange(void *context, const uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], uint8_t response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    split_link_t                    *link = context;
    noah_profile_split_reconciler_t *peer_reconciler;

    if (!link || !link->peer || !(peer_reconciler = noah_profile_owner_split_reconciler(link->peer))) {
        return false;
    }
    return noah_profile_split_reconciler_receive(peer_reconciler, request, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, response, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
}

static void write_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)value; target[1] = (uint8_t)(value >> 8u);
}

static void write_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value; target[1] = (uint8_t)(value >> 8u); target[2] = (uint8_t)(value >> 16u); target[3] = (uint8_t)(value >> 24u);
}

static noah_profile_candidate_v1_metadata_t metadata(void) {
    return (noah_profile_candidate_v1_metadata_t){
        .schema_major      = 1u,
        .schema_minor      = 0u,
        .payload_length    = sizeof(compiled_blob),
        .crc32             = crc_of(compiled_blob, sizeof(compiled_blob)),
        .digest            = digest_of(compiled_blob, sizeof(compiled_blob)),
        .action_abi_digest = UINT32_C(0x12345678),
    };
}

static void begin_frame(uint8_t frame[32], uint16_t transaction_id) {
    noah_profile_candidate_v1_metadata_t declaration = metadata();
    memset(frame, 0, 32u);
    frame[0] = NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET; frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL; frame[2] = NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN;
    write_u16(&frame[3], transaction_id); frame[5] = declaration.schema_major; frame[6] = declaration.schema_minor; frame[7] = declaration.requested_domains;
    write_u16(&frame[9], declaration.payload_length); write_u32(&frame[11], declaration.crc32); write_u32(&frame[15], declaration.digest); write_u32(&frame[19], declaration.action_abi_digest);
}

static void chunk_frame(uint8_t frame[32], uint16_t transaction_id) {
    memset(frame, 0, 32u);
    frame[0] = NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET; frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL; frame[2] = NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK;
    write_u16(&frame[3], transaction_id); write_u16(&frame[5], 0u); frame[7] = sizeof(compiled_blob); memcpy(&frame[8], compiled_blob, sizeof(compiled_blob));
}

static void simple_frame(uint8_t frame[32], uint8_t value, uint16_t transaction_id) {
    memset(frame, 0, 32u);
    frame[0] = value == NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT ? NOAH_PROFILE_CANDIDATE_V1_COMMAND_SAVE : NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET;
    frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL; frame[2] = value; write_u16(&frame[3], transaction_id);
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
}

static void send_and_scan(noah_profile_owner_t *owner, uint8_t frame[32], uint32_t now) {
    assert(noah_profile_owner_receive(owner, frame, 32u));
    assert(noah_profile_owner_scan(owner, true, now));
}

static void commit_blob(noah_profile_owner_t *owner, uint16_t transaction_id, uint32_t *now) {
    uint8_t frame[32];

    begin_frame(frame, transaction_id); send_and_scan(owner, frame, (*now)++);
    chunk_frame(frame, transaction_id); send_and_scan(owner, frame, (*now)++);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, transaction_id); send_and_scan(owner, frame, (*now)++);
    while (owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING) assert(noah_profile_owner_scan(owner, true, (*now)++));
    assert(owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, transaction_id); send_and_scan(owner, frame, (*now)++);
    for (uint32_t guard = 0u; guard < 256u && owner->host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE; guard++) assert(noah_profile_owner_scan(owner, true, (*now)++));
    assert(owner->state == NOAH_PROFILE_OWNER_READY_VALIDATED);
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
    compiled_reads = 0u; key_installed = false; rgb_installed = false;
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

    begin_frame(frame, 42u); send_and_scan(&rebooted, frame, now);
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

int main(void) {
    test_partial_runtime_install_rolls_back();
    test_empty_boot_live_commit_and_timeout();
    test_boot_reconciles_different_generations_before_activation();
    puts("profile owner host tests passed");
    return 0;
}
