#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/profile/split/profile_split_reconciler.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"

enum {
    ACTION_ABI_DIGEST   = UINT32_C(0x11223344),
    MAX_SCANS           = 4096u,
    MAX_PREPARE_TIME_MS = 60000u,
};

static const uint8_t empty_profile[] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};
static const uint8_t max_profile[NOAH_PROFILE_CANDIDATE_V1_MAX_BLOB_SIZE];

typedef struct {
    uint8_t  bytes[NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE];
    uint32_t reads;
    uint32_t writes;
    uint32_t fail_reads;  // the next reads fail, as a flash read that errors would
    uint32_t fail_writes; // likewise for writes
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
    // This exchange answers with the peer's previous reply, well formed, and
    // never delivers the request: QMK's RPC does this when the peer skips its
    // callback and the response buffer still holds the last answer.
    uint32_t replay_previous_response_exchange;
    // This exchange answers with `injected` and never delivers the request.
    uint32_t                      inject_response_exchange;
    noah_profile_split_v1_frame_t injected;
    uint8_t                       previous_response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    bool                          connected;
    bool                          corrupt_next_response;
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
    if (memory->fail_reads) {
        memory->fail_reads--;
        return false;
    }
    if (!target || length == 0u || (uint32_t)address + length > sizeof(memory->bytes)) {
        return false;
    }
    memcpy(target, &memory->bytes[address], length);
    return true;
}

static bool memory_write(void *context, uint16_t address, const uint8_t *source, uint16_t length) {
    memory_t *memory = context;

    memory->writes++;
    if (memory->fail_writes) {
        memory->fail_writes--;
        return false;
    }
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
        descriptor->logical                 = record->format_version == NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL;
    }
    return true;
}

