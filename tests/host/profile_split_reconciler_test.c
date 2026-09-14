#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/profile/split/profile_split_reconciler.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"

enum {
    ACTION_ABI_DIGEST = UINT32_C(0x11223344),
    MAX_SCANS         = 4096u,
    MAX_PREPARE_TIME_MS = 60000u,
};

static const uint8_t empty_profile[] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};
static const uint8_t max_profile[NOAH_PROFILE_CANDIDATE_V1_MAX_BLOB_SIZE];

typedef struct {
    uint8_t  bytes[NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE];
    uint32_t reads;
    uint32_t writes;
} memory_t;

typedef struct {
    const uint8_t *bytes;
    uint16_t       length;
    uint32_t       reads;
} staged_source_t;

typedef struct half half_t;

typedef struct {
    half_t  *peer;
    uint32_t exchanges;
    uint32_t drop_before_delivery_exchange;
    uint32_t drop_after_delivery_exchange;
    bool     connected;
    bool     corrupt_next_response;
} link_t;

struct half {
    memory_t                               memory;
    noah_profile_store_t                   store;
    noah_effective_profile_provider_t      provider;
    noah_profile_candidate_store_backend_t candidate_backend;
    noah_profile_peer_store_backend_t      peer_store;
    noah_profile_split_reconciler_t        reconciler;
    link_t                                 link;
    uint32_t                               compiled_digest;
};

static uint32_t always_safe(void *context) {
    (void)context;
    return 0u;
}

static bool memory_read(void *context, uint16_t address, uint8_t *target, uint16_t length) {
    memory_t *memory = context;

    memory->reads++;
    if (!target || length == 0u || (uint32_t)address + length > sizeof(memory->bytes)) {
        return false;
    }
    memcpy(target, &memory->bytes[address], length);
    return true;
}

static bool memory_write(void *context, uint16_t address, const uint8_t *source, uint16_t length) {
    memory_t *memory = context;

    memory->writes++;
    if (!source || length == 0u || (uint32_t)address + length > sizeof(memory->bytes)) {
        return false;
    }
    memcpy(&memory->bytes[address], source, length);
    return true;
}

static uint32_t payload_crc(const uint8_t *payload, uint16_t length) {
    return noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, payload, length));
}

static uint32_t payload_digest(const uint8_t *payload, uint16_t length) {
    return noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, payload, length);
}

static noah_profile_split_descriptor_t compiled_descriptor(const half_t *half) {
    return (noah_profile_split_descriptor_t){
        .compiled_default_digest = half->compiled_digest,
        .action_abi_digest       = ACTION_ABI_DIGEST,
        .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
        .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
        .readable                = true,
    };
}

static noah_profile_split_descriptor_t committed_descriptor(const half_t *half, uint32_t generation, uint8_t origin) {
    noah_profile_split_descriptor_t descriptor = compiled_descriptor(half);

    descriptor.generation     = generation;
    descriptor.payload_crc32  = payload_crc(empty_profile, sizeof(empty_profile));
    descriptor.payload_digest = payload_digest(empty_profile, sizeof(empty_profile));
    descriptor.payload_length = sizeof(empty_profile);
    descriptor.profile_flags  = NOAH_PROFILE_STORE_FLAG_OVERRIDE;
    descriptor.origin_half    = origin;
    descriptor.has_profile    = true;
    return descriptor;
}

static bool local_descriptor(void *context, noah_profile_split_descriptor_t *descriptor) {
    half_t *half = context;

    if (!half || !descriptor) {
        return false;
    }
    *descriptor = compiled_descriptor(half);
    if (half->store.committed.slot != NOAH_PROFILE_SLOT_NONE) {
        const noah_profile_store_record_t *record = &half->store.committed;

        descriptor->generation              = record->generation;
        descriptor->payload_crc32           = record->payload_crc32;
        descriptor->payload_digest          = record->payload_digest;
        descriptor->compiled_default_digest = record->compiled_default_digest;
        descriptor->action_abi_digest       = record->action_abi_digest;
        descriptor->payload_length          = record->payload_length;
        descriptor->schema_major            = record->schema_major;
        descriptor->schema_minor            = record->schema_minor;
        descriptor->domain_mask             = record->domain_mask;
        descriptor->profile_flags           = record->flags;
        descriptor->origin_half             = record->origin_half;
        descriptor->has_profile             = true;
    }
    return true;
}

static bool local_read(void *context, const noah_profile_split_descriptor_t *descriptor, uint16_t offset, uint8_t *bytes, uint8_t length) {
    half_t  *half = context;
    uint16_t base;

    if (!half || !descriptor || !bytes || length == 0u || (uint32_t)offset + length > descriptor->payload_length || half->store.committed.slot == NOAH_PROFILE_SLOT_NONE || half->store.committed.generation != descriptor->generation || half->store.committed.payload_digest != descriptor->payload_digest) {
        return false;
    }
    if (half->store.committed.slot == NOAH_PROFILE_SLOT_A) {
        base = (uint16_t)(NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
    } else if (half->store.committed.slot == NOAH_PROFILE_SLOT_B) {
        base = (uint16_t)(NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
    } else {
        return false;
    }
    return memory_read(&half->memory, (uint16_t)(base + offset), bytes, length);
}

static bool staged_read(void *context, const noah_profile_split_descriptor_t *descriptor, uint16_t offset, uint8_t *bytes, uint8_t length) {
    staged_source_t *source = context;

    if (!source || !descriptor || !bytes || length == 0u || descriptor->payload_length != source->length || (uint32_t)offset + length > source->length) {
        return false;
    }
    source->reads++;
    memcpy(bytes, &source->bytes[offset], length);
    return true;
}

static bool exchange(void *context, const uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], uint8_t response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    link_t *link = context;

    link->exchanges++;
    if (link->drop_before_delivery_exchange == link->exchanges) {
        link->drop_before_delivery_exchange = 0u;
        return false;
    }
    if (!link->connected || !link->peer || !noah_profile_split_reconciler_receive(&link->peer->reconciler, request, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, response, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE)) {
        return false;
    }
    if (link->drop_after_delivery_exchange == link->exchanges) {
        link->drop_after_delivery_exchange = 0u;
        return false;
    }
    if (link->corrupt_next_response) {
        link->corrupt_next_response = false;
        response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE - 1u] ^= 1u;
    }
    return true;
}

