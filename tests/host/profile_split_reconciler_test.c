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
};

static const uint8_t empty_profile[] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};

typedef struct {
    uint8_t  bytes[NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE];
    uint32_t reads;
    uint32_t writes;
} memory_t;

typedef struct half half_t;

typedef struct {
    half_t  *peer;
    uint32_t exchanges;
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

static bool exchange(void *context, const uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], uint8_t response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    link_t *link = context;

    link->exchanges++;
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
    puts("profile split reconciler host tests passed");
    return 0;
}