static bool local_binding(void *context, const noah_profile_split_descriptor_t *descriptor, uint32_t *via_generation, uint32_t *via_digest) {
    half_t *half = context;

    if (!half || !descriptor || !descriptor->logical || !via_generation || !via_digest || half->store.committed.slot == NOAH_PROFILE_SLOT_NONE || half->store.committed.generation != descriptor->generation || half->store.committed.payload_digest != descriptor->payload_digest || half->store.committed.format_version != NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL) {
        return false;
    }
    *via_generation = half->store.committed.via_generation;
    *via_digest     = half->store.committed.via_digest;
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
    if (link->replay_previous_response_exchange == link->exchanges) {
        link->replay_previous_response_exchange = 0u;
        memcpy(response, link->previous_response, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
        return true;
    }
    if (link->inject_response_exchange == link->exchanges) {
        link->inject_response_exchange = 0u;
        return noah_profile_split_v1_frame_encode(&link->injected, response);
    }
    if (!link->connected || !link->peer || !noah_profile_split_reconciler_receive(&link->peer->reconciler, request, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, response, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE)) {
        return false;
    }
    memcpy(link->previous_response, response, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
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
    noah_profile_store_init(&half->store, (noah_profile_store_io_t){.read = memory_read, .write = memory_write, .context = &half->memory},
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
    noah_profile_split_descriptor_t  descriptor = committed_descriptor(half, generation, origin);
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

static void install_logical_profile(half_t *half, uint32_t generation, uint8_t origin, uint32_t via_generation, uint32_t via_digest) {
    noah_profile_split_descriptor_t  descriptor = committed_descriptor(half, generation, origin);
    noah_profile_peer_store_result_t result;

    descriptor.logical = true;
    assert(noah_profile_peer_store_backend_begin_logical(&half->peer_store, &descriptor, via_generation, via_digest) == NOAH_PROFILE_PEER_STORE_OK);
    assert(noah_profile_peer_store_backend_write(&half->peer_store, descriptor.generation, descriptor.payload_digest, 0u, empty_profile, sizeof(empty_profile)) == NOAH_PROFILE_PEER_STORE_OK);
    result = noah_profile_peer_store_backend_commit_begin(&half->peer_store, &descriptor);
    while (result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS) {
        result = noah_profile_peer_store_backend_step(&half->peer_store, NOAH_PROFILE_SPLIT_V1_CHUNK_MAX);
    }
    assert(result == NOAH_PROFILE_PEER_STORE_OK);
}

static void pair_init(half_t *left, half_t *right) {
    noah_profile_split_reconciler_config_t left_config;
    noah_profile_split_reconciler_config_t right_config;

    left->link.peer  = right;
    right->link.peer = left;
    left_config      = (noah_profile_split_reconciler_config_t){
        .local_context     = left,
        .local_descriptor  = local_descriptor,
        .local_read        = local_read,
        .local_binding     = local_binding,
        .transport_context = &left->link,
        .exchange          = exchange,
        .peer_store        = &left->peer_store,
    };
    right_config = (noah_profile_split_reconciler_config_t){
        .local_context     = right,
        .local_descriptor  = local_descriptor,
        .local_read        = local_read,
        .local_binding     = local_binding,
        .transport_context = &right->link,
        .exchange          = exchange,
        .peer_store        = &right->peer_store,
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
    assert(a->format_version == b->format_version);
    assert(a->via_generation == b->via_generation);
    assert(a->via_digest == b->via_digest);
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

static void test_logical_binding_survives_master_push(void) {
    half_t left;
    half_t right;

    half_storage_init(&left);
    half_storage_init(&right);
    install_logical_profile(&right, 8u, 1u, 12u, UINT32_C(0x91a2b3c4));
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    assert_records_match(&left, &right);
    assert(left.store.committed.format_version == NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL);
    assert(left.store.committed.via_generation == 12u);
    assert(left.store.committed.via_digest == UINT32_C(0x91a2b3c4));
}

static void test_logical_binding_survives_slave_pull(void) {
    half_t left;
    half_t right;

    half_storage_init(&left);
    half_storage_init(&right);
    install_logical_profile(&left, 9u, 0u, 13u, UINT32_C(0xa1b2c3d4));
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    assert_records_match(&left, &right);
    assert(right.store.committed.format_version == NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL);
    assert(right.store.committed.via_generation == 13u);
    assert(right.store.committed.via_digest == UINT32_C(0xa1b2c3d4));
}

static void test_disconnect_invalidates_then_reconnects(void) {
    half_t                                left;
    half_t                                right;
    noah_profile_split_authority_status_t status;
    uint8_t                               unresolved = 0u;

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
    half_t                                 left;
    half_t                                 right;
    noah_profile_split_reconciler_status_t status;
    uint32_t                               left_writes;
    uint32_t                               right_writes;

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
    half_t                                 left;
    half_t                                 right;
    noah_profile_split_reconciler_status_t status;
    uint32_t                               left_writes;
    uint32_t                               right_writes;

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
    half_t                                 left;
    half_t                                 right;
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
    half_t                        left;
    half_t                        right;
    noah_profile_split_v1_frame_t request;
    noah_profile_split_v1_frame_t response;
    uint8_t                       request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    uint8_t                       response_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    uint32_t                      right_writes;

    half_storage_init(&left);
    half_storage_init(&right);
    install_profile(&left, 16u, 0u);
    pair_init(&left, &right);
    right_writes = right.memory.writes;
    request      = (noah_profile_split_v1_frame_t){
        .kind       = NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN,
        .status     = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .descriptor = committed_descriptor(&left, 16u, 0u),
    };
    assert(noah_profile_split_v1_frame_encode(&request, request_wire));
    assert(noah_profile_split_reconciler_receive(&right.reconciler, request_wire, sizeof(request_wire), response_wire, sizeof(response_wire)));
    assert(noah_profile_split_reconciler_scan_mode(&right.reconciler, false, 0u, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY));
    {
        noah_profile_split_descriptor_t       provisional;
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
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint32_t                        exchanges_at_barrier;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = committed_descriptor(&right, 20u, 1u);

    run_prepared_push_until_ready(&right, &left, &descriptor, &source);
    assert(source.reads == 1u);
    assert(left.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
    assert(noah_profile_peer_store_backend_state(&left.peer_store) == NOAH_PROFILE_PEER_STORE_PREPARED);
    assert(left.store.prepared_durable);
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

// After the commit is authorized, a replayed reply must still end with the
// peer holding the copy: the sender retries in place instead of reading the
// peer from the metadata poll mid-commit.
static void test_authorized_commit_survives_one_replayed_reply(void) {
    for (uint32_t fault = 1u;; fault++) {
        half_t                          left;
        half_t                          right;
        noah_profile_split_descriptor_t descriptor;
        staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
        bool                            done   = false;

        half_storage_init(&left);
        half_storage_init(&right);
        pair_init(&left, &right);
        run_pair_until_converged(&left, &right, false);
        descriptor = committed_descriptor(&right, 20u, 1u);
        run_prepared_push_until_ready(&right, &left, &descriptor, &source);
        right.link.replay_previous_response_exchange = right.link.exchanges + fault;
        assert(noah_profile_split_reconciler_prepared_push_authorize_commit(&right.reconciler, &descriptor));
        for (uint32_t scan = 0u; scan < MAX_SCANS && !done; scan++) {
            uint32_t now = 21000u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

            assert_one_scan_budget(&right, true, now);
            assert_one_scan_budget(&left, false, now);
            done = left.store.committed.slot != NOAH_PROFILE_SLOT_NONE && !right.reconciler.prepared_push_active;
        }
        if (!done) {
            fprintf(stderr, "authorized commit wedged: replayed reply at exchange %u; sender state %u, status %u, push active %u, peer committed %u\n", (unsigned)fault, (unsigned)right.reconciler.state,
                    (unsigned)right.reconciler.last_status, (unsigned)right.reconciler.prepared_push_active, (unsigned)(left.store.committed.slot != NOAH_PROFILE_SLOT_NONE));
            assert(!"one replayed reply wedged the authorized commit");
        }
        assert(left.store.committed.generation == descriptor.generation);
        if (right.link.replay_previous_response_exchange != 0u) {
            assert(fault > 2u);
            return;
        }
    }
}

static void test_prepared_push_cancel_aborts_peer_lease(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};

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

static bool peer_cleanup_pending(const half_t *half) {
    noah_profile_split_reconciler_status_t status;

    assert(noah_profile_split_reconciler_status(&half->reconciler, &status));
    return status.peer_cleanup_pending;
}

// Scans the sender alone until its prepared push is released, and returns how
// long that took. The peer is deliberately not scanned: it is either
// unreachable or wedged in a state that answers every ABORT with BUSY.
static uint32_t run_sender_until_prepared_push_released(half_t *sender, uint32_t start_at) {
    uint32_t now = start_at;

    for (uint32_t scan = 0u; scan < MAX_SCANS && sender->reconciler.prepared_push_active; scan++) {
        now = start_at + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        assert_one_scan_budget(sender, true, now);
    }
    assert(!sender->reconciler.prepared_push_active);
    return now - start_at;
}

// The keyboard wedged in PREPARING_PEER on 2026-09-23: the peer stopped
// acknowledging, the host cancelled, and the peer ABORT was retried forever.
// Every host-owned wait above this one depends on it ending.
static void test_prepared_abort_is_bounded_when_peer_goes_silent(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    noah_profile_split_descriptor_t next;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint32_t                        elapsed;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = committed_descriptor(&right, 23u, 1u);
    run_prepared_push_until_ready(&right, &left, &descriptor, &source);
    assert(noah_profile_peer_store_backend_state(&left.peer_store) == NOAH_PROFILE_PEER_STORE_PREPARED);

    right.link.connected = false;
    assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &descriptor));
    elapsed = run_sender_until_prepared_push_released(&right, 40000u);
    assert(elapsed >= NOAH_PROFILE_SPLIT_PREPARED_ABORT_TIMEOUT_MS);
    assert(elapsed < NOAH_PROFILE_SPLIT_PREPARED_ABORT_TIMEOUT_MS + 2u * NOAH_PROFILE_SPLIT_RETRY_MAX_MS);

    // Released locally, but the peer never confirmed: it may still hold the
    // provisional lease, so the sender remembers and says so.
    assert(peer_cleanup_pending(&right));
    assert(left.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
    next = committed_descriptor(&right, 24u, 1u);
    assert(!noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &next, &source, staged_read));

    // Once the peer answers again, the remembered ABORT releases its lease.
    right.link.connected = true;
    for (uint32_t scan = 0u; scan < MAX_SCANS && peer_cleanup_pending(&right); scan++) {
        uint32_t now = 60000u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(!peer_cleanup_pending(&right));
    assert(noah_profile_peer_store_backend_state(&left.peer_store) == NOAH_PROFILE_PEER_STORE_IDLE);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    assert(left.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
    run_pair_until_converged(&left, &right, false);
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &next, &source, staged_read));
}

// A peer store in RECONCILE_REQUIRED (durability unknown) answers every ABORT
// with BUSY until it restarts. The sender still releases in bounded time,
// keeps polling metadata, and never reports the peer as cleaned up.
static void test_prepared_abort_is_bounded_when_peer_stays_busy(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint32_t                        elapsed;
    uint32_t                        exchanges;
    uint32_t                        writes;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = committed_descriptor(&right, 25u, 1u);
    run_prepared_push_until_ready(&right, &left, &descriptor, &source);
    left.peer_store.state = NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED;

    assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &descriptor));
    for (uint32_t scan = 0u; scan < MAX_SCANS && right.reconciler.prepared_push_active; scan++) {
        uint32_t now = 70000u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
        elapsed = now - 70000u;
    }
    assert(!right.reconciler.prepared_push_active);
    assert(elapsed >= NOAH_PROFILE_SPLIT_PREPARED_ABORT_TIMEOUT_MS);
    assert(elapsed < NOAH_PROFILE_SPLIT_PREPARED_ABORT_TIMEOUT_MS + 2u * NOAH_PROFILE_SPLIT_RETRY_MAX_MS);
    assert(peer_cleanup_pending(&right));

    // The wedged peer keeps answering BUSY. The remembered ABORT is retried
    // at a bounded rate, metadata polling continues, and nothing is written.
    exchanges = right.link.exchanges;
    writes    = left.memory.writes + right.memory.writes;
    for (uint32_t scan = 0u; scan < 2250u; scan++) {
        uint32_t now = 90000u + (scan * 5000u) / 2250u;

        (void)noah_profile_split_reconciler_scan(&right.reconciler, true, now);
        (void)noah_profile_split_reconciler_scan(&left.reconciler, false, now);
    }
    assert(peer_cleanup_pending(&right));
    assert(right.link.exchanges > exchanges);
    assert(right.link.exchanges - exchanges <= 2u * (5000u / NOAH_PROFILE_SPLIT_RETRY_MAX_MS) + 2u);
    assert(left.memory.writes + right.memory.writes == writes);
    assert(left.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
}

static void test_maximum_candidate_resolves_within_owner_no_progress_window(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source      = {.bytes = max_profile, .length = sizeof(max_profile)};
    uint32_t                        start_at    = 100000u;
    uint32_t                        resolved_at = start_at;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor                = committed_descriptor(&right, 32u, 1u);
    descriptor.payload_length = sizeof(max_profile);
    descriptor.payload_crc32  = payload_crc(max_profile, sizeof(max_profile));
    descriptor.payload_digest = payload_digest(max_profile, sizeof(max_profile));

    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
    for (uint32_t scan = 0u; scan < MAX_SCANS; scan++) {
        resolved_at = start_at + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        assert_one_scan_budget(&right, true, resolved_at);
        assert_one_scan_budget(&left, false, resolved_at);
        if (right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED || noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL)) {
            break;
        }
    }
    // This deliberately maximal all-zero payload is semantically invalid,
    // but transfer plus whole-payload validation must still reach a terminal
    // result before the owner's no-progress guard can fire.
    assert(right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED);
    assert(right.reconciler.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_VALIDATION_ERROR);
    assert((uint32_t)(resolved_at - start_at) < MAX_PREPARE_TIME_MS);
    assert(source.reads == (sizeof(max_profile) + NOAH_PROFILE_SPLIT_V1_CHUNK_MAX - 1u) / NOAH_PROFILE_SPLIT_V1_CHUNK_MAX);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
}

// One lost exchange anywhere in a full-size copy, whether the request never
// arrived or its reply was lost after the peer acted on it, must still let the
// transfer finish. A copy that retries one chunk forever wedges Apply.
static void test_prepared_push_survives_one_lost_exchange_anywhere(void) {
    uint32_t exchanges_clean = 0u;

    for (uint8_t after = 0u; after <= 1u; after++) {
        for (uint32_t drop = 1u;; drop++) {
            half_t                          left;
            half_t                          right;
            noah_profile_split_descriptor_t descriptor;
            staged_source_t                 source   = {.bytes = max_profile, .length = sizeof(max_profile)};
            uint32_t                        start_at = 100000u;
            uint32_t                        base;
            bool                            done = false;

            half_storage_init(&left);
            half_storage_init(&right);
            pair_init(&left, &right);
            run_pair_until_converged(&left, &right, false);
            descriptor                = committed_descriptor(&right, 32u, 1u);
            descriptor.payload_length = sizeof(max_profile);
            descriptor.payload_crc32  = payload_crc(max_profile, sizeof(max_profile));
            descriptor.payload_digest = payload_digest(max_profile, sizeof(max_profile));
            base                      = right.link.exchanges;
            if (after)
                right.link.drop_after_delivery_exchange = base + drop;
            else
                right.link.drop_before_delivery_exchange = base + drop;
            assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
            for (uint32_t scan = 0u; scan < MAX_SCANS * 2u && !done; scan++) {
                uint32_t now = start_at + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
                assert_one_scan_budget(&right, true, now);
                assert_one_scan_budget(&left, false, now);
                done = right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED || noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL);
            }
            if (!done) {
                fprintf(stderr, "copy wedged: exchange %u lost %s delivery; sender state %u, offset %u of %u, peer store state %u, retries %u\n", (unsigned)drop, after ? "after" : "before",
                        (unsigned)right.reconciler.state, (unsigned)right.reconciler.transfer_offset, (unsigned)descriptor.payload_length, (unsigned)noah_profile_peer_store_backend_state(&left.peer_store), (unsigned)right.reconciler.retry_count);
                assert(!"a single lost exchange wedged the copy");
            }
            // The fault never fired: every exchange of a clean copy is covered.
            if ((after ? right.link.drop_after_delivery_exchange : right.link.drop_before_delivery_exchange) != 0u) {
                if (!exchanges_clean) exchanges_clean = drop;
                break;
            }
        }
    }
    assert(exchanges_clean > 2u * (sizeof(max_profile) / NOAH_PROFILE_SPLIT_V1_CHUNK_MAX));
}

// Sends a full-size prepared copy with one fault planted at exchange `fault`
// (counted from the start of the copy) and reports whether the fault fired.
// The copy must end on its own: at the barrier, or stopped with the
// receiver's real verdict on this all-zero payload. A copy that parks while
// the push is still active wedges Apply: the owner can no longer cancel it.
static bool run_prepared_push_with_fault(uint32_t fault, bool replay, const noah_profile_split_v1_frame_t *injected) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source   = {.bytes = max_profile, .length = sizeof(max_profile)};
    uint32_t                        start_at = 100000u;
    bool                            done     = false;
    bool                            fired;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor                = committed_descriptor(&right, 32u, 1u);
    descriptor.payload_length = sizeof(max_profile);
    descriptor.payload_crc32  = payload_crc(max_profile, sizeof(max_profile));
    descriptor.payload_digest = payload_digest(max_profile, sizeof(max_profile));
    if (replay) {
        right.link.replay_previous_response_exchange = right.link.exchanges + fault;
    } else {
        right.link.inject_response_exchange = right.link.exchanges + fault;
        right.link.injected                 = *injected;
    }
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
    for (uint32_t scan = 0u; scan < MAX_SCANS * 2u && !done; scan++) {
        uint32_t now = start_at + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
        done = right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED || noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL);
    }
    fired = (replay ? right.link.replay_previous_response_exchange : right.link.inject_response_exchange) == 0u;
    if (!done || (right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED && right.reconciler.last_status != NOAH_PROFILE_SPLIT_V1_STATUS_VALIDATION_ERROR)) {
        fprintf(stderr, "copy wedged: %s at exchange %u; sender state %u, status %u, offset %u of %u, push active %u\n", replay ? "replayed reply" : "injected reply", (unsigned)fault,
                (unsigned)right.reconciler.state, (unsigned)right.reconciler.last_status, (unsigned)right.reconciler.transfer_offset, (unsigned)descriptor.payload_length, (unsigned)right.reconciler.prepared_push_active);
        assert(!"one stray reply wedged the copy");
    }
    return fired;
}

// A reply to an earlier request is a lost exchange, not a verdict. Parking
// the push in the converged poll left byte 1050 of 2646 on a real keyboard
// with the host's ABORT unprocessed until it was power-cycled.
static void test_prepared_push_survives_one_replayed_reply_anywhere(void) {
    uint32_t fault = 1u;

    while (run_prepared_push_with_fault(fault, true, NULL)) {
        fault++;
    }
    assert(fault > 2u * (sizeof(max_profile) / NOAH_PROFILE_SPLIT_V1_CHUNK_MAX));
}

// Whatever sends an active push back to the metadata poll, here a stray
// STALE error that answers no request of this copy, both halves still hold
// the old profile there. The push must resume rather than read that as
// converged.
static void test_prepared_push_resumes_from_the_metadata_poll(void) {
    noah_profile_split_v1_frame_t stale = {.kind = NOAH_PROFILE_SPLIT_V1_ERROR, .status = NOAH_PROFILE_SPLIT_V1_STATUS_STALE};

    for (uint32_t fault = 1u; fault <= 8u; fault++) {
        assert(run_prepared_push_with_fault(fault, false, &stale));
    }
    assert(run_prepared_push_with_fault(60u, false, &stale));
}

// A receiver left holding a half-received lease, here because the sender
// restarted mid-copy, must release it once that copy stops arriving, even
// while the sender keeps talking. Otherwise every later copy is answered
// BUSY for as long as the halves stay connected.
static void run_stale_receive_lease(bool same_length) {
    half_t                                 left;
    half_t                                 right;
    noah_profile_split_descriptor_t        stale;
    noah_profile_split_descriptor_t        next;
    staged_source_t                        stale_source = {.bytes = max_profile, .length = sizeof(max_profile)};
    staged_source_t                        next_source  = {.bytes = same_length ? max_profile : empty_profile, .length = same_length ? sizeof(max_profile) : sizeof(empty_profile)};
    noah_profile_split_reconciler_config_t config;
    uint32_t                               now  = 100000u;
    bool                                   done = false;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    stale                = committed_descriptor(&right, 40u, 1u);
    stale.payload_length = sizeof(max_profile);
    stale.payload_crc32  = payload_crc(max_profile, sizeof(max_profile));
    stale.payload_digest = payload_digest(max_profile, sizeof(max_profile));
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &stale, &stale_source, staged_read));
    for (uint32_t scan = 0u; scan < 64u; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(noah_profile_peer_store_backend_state(&left.peer_store) == NOAH_PROFILE_PEER_STORE_RECEIVING);
    assert(noah_profile_peer_store_backend_next_offset(&left.peer_store) > sizeof(empty_profile));

    // The sender restarts: it forgets the copy without cancelling it.
    config = right.reconciler.config;
    noah_profile_split_reconciler_init(&right.reconciler, &config);
    next = committed_descriptor(&right, 41u, 1u);
    if (same_length) {
        // The field case: the new copy is as long as the stale one, so the
        // stale lease's BUSY reply stays well formed and would repeat forever.
        next.payload_length = stale.payload_length;
        next.payload_crc32  = stale.payload_crc32;
        next.payload_digest = stale.payload_digest ^ 1u;
        next_source.bytes   = max_profile;
    }
    for (uint32_t scan = 0u; scan < 64u && !noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &next, &next_source, staged_read); scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(right.reconciler.prepared_push_active);
    // While the stale lease holds, the sender can say why it waits.
    for (uint32_t scan = 0u; scan < 20u; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    {
        noah_profile_split_reconciler_status_t status;
        assert(noah_profile_split_reconciler_status(&right.reconciler, &status));
        assert(status.last_busy_reason == NOAH_PROFILE_SPLIT_V1_BUSY_OTHER_COPY);
        assert(status.last_busy_store_state == NOAH_PROFILE_PEER_STORE_RECEIVING);
        assert(status.busy_streak > 1u);
    }
    for (uint32_t scan = 0u; scan < MAX_SCANS && !done; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
        // Ready, or the whole copy arrived and was judged (the all-zero
        // payload fails validation or its digest): either way it got through.
        done = noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL) || (right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED && right.reconciler.last_status != NOAH_PROFILE_SPLIT_V1_STATUS_INVALID_FRAME);
    }
    if (!done) fprintf(stderr, "stale lease (%s length): receiver store %u owner %u | sender state %u status %u retries %u\n", same_length ? "same" : "shorter", (unsigned)noah_profile_peer_store_backend_state(&left.peer_store), (unsigned)left.reconciler.transfer_owner, (unsigned)right.reconciler.state, (unsigned)right.reconciler.last_status, (unsigned)right.reconciler.retry_count);
    assert(done && "the stale lease held the next copy off");
    assert(noah_profile_peer_store_backend_state(&left.peer_store) != NOAH_PROFILE_PEER_STORE_RECEIVING || left.peer_store.descriptor.generation == next.generation);
}

// A receiver left holding a half-received lease, here because the sender
// restarted mid-copy, must release it once that copy stops arriving, even
// while the sender keeps talking. Otherwise every later copy is answered
// BUSY for as long as the halves stay connected.
static void test_stale_receive_lease_expires_while_the_link_stays_busy(void) {
    run_stale_receive_lease(false);
    run_stale_receive_lease(true);
}

// If the receiver drops its lease mid-copy, the chunks it is then sent are
// answered BUSY. The sender must not retry one chunk forever: it restarts the
// copy with PREPARE_BEGIN, which re-creates the lease or resumes a live one.
static void test_prepared_push_restarts_when_the_receiver_drops_its_lease(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source = {.bytes = max_profile, .length = sizeof(max_profile)};
    uint32_t                        now    = 100000u;
    bool                            done   = false;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor                = committed_descriptor(&right, 50u, 1u);
    descriptor.payload_length = sizeof(max_profile);
    descriptor.payload_crc32  = payload_crc(max_profile, sizeof(max_profile));
    descriptor.payload_digest = payload_digest(max_profile, sizeof(max_profile));
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
    for (uint32_t scan = 0u; scan < 64u; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(right.reconciler.transfer_offset > 0u);
    // The receiver lets the lease go, as its lease timer would.
    assert(noah_profile_peer_store_backend_abort(&left.peer_store, &left.peer_store.descriptor) == NOAH_PROFILE_PEER_STORE_OK);
    left.reconciler.transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_NONE;
    for (uint32_t scan = 0u; scan < MAX_SCANS && !done; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
        done = noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL) || (right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED && right.reconciler.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_VALIDATION_ERROR);
    }
    if (!done) fprintf(stderr, "dropped lease: receiver store %u | sender state %u offset %u status %u retries %u\n", (unsigned)noah_profile_peer_store_backend_state(&left.peer_store), (unsigned)right.reconciler.state, (unsigned)right.reconciler.transfer_offset, (unsigned)right.reconciler.last_status, (unsigned)right.reconciler.retry_count);
    assert(done && "the sender retried a chunk the receiver would never accept");
}

// A storage failure while the receiver checks or stores a copy is retried:
// the sender sends the copy again, up to its storage retries. Past them the
// copy stops with the storage error, and the receiver must still take the
// next one. In the field it refused every later Apply until power cycled.
static void run_receiver_storage_failures(noah_profile_peer_store_state_t failing_phase, bool fail_write, uint8_t failures) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint32_t                        now    = 100000u;
    uint8_t                         armed  = 0u;
    bool                            in_phase = false;
    bool                            ready  = false;
    bool                            stopped = false;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = committed_descriptor(&right, 60u, 1u);
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
    for (uint32_t scan = 0u; scan < MAX_SCANS && !ready && !stopped; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        bool entering = noah_profile_peer_store_backend_state(&left.peer_store) == failing_phase;
        if (entering && !in_phase && armed < failures) {
            armed++;
            if (fail_write)
                left.memory.fail_writes = 1u;
            else
                left.memory.fail_reads = 1u;
        }
        in_phase = entering;
        assert_one_scan_budget(&left, false, now);
        ready   = noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL);
        stopped = right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED;
    }
    left.memory.fail_reads  = 0u;
    left.memory.fail_writes = 0u;
    assert(armed == failures);
    if (failures <= NOAH_PROFILE_SPLIT_PREPARED_STORAGE_RETRIES) {
        // Retried to success without the host doing anything.
        if (!ready) fprintf(stderr, "%u failed %s(s) while %s: sender state %u status %u retries %u\n", (unsigned)failures, fail_write ? "write" : "read", failing_phase == NOAH_PROFILE_PEER_STORE_PREPARING ? "storing" : "checking", (unsigned)right.reconciler.state, (unsigned)right.reconciler.last_status, (unsigned)right.reconciler.prepared_storage_retries);
        assert(ready && right.reconciler.prepared_storage_retries == failures);
        return;
    }
    assert(stopped && right.reconciler.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_STORAGE_ERROR);

    // The host cancels, then applies the same profile again: the same copy.
    assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &descriptor));
    for (uint32_t scan = 0u; scan < MAX_SCANS && right.reconciler.prepared_push_active; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(!right.reconciler.prepared_push_active);
    for (uint32_t scan = 0u; scan < 64u && !noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read); scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(right.reconciler.prepared_push_active);
    ready = false;
    for (uint32_t scan = 0u; scan < MAX_SCANS && !ready; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
        ready = noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL);
    }
    if (!ready) fprintf(stderr, "after failed storage: receiver store %u admission %u prepare %u | sender state %u status %u\n", (unsigned)noah_profile_peer_store_backend_state(&left.peer_store), (unsigned)noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend), (unsigned)left.store.prepare_active, (unsigned)right.reconciler.state, (unsigned)right.reconciler.last_status);
    assert(ready && "the receiver never recovered from the storage failure");
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_PEER);
}

