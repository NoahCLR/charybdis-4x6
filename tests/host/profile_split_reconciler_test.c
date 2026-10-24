#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "split_fault_link.h"
#include "users/noah/lib/profile/split/profile_split_reconciler.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "profile_store_saved_fixture.h"

_Static_assert((unsigned)NOAH_PROFILE_SPLIT_V1_FRAME_MAX <= (unsigned)SPLIT_FAULT_LINK_FRAME_MAX, "the fault link carries the largest split frame");

enum {
    ACTION_ABI_DIGEST   = UINT32_C(0x11223344),
    MAX_SCANS           = 4096u,
    MAX_PREPARE_TIME_MS = 60000u,
};

static const uint8_t empty_profile[] = {'N', 'L', 'P', '1', 4u, 0u, 0u, 1u};
static const uint8_t max_profile[NOAH_PROFILE_CANDIDATE_V1_MAX_BLOB_SIZE];
// Fault sweeps plant one fault at every exchange of a copy, so their cost
// grows with the square of its length. The protocol does not depend on
// length, so they copy 5,088 bytes; a full copy is sampled separately.
#define FAULT_SWEEP_LENGTH 5088u

typedef struct {
    uint8_t  bytes[NOAH_PROFILE_STORAGE_REGION_SIZE];
    uint32_t reads;
    uint32_t writes;
    uint32_t bytes_written;
    uint32_t fail_reads;  // the next reads fail, as a flash read that errors would
    uint32_t fail_writes; // likewise for writes
} memory_t;

typedef struct {
    const uint8_t *bytes;
    uint32_t       length;
    uint32_t       reads;
    // The copy's reuse plan, in order; none when range_count is zero.
    const noah_profile_split_reuse_range_t *ranges;
    uint8_t                                 range_count;
} staged_source_t;


typedef struct {
    memory_t                               memory;
    noah_profile_store_t                   store;
    noah_active_profile_provider_t         provider;
    noah_profile_candidate_store_backend_t candidate_backend;
    noah_profile_other_half_store_backend_t other_half_store;
    noah_profile_split_reconciler_t        reconciler;
    split_fault_link_t                     link; // to the other half
    // PAYLOAD_REQUEST frames sent: the cost of pulling the other half's
    // profile.
    uint32_t                               payload_requests;
    // METADATA polls sent, and those that said the sender holds no push.
    uint32_t                               metadata_requests;
    uint32_t                               no_push_claims;
    uint32_t                               compiled_digest;
    // The owner cannot say what it has saved yet, as while it boots.
    bool                                   descriptor_unreadable;
} half_t;

static uint32_t always_safe(void *context) {
    (void)context;
    return 0u;
}

// The flash backend refuses a read or write over the store's I/O bound; one
// here fails the test.
static bool memory_read(void *context, noah_profile_storage_address_t address, uint8_t *target, uint32_t length) {
    memory_t *memory = context;

    assert(length <= NOAH_PROFILE_STORE_IO_CHUNK_MAX);
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

static bool memory_write(void *context, noah_profile_storage_address_t address, const uint8_t *source, uint32_t length) {
    memory_t *memory = context;

    assert(length <= NOAH_PROFILE_STORE_IO_CHUNK_MAX);
    memory->writes++;
    if (memory->fail_writes) {
        memory->fail_writes--;
        return false;
    }
    if (!source || length == 0u || (uint32_t)address + length > sizeof(memory->bytes)) {
        return false;
    }
    memcpy(&memory->bytes[address], source, length);
    memory->bytes_written += length;
    return true;
}

static uint32_t payload_crc(const uint8_t *payload, uint32_t length) {
    return noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, payload, length));
}