static void half_storage_init(half_t *half) {
    noah_profile_reader_t                     reader = noah_profile_reader_from_memory(empty_profile, sizeof(empty_profile));
    noah_profile_validator_v1_profile_t       profile;
    noah_effective_profile_snapshot_t         compiled;
    noah_profile_validator_v1_compatibility_t compatibility = noah_profile_validator_v1_default_compatibility(ACTION_ABI_DIGEST);
    noah_profile_store_record_t               selected;

    memset(half, 0, sizeof(*half));
    memset(half->memory.bytes, 0xff, sizeof(half->memory.bytes));
    memset(&profile, 0, sizeof(profile));
    profile.byte_length       = sizeof(empty_profile);
    profile.crc32             = payload_crc(empty_profile, sizeof(empty_profile));
    profile.digest            = payload_digest(empty_profile, sizeof(empty_profile));
    profile.action_abi_digest = ACTION_ABI_DIGEST;
    assert(noah_effective_profile_snapshot_make_compiled(&profile, &reader, 0u, &compiled) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_init(&half->provider, &compiled, always_safe, NULL, NULL, 0u) == NOAH_EFFECTIVE_PROFILE_OK);
    half->compiled_digest = compiled.identity.payload_digest;
    noah_profile_store_init(&half->store,
                            (noah_profile_store_io_t){.read = memory_read, .write = memory_write, .context = &half->memory},
                            (noah_profile_store_compatibility_t){
                                .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
                                .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
                                .compiled_default_digest = half->compiled_digest,
                                .action_abi_digest       = ACTION_ABI_DIGEST,
                            });
    assert(noah_profile_store_boot_select(&half->store, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    compatibility.required_domain_mask = 0u;
    noah_profile_candidate_store_backend_init(&half->candidate_backend, &half->store, &half->provider, &compatibility, half->compiled_digest, 1u);
    assert(half->candidate_backend.reuse_guard_installed);
    noah_profile_peer_store_backend_init(&half->peer_store, &half->candidate_backend);
    half->link.connected = true;
}

static void install_profile_with_flags(half_t *half, uint32_t generation, uint8_t origin, uint8_t flags) {
    noah_profile_split_descriptor_t descriptor = committed_descriptor(half, generation, origin);
    noah_profile_peer_store_result_t result;

    descriptor.profile_flags = flags;
    assert(noah_profile_peer_store_backend_begin(&half->peer_store, &descriptor) == NOAH_PROFILE_PEER_STORE_OK);
    assert(noah_profile_peer_store_backend_write(&half->peer_store, descriptor.generation, descriptor.payload_digest, 0u, empty_profile, sizeof(empty_profile)) == NOAH_PROFILE_PEER_STORE_OK);
    result = noah_profile_peer_store_backend_commit_begin(&half->peer_store, &descriptor);
    while (result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS) {
        result = noah_profile_peer_store_backend_step(&half->peer_store, NOAH_PROFILE_SPLIT_V1_CHUNK_MAX);
    }
    assert(result == NOAH_PROFILE_PEER_STORE_OK);
}

static void install_profile(half_t *half, uint32_t generation, uint8_t origin) {
    install_profile_with_flags(half, generation, origin, NOAH_PROFILE_STORE_FLAG_OVERRIDE);
}

static void pair_init(half_t *left, half_t *right) {
    noah_profile_split_reconciler_config_t left_config;
    noah_profile_split_reconciler_config_t right_config;

    left->link.peer  = right;
    right->link.peer = left;
    left_config      = (noah_profile_split_reconciler_config_t){
        .local_context   = left,
        .local_descriptor = local_descriptor,
        .local_read       = local_read,
        .transport_context = &left->link,
        .exchange           = exchange,
        .peer_store         = &left->peer_store,
    };
    right_config = (noah_profile_split_reconciler_config_t){
        .local_context   = right,
        .local_descriptor = local_descriptor,
        .local_read       = local_read,
        .transport_context = &right->link,
        .exchange           = exchange,
        .peer_store         = &right->peer_store,
    };
    noah_profile_split_reconciler_init(&left->reconciler, &left_config);
    noah_profile_split_reconciler_init(&right->reconciler, &right_config);
}

static void assert_one_scan_budget(half_t *half, bool master, uint32_t now) {
    uint32_t reads_before     = half->memory.reads;
    uint32_t writes_before    = half->memory.writes;
    uint32_t exchanges_before = half->link.exchanges;

    (void)noah_profile_split_reconciler_scan(&half->reconciler, master, now);
    if (half->link.exchanges != exchanges_before) {
        assert(half->link.exchanges == exchanges_before + 1u);
        assert(half->memory.reads == reads_before);
        assert(half->memory.writes == writes_before);
    }
}

static bool pair_converged(const half_t *left, const half_t *right) {
    noah_profile_split_authority_status_t left_status;
    noah_profile_split_authority_status_t right_status;

    return noah_profile_split_authority_status(noah_profile_split_reconciler_authority(&left->reconciler), &left_status) && noah_profile_split_authority_status(noah_profile_split_reconciler_authority(&right->reconciler), &right_status) && (left_status.state == NOAH_PROFILE_SPLIT_AUTHORITY_COMPILED_CONVERGED || left_status.state == NOAH_PROFILE_SPLIT_AUTHORITY_COMMITTED_CONVERGED) && (right_status.state == NOAH_PROFILE_SPLIT_AUTHORITY_COMPILED_CONVERGED || right_status.state == NOAH_PROFILE_SPLIT_AUTHORITY_COMMITTED_CONVERGED) && !left_status.transfer_pending && !right_status.transfer_pending;
}

static void run_pair_until_converged(half_t *left, half_t *right, bool left_master) {
    for (uint32_t scan = 0u; scan < MAX_SCANS; scan++) {
        uint32_t now = scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        if (left_master) {
            assert_one_scan_budget(left, true, now);
            assert_one_scan_budget(right, false, now);
        } else {
            assert_one_scan_budget(right, true, now);
            assert_one_scan_budget(left, false, now);
        }
        if (pair_converged(left, right)) {
            return;
        }
    }
    assert(!"pair did not converge");
}

static void assert_records_match(const half_t *left, const half_t *right) {
    const noah_profile_store_record_t *a = &left->store.committed;
    const noah_profile_store_record_t *b = &right->store.committed;

    assert(a->slot != NOAH_PROFILE_SLOT_NONE && b->slot != NOAH_PROFILE_SLOT_NONE);
    assert(a->generation == b->generation);
    assert(a->origin_half == b->origin_half);
    assert(a->flags == b->flags);
    assert(a->payload_length == b->payload_length);
    assert(a->payload_crc32 == b->payload_crc32);
    assert(a->payload_digest == b->payload_digest);
    assert(a->compiled_default_digest == b->compiled_default_digest);
    assert(a->action_abi_digest == b->action_abi_digest);
}

static void test_compiled_convergence(void) {
    half_t left;
    half_t right;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    assert(left.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
    assert(right.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
}

static void test_newer_master_pushes_exact_record(void) {
    half_t left;
    half_t right;

    half_storage_init(&left);
    half_storage_init(&right);
    install_profile(&right, 5u, 1u);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    assert_records_match(&left, &right);
    assert(left.store.committed.generation == 5u && left.store.committed.origin_half == 1u);
}

static void test_newer_slave_is_pulled_without_role_authority(void) {
    half_t left;
    half_t right;

    half_storage_init(&left);
    half_storage_init(&right);
    install_profile(&left, 7u, 0u);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    assert_records_match(&left, &right);
    assert(right.store.committed.generation == 7u && right.store.committed.origin_half == 0u);
}

static void test_disconnect_invalidates_then_reconnects(void) {
    half_t                                 left;
    half_t                                 right;
    noah_profile_split_authority_status_t status;
    uint8_t                                unresolved = 0u;

    half_storage_init(&left);
    half_storage_init(&right);
    install_profile(&left, 3u, 0u);
    pair_init(&left, &right);
    right.link.connected = false;
    assert_one_scan_budget(&right, true, 0u);
    assert(noah_profile_split_authority_status(noah_profile_split_reconciler_authority(&right.reconciler), &status));
    assert(status.state == NOAH_PROFILE_SPLIT_AUTHORITY_PEER_UNREADABLE);
    assert(noah_profile_split_authority_peer_observer((void *)noah_profile_split_reconciler_authority(&right.reconciler), &unresolved));
    assert(unresolved == 1u);
    right.link.connected = true;
    run_pair_until_converged(&left, &right, false);
    assert_records_match(&left, &right);

    // The passive half cannot probe, so it expires a missing master poll and
    // fails closed instead of retaining convergence forever after link loss.
    assert_one_scan_budget(&left, false, (MAX_SCANS * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) + NOAH_PROFILE_SPLIT_PEER_TIMEOUT_MS);
    assert(noah_profile_split_authority_status(noah_profile_split_reconciler_authority(&left.reconciler), &status));
    assert(status.state == NOAH_PROFILE_SPLIT_AUTHORITY_PEER_UNREADABLE);
}

static void test_corrupt_response_restarts_fail_closed(void) {
    half_t left;
    half_t right;

    half_storage_init(&left);
    half_storage_init(&right);
    install_profile(&left, 4u, 0u);
    pair_init(&left, &right);
    right.link.corrupt_next_response = true;
    assert_one_scan_budget(&right, true, 0u);
    assert(noah_profile_split_authority_compare(&right.reconciler.local_descriptor, &right.reconciler.peer_descriptor) == NOAH_PROFILE_SPLIT_AUTHORITY_PEER_UNREADABLE);
    run_pair_until_converged(&left, &right, false);
    assert_records_match(&left, &right);
}

static void test_lost_reply_after_admission_is_idempotent(void) {
    half_t left;
    half_t right;

    half_storage_init(&left);
    half_storage_init(&right);
    install_profile(&right, 6u, 1u);
    pair_init(&left, &right);
    right.link.drop_after_delivery_exchange = 2u;
    run_pair_until_converged(&left, &right, false);
    assert_records_match(&left, &right);
    assert(left.store.committed.generation == 6u && left.store.committed.origin_half == 1u);
}

static void test_role_change_restarts_and_preserves_physical_origin(void) {
    half_t left;
    half_t right;

    half_storage_init(&left);
    half_storage_init(&right);
    install_profile(&left, 11u, 0u);
    pair_init(&left, &right);
    for (uint32_t scan = 0u; scan < 12u; scan++) {
        uint32_t now = scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    run_pair_until_converged(&left, &right, true);
    assert_records_match(&left, &right);
    assert(right.store.committed.origin_half == 0u);
}

static void test_concurrent_commit_stops_without_overwrite(void) {
    half_t                                  left;
    half_t                                  right;
    noah_profile_split_reconciler_status_t status;
    uint32_t                                left_writes;
    uint32_t                                right_writes;

    half_storage_init(&left);
    half_storage_init(&right);
    install_profile(&left, 8u, 0u);
    install_profile(&right, 8u, 1u);
    left_writes  = left.memory.writes;
    right_writes = right.memory.writes;
    pair_init(&left, &right);
    for (uint32_t scan = 0u; scan < 8u; scan++) {
        uint32_t now = scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(noah_profile_split_reconciler_status(&right.reconciler, &status));
    assert(status.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED);
    assert(status.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_CONFLICT);
    assert(left.memory.writes == left_writes);
    assert(right.memory.writes == right_writes);
}

static void test_same_tuple_corruption_stops_without_overwrite(void) {
    half_t                                  left;
    half_t                                  right;
    noah_profile_split_reconciler_status_t status;
    uint32_t                                left_writes;
    uint32_t                                right_writes;

    half_storage_init(&left);
    half_storage_init(&right);
    install_profile_with_flags(&left, 9u, 0u, NOAH_PROFILE_STORE_FLAG_OVERRIDE);
    install_profile_with_flags(&right, 9u, 0u, 0u);
    left_writes  = left.memory.writes;
    right_writes = right.memory.writes;
    pair_init(&left, &right);
    for (uint32_t scan = 0u; scan < 8u; scan++) {
        uint32_t now = scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(noah_profile_split_reconciler_status(&right.reconciler, &status));
    assert(status.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED);
    assert(status.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_CORRUPT);
    assert(left.memory.writes == left_writes);
    assert(right.memory.writes == right_writes);
}

static void test_incompatible_firmware_stops_without_write(void) {
    half_t                                  left;
    half_t                                  right;
    noah_profile_split_reconciler_status_t status;

    half_storage_init(&left);
    half_storage_init(&right);
    right.compiled_digest ^= 1u;
    pair_init(&left, &right);
    for (uint32_t scan = 0u; scan < 8u; scan++) {
        uint32_t now = scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(noah_profile_split_reconciler_status(&right.reconciler, &status));
    assert(status.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED);
    assert(status.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_INCOMPATIBLE);
    assert(left.memory.writes == 0u && right.memory.writes == 0u);
}

static void test_max_generation_transfers_without_local_increment(void) {
    half_t left;
    half_t right;

    half_storage_init(&left);
    half_storage_init(&right);
    install_profile(&left, UINT32_MAX, 0u);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    assert_records_match(&left, &right);
    assert(right.store.committed.generation == UINT32_MAX);
}

static void test_convergence_only_pushes_host_record_without_importing(void) {
    half_t   left;
    half_t   right;
    uint32_t right_writes;

    half_storage_init(&left);
    half_storage_init(&right);
    install_profile(&right, 14u, 1u);
    right_writes = right.memory.writes;
    pair_init(&left, &right);
    for (uint32_t scan = 0u; scan < MAX_SCANS && !pair_converged(&left, &right); scan++) {
        uint32_t now = scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        (void)noah_profile_split_reconciler_scan_mode(&right.reconciler, true, now, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY);
        (void)noah_profile_split_reconciler_scan(&left.reconciler, false, now);
    }
    assert(pair_converged(&left, &right));
    assert_records_match(&left, &right);
    assert(right.memory.writes == right_writes);
}

static void test_convergence_only_refuses_newer_peer_import(void) {
    half_t   left;
    half_t   right;
    uint32_t right_writes;

    half_storage_init(&left);
    half_storage_init(&right);
    install_profile(&left, 15u, 0u);
    right_writes = right.memory.writes;
    pair_init(&left, &right);
    for (uint32_t scan = 0u; scan < 64u; scan++) {
        uint32_t now = scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        (void)noah_profile_split_reconciler_scan_mode(&right.reconciler, true, now, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY);
        (void)noah_profile_split_reconciler_scan(&left.reconciler, false, now);
    }
    assert(right.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
    assert(right.memory.writes == right_writes);
    assert(noah_profile_candidate_store_backend_admission_owner(&right.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
}

static void test_convergence_only_answers_inbound_prepare_busy_without_storage(void) {
    half_t                         left;
    half_t                         right;
    noah_profile_split_v1_frame_t request;
    noah_profile_split_v1_frame_t response;
    uint8_t                        request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    uint8_t                        response_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    uint32_t                       right_writes;

    half_storage_init(&left);
    half_storage_init(&right);
    install_profile(&left, 16u, 0u);
    pair_init(&left, &right);
    right_writes = right.memory.writes;
    request = (noah_profile_split_v1_frame_t){
        .kind       = NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN,
        .status     = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .descriptor = committed_descriptor(&left, 16u, 0u),
    };
    assert(noah_profile_split_v1_frame_encode(&request, request_wire));
    assert(noah_profile_split_reconciler_receive(&right.reconciler, request_wire, sizeof(request_wire), response_wire, sizeof(response_wire)));
    assert(noah_profile_split_reconciler_scan_mode(&right.reconciler, false, 0u, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY));
    {
        noah_profile_split_descriptor_t provisional;
        noah_profile_split_authority_status_t authority;

        assert(noah_profile_split_reconciler_provisional_peer_descriptor(&right.reconciler, &provisional));
        assert(provisional.generation == request.descriptor.generation);
        assert(provisional.payload_digest == request.descriptor.payload_digest);
        assert(!right.reconciler.peer_descriptor.readable);
        assert(noah_profile_split_authority_status(noah_profile_split_reconciler_authority(&right.reconciler), &authority));
        assert(authority.state == NOAH_PROFILE_SPLIT_AUTHORITY_PEER_UNREADABLE);
    }
    assert(noah_profile_split_reconciler_receive(&right.reconciler, request_wire, sizeof(request_wire), response_wire, sizeof(response_wire)));
    assert(noah_profile_split_v1_frame_decode(response_wire, sizeof(response_wire), &response));
    assert(response.kind == NOAH_PROFILE_SPLIT_V1_ACK && response.status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY);
    assert(right.memory.writes == right_writes);
    assert(noah_profile_candidate_store_backend_admission_owner(&right.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
}

static uint32_t run_prepared_push_until_ready_at(half_t *sender, half_t *receiver, const noah_profile_split_descriptor_t *descriptor, staged_source_t *source, uint32_t start_at) {
    noah_profile_split_descriptor_t prepared;

    assert(noah_profile_split_reconciler_prepared_push_begin(&sender->reconciler, descriptor, source, staged_read));
    assert(noah_profile_split_reconciler_prepared_push_begin(&sender->reconciler, descriptor, source, staged_read));
    for (uint32_t scan = 0u; scan < MAX_SCANS; scan++) {
        uint32_t now = start_at + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(sender, true, now);
        assert_one_scan_budget(receiver, false, now);
        if (noah_profile_split_reconciler_prepared_push_ready(&sender->reconciler, &prepared)) {
            assert(prepared.generation == descriptor->generation);
            assert(prepared.payload_digest == descriptor->payload_digest);
            return now;
        }
    }
    assert(!"prepared push did not reach barrier");
    return start_at;
}

static void run_prepared_push_until_ready(half_t *sender, half_t *receiver, const noah_profile_split_descriptor_t *descriptor, staged_source_t *source) {
    (void)run_prepared_push_until_ready_at(sender, receiver, descriptor, source, 10000u);
}

static void test_prepared_push_collects_expected_mailbox_ack_without_failure_backoff(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint32_t                        exchanges;
    uint32_t                        started_at = 10000u;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = committed_descriptor(&right, 19u, 1u);
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));

    exchanges = right.link.exchanges;
    assert_one_scan_budget(&right, true, started_at);
    assert(right.link.exchanges == exchanges + 1u);
    assert(right.reconciler.next_attempt_at == started_at + NOAH_PROFILE_SPLIT_ADMISSION_RETRY_MS);
    assert(right.reconciler.retry_ms == NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS * 2u);
    assert(!noah_profile_split_reconciler_scan(&right.reconciler, true, started_at + NOAH_PROFILE_SPLIT_ADMISSION_RETRY_MS - 1u));
    assert(right.link.exchanges == exchanges + 1u);

    assert_one_scan_budget(&left, false, started_at);
    assert_one_scan_budget(&right, true, started_at + NOAH_PROFILE_SPLIT_ADMISSION_RETRY_MS);
    assert(right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_READ);
    assert(right.reconciler.retry_ms == NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS);
}

static void test_prepared_push_pauses_before_commit_then_authorizes(void) {
    half_t                           left;
    half_t                           right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                  source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint32_t                         exchanges_at_barrier;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = committed_descriptor(&right, 20u, 1u);

    run_prepared_push_until_ready(&right, &left, &descriptor, &source);
    assert(source.reads == 1u);
    assert(left.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
    assert(noah_profile_peer_store_backend_state(&left.peer_store) == NOAH_PROFILE_PEER_STORE_RECEIVING);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_PEER);

    exchanges_at_barrier = right.link.exchanges;
    assert(!noah_profile_split_reconciler_scan(&right.reconciler, true, 20000u));
    assert(right.link.exchanges == exchanges_at_barrier);
    assert(left.store.committed.slot == NOAH_PROFILE_SLOT_NONE);

    {
        noah_profile_split_descriptor_t wrong = descriptor;
        wrong.generation++;
        assert(!noah_profile_split_reconciler_prepared_push_authorize_commit(&right.reconciler, &wrong));
    }
    assert(noah_profile_split_reconciler_prepared_push_authorize_commit(&right.reconciler, &descriptor));
    assert(noah_profile_split_reconciler_prepared_push_authorize_commit(&right.reconciler, &descriptor));

    for (uint32_t scan = 0u; scan < MAX_SCANS && left.store.committed.slot == NOAH_PROFILE_SLOT_NONE; scan++) {
        uint32_t now = 21000u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(left.store.committed.slot != NOAH_PROFILE_SLOT_NONE);
    assert(left.store.committed.generation == descriptor.generation);
    assert(left.store.committed.payload_digest == descriptor.payload_digest);
}

static void test_prepared_push_cancel_aborts_peer_lease(void) {
    half_t                           left;
    half_t                           right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                  source = {.bytes = empty_profile, .length = sizeof(empty_profile)};

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = committed_descriptor(&right, 21u, 1u);
    run_prepared_push_until_ready(&right, &left, &descriptor, &source);

    assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &descriptor));
    assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &descriptor));
    for (uint32_t scan = 0u; scan < MAX_SCANS && right.reconciler.prepared_push_active; scan++) {
        uint32_t now = 30000u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(!right.reconciler.prepared_push_active);
    assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &descriptor));
    assert(left.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
}

static void test_maximum_candidate_prepares_within_owner_no_progress_window(void) {
    half_t                           left;
    half_t                           right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                  source = {.bytes = max_profile, .length = sizeof(max_profile)};
    uint32_t                         start_at = 100000u;
    uint32_t                         prepared_at;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor                         = committed_descriptor(&right, 32u, 1u);
    descriptor.payload_length          = sizeof(max_profile);
    descriptor.payload_crc32           = payload_crc(max_profile, sizeof(max_profile));
    descriptor.payload_digest          = payload_digest(max_profile, sizeof(max_profile));

    prepared_at = run_prepared_push_until_ready_at(&right, &left, &descriptor, &source, start_at);
    assert((uint32_t)(prepared_at - start_at) < MAX_PREPARE_TIME_MS);
    assert(source.reads == (sizeof(max_profile) + NOAH_PROFILE_SPLIT_V1_CHUNK_MAX - 1u) / NOAH_PROFILE_SPLIT_V1_CHUNK_MAX);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_PEER);

    assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &descriptor));
    for (uint32_t scan = 0u; scan < MAX_SCANS && right.reconciler.prepared_push_active; scan++) {
        uint32_t now = prepared_at + NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(!right.reconciler.prepared_push_active);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
}

static void test_prepared_push_restarts_across_both_half_role_changes(void) {
    half_t                           left;
    half_t                           right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                  source = {.bytes = empty_profile, .length = sizeof(empty_profile)};

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = committed_descriptor(&right, 23u, 1u);
    run_prepared_push_until_ready(&right, &left, &descriptor, &source);

    // Both physical halves swap roles. The new master discards its
    // pre-marker receiver lease; the staged-source owner remains fail-closed
    // and is no longer allowed to report the old handshake as ready.
    assert(!noah_profile_split_reconciler_scan(&right.reconciler, false, 40000u));
    assert(noah_profile_split_reconciler_scan(&left.reconciler, true, 40000u));
    assert(!noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL));
    assert(!right.reconciler.master);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);

    // Roles return and the sender replays PREPARE_BEGIN/chunks to rebuild the
    // exact receiver lease before the owner can authorize durability.
    for (uint32_t scan = 0u; scan < MAX_SCANS && !noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL); scan++) {
        uint32_t now = 41000u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&left, false, now);
        assert_one_scan_budget(&right, true, now);
    }
    assert(noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL));

    assert(noah_profile_split_reconciler_prepared_push_authorize_commit(&right.reconciler, &descriptor));
    assert(right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_COMMIT);

    // Swap both halves again after local durability authorization. The
    // reconciler remembers authorization but rebuilds the discarded lease;
    // payload completion therefore proceeds directly back to PUSH_COMMIT.
    assert(!noah_profile_split_reconciler_scan(&right.reconciler, false, 50000u));
    assert(noah_profile_split_reconciler_scan(&left.reconciler, true, 50000u));
    assert(right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BEGIN);
    assert(noah_profile_split_reconciler_prepared_push_authorize_commit(&right.reconciler, &descriptor));

    for (uint32_t scan = 0u; scan < MAX_SCANS && left.store.committed.slot == NOAH_PROFILE_SLOT_NONE; scan++) {
        uint32_t now = 51000u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&left, false, now);
        assert_one_scan_budget(&right, true, now);
    }
    assert(left.store.committed.slot != NOAH_PROFILE_SLOT_NONE);
    assert(left.store.committed.generation == descriptor.generation);
}