// The cancel's own write on the receiver fails: the lease must still end.
static void test_receiver_recovers_when_its_abort_write_fails(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint32_t                        now    = 100000u;
    bool                            ready  = false;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = committed_descriptor(&right, 70u, 1u);
    now        = run_prepared_push_until_ready_at(&right, &left, &descriptor, &source, now);
    assert(noah_profile_peer_store_backend_state(&left.peer_store) == NOAH_PROFILE_PEER_STORE_PREPARED);
    left.memory.fail_writes = 1u;
    assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &descriptor));
    for (uint32_t scan = 0u; scan < MAX_SCANS && right.reconciler.prepared_push_active; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    left.memory.fail_writes = 0u;
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    for (uint32_t scan = 0u; scan < 400u && !noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read); scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    for (uint32_t scan = 0u; scan < MAX_SCANS && !ready; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
        ready = noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL);
    }
    if (!ready) fprintf(stderr, "after failed abort write: receiver store %u admission %u prepare %u reconcile %u | sender active %u state %u status %u orphan %u busy %u reason %u\n", (unsigned)noah_profile_peer_store_backend_state(&left.peer_store), (unsigned)noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend), (unsigned)left.store.prepare_active, (unsigned)left.store.reconciliation_required, (unsigned)right.reconciler.prepared_push_active, (unsigned)right.reconciler.state, (unsigned)right.reconciler.last_status, (unsigned)right.reconciler.orphan_pending, (unsigned)right.reconciler.busy_streak, (unsigned)right.reconciler.last_busy_reason);
    assert(ready && "a failed abort write left the receiver unable to take the next copy");
}