static uint32_t payload_digest(const uint8_t *payload, uint32_t length) {
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

static noah_profile_split_descriptor_t saved_descriptor(const half_t *half, uint32_t generation, uint8_t origin) {
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

static uint32_t local_validations;
void noah_profile_split_test_local_validation(void) { local_validations++; }

static bool local_descriptor(void *context, noah_profile_split_descriptor_t *descriptor) {
    half_t *half = context;

    if (!half || !descriptor || half->descriptor_unreadable) {
        return false;
    }
    *descriptor = compiled_descriptor(half);
    const noah_profile_store_record_t *record = noah_profile_store_saved(&half->store);
    if (record) {
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

static bool local_read(void *context, const noah_profile_split_descriptor_t *descriptor, uint32_t offset, uint8_t *bytes, uint8_t length) {
    half_t                            *half   = context;
    const noah_profile_store_record_t *record = half ? noah_profile_store_saved(&half->store) : NULL;
    noah_profile_storage_address_t     base;

    if (!record || !descriptor || !bytes || length == 0u || (uint32_t)offset + length > descriptor->payload_length || record->generation != descriptor->generation || record->payload_digest != descriptor->payload_digest || !noah_profile_store_slot_payload_start(record->slot, &base)) {
        return false;
    }
    // A chunk is wider than one store read, so it is read in pieces, as the
    // owner's own reader does.
    return noah_profile_store_read_span(&half->store, base + offset, bytes, length);
}

static bool staged_read(void *context, const noah_profile_split_descriptor_t *descriptor, uint32_t offset, uint8_t *bytes, uint8_t length) {
    staged_source_t *source = context;

    if (!source || !descriptor || !bytes || length == 0u || descriptor->payload_length != source->length || (uint32_t)offset + length > source->length) {
        return false;
    }
    source->reads++;
    memcpy(bytes, &source->bytes[offset], length);
    return true;
}

static bool staged_plan(void *context, const noah_profile_split_descriptor_t *descriptor, uint32_t offset, noah_profile_split_reuse_range_t *range) {
    staged_source_t *source = context;

    if (!source || !descriptor || !range || descriptor->payload_length != source->length) {
        return false;
    }
    for (uint8_t index = 0u; index < source->range_count; index++) {
        if ((uint32_t)source->ranges[index].offset + source->ranges[index].length > offset) {
            *range = source->ranges[index];
            return true;
        }
    }
    return false;
}

// The other half's RPC callback body.
static bool other_half_receive(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    return noah_profile_split_reconciler_receive(&((half_t *)context)->reconciler, request, request_length, response, response_length);
}

// A half's end of the link: each request asks for the reply its own frame
// sizes, and a PAYLOAD_REQUEST is counted.
static bool exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    half_t                       *half = context;
    noah_profile_split_v1_frame_t sent;

    assert(noah_profile_split_v1_frame_decode(request, request_length, &sent) && noah_profile_split_v1_reply_capacity(&sent) == response_length);
    if (sent.kind == NOAH_PROFILE_SPLIT_V1_METADATA) {
        half->metadata_requests++;
        half->no_push_claims += sent.no_prepared_push;
    }
    if (sent.kind == NOAH_PROFILE_SPLIT_V1_PAYLOAD_REQUEST) half->payload_requests++;
    return split_fault_link_exchange(&half->link, request, request_length, response, response_length);
}

// The exchange numbered `at` answers with `frame`, never delivering its request.
static void inject_frame(split_fault_link_t *link, const noah_profile_split_v1_frame_t *frame, uint32_t at) {
    link->injected_length = noah_profile_split_v1_frame_encode(frame, link->injected, sizeof(link->injected));
    assert(link->injected_length != 0u);
    link->inject = at;
}

// The half's active profile is the compiled `bytes`, as a fresh half's is;
// both halves of a pair share it, as one build does.
static void half_storage_init_compiled(half_t *half, const uint8_t *bytes, uint32_t length) {
    noah_profile_reader_t                     reader = noah_profile_reader_from_memory(bytes, length);
    noah_profile_validator_v1_profile_t       profile;
    noah_active_profile_snapshot_t            compiled;
    noah_profile_validator_v1_compatibility_t compatibility = noah_profile_validator_v1_default_compatibility(ACTION_ABI_DIGEST);
    // The maximum fixture contains the production 16 x 60 keymap.
    compatibility.placement_positions = 60u;
    noah_profile_store_record_t               selected;

    memset(half, 0, sizeof(*half));
    memset(half->memory.bytes, 0xff, sizeof(half->memory.bytes));
    memset(&profile, 0, sizeof(profile));
    profile.byte_length       = length;
    profile.crc32             = payload_crc(bytes, length);
    profile.digest            = payload_digest(bytes, length);
    profile.action_abi_digest = ACTION_ABI_DIGEST;
    assert(noah_active_profile_snapshot_make_compiled(&profile, &reader, 0u, &compiled) == NOAH_ACTIVE_PROFILE_OK);
    assert(noah_active_profile_provider_init(&half->provider, &compiled, always_safe, NULL, NULL, 0u) == NOAH_ACTIVE_PROFILE_OK);
    half->compiled_digest = compiled.identity.payload_digest;
    noah_profile_store_init(&half->store, (noah_profile_store_io_t){.read = memory_read, .write = memory_write, .context = &half->memory},
                            (noah_profile_store_compatibility_t){
                                .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
                                .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
                                .compiled_default_digest = half->compiled_digest,
                                .action_abi_digest       = ACTION_ABI_DIGEST,
                            });
    assert(noah_profile_store_boot_select(&half->store, &selected) == NOAH_PROFILE_STORE_NO_SAVED_PROFILE);
    compatibility.required_domain_mask = 0u;
    noah_profile_candidate_store_backend_init(&half->candidate_backend, &half->store, &half->provider, &compatibility, half->compiled_digest, 1u);
    assert(half->candidate_backend.reuse_guard_installed);
    noah_profile_other_half_store_backend_init(&half->other_half_store, &half->candidate_backend);
}

static void half_storage_init(half_t *half) {
    half_storage_init_compiled(half, empty_profile, sizeof(empty_profile));
}

static void install_profile_with_flags(half_t *half, uint32_t generation, uint8_t origin, uint8_t flags) {
    noah_profile_split_descriptor_t  descriptor = saved_descriptor(half, generation, origin);
    noah_profile_other_half_store_result_t result;

    descriptor.profile_flags = flags;
    assert(noah_profile_other_half_store_backend_begin(&half->other_half_store, &descriptor) == NOAH_PROFILE_OTHER_HALF_STORE_OK);
    assert(noah_profile_other_half_store_backend_write(&half->other_half_store, descriptor.generation, descriptor.payload_digest, 0u, empty_profile, sizeof(empty_profile)) == NOAH_PROFILE_OTHER_HALF_STORE_OK);
    result = noah_profile_other_half_store_backend_commit_begin(&half->other_half_store, &descriptor);
    while (result == NOAH_PROFILE_OTHER_HALF_STORE_IN_PROGRESS) {
        result = noah_profile_other_half_store_backend_step(&half->other_half_store, NOAH_PROFILE_VALIDATOR_V1_STEP_READ_MAX);
    }
    assert(result == NOAH_PROFILE_OTHER_HALF_STORE_OK);
}

static void install_profile(half_t *half, uint32_t generation, uint8_t origin) {
    install_profile_with_flags(half, generation, origin, NOAH_PROFILE_STORE_FLAG_OVERRIDE);
}

static void pair_init(half_t *left, half_t *right) {
    noah_profile_split_reconciler_config_t left_config;
    noah_profile_split_reconciler_config_t right_config;

    left->link.receive  = other_half_receive;
    left->link.other_half = right;
    right->link.receive = other_half_receive;
    right->link.other_half = left;
    left_config      = (noah_profile_split_reconciler_config_t){
        .local_context     = left,
        .local_descriptor  = local_descriptor,
        .local_read        = local_read,
        .transport_context = left,
        .exchange          = exchange,
        .other_half_store  = &left->other_half_store,
    };
    right_config = (noah_profile_split_reconciler_config_t){
        .local_context     = right,
        .local_descriptor  = local_descriptor,
        .local_read        = local_read,
        .transport_context = right,
        .exchange          = exchange,
        .other_half_store  = &right->other_half_store,
    };
    noah_profile_split_reconciler_init(&left->reconciler, &left_config);
    noah_profile_split_reconciler_init(&right->reconciler, &right_config);
}

static void assert_one_scan_budget(half_t *half, bool master, uint32_t now) {
    uint32_t reads_before     = half->memory.reads;
    uint32_t writes_before    = half->memory.writes;
    uint32_t written_before   = half->memory.bytes_written;
    uint32_t exchanges_before = half->link.exchanges;

    (void)noah_profile_split_reconciler_scan(&half->reconciler, master, now);
    // A receiver stores at most one full chunk's worth of bytes in a scan,
    // whether they arrived in a chunk or it copies them for a reuse range.
    assert(half->memory.bytes_written - written_before <= NOAH_PROFILE_SPLIT_OTHER_HALF_SCAN_BYTES);
    if (half->link.exchanges != exchanges_before) {
        assert(half->link.exchanges == exchanges_before + 1u);
        assert(half->memory.reads == reads_before);
        assert(half->memory.writes == writes_before);
    }
}

static bool pair_converged(const half_t *left, const half_t *right) {
    noah_profile_split_authority_status_t left_status;
    noah_profile_split_authority_status_t right_status;

    return noah_profile_split_authority_status(noah_profile_split_reconciler_authority(&left->reconciler), &left_status) && noah_profile_split_authority_status(noah_profile_split_reconciler_authority(&right->reconciler), &right_status) && (left_status.state == NOAH_PROFILE_SPLIT_AUTHORITY_COMPILED_CONVERGED || left_status.state == NOAH_PROFILE_SPLIT_AUTHORITY_SAVED_CONVERGED) && (right_status.state == NOAH_PROFILE_SPLIT_AUTHORITY_COMPILED_CONVERGED || right_status.state == NOAH_PROFILE_SPLIT_AUTHORITY_SAVED_CONVERGED) && !left_status.transfer_pending && !right_status.transfer_pending;
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
    const noah_profile_store_record_t *a = saved_of(&left->store);
    const noah_profile_store_record_t *b = saved_of(&right->store);

    assert(a->generation == b->generation);
    assert(a->origin_half == b->origin_half);
    assert(a->flags == b->flags);
    assert(a->payload_length == b->payload_length);
    assert(a->payload_crc32 == b->payload_crc32);
    assert(a->payload_digest == b->payload_digest);
    assert(a->compiled_default_digest == b->compiled_default_digest);
    assert(a->action_abi_digest == b->action_abi_digest);
    assert(a->format_version == b->format_version);
}

static void test_compiled_convergence(void) {
    half_t left;
    half_t right;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    assert(!noah_profile_store_saved(&left.store));
    assert(!noah_profile_store_saved(&right.store));
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
    assert(saved_generation(&left.store) == 5u && saved_of(&left.store)->origin_half == 1u);
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
    assert(saved_generation(&right.store) == 7u && saved_of(&right.store)->origin_half == 0u);
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
    right.link.disconnected = true;
    assert_one_scan_budget(&right, true, 0u);
    assert(noah_profile_split_authority_status(noah_profile_split_reconciler_authority(&right.reconciler), &status));
    assert(status.state == NOAH_PROFILE_SPLIT_AUTHORITY_OTHER_HALF_UNREADABLE);
    assert(noah_profile_split_authority_other_half_observer((void *)noah_profile_split_reconciler_authority(&right.reconciler), &unresolved));
    assert(unresolved == 1u);
    right.link.disconnected = false;
    run_pair_until_converged(&left, &right, false);
    assert_records_match(&left, &right);

    // The passive half cannot probe, so it expires a missing master poll and
    // fails closed instead of retaining convergence forever after link loss.
    assert_one_scan_budget(&left, false, (MAX_SCANS * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) + NOAH_PROFILE_SPLIT_OTHER_HALF_TIMEOUT_MS);
    assert(noah_profile_split_authority_status(noah_profile_split_reconciler_authority(&left.reconciler), &status));
    assert(status.state == NOAH_PROFILE_SPLIT_AUTHORITY_OTHER_HALF_UNREADABLE);
}

static void test_corrupt_response_restarts_fail_closed(void) {
    half_t left;
    half_t right;

    half_storage_init(&left);
    half_storage_init(&right);
    install_profile(&left, 4u, 0u);
    pair_init(&left, &right);
    right.link.corrupt_next_reply = true;
    assert_one_scan_budget(&right, true, 0u);
    assert(noah_profile_split_authority_compare(&right.reconciler.local_descriptor, &right.reconciler.other_half_descriptor) == NOAH_PROFILE_SPLIT_AUTHORITY_OTHER_HALF_UNREADABLE);
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
    right.link.drop_after_delivery = 2u;
    run_pair_until_converged(&left, &right, false);
    assert_records_match(&left, &right);
    assert(saved_generation(&left.store) == 6u && saved_of(&left.store)->origin_half == 1u);
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
    assert(saved_of(&right.store)->origin_half == 0u);
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
    assert(saved_generation(&right.store) == UINT32_MAX);
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

static void test_convergence_only_refuses_newer_other_half_import(void) {
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
    assert(!noah_profile_store_saved(&right.store));
    assert(right.memory.writes == right_writes);
    assert(noah_profile_candidate_store_backend_admission_owner(&right.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
}

static void test_convergence_only_answers_inbound_prepare_busy_without_storage(void) {
    half_t                                 left;
    half_t                                 right;
    noah_profile_split_reconciler_status_t status;
    noah_profile_split_v1_frame_t          request;
    noah_profile_split_v1_frame_t          response;
    uint8_t                                request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    uint8_t                                response_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    uint32_t                               right_writes;

    half_storage_init(&left);
    half_storage_init(&right);
    install_profile(&left, 16u, 0u);
    pair_init(&left, &right);
    right_writes = right.memory.writes;
    request      = (noah_profile_split_v1_frame_t){
        .kind       = NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN,
        .status     = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .descriptor = saved_descriptor(&left, 16u, 0u),
    };
    assert(noah_profile_split_v1_frame_encode(&request, request_wire, sizeof(request_wire)));
    assert(noah_profile_split_reconciler_receive(&right.reconciler, request_wire, sizeof(request_wire), response_wire, sizeof(response_wire)));
    assert(noah_profile_split_reconciler_scan_mode(&right.reconciler, false, 0u, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY));
    {
        noah_profile_split_descriptor_t       provisional;
        noah_profile_split_authority_status_t authority;

        assert(noah_profile_split_reconciler_provisional_other_half_descriptor(&right.reconciler, &provisional));
        assert(provisional.generation == request.descriptor.generation);
        assert(provisional.payload_digest == request.descriptor.payload_digest);
        assert(!right.reconciler.other_half_descriptor.readable);
        assert(noah_profile_split_authority_status(noah_profile_split_reconciler_authority(&right.reconciler), &authority));
        assert(authority.state == NOAH_PROFILE_SPLIT_AUTHORITY_OTHER_HALF_UNREADABLE);
    }
    assert(noah_profile_split_reconciler_receive(&right.reconciler, request_wire, sizeof(request_wire), response_wire, sizeof(response_wire)));
    assert(noah_profile_split_v1_frame_decode(response_wire, sizeof(response_wire), &response));
    assert(response.kind == NOAH_PROFILE_SPLIT_V1_ACK && response.status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY);
    assert(right.memory.writes == right_writes);
    assert(noah_profile_candidate_store_backend_admission_owner(&right.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    // The cached BUSY answers that one byte-identical retry without admitting
    // it; the next scan releases it, and the same request is admitted again.
    assert(noah_profile_split_reconciler_status(&right.reconciler, &status) && !status.mailbox_pending);
    (void)noah_profile_split_reconciler_scan_mode(&right.reconciler, false, NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY);
    assert(noah_profile_split_reconciler_receive(&right.reconciler, request_wire, sizeof(request_wire), response_wire, sizeof(response_wire)));
    assert(noah_profile_split_v1_frame_decode(response_wire, sizeof(response_wire), &response));
    assert(response.kind == NOAH_PROFILE_SPLIT_V1_ACK && response.status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY && response.busy_reason == NOAH_PROFILE_SPLIT_V1_BUSY_ADMITTED);
    assert(noah_profile_split_reconciler_status(&right.reconciler, &status) && status.mailbox_pending);
    assert(right.memory.writes == right_writes);
}

// QMK's split RPC hands the other half each request as it came, with the
// reply buffer the sender asked for. The receive takes only one whole frame
// with room for its reply: anything else is refused, with nothing written and
// nothing admitted, and the RPC answers zeros.
static void test_receive_refuses_anything_but_one_frame(void) {
    static const struct {
        uint8_t request_length;
        uint8_t response_length;
        bool    no_request;
        bool    no_response;
    } refused[] = {
        {NOAH_PROFILE_SPLIT_V1_FRAME_SIZE - 1u, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, false, false},
        {NOAH_PROFILE_SPLIT_V1_FRAME_MAX + 1u, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, false, false},
        {NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE - 1u, false, false},
        {NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, NOAH_PROFILE_SPLIT_V1_FRAME_MAX + 1u, false, false},
        {NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, true, false},
        {NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, false, true},
    };
    static noah_profile_split_reconciler_t uninitialized;
    half_t                                 left;
    half_t                                 right;
    noah_profile_split_reconciler_status_t status;
    noah_profile_split_v1_frame_t          request;
    uint8_t                                request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_MAX + 1u] = {0};
    uint8_t                                response_wire[NOAH_PROFILE_SPLIT_V1_FRAME_MAX + 1u];

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    request = (noah_profile_split_v1_frame_t){
        .kind       = NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN,
        .status     = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .descriptor = saved_descriptor(&left, 16u, 0u),
    };
    assert(noah_profile_split_v1_frame_encode(&request, request_wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE) == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    for (size_t index = 0u; index < sizeof(refused) / sizeof(refused[0]); index++) {
        memset(response_wire, 0xA5, sizeof(response_wire));
        assert(!noah_profile_split_reconciler_receive(&right.reconciler, refused[index].no_request ? NULL : request_wire, refused[index].request_length, refused[index].no_response ? NULL : response_wire, refused[index].response_length));
        for (size_t byte = 0u; byte < sizeof(response_wire); byte++) assert(response_wire[byte] == 0xA5u);
        assert(noah_profile_split_reconciler_status(&right.reconciler, &status) && !status.mailbox_pending);
    }
    // So does a half whose reconciler is not initialized.
    assert(!noah_profile_split_reconciler_receive(&uninitialized, request_wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, response_wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE));
    for (size_t byte = 0u; byte < sizeof(response_wire); byte++) assert(response_wire[byte] == 0xA5u);
    // The same frame, whole, is admitted.
    assert(noah_profile_split_reconciler_receive(&right.reconciler, request_wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, response_wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE));
    assert(noah_profile_split_reconciler_status(&right.reconciler, &status) && status.mailbox_pending);
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
    descriptor = saved_descriptor(&right, 19u, 1u);
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));

    exchanges = right.link.exchanges;
    assert_one_scan_budget(&right, true, started_at);
    assert(right.link.exchanges == exchanges + 1u);
    assert(right.reconciler.next_attempt_at == started_at + NOAH_PROFILE_SPLIT_ADMISSION_RETRY_MS);
    assert(right.reconciler.retry_ms == NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS * 2u);
    assert(!noah_profile_split_reconciler_scan(&right.reconciler, true, started_at + NOAH_PROFILE_SPLIT_ADMISSION_RETRY_MS - 1u));
    assert(right.link.exchanges == exchanges + 1u);

    assert_one_scan_budget(&left, false, started_at);
    // PREPARE_BEGIN is the copy's first exchange (D-F31): its collected ACK
    // moves straight on to the payload.
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
    descriptor = saved_descriptor(&right, 20u, 1u);

    run_prepared_push_until_ready(&right, &left, &descriptor, &source);
    assert(source.reads == 1u);
    assert(!noah_profile_store_saved(&left.store));
    assert(noah_profile_other_half_store_backend_state(&left.other_half_store) == NOAH_PROFILE_OTHER_HALF_STORE_PREPARED);
    assert(noah_profile_store_durably_prepared(&left.store));
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_PEER);

    exchanges_at_barrier = right.link.exchanges;
    assert(!noah_profile_split_reconciler_scan(&right.reconciler, true, 20000u));
    assert(right.link.exchanges == exchanges_at_barrier);
    assert(!noah_profile_store_saved(&left.store));

    {
        noah_profile_split_descriptor_t wrong = descriptor;
        wrong.generation++;
        assert(!noah_profile_split_reconciler_prepared_push_authorize_commit(&right.reconciler, &wrong));
    }
    assert(noah_profile_split_reconciler_prepared_push_authorize_commit(&right.reconciler, &descriptor));
    assert(noah_profile_split_reconciler_prepared_push_authorize_commit(&right.reconciler, &descriptor));

    for (uint32_t scan = 0u; scan < MAX_SCANS && !noah_profile_store_saved(&left.store); scan++) {
        uint32_t now = 21000u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(noah_profile_store_saved(&left.store));
    assert(saved_generation(&left.store) == descriptor.generation);
    assert(saved_of(&left.store)->payload_digest == descriptor.payload_digest);
}

// After the commit is authorized, a replayed reply must still end with the
// other half holding the copy. The replayed PREPARE_DURABLE acknowledgement
// passes for the commit's, so the sender reaches its metadata poll with the
// other half still on the old profile and has to resume from there.
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
        descriptor = saved_descriptor(&right, 20u, 1u);
        run_prepared_push_until_ready(&right, &left, &descriptor, &source);
        right.link.replay_previous = right.link.exchanges + fault;
        assert(noah_profile_split_reconciler_prepared_push_authorize_commit(&right.reconciler, &descriptor));
        for (uint32_t scan = 0u; scan < MAX_SCANS && !done; scan++) {
            uint32_t now = 21000u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

            assert_one_scan_budget(&right, true, now);
            assert_one_scan_budget(&left, false, now);
            done = noah_profile_store_saved(&left.store) && !right.reconciler.prepared_push_active;
        }
        if (!done) {
            fprintf(stderr, "authorized commit wedged: replayed reply at exchange %u; sender state %u, status %u, push active %u, other half committed %u\n", (unsigned)fault, (unsigned)right.reconciler.state,
                    (unsigned)right.reconciler.last_status, (unsigned)right.reconciler.prepared_push_active, (unsigned)(noah_profile_store_saved(&left.store) != NULL));
            assert(!"one replayed reply wedged the authorized commit");
        }
        assert(saved_generation(&left.store) == descriptor.generation);
        if (right.link.replay_previous != 0u) {
            assert(fault > 2u);
            return;
        }
    }
}

static void test_prepared_push_cancel_aborts_other_half_lease(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = saved_descriptor(&right, 21u, 1u);
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
    assert(!noah_profile_store_saved(&left.store));
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
}

static bool peer_cleanup_pending(const half_t *half) {
    noah_profile_split_reconciler_status_t status;

    assert(noah_profile_split_reconciler_status(&half->reconciler, &status));
    return status.peer_cleanup_pending;
}

// Scans the sender alone until its prepared push is released, and returns how
// long that took. The other half is deliberately not scanned: it is either
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

// The keyboard wedged in PREPARING_PEER on 2026-09-23: the other half stopped
// acknowledging, the host cancelled, and the other-half ABORT was retried
// forever. Every host-owned wait above this one depends on it ending.
static void test_prepared_abort_is_bounded_when_other_half_goes_silent(void) {
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
    descriptor = saved_descriptor(&right, 23u, 1u);
    run_prepared_push_until_ready(&right, &left, &descriptor, &source);
    assert(noah_profile_other_half_store_backend_state(&left.other_half_store) == NOAH_PROFILE_OTHER_HALF_STORE_PREPARED);

    right.link.disconnected = true;
    assert(noah_profile_split_reconciler_prepared_push_cancel(&right.reconciler, &descriptor));
    elapsed = run_sender_until_prepared_push_released(&right, 40000u);
    assert(elapsed >= NOAH_PROFILE_SPLIT_PREPARED_ABORT_TIMEOUT_MS);
    assert(elapsed < NOAH_PROFILE_SPLIT_PREPARED_ABORT_TIMEOUT_MS + 2u * NOAH_PROFILE_SPLIT_RETRY_MAX_MS);

    // Released locally, but the other half never confirmed: it may still hold
    // the provisional lease, so the sender remembers and says so.
    assert(peer_cleanup_pending(&right));
    assert(!noah_profile_store_saved(&left.store));
    next = saved_descriptor(&right, 24u, 1u);
    assert(!noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &next, &source, staged_read));

    // Once the other half answers again, the remembered ABORT releases its
    // lease.
    right.link.disconnected = false;
    for (uint32_t scan = 0u; scan < MAX_SCANS && peer_cleanup_pending(&right); scan++) {
        uint32_t now = 60000u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(!peer_cleanup_pending(&right));
    assert(noah_profile_other_half_store_backend_state(&left.other_half_store) == NOAH_PROFILE_OTHER_HALF_STORE_IDLE);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    assert(!noah_profile_store_saved(&left.store));
    run_pair_until_converged(&left, &right, false);
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &next, &source, staged_read));
}

// The other-half store in RECONCILE_REQUIRED (durability unknown) answers every
// ABORT with BUSY until it restarts. The sender still releases in bounded time,
// keeps polling metadata, and never reports the other half as cleaned up.
static void test_prepared_abort_is_bounded_when_other_half_stays_busy(void) {
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
    descriptor = saved_descriptor(&right, 25u, 1u);
    run_prepared_push_until_ready(&right, &left, &descriptor, &source);
    left.other_half_store.state = NOAH_PROFILE_OTHER_HALF_STORE_RECONCILE_REQUIRED;

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

    // The wedged other half keeps answering BUSY. The remembered ABORT is
    // retried at a bounded rate, metadata polling continues, and nothing is
    // written.
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
    assert(!noah_profile_store_saved(&left.store));
}

static void test_maximum_candidate_resolves_within_owner_no_progress_window(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source       = {.bytes = max_profile, .length = sizeof(max_profile)};
    uint32_t                        start_at     = 100000u;
    uint32_t                        resolved_at  = start_at;
    uint32_t                        progress_at  = start_at;
    uint32_t                        longest_gap  = 0u;
    size_t                          last_reads   = 0u;
    uint8_t last_phase = 0u;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor                = saved_descriptor(&right, 32u, 1u);
    descriptor.payload_length = sizeof(max_profile);
    descriptor.payload_crc32  = payload_crc(max_profile, sizeof(max_profile));
    descriptor.payload_digest = payload_digest(max_profile, sizeof(max_profile));

    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
    // A full 131,040-byte payload is thousands of split chunks, then the other
    // half's validation; the loop bound is generous, the timing assertion is
    // not.
    for (uint32_t scan = 0u; scan < 16u * MAX_SCANS; scan++) {
        resolved_at = start_at + scan * NOAH_PROFILE_SPLIT_ADMISSION_RETRY_MS;
        assert_one_scan_budget(&right, true, resolved_at);
        assert_one_scan_budget(&left, false, resolved_at);
        if (source.reads != last_reads || right.reconciler.prepare_progress_phase > last_phase) {
            last_phase = right.reconciler.prepare_progress_phase;
            last_reads  = source.reads;
            progress_at = resolved_at;
        }
        if (resolved_at - progress_at > longest_gap) longest_gap = resolved_at - progress_at;
        if (right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED || noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL)) {
            break;
        }
    }
    // This deliberately maximal all-zero payload is semantically invalid,
    // but transfer plus whole-payload validation must still reach a terminal
    // result before the owner's no-progress guard can fire. That guard is
    // refreshed by acknowledged transfer progress, so the bound is the longest
    // stretch without it (the other half's validation), not the whole transfer.
    assert(right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED);
    assert(right.reconciler.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_VALIDATION_ERROR);
    assert(longest_gap < MAX_PREPARE_TIME_MS);
    assert(source.reads == (sizeof(max_profile) + NOAH_PROFILE_SPLIT_V1_CHUNK_MAX - 1u) / NOAH_PROFILE_SPLIT_V1_CHUNK_MAX);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
}

// The maximum profile fixture (D-F14), every table at its maximum: the other
// half receives it, validates it, holds it prepared, and commits it once
// authorized.
static uint8_t maximum_profile[NOAH_PROFILE_CANDIDATE_V1_MAX_BLOB_SIZE];
static size_t  maximum_profile_length;
static void    load_maximum_profile(const char *path) {
    static char line[2u * NOAH_PROFILE_CANDIDATE_V1_MAX_BLOB_SIZE + 64u];
    FILE       *file = fopen(path, "r");
    assert(file);
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "profile.hex=", 12) != 0) continue;
        for (const char *hex = line + 12; hex[0] && hex[0] != '\n'; hex += 2) {
            unsigned byte;
            assert(maximum_profile_length < sizeof(maximum_profile) && sscanf(hex, "%2x", &byte) == 1);
            maximum_profile[maximum_profile_length++] = (uint8_t)byte;
        }
    }
    fclose(file);
    assert(maximum_profile_length > 32768u);
}

