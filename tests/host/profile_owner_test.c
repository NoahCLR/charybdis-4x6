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
    bool     fail_next_write;
} memory_t;

typedef struct {
    noah_profile_owner_t *peer;
} split_link_t;

static uint32_t compiled_reads;
static bool     key_installed;
static bool     rgb_installed;
static bool     rgb_install_allowed = true;

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
    if (memory->fail_next_write) {
        memory->fail_next_write = false;
        return false;
    }
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

static void scan_pair(noah_profile_owner_t *master, noah_profile_owner_t *peer, uint32_t *now) {
    (void)noah_profile_owner_scan(peer, false, *now);
    (void)noah_profile_owner_scan(master, true, *now);
    *now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
}

static void boot_empty_pair(noah_profile_owner_t *left, memory_t *left_memory, noah_profile_owner_t *right, memory_t *right_memory, split_link_t *left_link, split_link_t *right_link, uint32_t *now) {
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

    begin_frame(frame, transaction_id); send_and_scan(owner, frame, (*now)++);
    chunk_frame(frame, transaction_id); send_and_scan(owner, frame, (*now)++);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, transaction_id); send_and_scan(owner, frame, (*now)++);
    while (owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING) assert(noah_profile_owner_scan(owner, true, (*now)++));
    assert(owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, transaction_id); send_and_scan(owner, frame, (*now)++);
    for (uint32_t guard = 0u; guard < 256u && owner->host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE; guard++) assert(noah_profile_owner_scan(owner, true, (*now)++));
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
    chunk_frame(frame, transaction_id);
    send_and_scan_pair(master, peer, frame, now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, transaction_id);
    send_and_scan_pair(master, peer, frame, now);
    for (uint32_t guard = 0u; guard < 256u && master->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING; guard++) {
        scan_pair(master, peer, now);
    }
    assert(master->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);
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

    begin_frame(frame, 90u); queue_candidate_operation_dual(&left, &left, &right, frame, &now);
    chunk_frame(frame, 90u); queue_candidate_operation_dual(&left, &left, &right, frame, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, 90u); queue_candidate_operation_dual(&left, &left, &right, frame, &now);
    while (left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING) scan_dual_master(&left, &right, &now);
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);

    begin_frame(frame, 91u); queue_candidate_operation_dual(&right, &left, &right, frame, &now);
    chunk_frame(frame, 91u); queue_candidate_operation_dual(&right, &left, &right, frame, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, 91u); queue_candidate_operation_dual(&right, &left, &right, frame, &now);
    while (right.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING) scan_dual_master(&left, &right, &now);
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
        uint32_t             now = (uint32_t)(120000u + phase * 10000u);

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
    started_at = left.host_last_activity_at;
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

static void test_unexpected_newer_postcommit_authority_never_activates(void) {
    noah_profile_owner_t             left;
    noah_profile_owner_t             right;
    noah_effective_profile_status_t  provider;
    memory_t                         left_memory;
    memory_t                         right_memory;
    split_link_t                     left_link  = {.peer = &right};
    split_link_t                     right_link = {.peer = &left};
    noah_profile_split_descriptor_t  newer;
    uint8_t                          frame[32];
    uint32_t                         now = 90000u;

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

    begin_frame(frame, 71u); send_and_scan(&owner, frame, now++);
    assert(owner.store.candidate.generation == 2u);
    publish_peer(&owner, 1u);
    (void)noah_profile_owner_scan(&owner, true, now++);
    assert(owner.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING);

    chunk_frame(frame, 71u); send_and_scan(&owner, frame, now++);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, 71u); send_and_scan(&owner, frame, now++);
    while (owner.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING) assert(noah_profile_owner_scan(&owner, true, now++));
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

int main(void) {
    test_partial_runtime_install_rolls_back();
    test_clean_idle_scans_do_not_repeat_host_session_cleanup();
    test_empty_boot_live_commit_and_timeout();
    test_two_consecutive_live_commits_use_distributed_prepare_barrier();
    test_simultaneous_hosts_choose_stable_physical_origin_before_durability();
    test_prepared_peer_is_aborted_before_host_timeout_releases_candidate();
    test_host_abort_is_coordinated_at_each_prepare_boundary();
    test_barrier_mailbox_retries_and_postcommit_abort_do_not_starve();
    test_local_known_commit_failure_aborts_prepared_peer_first();
    test_prepare_uses_extended_no_progress_timeout();
    test_unexpected_newer_postcommit_authority_never_activates();
    test_boot_reconciles_different_generations_before_activation();
    test_peer_authority_supersedes_only_precommit_host_generation();
    puts("profile owner host tests passed");
    return 0;
}