static void test_receiver_prepare_lease_expires_when_sender_vanishes(void) {
    half_t                           left;
    half_t                           right;
    noah_profile_split_descriptor_t descriptor;
    noah_profile_split_descriptor_t provisional;
    staged_source_t                  source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint32_t                         expired_at;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = committed_descriptor(&right, 24u, 1u);
    run_prepared_push_until_ready(&right, &left, &descriptor, &source);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_PEER);
    assert(noah_profile_split_reconciler_provisional_peer_descriptor(&left.reconciler, &provisional));

    expired_at = left.reconciler.last_peer_activity_at + NOAH_PROFILE_SPLIT_PREPARE_LEASE_MS;
    assert(noah_profile_split_reconciler_scan(&left.reconciler, false, expired_at));
    assert(noah_profile_peer_store_backend_state(&left.peer_store) == NOAH_PROFILE_PEER_STORE_IDLE);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    assert(left.reconciler.transfer_owner == NOAH_PROFILE_SPLIT_TRANSFER_NONE);
    assert(!noah_profile_split_reconciler_provisional_peer_descriptor(&left.reconciler, &provisional));

    // If the sender returns after authorizing durability, the receiver's
    // short-offset BUSY makes it rebuild the expired lease before commit.
    assert(noah_profile_split_reconciler_prepared_push_authorize_commit(&right.reconciler, &descriptor));
    for (uint32_t scan = 0u; scan < MAX_SCANS && left.store.committed.slot == NOAH_PROFILE_SLOT_NONE; scan++) {
        uint32_t now = expired_at + NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(left.store.committed.slot != NOAH_PROFILE_SLOT_NONE);
    assert(left.store.committed.generation == descriptor.generation);
}