static void test_maximum_profile_prepares_and_commits_on_the_other_half(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source      = {.bytes = maximum_profile, .length = (uint32_t)maximum_profile_length};
    uint32_t                        start_at    = 100000u;
    uint32_t                        now         = start_at;
    uint32_t                        progress_at = start_at;
    uint32_t                        longest_gap = 0u;
    size_t                          last_reads  = 0u;
    uint8_t last_phase = 0u;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor                = saved_descriptor(&right, 32u, 1u);
    descriptor.payload_length = (uint32_t)maximum_profile_length;
    descriptor.domain_mask    = NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS;
    descriptor.payload_crc32  = payload_crc(maximum_profile, maximum_profile_length);
    descriptor.payload_digest = payload_digest(maximum_profile, maximum_profile_length);

    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
    for (uint32_t scan = 0u; scan < 16u * MAX_SCANS && right.reconciler.state != NOAH_PROFILE_SPLIT_RECONCILER_STOPPED && !noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL); scan++) {
        now = start_at + scan * NOAH_PROFILE_SPLIT_ADMISSION_RETRY_MS;
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
        if (source.reads != last_reads || right.reconciler.prepare_progress_phase > last_phase) {
            last_phase = right.reconciler.prepare_progress_phase;
            last_reads  = source.reads;
            progress_at = now;
        }
        if (now - progress_at > longest_gap) longest_gap = now - progress_at;
    }
    if (!noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL))
        fprintf(stderr, "maximum push: state %u status %u offset %u reads %u; other-half validator code %u offset %u domain %02x row %u field %u detail %u/%u\n", (unsigned)right.reconciler.state, (unsigned)right.reconciler.last_status, (unsigned)right.reconciler.transfer_offset, (unsigned)source.reads, left.candidate_backend.validator.terminal_error.code, (unsigned)left.candidate_backend.validator.terminal_error.byte_offset, left.candidate_backend.validator.terminal_error.domain_id, left.candidate_backend.validator.terminal_error.row_index, left.candidate_backend.validator.terminal_error.field_id, left.candidate_backend.validator.terminal_error.detail_kind, left.candidate_backend.validator.terminal_error.detail_code);
    assert(noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL));
    fprintf(stderr, "maximum push: longest gap %u ms = %u scans; total %u ms\n", (unsigned)longest_gap, (unsigned)(longest_gap / NOAH_PROFILE_SPLIT_ADMISSION_RETRY_MS), (unsigned)(now - start_at));
    assert(longest_gap < MAX_PREPARE_TIME_MS);
    assert(noah_profile_other_half_store_backend_state(&left.other_half_store) == NOAH_PROFILE_OTHER_HALF_STORE_PREPARED && noah_profile_store_durably_prepared(&left.store));
    assert(!noah_profile_store_saved(&left.store));

    assert(noah_profile_split_reconciler_prepared_push_authorize_commit(&right.reconciler, &descriptor));
    for (uint32_t scan = 0u; scan < MAX_SCANS && !noah_profile_store_saved(&left.store); scan++) {
        now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(saved_generation(&left.store) == descriptor.generation);
    assert(saved_of(&left.store)->payload_length == maximum_profile_length && saved_of(&left.store)->payload_digest == descriptor.payload_digest);
}

// Both halves run on the maximum profile as their active one; right copies
// it to left as a new record. Each test sets the reuse plan it needs.
typedef struct {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source;
    uint32_t                        now;
    uint32_t                        longest_send_retry_ms; // the sender's longest wait while sending
} reuse_pair_t;

static void reuse_pair_init(reuse_pair_t *pair, const noah_profile_split_reuse_range_t *ranges, uint8_t range_count) {
    half_storage_init_compiled(&pair->left, maximum_profile, (uint32_t)maximum_profile_length);
    half_storage_init_compiled(&pair->right, maximum_profile, (uint32_t)maximum_profile_length);
    pair_init(&pair->left, &pair->right);
    pair->right.reconciler.config.prepared_reuse_plan = staged_plan;
    run_pair_until_converged(&pair->left, &pair->right, false);
    pair->descriptor                = saved_descriptor(&pair->right, 32u, 1u);
    pair->descriptor.payload_length = (uint32_t)maximum_profile_length;
    pair->descriptor.domain_mask    = NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS;
    pair->descriptor.payload_crc32  = payload_crc(maximum_profile, maximum_profile_length);
    pair->descriptor.payload_digest = payload_digest(maximum_profile, maximum_profile_length);
    pair->source                    = (staged_source_t){.bytes = maximum_profile, .length = (uint32_t)maximum_profile_length, .ranges = ranges, .range_count = range_count};
    pair->now                       = 100000u;
    pair->longest_send_retry_ms     = 0u;
}