static void test_receiver_recovers_after_a_storage_failure(void) {
    for (uint8_t failures = 1u; failures <= NOAH_PROFILE_SPLIT_PREPARED_STORAGE_RETRIES + 1u; failures++) {
        run_receiver_storage_failures(NOAH_PROFILE_PEER_STORE_VALIDATING, false, failures);
        run_receiver_storage_failures(NOAH_PROFILE_PEER_STORE_PREPARING, true, failures);
        run_receiver_storage_failures(NOAH_PROFILE_PEER_STORE_PREPARING, false, failures);
    }
}

static void test_prepared_push_restarts_across_both_half_role_changes(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};

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

static void test_receiver_durable_prepare_survives_sender_pause(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    noah_profile_split_descriptor_t provisional;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint32_t                        expired_at;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = committed_descriptor(&right, 24u, 1u);
    run_prepared_push_until_ready(&right, &left, &descriptor, &source);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_PEER);
    assert(noah_profile_split_reconciler_provisional_peer_descriptor(&left.reconciler, &provisional));

    expired_at = left.reconciler.last_peer_activity_at + NOAH_PROFILE_SPLIT_PREPARE_LEASE_MS;
    assert(!noah_profile_split_reconciler_scan(&left.reconciler, false, expired_at));
    assert(noah_profile_peer_store_backend_state(&left.peer_store) == NOAH_PROFILE_PEER_STORE_PREPARED);
    assert(left.store.prepared_durable);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_PEER);
    assert(left.reconciler.transfer_owner == NOAH_PROFILE_SPLIT_TRANSFER_REMOTE_PUSH);
    assert(!noah_profile_split_reconciler_provisional_peer_descriptor(&left.reconciler, &provisional));

    // The sender can authorize the decision later without retransferring.
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
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t cancel_descriptor;
    noah_profile_split_descriptor_t commit_descriptor;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint32_t                        now;
    uint32_t                        exchanges;

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
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};

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
        half_t                          left;
        half_t                          right;
        noah_profile_split_descriptor_t descriptor;
        staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};

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
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};

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
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t left_descriptor;
    noah_profile_split_descriptor_t right_descriptor;
    noah_profile_split_descriptor_t observed;
    staged_source_t                 left_source  = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    staged_source_t                 right_source = {.bytes = empty_profile, .length = sizeof(empty_profile)};

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
    half_t                                left;
    half_t                                right;
    noah_profile_split_authority_status_t authority;
    uint32_t                              before_publications;
    uint32_t                              before_exchanges;
    uint32_t                              before_deadline;

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

    exchanges      = (left.link.exchanges + right.link.exchanges) - exchanges;
    reads          = (left.memory.reads + right.memory.reads) - reads;
    writes         = (left.memory.writes + right.memory.writes) - writes;
    expected_polls = elapsed_ms / NOAH_PROFILE_SPLIT_POLL_MS;

    printf("steady state: %u scans over %u ms -> %u exchanges, %u reads, %u writes (expected <= %u exchanges)\n", (unsigned)scans, (unsigned)elapsed_ms, (unsigned)exchanges, (unsigned)reads, (unsigned)writes, (unsigned)(expected_polls + 1u));

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

    left_metadata_sequence       = left.reconciler.metadata_sequence;
    right_metadata_sequence      = right.reconciler.metadata_sequence;
    left_authority_publications  = left.reconciler.authority.status.publication_count;
    right_authority_publications = right.reconciler.authority.status.publication_count;
    left_reads                   = left.memory.reads;
    right_reads                  = right.memory.reads;
    left_writes                  = left.memory.writes;
    right_writes                 = right.memory.writes;
    left_exchanges               = left.link.exchanges;
    right_exchanges              = right.link.exchanges;
    before_deadline              = right.reconciler.next_attempt_at == 0u ? 0u : right.reconciler.next_attempt_at - 1u;

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
    test_logical_binding_survives_master_push();
    test_logical_binding_survives_slave_pull();
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
    test_authorized_commit_survives_one_replayed_reply();
    test_prepared_push_cancel_aborts_peer_lease();
    test_prepared_abort_is_bounded_when_peer_goes_silent();
    test_prepared_abort_is_bounded_when_peer_stays_busy();
    test_maximum_candidate_resolves_within_owner_no_progress_window();
    test_prepared_push_restarts_across_both_half_role_changes();
    test_receiver_durable_prepare_survives_sender_pause();
    test_prepare_authorize_and_cancel_are_immediate_after_timer_high_bit();
    test_cancel_after_prepare_rejection_is_idempotent();
    test_cancel_after_dropped_prepare_begin_releases_or_noops();
    test_cancel_refuses_matching_durable_peer();
    test_crossed_prepare_begin_loser_abort_does_not_deadlock();
    test_prepared_push_survives_one_lost_exchange_anywhere();
    test_prepared_push_survives_one_replayed_reply_anywhere();
    test_prepared_push_resumes_from_the_metadata_poll();
    test_stale_receive_lease_expires_while_the_link_stays_busy();
    test_prepared_push_restarts_when_the_receiver_drops_its_lease();
    test_receiver_recovers_after_a_storage_failure();
    test_receiver_recovers_when_its_abort_write_fails();
    test_refresh_publishes_new_local_authority_during_backoff();
    test_idle_scans_do_not_republish_unchanged_metadata();
    test_converged_steady_state_cost_is_bounded_by_poll_deadline();
    puts("profile split reconciler host tests passed");
    return 0;
}