static void test_prepare_authorize_and_cancel_are_immediate_after_timer_high_bit(void) {
    half_t                           left;
    half_t                           right;
    noah_profile_split_descriptor_t cancel_descriptor;
    noah_profile_split_descriptor_t commit_descriptor;
    staged_source_t                  source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint32_t                         now;
    uint32_t                         exchanges;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    cancel_descriptor = committed_descriptor(&right, 25u, 1u);

    now       = UINT32_C(0x80000020);
    exchanges = right.link.exchanges;
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &cancel_descriptor, &source, staged_read));
    assert_one_scan_budget(&right, true, now);
    assert(right.link.exchanges == exchanges + 1u);
    assert_one_scan_budget(&left, false, now);
    for (uint32_t scan = 1u; scan < MAX_SCANS && !noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL); scan++) {
        now = UINT32_C(0x80000020) + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL));

    exchanges = right.link.exchanges;
    assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &cancel_descriptor));
    assert_one_scan_budget(&right, true, now);
    assert(right.link.exchanges == exchanges + 1u);
    assert_one_scan_budget(&left, false, now);
    for (uint32_t scan = 1u; scan < MAX_SCANS && right.reconciler.prepared_push_active; scan++) {
        now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(!right.reconciler.prepared_push_active);

    commit_descriptor = committed_descriptor(&right, 26u, 1u);
    now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
    (void)run_prepared_push_until_ready_at(&right, &left, &commit_descriptor, &source, now);
    exchanges = right.link.exchanges;
    assert(noah_profile_split_reconciler_prepared_push_authorize_commit(&right.reconciler, &commit_descriptor));
    assert_one_scan_budget(&right, true, now);
    assert(right.link.exchanges == exchanges + 1u);
}