static void reuse_pair_scan(reuse_pair_t *pair) {
    pair->now += NOAH_PROFILE_SPLIT_ADMISSION_RETRY_MS;
    assert_one_scan_budget(&pair->right, true, pair->now);
    assert_one_scan_budget(&pair->left, false, pair->now);
    if (pair->right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_SEND && (int32_t)(pair->right.reconciler.next_attempt_at - pair->now) > (int32_t)pair->longest_send_retry_ms) {
        pair->longest_send_retry_ms = pair->right.reconciler.next_attempt_at - pair->now;
    }
}

static void reuse_pair_finish_and_commit(reuse_pair_t *pair) {
    for (uint32_t scan = 0u; scan < 16u * MAX_SCANS && pair->right.reconciler.state != NOAH_PROFILE_SPLIT_RECONCILER_STOPPED && !noah_profile_split_reconciler_prepared_push_ready(&pair->right.reconciler, NULL); scan++) {
        reuse_pair_scan(pair);
    }
    assert(noah_profile_split_reconciler_prepared_push_ready(&pair->right.reconciler, NULL));
    assert(noah_profile_split_reconciler_prepared_push_authorize_commit(&pair->right.reconciler, &pair->descriptor));
    for (uint32_t scan = 0u; scan < MAX_SCANS && !noah_profile_store_saved(&pair->left.store); scan++) {
        reuse_pair_scan(pair);
    }
    assert(saved_generation(&pair->left.store) == pair->descriptor.generation && saved_of(&pair->left.store)->payload_digest == pair->descriptor.payload_digest);
}

static void reuse_pair_prepare_and_commit(reuse_pair_t *pair) {
    assert(noah_profile_split_reconciler_prepared_push_begin(&pair->right.reconciler, &pair->descriptor, &pair->source, staged_read));
    reuse_pair_finish_and_commit(pair);
}

static noah_profile_split_v1_source_t compiled_maximum_source(void) {
    return (noah_profile_split_v1_source_t){.digest = payload_digest(maximum_profile, maximum_profile_length), .crc32 = payload_crc(maximum_profile, maximum_profile_length), .kind = 0u, .origin = 255u};
}

static uint32_t chunks_for(uint32_t bytes) {
    return (bytes + NOAH_PROFILE_SPLIT_V1_CHUNK_MAX - 1u) / NOAH_PROFILE_SPLIT_V1_CHUNK_MAX;
}

// Ranges the other half holds in its own active profile cross the link as two
// reuse frames; the receiver copies them over many scans while the sender
// collects its progress, and only the rest travels as bytes.
static void test_reuse_ranges_copy_from_the_active_profile_on_the_other_half(void) {
    static reuse_pair_t                     pair;
    static noah_profile_split_reuse_range_t ranges[2];
    noah_profile_split_reconciler_status_t  status;
    uint32_t                                chunk_bytes;

    ranges[0] = (noah_profile_split_reuse_range_t){.source = compiled_maximum_source(), .offset = 0u, .source_offset = 0u, .length = 10000u};
    ranges[1] = (noah_profile_split_reuse_range_t){.source = compiled_maximum_source(), .offset = 12000u, .source_offset = 12000u, .length = 18000u};
    reuse_pair_init(&pair, ranges, 2u);
    reuse_pair_prepare_and_commit(&pair);
    chunk_bytes = (uint32_t)maximum_profile_length - 28000u;
    assert(noah_profile_split_reconciler_status(&pair.right.reconciler, &status) && status.reused_bytes == 28000u);
    // The sender read only the bytes it sent, in full chunks between ranges.
    assert(pair.source.reads == chunks_for(2000u) + chunks_for(chunk_bytes - 2000u));
    // The receiver copied 28,000 bytes in at most 110 a scan; the sender
    // collected that in short polls and never fell back to the backoff.
    assert(pair.longest_send_retry_ms <= NOAH_PROFILE_SPLIT_ADMISSION_RETRY_MS);
    fprintf(stderr, "reuse push: %u exchanges, %u reads, %u reused bytes\n", (unsigned)pair.right.link.exchanges, (unsigned)pair.source.reads, (unsigned)status.reused_bytes);
}

// A source the other half's active profile is not makes it send the bytes: the
// first reuse frame is refused and the rest of the copy goes as chunks.
static void test_reuse_falls_back_to_bytes_without_the_source(void) {
    static reuse_pair_t                     pair;
    static noah_profile_split_reuse_range_t ranges[1];
    noah_profile_split_reconciler_status_t  status;

    ranges[0] = (noah_profile_split_reuse_range_t){.source = {.generation = 9u, .digest = UINT32_C(0x01020304), .crc32 = UINT32_C(0x05060708), .kind = 1u, .origin = 0u}, .offset = 500u, .source_offset = 500u, .length = 20000u};
    reuse_pair_init(&pair, ranges, 1u);
    reuse_pair_prepare_and_commit(&pair);
    assert(noah_profile_split_reconciler_status(&pair.right.reconciler, &status) && status.reused_bytes == 0u);
    assert(pair.right.reconciler.reuse_unavailable);
    assert(pair.right.reconciler.source_unavailable_replies == 1u && pair.right.reconciler.reuse_exchanges >= 1u);
    assert(pair.source.reads == chunks_for(500u) + chunks_for((uint32_t)maximum_profile_length - 500u));
}

// Losing a poll or its reply while the receiver copies must not duplicate
// writes, discard the source plan, or authorize an incomplete candidate.
static void test_lost_exchanges_during_reuse_resume_the_same_copy(void) {
    static reuse_pair_t              pair;
    noah_profile_split_reuse_range_t range = {.source = compiled_maximum_source(), .offset = 0u, .source_offset = 0u, .length = 30000u};

    for (uint8_t after = 0u; after < 2u; after++) {
        reuse_pair_init(&pair, &range, 1u);
        assert(noah_profile_split_reconciler_prepared_push_begin(&pair.right.reconciler, &pair.descriptor, &pair.source, staged_read));
        for (uint32_t scan = 0u; scan < MAX_SCANS && noah_profile_other_half_store_backend_next_offset(&pair.left.other_half_store) < 5000u; scan++) {
            reuse_pair_scan(&pair);
        }
        assert(pair.left.reconciler.incoming_reuse_active && pair.right.reconciler.outbound_reuse_pending);
        assert(!noah_profile_store_saved(&pair.left.store));
        uint32_t drop = pair.right.link.exchanges + 1u;
        if (after) {
            pair.right.link.drop_after_delivery = drop;
        } else {
            pair.right.link.drop_before_delivery = drop;
        }
        reuse_pair_finish_and_commit(&pair);
        assert(pair.right.reconciler.transport_failure_count == 1u);
        assert(pair.right.reconciler.reused_bytes == range.length);
        assert(pair.source.reads == chunks_for((uint32_t)maximum_profile_length - range.length));
    }
}

// A plan no reuse frame can carry (here an unknown source kind) never
// reaches the link: the copy sends its bytes.
static void test_an_unencodable_reuse_plan_sends_bytes(void) {
    static reuse_pair_t                     pair;
    static noah_profile_split_reuse_range_t ranges[1];
    noah_profile_split_reconciler_status_t  status;

    ranges[0] = (noah_profile_split_reuse_range_t){.source = {.digest = 1u, .crc32 = 2u, .kind = 2u, .origin = 255u}, .offset = 0u, .source_offset = 0u, .length = 4000u};
    reuse_pair_init(&pair, ranges, 1u);
    reuse_pair_prepare_and_commit(&pair);
    assert(noah_profile_split_reconciler_status(&pair.right.reconciler, &status) && status.reused_bytes == 0u && pair.right.reconciler.transport_failure_count == 0u);
    assert(pair.source.reads == chunks_for((uint32_t)maximum_profile_length));
}

// Cancelling the copy while the receiver is inside a reuse range: the ABORT
// ends the range where it stands and releases the receiver's storage.
static void test_a_cancel_ends_a_reuse_range_midway(void) {
    static reuse_pair_t                     pair;
    static noah_profile_split_reuse_range_t ranges[1];

    ranges[0] = (noah_profile_split_reuse_range_t){.source = compiled_maximum_source(), .offset = 0u, .source_offset = 0u, .length = 30000u};
    reuse_pair_init(&pair, ranges, 1u);
    assert(noah_profile_split_reconciler_prepared_push_begin(&pair.right.reconciler, &pair.descriptor, &pair.source, staged_read));
    for (uint32_t scan = 0u; scan < MAX_SCANS && noah_profile_other_half_store_backend_next_offset(&pair.left.other_half_store) < 5000u; scan++) {
        reuse_pair_scan(&pair);
    }
    assert(pair.left.reconciler.incoming_reuse_active);
    assert(noah_profile_other_half_store_backend_next_offset(&pair.left.other_half_store) < 30000u);
    assert(noah_profile_split_reconciler_prepared_push_cancel(&pair.right.reconciler, &pair.descriptor));
    for (uint32_t scan = 0u; scan < MAX_SCANS && pair.right.reconciler.prepared_push_active; scan++) {
        reuse_pair_scan(&pair);
    }
    assert(!pair.right.reconciler.prepared_push_active && !pair.right.reconciler.orphan_pending);
    assert(!pair.left.reconciler.incoming_reuse_active);
    assert(noah_profile_other_half_store_backend_state(&pair.left.other_half_store) == NOAH_PROFILE_OTHER_HALF_STORE_IDLE);
    assert(noah_profile_candidate_store_backend_admission_owner(&pair.left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);

    // The next copy runs from the start.
    pair.source.reads = 0u;
    reuse_pair_prepare_and_commit(&pair);
}

// Runs one prepared copy of `length` bytes of max_profile with exchange
// `drop` lost before or after delivery. Returns false when the fault never
// fired, which means `drop` is past the last exchange of a clean copy.
static bool run_copy_with_lost_exchange(uint32_t length, bool after, uint32_t drop) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source   = {.bytes = max_profile, .length = length};
    uint32_t                        start_at = 100000u;
    uint32_t                        base;
    bool                            done = false;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor                = saved_descriptor(&right, 32u, 1u);
    descriptor.payload_length = length;
    descriptor.payload_crc32  = payload_crc(max_profile, length);
    descriptor.payload_digest = payload_digest(max_profile, length);
    base                      = right.link.exchanges;
    if (after)
        right.link.drop_after_delivery = base + drop;
    else
        right.link.drop_before_delivery = base + drop;
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
    for (uint32_t scan = 0u; scan < 32u * MAX_SCANS && !done; scan++) {
        uint32_t now = start_at + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
        done = right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED || noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL);
    }
    if (!done) {
        fprintf(stderr, "copy wedged: exchange %u lost %s delivery; sender state %u, offset %u of %u, other-half store state %u, retries %u\n", (unsigned)drop, after ? "after" : "before",
                (unsigned)right.reconciler.state, (unsigned)right.reconciler.transfer_offset, (unsigned)descriptor.payload_length, (unsigned)noah_profile_other_half_store_backend_state(&left.other_half_store), (unsigned)right.reconciler.retry_count);
        assert(!"a single lost exchange wedged the copy");
    }
    return (after ? right.link.drop_after_delivery : right.link.drop_before_delivery) == 0u;
}

// One lost exchange anywhere in a copy, whether the request never arrived or
// its reply was lost after the other half acted on it, must still let the
// transfer finish. A copy that retries one chunk forever wedges Apply. Every
// exchange of a 5,088-byte copy is covered (the protocol does not depend on
// size); a full 53,216-byte copy is sampled at its start, middle and end,
// including the offsets whose 16-bit sums are largest.
static void test_prepared_push_survives_one_lost_exchange_anywhere(void) {
    enum { EXHAUSTIVE_LENGTH = 5088u };
    uint32_t exchanges_clean = 0u;

    for (uint8_t after = 0u; after <= 1u; after++) {
        for (uint32_t drop = 1u;; drop++) {
            if (!run_copy_with_lost_exchange(EXHAUSTIVE_LENGTH, after, drop)) {
                if (!exchanges_clean) exchanges_clean = drop;
                break;
            }
        }
    }
    assert(exchanges_clean > 2u * (EXHAUSTIVE_LENGTH / NOAH_PROFILE_SPLIT_V1_CHUNK_MAX));

    uint32_t full_exchanges = 0u;
    while (run_copy_with_lost_exchange(sizeof(max_profile), false, full_exchanges + 1u)) {
        full_exchanges += 2048u;
    }
    assert(full_exchanges + 2048u > 2u * (sizeof(max_profile) / NOAH_PROFILE_SPLIT_V1_CHUNK_MAX));
    for (uint8_t after = 0u; after <= 1u; after++) {
        static const uint32_t near_start[] = {1u, 2u, 3u, 4u, 5u};
        for (size_t index = 0u; index < sizeof(near_start) / sizeof(near_start[0]); index++) {
            assert(run_copy_with_lost_exchange(sizeof(max_profile), after, near_start[index]));
        }
        for (uint32_t drop = 2u * (sizeof(max_profile) / NOAH_PROFILE_SPLIT_V1_CHUNK_MAX) / 2u; drop < 2u * (sizeof(max_profile) / NOAH_PROFILE_SPLIT_V1_CHUNK_MAX) / 2u + 4u; drop++) {
            assert(run_copy_with_lost_exchange(sizeof(max_profile), after, drop));
        }
        for (uint32_t drop = 2u * (sizeof(max_profile) / NOAH_PROFILE_SPLIT_V1_CHUNK_MAX) - 4u; drop < 2u * (sizeof(max_profile) / NOAH_PROFILE_SPLIT_V1_CHUNK_MAX) + 2u; drop++) {
            assert(run_copy_with_lost_exchange(sizeof(max_profile), after, drop));
        }
    }
}

// The wire garbles a request: the other half cannot decode it, answers
// INVALID_FRAME and acts on nothing. That is a lost exchange like a garbled
// reply, and the copy carries on; on hardware one garbled request anywhere
// in the copy used to end the whole Apply.
static void test_prepared_push_survives_one_garbled_request_anywhere(void) {
    uint32_t exchanges_clean = 0u;

    for (uint32_t garble = 1u;; garble++) {
        half_t                          left;
        half_t                          right;
        noah_profile_split_descriptor_t descriptor;
        staged_source_t                 source   = {.bytes = max_profile, .length = FAULT_SWEEP_LENGTH};
        uint32_t                        start_at = 100000u;
        bool                            done     = false;

        half_storage_init(&left);
        half_storage_init(&right);
        pair_init(&left, &right);
        run_pair_until_converged(&left, &right, false);
        descriptor                = saved_descriptor(&right, 32u, 1u);
        descriptor.payload_length = FAULT_SWEEP_LENGTH;
        descriptor.payload_crc32  = payload_crc(max_profile, FAULT_SWEEP_LENGTH);
        descriptor.payload_digest = payload_digest(max_profile, FAULT_SWEEP_LENGTH);
        right.link.garble = right.link.exchanges + garble;
        assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
        for (uint32_t scan = 0u; scan < MAX_SCANS * 2u && !done; scan++) {
            uint32_t now = start_at + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
            assert_one_scan_budget(&right, true, now);
            assert_one_scan_budget(&left, false, now);
            done = right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED || noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL);
        }
        // The copy ends at the barrier or on the receiver's real verdict on
        // this all-zero payload, never on the garbled frame itself.
        if (right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED && right.reconciler.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_INVALID_FRAME) {
            fprintf(stderr, "copy stopped: exchange %u garbled, last status %u\n", (unsigned)garble, (unsigned)right.reconciler.last_status);
            assert(!"a garbled request stopped the copy");
        }
        assert(done);
        if (right.link.garble != 0u) {
            exchanges_clean = garble;
            break;
        }
    }
    assert(exchanges_clean > 2u * (FAULT_SWEEP_LENGTH / NOAH_PROFILE_SPLIT_V1_CHUNK_MAX));
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
    staged_source_t                 source   = {.bytes = max_profile, .length = FAULT_SWEEP_LENGTH};
    uint32_t                        start_at = 100000u;
    bool                            done     = false;
    bool                            fired;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor                = saved_descriptor(&right, 32u, 1u);
    descriptor.payload_length = FAULT_SWEEP_LENGTH;
    descriptor.payload_crc32  = payload_crc(max_profile, FAULT_SWEEP_LENGTH);
    descriptor.payload_digest = payload_digest(max_profile, FAULT_SWEEP_LENGTH);
    if (replay) {
        right.link.replay_previous = right.link.exchanges + fault;
    } else {
        inject_frame(&right.link, injected, right.link.exchanges + fault);
    }
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
    for (uint32_t scan = 0u; scan < MAX_SCANS * 2u && !done; scan++) {
        uint32_t now = start_at + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
        done = right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED || noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL);
    }
    fired = (replay ? right.link.replay_previous : right.link.inject) == 0u;
    if (!done || (right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED && right.reconciler.last_status != NOAH_PROFILE_SPLIT_V1_STATUS_VALIDATION_ERROR)) {
        fprintf(stderr, "copy wedged: %s at exchange %u; sender state %u, status %u, offset %u of %u, push active %u\n", replay ? "replayed reply" : "injected reply", (unsigned)fault,
                (unsigned)right.reconciler.state, (unsigned)right.reconciler.last_status, (unsigned)right.reconciler.transfer_offset, (unsigned)descriptor.payload_length, (unsigned)right.reconciler.prepared_push_active);
        assert(!"one stray reply wedged the copy");
    }
    return fired;
}

// A reply to an earlier request is a lost exchange, not a verdict. It sends
// the sender to its metadata poll, where both halves still agree on the old
// profile; reading that as converged parked the push at byte 1050 of 2646 on
// a real keyboard, with the host's ABORT unprocessed until a power cycle.
static void test_prepared_push_survives_one_replayed_reply_anywhere(void) {
    uint32_t fault = 1u;

    while (run_prepared_push_with_fault(fault, true, NULL)) {
        fault++;
    }
    assert(fault > 2u * (FAULT_SWEEP_LENGTH / NOAH_PROFILE_SPLIT_V1_CHUNK_MAX));
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
    staged_source_t                        stale_source = {.bytes = max_profile, .length = FAULT_SWEEP_LENGTH};
    staged_source_t                        next_source  = {.bytes = same_length ? max_profile : empty_profile, .length = same_length ? FAULT_SWEEP_LENGTH : sizeof(empty_profile)};
    noah_profile_split_reconciler_config_t config;
    uint32_t                               now  = 100000u;
    bool                                   done = false;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    stale                = saved_descriptor(&right, 40u, 1u);
    stale.payload_length = FAULT_SWEEP_LENGTH;
    stale.payload_crc32  = payload_crc(max_profile, FAULT_SWEEP_LENGTH);
    stale.payload_digest = payload_digest(max_profile, FAULT_SWEEP_LENGTH);
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &stale, &stale_source, staged_read));
    for (uint32_t scan = 0u; scan < 64u; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(noah_profile_other_half_store_backend_state(&left.other_half_store) == NOAH_PROFILE_OTHER_HALF_STORE_RECEIVING);
    assert(noah_profile_other_half_store_backend_next_offset(&left.other_half_store) > sizeof(empty_profile));

    // The sender restarts: it forgets the copy without cancelling it.
    config = right.reconciler.config;
    noah_profile_split_reconciler_init(&right.reconciler, &config);
    next = saved_descriptor(&right, 41u, 1u);
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
    for (uint32_t scan = 0u; scan < 40u; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    {
        noah_profile_split_reconciler_status_t status;
        assert(noah_profile_split_reconciler_status(&right.reconciler, &status));
        assert(status.last_busy_reason == NOAH_PROFILE_SPLIT_V1_BUSY_OTHER_COPY);
        assert(status.last_busy_store_state == NOAH_PROFILE_OTHER_HALF_STORE_RECEIVING);
        assert(status.busy_streak > 1u);
    }
    for (uint32_t scan = 0u; scan < MAX_SCANS && !done; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
        // Ready, or the whole copy arrived and was judged (the all-zero
        // payload fails validation or its digest): either way it got through.
        done = noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL) || (right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED && right.reconciler.last_status != NOAH_PROFILE_SPLIT_V1_STATUS_INVALID_FRAME);
    }
    if (!done) fprintf(stderr, "stale lease (%s length): receiver store %u owner %u | sender state %u status %u retries %u\n", same_length ? "same" : "shorter", (unsigned)noah_profile_other_half_store_backend_state(&left.other_half_store), (unsigned)left.reconciler.transfer_owner, (unsigned)right.reconciler.state, (unsigned)right.reconciler.last_status, (unsigned)right.reconciler.retry_count);
    assert(done && "the stale lease held the next copy off");
    assert(noah_profile_other_half_store_backend_state(&left.other_half_store) != NOAH_PROFILE_OTHER_HALF_STORE_RECEIVING || noah_profile_other_half_store_backend_copy_matches(&left.other_half_store, next.generation, next.payload_digest, next.payload_length));
}

// A receiver left holding a half-received lease, here because the sender
// restarted mid-copy, must release it once that copy stops arriving, even
// while the sender keeps talking. Otherwise every later copy is answered
// BUSY for as long as the halves stay connected.
static void test_stale_receive_lease_expires_while_the_link_stays_busy(void) {
    run_stale_receive_lease(false);
    run_stale_receive_lease(true);
}

static noah_profile_other_half_store_state_t receiver_store_state(const half_t *receiver) {
    return noah_profile_other_half_store_backend_state(&receiver->other_half_store);
}

// The receiver holds the copy, prepared or on its way to committed, and never
// let it go.
static void assert_copy_kept(const half_t *receiver) {
    noah_profile_other_half_store_state_t state = receiver_store_state(receiver);

    assert(state == NOAH_PROFILE_OTHER_HALF_STORE_PREPARED || state == NOAH_PROFILE_OTHER_HALF_STORE_COMMITTING || state == NOAH_PROFILE_OTHER_HALF_STORE_COMMITTED);
}

// Authorizes the push at its barrier and scans until the receiver commits it.
static uint32_t commit_prepared_push(half_t *sender, half_t *receiver, const noah_profile_split_descriptor_t *descriptor, uint32_t now) {
    assert(noah_profile_split_reconciler_prepared_push_authorize_commit(&sender->reconciler, descriptor));
    for (uint32_t scan = 0u; scan < MAX_SCANS && !noah_profile_store_saved(&receiver->store); scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(sender, true, now);
        assert_one_scan_budget(receiver, false, now);
        assert_copy_kept(receiver);
    }
    assert(noah_profile_store_saved(&receiver->store) && saved_generation(&receiver->store) == descriptor->generation);
    return now;
}

// A sender that restarts while its receiver holds the prepared copy forgets
// the push without cancelling it (D-F34). Restarted before its commit marker,
// its poll says it holds no push over the old saved profile, and the receiver
// releases the copy at once, not after any lease: the halves agree again and
// take the next copy. Until the sender has read what it saved, as while it
// boots, the claim releases nothing. Restarted after its marker, its saved
// profile is the copy: the receiver keeps it and the sender's repair commits
// it without sending it again.
static void run_restarted_sender(bool sender_saved_the_copy) {
    half_t                                 left;
    half_t                                 right;
    noah_profile_split_descriptor_t        descriptor;
    noah_profile_split_descriptor_t        next;
    noah_profile_split_reconciler_config_t config;
    staged_source_t                        source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint32_t                               now;
    uint32_t                               restarted_at;
    uint32_t                               claims;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = saved_descriptor(&right, 27u, 1u);
    now        = run_prepared_push_until_ready_at(&right, &left, &descriptor, &source, 10000u);
    assert(receiver_store_state(&left) == NOAH_PROFILE_OTHER_HALF_STORE_PREPARED);

    if (sender_saved_the_copy) {
        install_profile(&right, descriptor.generation, descriptor.origin_half);
    }
    config = right.reconciler.config;
    noah_profile_split_reconciler_init(&right.reconciler, &config);
    right.descriptor_unreadable = true;
    claims                      = right.no_push_claims;
    for (restarted_at = now; now - restarted_at < 2u * NOAH_PROFILE_SPLIT_PREPARE_LEASE_MS; now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
        assert(receiver_store_state(&left) == NOAH_PROFILE_OTHER_HALF_STORE_PREPARED);
    }
    assert(right.no_push_claims > claims);

    right.descriptor_unreadable = false;
    for (restarted_at = now; now - restarted_at < MAX_SCANS * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS && !pair_converged(&left, &right); now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
        if (sender_saved_the_copy) {
            assert_copy_kept(&left);
        }
    }
    assert(pair_converged(&left, &right));
    assert(now - restarted_at < NOAH_PROFILE_SPLIT_PREPARE_LEASE_MS);
    assert(!noah_profile_store_prepare_active(&left.store) && !noah_profile_store_durably_prepared(&left.store));
    if (sender_saved_the_copy) {
        // Committed, it awaits activation like any copy taken.
        assert(saved_generation(&left.store) == descriptor.generation);
        assert_records_match(&left, &right);
        return;
    }
    assert(receiver_store_state(&left) == NOAH_PROFILE_OTHER_HALF_STORE_IDLE);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    assert(!noah_profile_store_saved(&left.store));
    next = saved_descriptor(&right, descriptor.generation + 1u, 1u);
    now  = run_prepared_push_until_ready_at(&right, &left, &next, &source, now);
    (void)commit_prepared_push(&right, &left, &next, now);
}