static void test_cancel_after_prepare_rejection_is_idempotent(void) {
    half_t                           left;
    half_t                           right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                  source = {.bytes = empty_profile, .length = sizeof(empty_profile)};

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = committed_descriptor(&right, 27u, 1u);
    descriptor.compiled_default_digest ^= 1u;
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
    for (uint32_t scan = 0u; scan < 8u && right.reconciler.state != NOAH_PROFILE_SPLIT_RECONCILER_STOPPED; scan++) {
        uint32_t now = 60000u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED);
    assert(right.reconciler.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_INCOMPATIBLE);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);

    assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &descriptor));
    assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &descriptor));
    for (uint32_t scan = 0u; scan < MAX_SCANS && right.reconciler.prepared_push_active; scan++) {
        uint32_t now = 61000u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(!right.reconciler.prepared_push_active);
    assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &descriptor));
}

static void test_cancel_after_dropped_prepare_begin_releases_or_noops(void) {
    for (uint8_t drop_before = 0u; drop_before <= 1u; drop_before++) {
        half_t                           left;
        half_t                           right;
        noah_profile_split_descriptor_t descriptor;
        staged_source_t                  source = {.bytes = empty_profile, .length = sizeof(empty_profile)};

        half_storage_init(&left);
        half_storage_init(&right);
        pair_init(&left, &right);
        run_pair_until_converged(&left, &right, false);
        descriptor = committed_descriptor(&right, (uint32_t)(28u + drop_before), 1u);
        if (drop_before) {
            right.link.drop_before_delivery_exchange = right.link.exchanges + 1u;
        } else {
            right.link.drop_after_delivery_exchange = right.link.exchanges + 1u;
        }
        assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
        assert_one_scan_budget(&right, true, 70000u);
        assert(right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BEGIN);
        assert_one_scan_budget(&left, false, 70000u);

        assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &descriptor));
        for (uint32_t scan = 0u; scan < MAX_SCANS && right.reconciler.prepared_push_active; scan++) {
            uint32_t now = 70100u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

            assert_one_scan_budget(&right, true, now);
            assert_one_scan_budget(&left, false, now);
        }
        assert(!right.reconciler.prepared_push_active);
        assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    }
}