static void test_a_restarted_sender_settles_the_prepared_copy(void) {
    run_restarted_sender(false);
    run_restarted_sender(true);
}

// A sender paused at the barrier while its owner decides is there but silent,
// past the lease and the other-half timeout. Nothing releases the prepared copy
// meanwhile, and the commit authorized afterwards finds it.
static void test_a_paused_sender_keeps_the_prepared_copy(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint32_t                        now;
    uint32_t                        paused_at;
    uint32_t                        exchanges;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = saved_descriptor(&right, 28u, 1u);
    now        = run_prepared_push_until_ready_at(&right, &left, &descriptor, &source, 10000u);
    exchanges  = right.link.exchanges;
    for (paused_at = now; now - paused_at < 3u * NOAH_PROFILE_SPLIT_PREPARE_LEASE_MS; now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(right.link.exchanges == exchanges);
    assert(receiver_store_state(&left) == NOAH_PROFILE_OTHER_HALF_STORE_PREPARED && noah_profile_store_durably_prepared(&left.store));
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_PEER);
    (void)commit_prepared_push(&right, &left, &descriptor, now);
}

// A sender that holds the push and falls back to its metadata poll, here on a
// stray STALE reply once the copy is prepared, claims nothing there, before
// the commit is authorized or after. The receiver keeps the copy and the push
// resumes at its end without sending any of it again.
static void test_a_sender_holding_the_push_keeps_the_prepared_copy(void) {
    const noah_profile_split_v1_frame_t stale = {.kind = NOAH_PROFILE_SPLIT_V1_ERROR, .status = NOAH_PROFILE_SPLIT_V1_STATUS_STALE};
    half_t                              left;
    half_t                              right;
    noah_profile_split_descriptor_t     descriptor;
    staged_source_t                     source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint32_t                            now    = 10000u;
    uint32_t                            polls;
    uint32_t                            claims;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = saved_descriptor(&right, 29u, 1u);
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
    for (uint32_t scan = 0u; scan < MAX_SCANS && receiver_store_state(&left) != NOAH_PROFILE_OTHER_HALF_STORE_PREPARED; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(receiver_store_state(&left) == NOAH_PROFILE_OTHER_HALF_STORE_PREPARED && !noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL));
    polls  = right.metadata_requests;
    claims = right.no_push_claims;
    inject_frame(&right.link, &stale, right.link.exchanges + 1u);
    for (uint32_t scan = 0u; scan < MAX_SCANS && !noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL); scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
        assert(receiver_store_state(&left) == NOAH_PROFILE_OTHER_HALF_STORE_PREPARED);
    }
    assert(noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL));
    assert(right.metadata_requests > polls && right.no_push_claims == claims);
    assert(source.reads == 1u);

    // The same once the commit is authorized: the stray reply answers the
    // PREPARE_COMMIT, which never arrives.
    polls = right.metadata_requests;
    inject_frame(&right.link, &stale, right.link.exchanges + 1u);
    (void)commit_prepared_push(&right, &left, &descriptor, now);
    assert(right.metadata_requests > polls && right.no_push_claims == claims);
    assert(source.reads == 1u);
}

// Delivers a METADATA poll from a sender that holds no prepared push over
// `descriptor`, as a restarted one sends, into the receiver's mailbox.
static void deliver_no_push_poll(half_t *receiver, const noah_profile_split_descriptor_t *descriptor) {
    noah_profile_split_v1_frame_t          request = {.kind = NOAH_PROFILE_SPLIT_V1_METADATA, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .descriptor = *descriptor, .no_prepared_push = true};
    noah_profile_split_reconciler_status_t status;
    uint8_t                                request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    uint8_t                                response_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];

    assert(noah_profile_split_v1_frame_encode(&request, request_wire, sizeof(request_wire)) == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    assert(noah_profile_split_reconciler_receive(&receiver->reconciler, request_wire, sizeof(request_wire), response_wire, sizeof(response_wire)));
    assert(noah_profile_split_reconciler_status(&receiver->reconciler, &status) && status.mailbox_pending);
}

// A copy the receiver has begun to commit is the receiver's to finish: a poll
// saying the sender holds no push releases nothing, while it commits or after.
static void test_a_committing_copy_is_never_released(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    noah_profile_split_descriptor_t old;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint32_t                        now;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    old        = compiled_descriptor(&right);
    descriptor = saved_descriptor(&right, 30u, 1u);
    now        = run_prepared_push_until_ready_at(&right, &left, &descriptor, &source, 10000u);
    assert(noah_profile_split_reconciler_prepared_push_authorize_commit(&right.reconciler, &descriptor));
    for (uint32_t scan = 0u; scan < MAX_SCANS && receiver_store_state(&left) == NOAH_PROFILE_OTHER_HALF_STORE_PREPARED; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(receiver_store_state(&left) == NOAH_PROFILE_OTHER_HALF_STORE_COMMITTING);
    deliver_no_push_poll(&left, &old);
    for (uint32_t scan = 0u; scan < MAX_SCANS && receiver_store_state(&left) == NOAH_PROFILE_OTHER_HALF_STORE_COMMITTING; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&left, false, now);
    }
    assert(receiver_store_state(&left) == NOAH_PROFILE_OTHER_HALF_STORE_COMMITTED && saved_generation(&left.store) == descriptor.generation);
    assert_one_scan_budget(&left, false, now);
    deliver_no_push_poll(&left, &old);
    assert_one_scan_budget(&left, false, now);
    assert(receiver_store_state(&left) == NOAH_PROFILE_OTHER_HALF_STORE_COMMITTED && saved_generation(&left.store) == descriptor.generation);
}

// A request that matches the receiver's cached reply waits behind a queued
// poll. Answered from the cache, a restarted sender's identical new push could
// pass a poll saying it holds none, which then releases the copy that push
// had just been told is there.
static void test_a_cached_reply_waits_behind_a_queued_poll(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    noah_profile_split_descriptor_t old;
    noah_profile_split_v1_frame_t   reply;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
    uint8_t                         request[NOAH_PROFILE_SPLIT_V1_FRAME_MAX];
    uint8_t                         response[NOAH_PROFILE_SPLIT_V1_FRAME_MAX];
    uint8_t                         length;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    old        = compiled_descriptor(&right);
    descriptor = saved_descriptor(&right, 31u, 1u);
    (void)run_prepared_push_until_ready_at(&right, &left, &descriptor, &source, 10000u);
    length = left.reconciler.cached_request_length;
    assert(length >= NOAH_PROFILE_SPLIT_V1_FRAME_SIZE && length <= sizeof(request));
    memcpy(request, left.reconciler.cached_request_wire, length);

    assert(noah_profile_split_reconciler_receive(&left.reconciler, request, length, response, sizeof(response)));
    assert(noah_profile_split_v1_reply_decode(response, sizeof(response), &reply) && !(reply.kind == NOAH_PROFILE_SPLIT_V1_ACK && reply.status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY));

    deliver_no_push_poll(&left, &old);
    assert(noah_profile_split_reconciler_receive(&left.reconciler, request, length, response, sizeof(response)));
    assert(noah_profile_split_v1_reply_decode(response, sizeof(response), &reply) && reply.kind == NOAH_PROFILE_SPLIT_V1_ACK && reply.status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY);
}

// If the receiver drops its lease mid-copy, the chunks it is then sent are
// answered BUSY. The sender must not retry one chunk forever: it restarts the
// copy with PREPARE_BEGIN, which re-creates the lease or resumes a live one.
static void test_prepared_push_restarts_when_the_receiver_drops_its_lease(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source = {.bytes = max_profile, .length = FAULT_SWEEP_LENGTH};
    uint32_t                        now    = 100000u;
    bool                            done   = false;

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor                = saved_descriptor(&right, 50u, 1u);
    descriptor.payload_length = FAULT_SWEEP_LENGTH;
    descriptor.payload_crc32  = payload_crc(max_profile, FAULT_SWEEP_LENGTH);
    descriptor.payload_digest = payload_digest(max_profile, FAULT_SWEEP_LENGTH);
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
    for (uint32_t scan = 0u; scan < 64u; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(right.reconciler.transfer_offset > 0u);
    // The receiver lets the lease go, as its lease timer would.
    assert(noah_profile_other_half_store_backend_abort(&left.other_half_store, &left.other_half_store.descriptor) == NOAH_PROFILE_OTHER_HALF_STORE_OK);
    left.reconciler.transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_NONE;
    for (uint32_t scan = 0u; scan < MAX_SCANS && !done; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
        done = noah_profile_split_reconciler_prepared_push_ready(&right.reconciler, NULL) || (right.reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED && right.reconciler.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_VALIDATION_ERROR);
    }
    if (!done) fprintf(stderr, "dropped lease: receiver store %u | sender state %u offset %u status %u retries %u\n", (unsigned)noah_profile_other_half_store_backend_state(&left.other_half_store), (unsigned)right.reconciler.state, (unsigned)right.reconciler.transfer_offset, (unsigned)right.reconciler.last_status, (unsigned)right.reconciler.retry_count);
    assert(done && "the sender retried a chunk the receiver would never accept");
}

// A storage failure while the receiver checks or stores a copy is retried:
// the sender sends the copy again, up to its storage retries. Past them the
// copy stops with the storage error, and the receiver must still take the
// next one. In the field it refused every later Apply until power cycled.
static void run_receiver_storage_failures(noah_profile_other_half_store_state_t failing_phase, bool fail_write, uint8_t failures) {
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
    descriptor = saved_descriptor(&right, 60u, 1u);
    assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
    for (uint32_t scan = 0u; scan < MAX_SCANS && !ready && !stopped; scan++, now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        assert_one_scan_budget(&right, true, now);
        bool entering = noah_profile_other_half_store_backend_state(&left.other_half_store) == failing_phase;
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
        if (!ready) fprintf(stderr, "%u failed %s(s) while %s: sender state %u status %u retries %u\n", (unsigned)failures, fail_write ? "write" : "read", failing_phase == NOAH_PROFILE_OTHER_HALF_STORE_PREPARING ? "storing" : "checking", (unsigned)right.reconciler.state, (unsigned)right.reconciler.last_status, (unsigned)right.reconciler.prepared_storage_retries);
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
    if (!ready) fprintf(stderr, "after failed storage: receiver store %u admission %u prepare %u | sender state %u status %u\n", (unsigned)noah_profile_other_half_store_backend_state(&left.other_half_store), (unsigned)noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend), (unsigned)noah_profile_store_prepare_active(&left.store), (unsigned)right.reconciler.state, (unsigned)right.reconciler.last_status);
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
    descriptor = saved_descriptor(&right, 70u, 1u);
    now        = run_prepared_push_until_ready_at(&right, &left, &descriptor, &source, now);
    assert(noah_profile_other_half_store_backend_state(&left.other_half_store) == NOAH_PROFILE_OTHER_HALF_STORE_PREPARED);
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
    if (!ready) fprintf(stderr, "after failed abort write: receiver store %u admission %u prepare %u reconcile %u | sender active %u state %u status %u orphan %u busy %u reason %u\n", (unsigned)noah_profile_other_half_store_backend_state(&left.other_half_store), (unsigned)noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend), (unsigned)noah_profile_store_prepare_active(&left.store), (unsigned)noah_profile_store_reconciliation_required(&left.store), (unsigned)right.reconciler.prepared_push_active, (unsigned)right.reconciler.state, (unsigned)right.reconciler.last_status, (unsigned)right.reconciler.orphan_pending, (unsigned)right.reconciler.busy_streak, (unsigned)right.reconciler.last_busy_reason);
    assert(ready && "a failed abort write left the receiver unable to take the next copy");
}