static void test_cancel_refuses_matching_durable_peer(void) {
    half_t                           left;
    half_t                           right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                  source = {.bytes = empty_profile, .length = sizeof(empty_profile)};

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = committed_descriptor(&left, 31u, 0u);
    install_profile(&left, descriptor.generation, descriptor.origin_half);

    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
    for (uint32_t scan = 0u; scan < MAX_SCANS && !noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL); scan++) {
        uint32_t now = 80000u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL));
    assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &descriptor));
    for (uint32_t scan = 0u; scan < 8u && !right.reconciler.prepared_cancel_refused; scan++) {
        uint32_t now = 81000u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(right.reconciler.prepared_cancel_refused);
    assert(right.reconciler.prepared_push_active);
    assert(!noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &descriptor));
    assert(left.store.committed.generation == descriptor.generation);
}

static void run_crossed_begin_loser_abort(bool left_sends_first) {
    half_t                           left;
    half_t                           right;
    noah_profile_split_descriptor_t left_descriptor;
    noah_profile_split_descriptor_t right_descriptor;
    noah_profile_split_descriptor_t observed;
    staged_source_t                  left_source  = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    staged_source_t                  right_source = {.bytes = empty_profile, .length = sizeof(empty_profile)};

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    left.link.connected  = false;
    right.link.connected = false;
    assert_one_scan_budget(&left, true, 90000u);
    assert_one_scan_budget(&right, true, 90000u);
    left.link.connected  = true;
    right.link.connected = true;
    left_descriptor      = committed_descriptor(&left, 32u, 0u);
    right_descriptor     = committed_descriptor(&right, 32u, 1u);
    assert(noah_profile_split_reconciler_prepared_push_begin(&left.reconciler, &left_descriptor, &left_source, staged_read));
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &right_descriptor, &right_source, staged_read));

    if (left_sends_first) {
        (void)noah_profile_split_reconciler_scan_mode(&left.reconciler, true, 90100u, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY);
        (void)noah_profile_split_reconciler_scan_mode(&right.reconciler, true, 90100u, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY);
        (void)noah_profile_split_reconciler_scan_mode(&right.reconciler, true, 90150u, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY);
        (void)noah_profile_split_reconciler_scan_mode(&left.reconciler, true, 90150u, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY);
    } else {
        (void)noah_profile_split_reconciler_scan_mode(&right.reconciler, true, 90100u, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY);
        (void)noah_profile_split_reconciler_scan_mode(&left.reconciler, true, 90100u, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY);
        (void)noah_profile_split_reconciler_scan_mode(&left.reconciler, true, 90150u, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY);
        (void)noah_profile_split_reconciler_scan_mode(&right.reconciler, true, 90150u, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY);
    }
    assert(noah_profile_split_reconciler_provisional_peer_descriptor(&left.reconciler, &observed));
    assert(observed.origin_half == 1u);
    assert(noah_profile_split_reconciler_provisional_peer_descriptor(&right.reconciler, &observed));
    assert(observed.origin_half == 0u);

    // Stable physical-origin arbitration selects left. Right's loser ABORT
    // must be acknowledged even while left remains HOST/convergence-only.
    assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &right_descriptor));
    for (uint32_t scan = 0u; scan < MAX_SCANS && right.reconciler.prepared_push_active; scan++) {
        uint32_t now = 90200u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        (void)noah_profile_split_reconciler_scan_mode(&right.reconciler, true, now, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY);
        (void)noah_profile_split_reconciler_scan_mode(&left.reconciler, true, now, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY);
    }
    assert(!right.reconciler.prepared_push_active);
    assert(left.reconciler.prepared_push_active);
}

static void test_crossed_prepare_begin_loser_abort_does_not_deadlock(void) {
    run_crossed_begin_loser_abort(true);
    run_crossed_begin_loser_abort(false);
}

static void test_refresh_publishes_new_local_authority_during_backoff(void) {
    half_t                                 left;
    half_t                                 right;
    noah_profile_split_authority_status_t authority;
    uint32_t                               before_publications;
    uint32_t                               before_exchanges;
    uint32_t                               before_deadline;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    assert(noah_profile_split_authority_status(noah_profile_split_reconciler_authority(&right.reconciler), &authority));
    before_publications = authority.publication_count;
    before_exchanges    = right.link.exchanges;
    before_deadline     = right.reconciler.next_attempt_at == 0u ? 0u : right.reconciler.next_attempt_at - 1u;

    install_profile(&right, 22u, 1u);
    assert(!noah_profile_split_reconciler_scan(&right.reconciler, true, before_deadline));
    assert(right.link.exchanges == before_exchanges);
    assert(noah_profile_split_authority_status(noah_profile_split_reconciler_authority(&right.reconciler), &authority));
    assert(authority.publication_count > before_publications);
    assert(authority.local.generation == 22u);
    assert(authority.state == NOAH_PROFILE_SPLIT_AUTHORITY_LOCAL_NEWER);

    assert(noah_profile_split_reconciler_refresh_authority(&right.reconciler));
    assert(noah_profile_split_authority_status(noah_profile_split_reconciler_authority(&right.reconciler), &authority));
    assert(authority.local.generation == 22u);
}