static void test_receiver_recovers_after_a_storage_failure(void) {
    for (uint8_t failures = 1u; failures <= NOAH_PROFILE_SPLIT_PREPARED_STORAGE_RETRIES + 1u; failures++) {
        run_receiver_storage_failures(NOAH_PROFILE_OTHER_HALF_STORE_VALIDATING, false, failures);
        run_receiver_storage_failures(NOAH_PROFILE_OTHER_HALF_STORE_PREPARING, true, failures);
        run_receiver_storage_failures(NOAH_PROFILE_OTHER_HALF_STORE_PREPARING, false, failures);
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
    descriptor = saved_descriptor(&right, 23u, 1u);
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

    for (uint32_t scan = 0u; scan < MAX_SCANS && !noah_profile_store_saved(&left.store); scan++) {
        uint32_t now = 51000u + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&left, false, now);
        assert_one_scan_budget(&right, true, now);
    }
    assert(noah_profile_store_saved(&left.store));
    assert(saved_generation(&left.store) == descriptor.generation);
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
    descriptor = saved_descriptor(&right, 24u, 1u);
    run_prepared_push_until_ready(&right, &left, &descriptor, &source);
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_PEER);
    assert(noah_profile_split_reconciler_provisional_other_half_descriptor(&left.reconciler, &provisional));

    expired_at = left.reconciler.last_other_half_activity_at + NOAH_PROFILE_SPLIT_PREPARE_LEASE_MS;
    assert(!noah_profile_split_reconciler_scan(&left.reconciler, false, expired_at));
    assert(noah_profile_other_half_store_backend_state(&left.other_half_store) == NOAH_PROFILE_OTHER_HALF_STORE_PREPARED);
    assert(noah_profile_store_durably_prepared(&left.store));
    assert(noah_profile_candidate_store_backend_admission_owner(&left.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_PEER);
    assert(left.reconciler.transfer_owner == NOAH_PROFILE_SPLIT_TRANSFER_REMOTE_PUSH);
    assert(!noah_profile_split_reconciler_provisional_other_half_descriptor(&left.reconciler, &provisional));

    // The sender can authorize the decision later without retransferring.
    assert(noah_profile_split_reconciler_prepared_push_authorize_commit(&right.reconciler, &descriptor));
    for (uint32_t scan = 0u; scan < MAX_SCANS && !noah_profile_store_saved(&left.store); scan++) {
        uint32_t now = expired_at + NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS + scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(noah_profile_store_saved(&left.store));
    assert(saved_generation(&left.store) == descriptor.generation);
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
    cancel_descriptor = saved_descriptor(&right, 25u, 1u);

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

    commit_descriptor = saved_descriptor(&right, 26u, 1u);
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
    descriptor = saved_descriptor(&right, 27u, 1u);
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
        descriptor = saved_descriptor(&right, (uint32_t)(28u + drop_before), 1u);
        if (drop_before) {
            right.link.drop_before_delivery = right.link.exchanges + 1u;
        } else {
            right.link.drop_after_delivery = right.link.exchanges + 1u;
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

static void test_cancel_refuses_matching_durable_other_half(void) {
    half_t                          left;
    half_t                          right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source = {.bytes = empty_profile, .length = sizeof(empty_profile)};

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = saved_descriptor(&left, 31u, 0u);
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
    assert(saved_generation(&left.store) == descriptor.generation);
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
    left.link.disconnected  = true;
    right.link.disconnected = true;
    assert_one_scan_budget(&left, true, 90000u);
    assert_one_scan_budget(&right, true, 90000u);
    left.link.disconnected  = false;
    right.link.disconnected = false;
    left_descriptor      = saved_descriptor(&left, 32u, 0u);
    right_descriptor     = saved_descriptor(&right, 32u, 1u);
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
    // Cross PREPARE_BEGIN in both directions before checking the provisional
    // descriptors used to arbitrate host copies.
    for (uint32_t t = 90160u; t <= 90190u; t += 5u) {
        (void)noah_profile_split_reconciler_scan_mode(left_sends_first ? &left.reconciler : &right.reconciler, true, t, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY);
        (void)noah_profile_split_reconciler_scan_mode(left_sends_first ? &right.reconciler : &left.reconciler, true, t, NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY);
    }
    assert(noah_profile_split_reconciler_provisional_other_half_descriptor(&left.reconciler, &observed));
    assert(observed.origin_half == 1u);
    assert(noah_profile_split_reconciler_provisional_other_half_descriptor(&right.reconciler, &observed));
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
static void test_converged_steady_state_cost_is_bounded_by_poll_deadline(bool saved) {
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
    if (saved) {
        install_profile(&right, 5u, 1u);
    }
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);

    start_ms  = 1000000u;
    reads     = left.memory.reads + right.memory.reads;
    writes    = left.memory.writes + right.memory.writes;
    exchanges = left.link.exchanges + right.link.exchanges;
    local_validations = 0u;

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
    assert(local_validations == 0u);
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

typedef struct {
    noah_profile_split_descriptor_t value;
    bool available;
} descriptor_fixture_t;

static bool fixture_descriptor(void *context, noah_profile_split_descriptor_t *out) {
    descriptor_fixture_t *fixture = context;
    *out = fixture->value; // A failing provider may leave nonzero output.
    return fixture->available;
}

static void test_descriptor_changes_are_validated_before_backoff(void) {
    half_t left;
    half_t right;
    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor_fixture_t fixture = {.value = right.reconciler.local_descriptor, .available = true};
    right.reconciler.config.local_descriptor = fixture_descriptor;
    right.reconciler.config.local_context = &fixture;
    uint32_t now = right.reconciler.next_attempt_at - 1u;
    uint32_t exchanges = right.link.exchanges;
    local_validations = 0u;
    assert(!noah_profile_split_reconciler_scan(&right.reconciler, true, now));
    assert(local_validations == 0u);

    // A compatibility change with the same generation still refreshes authority.
    fixture.value.compiled_default_digest++;
    assert(!noah_profile_split_reconciler_scan(&right.reconciler, true, now));
    assert(local_validations == 1u);
    assert(right.reconciler.authority.status.local.compiled_default_digest == fixture.value.compiled_default_digest);
    uint8_t valid_schema = fixture.value.schema_major;
    fixture.value.schema_major = UINT8_MAX;
    assert(!noah_profile_split_reconciler_scan(&right.reconciler, true, now));
    assert(local_validations == 2u);
    assert(!right.reconciler.authority.status.local.readable);

    fixture.value.schema_major = valid_schema;
    assert(!noah_profile_split_reconciler_scan(&right.reconciler, true, now));
    assert(right.reconciler.authority.status.local.readable);
    fixture.available = false;
    assert(!noah_profile_split_reconciler_scan(&right.reconciler, true, now));
    assert(!right.reconciler.authority.status.local.readable);
    local_validations = 0u;
    assert(!noah_profile_split_reconciler_scan(&right.reconciler, true, now));
    assert(local_validations == 0u);
    fixture.available = true;
    assert(!noah_profile_split_reconciler_scan(&right.reconciler, true, now));
    assert(local_validations == 1u);
    assert(right.reconciler.authority.status.local.readable);
    assert(right.link.exchanges == exchanges);
}

// STORE_WORKING is quick only for a complete, correlated remote copy.
// A stale tuple, short copy or different storage owner keeps the backoff.
static void test_store_completion_polling_requires_the_matching_full_copy(void) {
    for (unsigned variant = 0u; variant < 11u; variant++) {
        half_t left, right;
        staged_source_t source = {.bytes = empty_profile, .length = sizeof(empty_profile)};
        half_storage_init(&left);
        half_storage_init(&right);
        pair_init(&left, &right);
        run_pair_until_converged(&left, &right, false);
        noah_profile_split_descriptor_t descriptor = saved_descriptor(&right, 32u, 1u);
        assert(noah_profile_split_reconciler_prepared_push_begin(&right.reconciler, &descriptor, &source, staged_read));
        right.reconciler.state = variant == 7u ? NOAH_PROFILE_SPLIT_RECONCILER_PUSH_COMMIT : NOAH_PROFILE_SPLIT_RECONCILER_PUSH_DURABLE;
        right.reconciler.retry_ms = 100u;
        noah_profile_split_v1_frame_t injected = {
            .kind = NOAH_PROFILE_SPLIT_V1_ACK, .status = NOAH_PROFILE_SPLIT_V1_STATUS_BUSY,
            .generation = descriptor.generation, .payload_digest = descriptor.payload_digest,
            .offset = descriptor.payload_length, .payload_length = descriptor.payload_length,
            .busy_reason = NOAH_PROFILE_SPLIT_V1_BUSY_STORE_WORKING,
            .busy_store_state = NOAH_PROFILE_OTHER_HALF_STORE_VALIDATING,
            .busy_owner = NOAH_PROFILE_SPLIT_TRANSFER_REMOTE_PUSH,
            .busy_admission = NOAH_PROFILE_STORAGE_ADMISSION_PEER,
        };
        switch (variant) {
            case 1u: injected.generation++; break;
            case 2u: injected.offset--; break;
            case 3u: injected.busy_admission = NOAH_PROFILE_STORAGE_ADMISSION_HOST; break;
            case 4u: injected.busy_reason = NOAH_PROFILE_SPLIT_V1_BUSY_OTHER_COPY; break;
            case 5u: injected.busy_store_state = NOAH_PROFILE_OTHER_HALF_STORE_RECEIVING; break;
            case 6u: injected.busy_store_state = NOAH_PROFILE_OTHER_HALF_STORE_PREPARING; break;
            case 7u: injected.busy_store_state = NOAH_PROFILE_OTHER_HALF_STORE_COMMITTING; break;
            case 8u: injected.payload_digest++; break;
            case 9u: injected.payload_length++; break;
            case 10u: injected.busy_owner = NOAH_PROFILE_SPLIT_TRANSFER_LOCAL_PULL; break;
        }
        inject_frame(&right.link, &injected, right.link.exchanges + 1u);
        assert_one_scan_budget(&right, true, 100000u);
        bool working = variant == 0u || variant == 6u || variant == 7u;
        assert(right.reconciler.prepare_progress_phase == (working ? injected.busy_store_state : 0u));
        assert(right.reconciler.next_attempt_at == 100000u + (working ? NOAH_PROFILE_SPLIT_STORE_POLL_MS : 100u));
        assert(right.reconciler.retry_ms == (working ? 100u : 200u));
    }
}

// Installs `bytes` as the half's saved profile through its own other-half
// store, chunk by chunk, as a copy from the other half would arrive.
static void install_bytes(half_t *half, uint32_t generation, uint8_t origin, const uint8_t *bytes, uint32_t length, uint8_t domain_mask) {
    noah_profile_split_descriptor_t  descriptor = saved_descriptor(half, generation, origin);
    noah_profile_other_half_store_result_t result;

    descriptor.payload_length = length;
    descriptor.payload_crc32  = payload_crc(bytes, length);
    descriptor.payload_digest = payload_digest(bytes, length);
    descriptor.domain_mask    = domain_mask;
    assert(noah_profile_other_half_store_backend_begin(&half->other_half_store, &descriptor) == NOAH_PROFILE_OTHER_HALF_STORE_OK);
    for (uint32_t offset = 0u; offset < length;) {
        uint8_t piece = length - offset < NOAH_PROFILE_SPLIT_V1_CHUNK_MAX ? (uint8_t)(length - offset) : NOAH_PROFILE_SPLIT_V1_CHUNK_MAX;

        assert(noah_profile_other_half_store_backend_write(&half->other_half_store, descriptor.generation, descriptor.payload_digest, offset, &bytes[offset], piece) == NOAH_PROFILE_OTHER_HALF_STORE_OK);
        offset += piece;
    }
    result = noah_profile_other_half_store_backend_commit_begin(&half->other_half_store, &descriptor);
    while (result == NOAH_PROFILE_OTHER_HALF_STORE_IN_PROGRESS) {
        result = noah_profile_other_half_store_backend_step(&half->other_half_store, NOAH_PROFILE_VALIDATOR_V1_STEP_READ_MAX);
    }
    assert(result == NOAH_PROFILE_OTHER_HALF_STORE_OK);
}

// Pulling the other half's newer profile costs a PAYLOAD_REQUEST per chunk of
// up to 106 bytes, each answered once the other half has scanned (D-F28).
// Pulled replies of 10 bytes made the maximum profile thousands of requests.
static void test_maximum_profile_pull_is_bounded(void) {
    half_t   left;
    half_t   right;
    uint32_t chunks = chunks_for((uint32_t)maximum_profile_length);

    half_storage_init(&left);
    half_storage_init(&right);
    install_bytes(&left, 40u, 0u, maximum_profile, (uint32_t)maximum_profile_length, NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS);
    pair_init(&left, &right);
    // Right is the master; left holds the newer profile, so right pulls it,
    // then validates the whole profile a step a scan.
    for (uint32_t scan = 0u; scan < 16u * MAX_SCANS && !pair_converged(&left, &right); scan++) {
        uint32_t now = scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    assert(pair_converged(&left, &right));
    assert_records_match(&left, &right);
    assert(saved_of(&right.store)->payload_length == maximum_profile_length);
    fprintf(stderr, "maximum pull: %u bytes in %u payload requests, %u exchanges\n", (unsigned)maximum_profile_length, (unsigned)right.payload_requests, (unsigned)right.link.exchanges);
    assert(right.payload_requests <= 2u * chunks + 4u);
    assert(right.link.exchanges <= 2u * chunks + 64u);
}

enum { WIDE_LENGTH = 70000u };
static uint8_t wide_profile[WIDE_LENGTH];

// A schema 4 blob of one RGB domain 69,986 bytes long. Its envelope and
// checksums are sound, so a copy is stored whole and checked to its last
// byte, then refused: an RGB domain keeps 16-bit internal offsets (D-F28),
// and no profile the schema accepts reaches 64 KiB yet.
static void make_wide_profile(void) {
    memcpy(wide_profile, (const uint8_t[]){'N', 'L', 'P', '1', NOAH_PROFILE_SCHEMA_MAJOR, 0u, 1u, 1u, 0x10u, NOAH_PROFILE_DOMAIN_VERSION_RGB}, 10u);
    wide_profile[10] = (uint8_t)(WIDE_LENGTH - 14u);
    wide_profile[11] = (uint8_t)((WIDE_LENGTH - 14u) >> 8u);
    wide_profile[12] = (uint8_t)((WIDE_LENGTH - 14u) >> 16u);
    wide_profile[13] = 0u;
    for (uint32_t index = 14u; index < WIDE_LENGTH; index++) wide_profile[index] = (uint8_t)(index * 13u + 7u);
}

static noah_profile_split_descriptor_t wide_descriptor(const half_t *half, const uint8_t *bytes) {
    noah_profile_split_descriptor_t descriptor = saved_descriptor(half, 32u, 1u);

    descriptor.payload_length = WIDE_LENGTH;
    descriptor.payload_crc32  = payload_crc(bytes, WIDE_LENGTH);
    descriptor.payload_digest = payload_digest(bytes, WIDE_LENGTH);
    descriptor.domain_mask    = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    return descriptor;
}

// Whether either of the half's slots holds `bytes` as its payload.
static bool slot_holds(const half_t *half, const uint8_t *bytes, uint32_t length) {
    return memcmp(&half->memory.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE], bytes, length) == 0 || memcmp(&half->memory.bytes[NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE], bytes, length) == 0;
}

// The receiver checked every byte of the copy, then refused the domain.
static void assert_wide_copy_refused_after_checksums(const half_t *receiver) {
    assert(noah_profile_other_half_store_backend_state(&receiver->other_half_store) == NOAH_PROFILE_OTHER_HALF_STORE_REJECTED);
    assert(noah_profile_other_half_store_backend_result(&receiver->other_half_store) == NOAH_PROFILE_OTHER_HALF_STORE_VALIDATION_ERROR);
    // Located whole, past 64 KiB: the end of the RGB domain.
    assert(receiver->other_half_store.validation_error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_VALIDATION_REJECTED);
    assert(receiver->other_half_store.validation_error.domain_id == NOAH_PROFILE_DOMAIN_V1_RGB && receiver->other_half_store.validation_error.byte_offset == WIDE_LENGTH);
    assert(noah_profile_candidate_store_backend_admission_owner(&receiver->candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
}

static void run_wide_prepared_push(half_t *left, half_t *right, const noah_profile_split_descriptor_t *descriptor, staged_source_t *source) {
    uint32_t now = 100000u;

    assert(noah_profile_split_reconciler_prepared_push_begin(&right->reconciler, descriptor, source, staged_read));
    for (uint32_t scan = 0u; scan < 64u * MAX_SCANS && right->reconciler.state != NOAH_PROFILE_SPLIT_RECONCILER_STOPPED && !noah_profile_split_reconciler_prepared_push_ready(&right->reconciler, NULL); scan++) {
        now += NOAH_PROFILE_SPLIT_ADMISSION_RETRY_MS;
        assert_one_scan_budget(right, true, now);
        assert_one_scan_budget(left, false, now);
    }
    assert(right->reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED);
    assert(right->reconciler.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_VALIDATION_ERROR);
}

// A prepared push past 65,535 bytes: every chunk's offset is whole, the
// receiver stages all 70,000 bytes and its validator checks them all.
static void test_wide_push_through_the_reconciler(void) {
    static half_t                   left;
    static half_t                   right;
    noah_profile_split_descriptor_t descriptor;
    staged_source_t                 source = {.bytes = wide_profile, .length = WIDE_LENGTH};

    half_storage_init(&left);
    half_storage_init(&right);
    pair_init(&left, &right);
    run_pair_until_converged(&left, &right, false);
    descriptor = wide_descriptor(&right, wide_profile);
    run_wide_prepared_push(&left, &right, &descriptor, &source);
    assert(source.reads == chunks_for(WIDE_LENGTH));
    assert(slot_holds(&left, wide_profile, WIDE_LENGTH));
    assert_wide_copy_refused_after_checksums(&left);
}

// A pull past 65,535 bytes: the master asks for every chunk at its whole
// offset and the slave answers each in one large reply.
static void test_wide_pull_through_the_reconciler(void) {
    static half_t                  left;
    static half_t                  right;
    noah_profile_store_record_t    record;
    noah_profile_store_candidate_t candidate;

    half_storage_init(&left);
    half_storage_init(&right);
    // Left committed the wide blob itself; its store keeps no schema rules.
    candidate = (noah_profile_store_candidate_t){
        .format_version          = NOAH_PROFILE_STORE_FORMAT_VERSION,
        .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
        .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
        .domain_mask             = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB,
        .flags                   = NOAH_PROFILE_STORE_FLAG_OVERRIDE,
        .origin_half             = 0u,
        .payload_length          = WIDE_LENGTH,
        .generation              = 32u,
        .payload_crc32           = payload_crc(wide_profile, WIDE_LENGTH),
        .payload_digest          = payload_digest(wide_profile, WIDE_LENGTH),
        .compiled_default_digest = left.compiled_digest,
        .action_abi_digest       = ACTION_ABI_DIGEST,
    };
    assert(noah_profile_store_prepare_begin(&left.store, &candidate) == NOAH_PROFILE_STORE_OK);
    for (uint32_t offset = 0u; offset < WIDE_LENGTH; offset += NOAH_PROFILE_STORE_IO_CHUNK_MAX) {
        assert(noah_profile_store_prepare_write(&left.store, offset, &wide_profile[offset], WIDE_LENGTH - offset < NOAH_PROFILE_STORE_IO_CHUNK_MAX ? WIDE_LENGTH - offset : NOAH_PROFILE_STORE_IO_CHUNK_MAX) == NOAH_PROFILE_STORE_OK);
    }
    assert(noah_profile_store_prepare_commit(&left.store, &record) == NOAH_PROFILE_STORE_OK && record.payload_length == WIDE_LENGTH);
    pair_init(&left, &right);
    for (uint32_t scan = 0u; scan < 16u * MAX_SCANS && noah_profile_other_half_store_backend_state(&right.other_half_store) != NOAH_PROFILE_OTHER_HALF_STORE_REJECTED; scan++) {
        uint32_t now = scan * NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;

        assert_one_scan_budget(&right, true, now);
        assert_one_scan_budget(&left, false, now);
    }
    fprintf(stderr, "wide pull: %u bytes in %u payload requests\n", (unsigned)WIDE_LENGTH, (unsigned)right.payload_requests);
    assert(right.payload_requests <= 2u * chunks_for(WIDE_LENGTH) + 4u);
    assert(slot_holds(&right, wide_profile, WIDE_LENGTH));
    assert_wide_copy_refused_after_checksums(&right);
    assert(!noah_profile_store_saved(&right.store));
}

// Reuse ranges whose source offsets lie past 65,535 bytes: the receiver
// copies them from its own 70,000-byte active profile.
static void test_wide_reuse_through_the_reconciler(void) {
    static half_t                           left;
    static half_t                           right;
    static uint8_t                          copy[WIDE_LENGTH];
    static noah_profile_split_reuse_range_t ranges[2];
    noah_profile_split_v1_source_t          active = {.digest = payload_digest(wide_profile, WIDE_LENGTH), .crc32 = payload_crc(wide_profile, WIDE_LENGTH), .kind = 0u, .origin = 255u};
    noah_profile_split_descriptor_t         descriptor;
    noah_profile_split_reconciler_status_t  status;
    staged_source_t                         source;

    memcpy(copy, wide_profile, WIDE_LENGTH);
    copy[100] ^= 0xFFu;
    memcpy(&copy[2000], &wide_profile[67000], 500u);
    ranges[0] = (noah_profile_split_reuse_range_t){.source = active, .offset = 2000u, .source_offset = 67000u, .length = 500u};
    ranges[1] = (noah_profile_split_reuse_range_t){.source = active, .offset = 66000u, .source_offset = 66000u, .length = 3000u};
    source    = (staged_source_t){.bytes = copy, .length = WIDE_LENGTH, .ranges = ranges, .range_count = 2u};

    half_storage_init_compiled(&left, wide_profile, WIDE_LENGTH);
    half_storage_init_compiled(&right, wide_profile, WIDE_LENGTH);
    pair_init(&left, &right);
    right.reconciler.config.prepared_reuse_plan = staged_plan;
    run_pair_until_converged(&left, &right, false);
    descriptor = wide_descriptor(&right, copy);
    run_wide_prepared_push(&left, &right, &descriptor, &source);
    assert(noah_profile_split_reconciler_status(&right.reconciler, &status) && status.reused_bytes == 3500u);
    assert(right.reconciler.source_unavailable_replies == 0u);
    assert(source.reads == chunks_for(2000u) + chunks_for(66000u - 2500u) + chunks_for(WIDE_LENGTH - 69000u));
    assert(slot_holds(&left, copy, WIDE_LENGTH));
    assert_wide_copy_refused_after_checksums(&left);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    load_maximum_profile(argv[1]);
    // Production slots now exercise both the wide copies and the full fault
    // suite; no early return may hide the original coverage.
    make_wide_profile();
    test_wide_push_through_the_reconciler();
    test_wide_pull_through_the_reconciler();
    test_wide_reuse_through_the_reconciler();
    test_maximum_profile_pull_is_bounded();
    test_maximum_profile_prepares_and_commits_on_the_other_half();
    test_store_completion_polling_requires_the_matching_full_copy();
    test_reuse_ranges_copy_from_the_active_profile_on_the_other_half();
    test_reuse_falls_back_to_bytes_without_the_source();
    test_lost_exchanges_during_reuse_resume_the_same_copy();
    test_an_unencodable_reuse_plan_sends_bytes();
    test_a_cancel_ends_a_reuse_range_midway();
    test_descriptor_changes_are_validated_before_backoff();
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
    test_convergence_only_refuses_newer_other_half_import();
    test_convergence_only_answers_inbound_prepare_busy_without_storage();
    test_receive_refuses_anything_but_one_frame();
    test_prepared_push_collects_expected_mailbox_ack_without_failure_backoff();
    test_prepared_push_pauses_before_commit_then_authorizes();
    test_authorized_commit_survives_one_replayed_reply();
    test_prepared_push_cancel_aborts_other_half_lease();
    test_prepared_abort_is_bounded_when_other_half_goes_silent();
    test_prepared_abort_is_bounded_when_other_half_stays_busy();
    test_maximum_candidate_resolves_within_owner_no_progress_window();
    test_prepared_push_restarts_across_both_half_role_changes();
    test_receiver_durable_prepare_survives_sender_pause();
    test_prepare_authorize_and_cancel_are_immediate_after_timer_high_bit();
    test_cancel_after_prepare_rejection_is_idempotent();
    test_cancel_after_dropped_prepare_begin_releases_or_noops();
    test_cancel_refuses_matching_durable_other_half();
    test_crossed_prepare_begin_loser_abort_does_not_deadlock();
    test_prepared_push_survives_one_lost_exchange_anywhere();
    test_prepared_push_survives_one_garbled_request_anywhere();
    test_prepared_push_survives_one_replayed_reply_anywhere();
    test_prepared_push_resumes_from_the_metadata_poll();
    test_stale_receive_lease_expires_while_the_link_stays_busy();
    test_a_restarted_sender_settles_the_prepared_copy();
    test_a_paused_sender_keeps_the_prepared_copy();
    test_a_sender_holding_the_push_keeps_the_prepared_copy();
    test_a_committing_copy_is_never_released();
    test_a_cached_reply_waits_behind_a_queued_poll();
    test_prepared_push_restarts_when_the_receiver_drops_its_lease();
    test_receiver_recovers_after_a_storage_failure();
    test_receiver_recovers_when_its_abort_write_fails();
    test_refresh_publishes_new_local_authority_during_backoff();
    test_idle_scans_do_not_republish_unchanged_metadata();
    test_converged_steady_state_cost_is_bounded_by_poll_deadline(false);
    test_converged_steady_state_cost_is_bounded_by_poll_deadline(true);
    puts("profile split reconciler host tests passed");
    return 0;
}