// Steady-state cost probe: a converged pair scanned across real elapsed time
// at a realistic 450 Hz main-loop rate. A converged reconciler should only
// wake on its NOAH_PROFILE_SPLIT_POLL_MS deadline, so the split RPC and
// EEPROM budgets must scale with elapsed seconds, not with scan count.
static void test_converged_steady_state_cost_is_bounded_by_poll_deadline(void) {
    half_t   left;
    half_t   right;
    uint32_t reads;
    uint32_t writes;
    uint32_t exchanges;
    uint32_t start_ms;
    uint32_t elapsed_ms = 5000u;
    uint32_t scans      = 2250u; // 5 s at ~450 scans/s
    uint32_t expected_polls;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);

    start_ms  = 1000000u;
    reads     = left.memory.reads + right.memory.reads;
    writes    = left.memory.writes + right.memory.writes;
    exchanges = left.link.exchanges + right.link.exchanges;

    for (uint32_t scan = 0u; scan < scans; scan++) {
        uint32_t now = start_ms + (scan * elapsed_ms) / scans;

        (void)noah_profile_split_reconciler_scan(&right.reconciler, true, now);
        (void)noah_profile_split_reconciler_scan(&left.reconciler, false, now);
    }

    exchanges = (left.link.exchanges + right.link.exchanges) - exchanges;
    reads     = (left.memory.reads + right.memory.reads) - reads;
    writes    = (left.memory.writes + right.memory.writes) - writes;
    expected_polls = elapsed_ms / NOAH_PROFILE_SPLIT_POLL_MS;

    printf("steady state: %u scans over %u ms -> %u exchanges, %u reads, %u writes (expected <= %u exchanges)\n",
           (unsigned)scans, (unsigned)elapsed_ms, (unsigned)exchanges, (unsigned)reads, (unsigned)writes,
           (unsigned)(expected_polls + 1u));

    assert(writes == 0u);
    assert(exchanges <= expected_polls + 1u);
}

static void test_idle_scans_do_not_republish_unchanged_metadata(void) {
    half_t   left;
    half_t   right;
    uint8_t  left_metadata_sequence;
    uint8_t  right_metadata_sequence;
    uint32_t left_authority_publications;
    uint32_t right_authority_publications;
    uint32_t left_reads;
    uint32_t right_reads;
    uint32_t left_writes;
    uint32_t right_writes;
    uint32_t left_exchanges;
    uint32_t right_exchanges;
    uint32_t before_deadline;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);

    left_metadata_sequence        = left.reconciler.metadata_sequence;
    right_metadata_sequence       = right.reconciler.metadata_sequence;
    left_authority_publications   = left.reconciler.authority.status.publication_count;
    right_authority_publications  = right.reconciler.authority.status.publication_count;
    left_reads                    = left.memory.reads;
    right_reads                   = right.memory.reads;
    left_writes                   = left.memory.writes;
    right_writes                  = right.memory.writes;
    left_exchanges                = left.link.exchanges;
    right_exchanges               = right.link.exchanges;
    before_deadline               = right.reconciler.next_attempt_at == 0u ? 0u : right.reconciler.next_attempt_at - 1u;

    for (uint32_t scan = 0u; scan < MAX_SCANS; scan++) {
        assert(!noah_profile_split_reconciler_scan(&left.reconciler, false, before_deadline));
        assert(!noah_profile_split_reconciler_scan(&right.reconciler, true, before_deadline));
    }
    assert(left.reconciler.metadata_sequence == left_metadata_sequence);
    assert(right.reconciler.metadata_sequence == right_metadata_sequence);
    assert(left.reconciler.authority.status.publication_count == left_authority_publications);
    assert(right.reconciler.authority.status.publication_count == right_authority_publications);
    assert(left.memory.reads == left_reads && right.memory.reads == right_reads);
    assert(left.memory.writes == left_writes && right.memory.writes == right_writes);
    assert(left.link.exchanges == left_exchanges && right.link.exchanges == right_exchanges);
}

int main(void) {
    test_compiled_convergence();
    test_newer_master_pushes_exact_record();
    test_newer_slave_is_pulled_without_role_authority();
    test_disconnect_invalidates_then_reconnects();
    test_corrupt_response_restarts_fail_closed();
    test_lost_reply_after_admission_is_idempotent();
    test_role_change_restarts_and_preserves_physical_origin();
    test_concurrent_commit_stops_without_overwrite();
    test_same_tuple_corruption_stops_without_overwrite();
    test_incompatible_firmware_stops_without_write();
    test_max_generation_transfers_without_local_increment();
    test_convergence_only_pushes_host_record_without_importing();
    test_convergence_only_refuses_newer_peer_import();
    test_convergence_only_answers_inbound_prepare_busy_without_storage();
    test_prepared_push_collects_expected_mailbox_ack_without_failure_backoff();
    test_prepared_push_pauses_before_commit_then_authorizes();
    test_prepared_push_cancel_aborts_peer_lease();
    test_maximum_candidate_prepares_within_owner_no_progress_window();
    test_prepared_push_restarts_across_both_half_role_changes();
    test_receiver_prepare_lease_expires_when_sender_vanishes();
    test_prepare_authorize_and_cancel_are_immediate_after_timer_high_bit();
    test_cancel_after_prepare_rejection_is_idempotent();
    test_cancel_after_dropped_prepare_begin_releases_or_noops();
    test_cancel_refuses_matching_durable_peer();
    test_crossed_prepare_begin_loser_abort_does_not_deadlock();
    test_refresh_publishes_new_local_authority_during_backoff();
    test_idle_scans_do_not_republish_unchanged_metadata();
    test_converged_steady_state_cost_is_bounded_by_poll_deadline();
    puts("profile split reconciler host tests passed");
    return 0;
}
