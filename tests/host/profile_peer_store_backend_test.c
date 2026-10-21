#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "users/noah/lib/profile/storage/profile_peer_store_backend.h"
#include "users/noah/lib/profile/storage/profile_record_identity.h"

enum {
    ACTION_ABI_DIGEST = UINT32_C(0x11223344),
};

typedef struct {
    uint8_t  bytes[NOAH_PROFILE_STORAGE_REGION_SIZE];
    uint32_t reads;
    uint32_t writes;
    bool     fail_reads;
} memory_t;

typedef struct {
    memory_t                               memory;
    noah_profile_store_t                   store;
    noah_effective_profile_provider_t      provider;
    noah_profile_candidate_store_backend_t candidate_backend;
    noah_profile_peer_store_backend_t      peer;
    uint32_t                               compiled_digest;
} fixture_t;

static const uint8_t empty_profile[] = {'N', 'L', 'P', '1', 4u, 0u, 0u, 1u};

static uint32_t always_safe(void *context) {
    (void)context;
    return 0u;
}

static bool memory_read(void *context, noah_profile_storage_address_t address, uint8_t *target, uint32_t length) {
    memory_t *memory = context;

    memory->reads++;
    if (memory->fail_reads || !target || length == 0u || (uint32_t)address + length > sizeof(memory->bytes)) {
        return false;
    }
    memcpy(target, &memory->bytes[address], length);
    return true;
}

static bool memory_write(void *context, noah_profile_storage_address_t address, const uint8_t *source, uint32_t length) {
    memory_t *memory = context;

    memory->writes++;
    if (!source || length == 0u || (uint32_t)address + length > sizeof(memory->bytes)) {
        return false;
    }
    memcpy(&memory->bytes[address], source, length);
    return true;
}

static uint32_t payload_crc(const uint8_t *payload, uint32_t length) {
    return noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, payload, length));
}

static uint32_t payload_digest(const uint8_t *payload, uint32_t length) {
    return noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, payload, length);
}

static noah_profile_split_descriptor_t descriptor_for(const fixture_t *fixture, uint32_t generation, uint8_t origin, uint8_t profile_flags, uint8_t domain_mask) {
    return (noah_profile_split_descriptor_t){
        .generation              = generation,
        .payload_crc32           = payload_crc(empty_profile, sizeof(empty_profile)),
        .payload_digest          = payload_digest(empty_profile, sizeof(empty_profile)),
        .compiled_default_digest = fixture->compiled_digest,
        .action_abi_digest       = ACTION_ABI_DIGEST,
        .payload_length          = sizeof(empty_profile),
        .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
        .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
        .domain_mask             = domain_mask,
        .profile_flags           = profile_flags,
        .origin_half             = origin,
        .readable                = true,
        .has_profile             = true,
    };
}

// The half's active profile is the compiled `bytes`, as a fresh half's is.
static void fixture_init_compiled(fixture_t *fixture, const uint8_t *bytes, uint32_t length) {
    noah_profile_reader_t                     reader = noah_profile_reader_from_memory(bytes, length);
    noah_profile_validator_v1_profile_t       profile;
    noah_effective_profile_snapshot_t         compiled;
    noah_profile_validator_v1_compatibility_t compatibility = noah_profile_validator_v1_default_compatibility(ACTION_ABI_DIGEST);
    noah_profile_store_record_t               selected;

    memset(fixture, 0, sizeof(*fixture));
    memset(fixture->memory.bytes, 0xff, sizeof(fixture->memory.bytes));
    memset(&profile, 0, sizeof(profile));
    profile.byte_length       = length;
    profile.crc32             = payload_crc(bytes, length);
    profile.digest            = payload_digest(bytes, length);
    profile.action_abi_digest = ACTION_ABI_DIGEST;
    assert(noah_effective_profile_snapshot_make_compiled(&profile, &reader, 0u, &compiled) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_init(&fixture->provider, &compiled, always_safe, NULL, NULL, 0u) == NOAH_EFFECTIVE_PROFILE_OK);
    fixture->compiled_digest = compiled.identity.payload_digest;

    noah_profile_store_init(&fixture->store,
                            (noah_profile_store_io_t){
                                .read    = memory_read,
                                .write   = memory_write,
                                .context = &fixture->memory,
                            },
                            (noah_profile_store_compatibility_t){
                                .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
                                .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
                                .compiled_default_digest = fixture->compiled_digest,
                                .action_abi_digest       = ACTION_ABI_DIGEST,
                            });
    assert(noah_profile_store_boot_select(&fixture->store, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    compatibility.required_domain_mask = 0u;
    noah_profile_candidate_store_backend_init(&fixture->candidate_backend, &fixture->store, &fixture->provider, &compatibility, fixture->compiled_digest, 1u);
    assert(fixture->candidate_backend.reuse_guard_installed);
    noah_profile_peer_store_backend_init(&fixture->peer, &fixture->candidate_backend);
}

static void fixture_init(fixture_t *fixture) {
    fixture_init_compiled(fixture, empty_profile, sizeof(empty_profile));
}

static noah_profile_peer_store_result_t finish_commit(fixture_t *fixture, const noah_profile_split_descriptor_t *descriptor) {
    noah_profile_peer_store_result_t result = noah_profile_peer_store_backend_commit_begin(&fixture->peer, descriptor);
    uint16_t                         steps  = 0u;

    while (result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS) {
        assert(++steps < 128u);
        result = noah_profile_peer_store_backend_step(&fixture->peer, NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET);
    }
    return result;
}

static void transfer_payload(fixture_t *fixture, const noah_profile_split_descriptor_t *descriptor) {
    uint16_t offset = 0u;

    assert(noah_profile_peer_store_backend_begin(&fixture->peer, descriptor) == NOAH_PROFILE_PEER_STORE_OK);
    while (offset < descriptor->payload_length) {
        uint8_t length = (uint8_t)(descriptor->payload_length - offset);
        if (length > NOAH_PROFILE_SPLIT_V1_CHUNK_MAX) {
            length = NOAH_PROFILE_SPLIT_V1_CHUNK_MAX;
        }
        assert(noah_profile_peer_store_backend_write(&fixture->peer, descriptor->generation, descriptor->payload_digest, offset, &empty_profile[offset], length) == NOAH_PROFILE_PEER_STORE_OK);
        offset = (uint16_t)(offset + length);
    }
}

static void test_exact_identity_commit_reboot_and_idempotence(void) {
    fixture_t                       fixture;
    noah_profile_split_descriptor_t descriptor;
    noah_profile_store_t            rebooted;
    noah_profile_store_record_t     selected;
    noah_effective_profile_status_t provider_status;
    uint32_t                        writes_after_commit;

    fixture_init(&fixture);
    descriptor = descriptor_for(&fixture, 5u, 0u, NOAH_PROFILE_STORE_FLAG_OVERRIDE, 0u);
    uint32_t writes_before = fixture.memory.writes;
    assert(noah_profile_peer_store_backend_begin(&fixture.peer, NULL) == NOAH_PROFILE_PEER_STORE_INVALID_METADATA);
    assert(fixture.memory.writes == writes_before);
    transfer_payload(&fixture, &descriptor);
    assert(noah_profile_peer_store_backend_begin(&fixture.peer, &descriptor) == NOAH_PROFILE_PEER_STORE_IN_PROGRESS);
    assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, 0u, empty_profile, sizeof(empty_profile)) == NOAH_PROFILE_PEER_STORE_OK);
    assert(finish_commit(&fixture, &descriptor) == NOAH_PROFILE_PEER_STORE_OK);
    assert(noah_profile_peer_store_backend_state(&fixture.peer) == NOAH_PROFILE_PEER_STORE_COMMITTED);
    assert(fixture.store.committed.generation == 5u);
    assert(fixture.store.committed.origin_half == 0u);
    assert(fixture.store.committed.flags == NOAH_PROFILE_STORE_FLAG_OVERRIDE);
    assert(fixture.store.committed.domain_mask == 0u);
    assert(fixture.store.committed.compiled_default_digest == fixture.compiled_digest);
    writes_after_commit = fixture.memory.writes;
    assert(noah_profile_peer_store_backend_begin(&fixture.peer, &descriptor) == NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED);
    assert(fixture.memory.writes == writes_after_commit);

    noah_profile_peer_store_backend_t restarted_peer;
    noah_profile_peer_store_backend_init(&restarted_peer, &fixture.candidate_backend);
    assert(noah_profile_peer_store_backend_begin(&restarted_peer, &descriptor) == NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED);
    assert(noah_profile_peer_store_backend_begin(&restarted_peer, &descriptor) == NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED);
    assert(fixture.memory.writes == writes_after_commit);

    noah_profile_store_init(&rebooted, fixture.store.io, fixture.store.compatibility);
    assert(noah_profile_store_boot_select(&rebooted, &selected) == NOAH_PROFILE_STORE_OK);
    assert(selected.generation == descriptor.generation && selected.origin_half == descriptor.origin_half);
    assert(selected.flags == descriptor.profile_flags && selected.domain_mask == descriptor.domain_mask);
    assert(selected.payload_crc32 == descriptor.payload_crc32 && selected.payload_digest == descriptor.payload_digest);
    assert(noah_effective_profile_provider_status(&fixture.provider, &provider_status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(provider_status.active.generation == 0u && provider_status.active.kind == NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS);
}

static void test_reset_record_preserves_zero_flags_without_activation(void) {
    fixture_t                       fixture;
    noah_profile_split_descriptor_t descriptor;
    noah_effective_profile_status_t status;

    fixture_init(&fixture);
    descriptor = descriptor_for(&fixture, 9u, 1u, 0u, 0u);
    transfer_payload(&fixture, &descriptor);
    assert(finish_commit(&fixture, &descriptor) == NOAH_PROFILE_PEER_STORE_OK);
    assert(fixture.store.committed.flags == 0u);
    assert(!fixture.candidate_backend.activation_requested);
    assert(noah_effective_profile_provider_status(&fixture.provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(status.active.kind == NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS);
}

static void test_ordering_and_compatibility_reject_before_write(void) {
    fixture_t                       fixture;
    noah_profile_split_descriptor_t committed;
    noah_profile_split_descriptor_t candidate;
    uint32_t                        writes;

    fixture_init(&fixture);
    committed = descriptor_for(&fixture, 3u, 0u, NOAH_PROFILE_STORE_FLAG_OVERRIDE, 0u);
    transfer_payload(&fixture, &committed);
    assert(finish_commit(&fixture, &committed) == NOAH_PROFILE_PEER_STORE_OK);

    writes    = fixture.memory.writes;
    candidate = descriptor_for(&fixture, 2u, 1u, NOAH_PROFILE_STORE_FLAG_OVERRIDE, 0u);
    assert(noah_profile_peer_store_backend_begin(&fixture.peer, &candidate) == NOAH_PROFILE_PEER_STORE_STALE);
    assert(fixture.memory.writes == writes);

    candidate = descriptor_for(&fixture, 3u, 1u, NOAH_PROFILE_STORE_FLAG_OVERRIDE, 0u);
    assert(noah_profile_peer_store_backend_begin(&fixture.peer, &candidate) == NOAH_PROFILE_PEER_STORE_CONFLICT);
    assert(fixture.memory.writes == writes);

    candidate = committed;
    candidate.payload_digest ^= 1u;
    assert(noah_profile_peer_store_backend_begin(&fixture.peer, &candidate) == NOAH_PROFILE_PEER_STORE_CORRUPT);
    assert(fixture.memory.writes == writes);

    candidate = descriptor_for(&fixture, 4u, 1u, NOAH_PROFILE_STORE_FLAG_OVERRIDE, 0u);
    candidate.compiled_default_digest ^= 1u;
    assert(noah_profile_peer_store_backend_begin(&fixture.peer, &candidate) == NOAH_PROFILE_PEER_STORE_INCOMPATIBLE);
    assert(fixture.memory.writes == writes);
}

static void test_chunk_replay_gap_overlap_and_conflict(void) {
    fixture_t                       fixture;
    noah_profile_split_descriptor_t descriptor;
    uint8_t                         conflicting[4];
    uint32_t                        writes;

    fixture_init(&fixture);
    descriptor = descriptor_for(&fixture, 1u, 0u, NOAH_PROFILE_STORE_FLAG_OVERRIDE, 0u);
    assert(noah_profile_peer_store_backend_begin(&fixture.peer, &descriptor) == NOAH_PROFILE_PEER_STORE_OK);
    assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, 0u, empty_profile, 4u) == NOAH_PROFILE_PEER_STORE_OK);
    writes = fixture.memory.writes;
    assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, 0u, empty_profile, 4u) == NOAH_PROFILE_PEER_STORE_OK);
    assert(fixture.memory.writes == writes);
    assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, 2u, &empty_profile[2], 4u) == NOAH_PROFILE_PEER_STORE_RANGE_ERROR);
    assert(noah_profile_peer_store_backend_state(&fixture.peer) == NOAH_PROFILE_PEER_STORE_REJECTED);

    assert(noah_profile_peer_store_backend_begin(&fixture.peer, &descriptor) == NOAH_PROFILE_PEER_STORE_OK);
    assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, 2u, &empty_profile[2], 2u) == NOAH_PROFILE_PEER_STORE_RANGE_ERROR);

    assert(noah_profile_peer_store_backend_begin(&fixture.peer, &descriptor) == NOAH_PROFILE_PEER_STORE_OK);
    assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, 0u, empty_profile, 4u) == NOAH_PROFILE_PEER_STORE_OK);
    memcpy(conflicting, empty_profile, sizeof(conflicting));
    conflicting[0] ^= 1u;
    assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, 0u, conflicting, sizeof(conflicting)) == NOAH_PROFILE_PEER_STORE_CHUNK_CONFLICT);
}

static void test_domain_mismatch_rejects_without_commit(void) {
    fixture_t                       fixture;
    noah_profile_split_descriptor_t descriptor;

    fixture_init(&fixture);
    descriptor = descriptor_for(&fixture, 1u, 0u, NOAH_PROFILE_STORE_FLAG_OVERRIDE, NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB);
    transfer_payload(&fixture, &descriptor);
    assert(finish_commit(&fixture, &descriptor) == NOAH_PROFILE_PEER_STORE_VALIDATION_ERROR);
    assert(fixture.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
    assert(!fixture.store.prepare_active && !fixture.provider.backing_reuse_in_progress);
    assert(noah_profile_peer_store_backend_validation_error(&fixture.peer) != NULL);
}

static void test_durability_unknown_requires_boot_reconciliation(void) {
    fixture_t                         fixture;
    noah_profile_split_descriptor_t   descriptor;
    noah_profile_peer_store_backend_t restarted_peer;
    noah_profile_store_record_t       selected;
    noah_profile_peer_store_result_t  result;
    uint16_t                          steps = 0u;

    fixture_init(&fixture);
    descriptor = descriptor_for(&fixture, 1u, 0u, NOAH_PROFILE_STORE_FLAG_OVERRIDE, 0u);
    transfer_payload(&fixture, &descriptor);
    result = noah_profile_peer_store_backend_commit_begin(&fixture.peer, &descriptor);
    while (result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS && fixture.store.commit_phase != NOAH_PROFILE_STORE_COMMIT_MARKER_READBACK) {
        assert(++steps < 128u);
        result = noah_profile_peer_store_backend_step(&fixture.peer, NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET);
    }
    assert(result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS);
    fixture.memory.fail_reads = true;
    assert(noah_profile_peer_store_backend_step(&fixture.peer, NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET) == NOAH_PROFILE_PEER_STORE_DURABILITY_UNKNOWN);
    assert(fixture.store.reconciliation_required);
    assert(noah_profile_peer_store_backend_abort(&fixture.peer, &descriptor) == NOAH_PROFILE_PEER_STORE_DURABILITY_UNKNOWN);

    noah_profile_peer_store_backend_init(&restarted_peer, &fixture.candidate_backend);
    assert(noah_profile_peer_store_backend_begin(&restarted_peer, &descriptor) == NOAH_PROFILE_PEER_STORE_DURABILITY_UNKNOWN);
    fixture.memory.fail_reads = false;
    assert(noah_profile_store_boot_select(&fixture.store, &selected) == NOAH_PROFILE_STORE_OK);
    assert(selected.generation == descriptor.generation && selected.origin_half == descriptor.origin_half);
    assert(selected.payload_crc32 == descriptor.payload_crc32 && selected.payload_digest == descriptor.payload_digest);
    assert(selected.compiled_default_digest == descriptor.compiled_default_digest && selected.action_abi_digest == descriptor.action_abi_digest);
    assert(!fixture.store.reconciliation_required);
    noah_profile_peer_store_backend_init(&restarted_peer, &fixture.candidate_backend);
    assert(noah_profile_peer_store_backend_begin(&restarted_peer, &descriptor) == NOAH_PROFILE_PEER_STORE_BUSY);
}

static bool staged_equals(fixture_t *fixture, const uint8_t *expected, uint32_t length) {
    uint8_t bytes[NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX];

    for (uint32_t offset = 0u; offset < length; offset += sizeof(bytes)) {
        uint8_t piece = length - offset < sizeof(bytes) ? (uint8_t)(length - offset) : (uint8_t)sizeof(bytes);

        if (fixture->peer.interface.read(fixture->peer.interface.context, offset, bytes, piece) != NOAH_PROFILE_CANDIDATE_BACKEND_OK || memcmp(bytes, &expected[offset], piece) != 0) {
            return false;
        }
    }
    return true;
}

// A copy is written in full split chunks (up to 106 bytes, stored in pieces)
// and in reuse ranges read from this half's own active profile, whose
// identity every read checks.
static void test_reuse_fills_ranges_from_the_active_profile(void) {
    static fixture_t                fixture;
    static uint8_t                  active[300];
    static uint8_t                  target[400];
    noah_profile_split_descriptor_t descriptor;
    noah_profile_split_v1_source_t  source;
    noah_profile_split_v1_source_t  wrong;

    for (uint16_t index = 0u; index < sizeof(active); index++) {
        active[index] = (uint8_t)(index * 7u + 3u);
    }
    for (uint16_t index = 0u; index < sizeof(target); index++) {
        target[index] = (uint8_t)(index ^ 0x5Au);
    }
    // [110, 230) comes from the active profile's [150, 270).
    memcpy(&target[110], &active[150], 120u);
    fixture_init_compiled(&fixture, active, sizeof(active));
    source = (noah_profile_split_v1_source_t){.generation = 0u, .digest = payload_digest(active, sizeof(active)), .crc32 = payload_crc(active, sizeof(active)), .kind = 0u, .origin = 255u};

    descriptor                = descriptor_for(&fixture, 4u, 1u, NOAH_PROFILE_STORE_FLAG_OVERRIDE, 0u);
    descriptor.payload_length = sizeof(target);
    descriptor.payload_crc32  = payload_crc(target, sizeof(target));
    descriptor.payload_digest = payload_digest(target, sizeof(target));
    assert(noah_profile_peer_store_backend_begin(&fixture.peer, &descriptor) == NOAH_PROFILE_PEER_STORE_OK);

    // One full chunk and its exact repeat.
    assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, 0u, target, NOAH_PROFILE_SPLIT_V1_CHUNK_MAX) == NOAH_PROFILE_PEER_STORE_OK);
    assert(fixture.peer.next_offset == NOAH_PROFILE_SPLIT_V1_CHUNK_MAX);
    assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, 0u, target, NOAH_PROFILE_SPLIT_V1_CHUNK_MAX) == NOAH_PROFILE_PEER_STORE_OK);
    // ...then the bytes up to the reuse range.
    assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, NOAH_PROFILE_SPLIT_V1_CHUNK_MAX, &target[NOAH_PROFILE_SPLIT_V1_CHUNK_MAX], (uint8_t)(110u - NOAH_PROFILE_SPLIT_V1_CHUNK_MAX)) == NOAH_PROFILE_PEER_STORE_OK);
    assert(fixture.peer.next_offset == 110u);

    // A source this half's active profile is not leaves the copy open.
    wrong = source;
    wrong.digest ^= 1u;
    assert(noah_profile_peer_store_backend_reuse(&fixture.peer, descriptor.generation, descriptor.payload_digest, 110u, &wrong, 150u, 230u, 50u) == NOAH_PROFILE_PEER_STORE_SOURCE_UNAVAILABLE);
    wrong = (noah_profile_split_v1_source_t){.generation = 3u, .digest = source.digest, .crc32 = source.crc32, .kind = 1u, .origin = 0u};
    assert(noah_profile_peer_store_backend_reuse(&fixture.peer, descriptor.generation, descriptor.payload_digest, 110u, &wrong, 150u, 230u, 50u) == NOAH_PROFILE_PEER_STORE_SOURCE_UNAVAILABLE);
    // Past the source's end.
    assert(noah_profile_peer_store_backend_reuse(&fixture.peer, descriptor.generation, descriptor.payload_digest, 110u, &source, 290u, 230u, 50u) == NOAH_PROFILE_PEER_STORE_SOURCE_UNAVAILABLE);
    assert(noah_profile_peer_store_backend_state(&fixture.peer) == NOAH_PROFILE_PEER_STORE_RECEIVING && fixture.peer.next_offset == 110u);

    // The range is copied a budget at a time, continuing from the next offset.
    assert(noah_profile_peer_store_backend_reuse(&fixture.peer, descriptor.generation, descriptor.payload_digest, 110u, &source, 150u, 230u, 50u) == NOAH_PROFILE_PEER_STORE_OK);
    assert(fixture.peer.next_offset == 160u);
    assert(noah_profile_peer_store_backend_reuse(&fixture.peer, descriptor.generation, descriptor.payload_digest, 110u, &source, 150u, 230u, 50u) == NOAH_PROFILE_PEER_STORE_OK);
    assert(fixture.peer.next_offset == 210u);
    assert(noah_profile_peer_store_backend_reuse(&fixture.peer, descriptor.generation, descriptor.payload_digest, 110u, &source, 150u, 230u, 50u) == NOAH_PROFILE_PEER_STORE_OK);
    assert(fixture.peer.next_offset == 230u);
    // A finished range asked again changes nothing.
    assert(noah_profile_peer_store_backend_reuse(&fixture.peer, descriptor.generation, descriptor.payload_digest, 110u, &source, 150u, 230u, 50u) == NOAH_PROFILE_PEER_STORE_OK);
    assert(fixture.peer.next_offset == 230u);

    assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, 230u, &target[230], NOAH_PROFILE_SPLIT_V1_CHUNK_MAX) == NOAH_PROFILE_PEER_STORE_OK);
    assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, 230u + NOAH_PROFILE_SPLIT_V1_CHUNK_MAX, &target[230u + NOAH_PROFILE_SPLIT_V1_CHUNK_MAX], (uint8_t)(sizeof(target) - 230u - NOAH_PROFILE_SPLIT_V1_CHUNK_MAX)) == NOAH_PROFILE_PEER_STORE_OK);
    assert(fixture.peer.next_offset == sizeof(target));
    assert(staged_equals(&fixture, target, sizeof(target)));
    assert(fixture.store.candidate_written == sizeof(target));
    // The store's running checksums cover exactly the staged bytes.
    assert(noah_profile_crc32_finish(fixture.store.candidate_crc32_state) == descriptor.payload_crc32 && fixture.store.candidate_digest_state == descriptor.payload_digest);

    // A conflicting repeat of a full chunk is caught in any of its pieces.
    {
        uint8_t changed[NOAH_PROFILE_SPLIT_V1_CHUNK_MAX];

        memcpy(changed, &target[230], sizeof(changed));
        changed[sizeof(changed) - 1u] ^= 1u;
        assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, 230u, changed, sizeof(changed)) == NOAH_PROFILE_PEER_STORE_CHUNK_CONFLICT);
        assert(noah_profile_peer_store_backend_state(&fixture.peer) == NOAH_PROFILE_PEER_STORE_REJECTED);
        assert(noah_profile_candidate_store_backend_admission_owner(&fixture.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    }

    // A range that starts past the next offset is a gap: the copy is
    // rejected and its storage released.
    descriptor.generation = 5u;
    assert(noah_profile_peer_store_backend_begin(&fixture.peer, &descriptor) == NOAH_PROFILE_PEER_STORE_OK);
    assert(noah_profile_peer_store_backend_reuse(&fixture.peer, descriptor.generation, descriptor.payload_digest, 1u, &source, 0u, 20u, 50u) == NOAH_PROFILE_PEER_STORE_RANGE_ERROR);
    assert(noah_profile_peer_store_backend_state(&fixture.peer) == NOAH_PROFILE_PEER_STORE_REJECTED);
    assert(noah_profile_candidate_store_backend_admission_owner(&fixture.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
}

// The receiving end of the split copy past 65,535 bytes (D-F28): chunks
// across the 64 KiB line, a reuse range landing past it, and the rest, staged
// whole with the store's running checksums over all 70,000 bytes.
static void test_copy_over_64_kib(void) {
    enum { WIDE = 70000u, REUSED_AT = 66000u, REUSED = 120u };
    static fixture_t                fixture;
    static uint8_t                  active[300];
    static uint8_t                  target[WIDE];
    noah_profile_split_descriptor_t descriptor;
    noah_profile_split_v1_source_t  source;
    uint32_t                        offset;

    for (uint32_t index = 0u; index < sizeof(active); index++) active[index] = (uint8_t)(index * 7u + 3u);
    for (uint32_t index = 0u; index < WIDE; index++) target[index] = (uint8_t)(index ^ (index >> 9u));
    memcpy(&target[REUSED_AT], &active[150], REUSED);
    fixture_init_compiled(&fixture, active, sizeof(active));
    source = (noah_profile_split_v1_source_t){.generation = 0u, .digest = payload_digest(active, sizeof(active)), .crc32 = payload_crc(active, sizeof(active)), .kind = 0u, .origin = 255u};
    descriptor                = descriptor_for(&fixture, 4u, 1u, NOAH_PROFILE_STORE_FLAG_OVERRIDE, 0u);
    descriptor.payload_length = WIDE;
    descriptor.payload_crc32  = payload_crc(target, WIDE);
    descriptor.payload_digest = payload_digest(target, WIDE);
    assert(noah_profile_peer_store_backend_begin(&fixture.peer, &descriptor) == NOAH_PROFILE_PEER_STORE_OK);

    for (offset = 0u; offset < REUSED_AT;) {
        uint8_t length = REUSED_AT - offset < NOAH_PROFILE_SPLIT_V1_CHUNK_MAX ? (uint8_t)(REUSED_AT - offset) : NOAH_PROFILE_SPLIT_V1_CHUNK_MAX;
        assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, offset, &target[offset], length) == NOAH_PROFILE_PEER_STORE_OK);
        offset += length;
        assert(noah_profile_peer_store_backend_next_offset(&fixture.peer) == offset);
    }
    // A repeat of a chunk past the 64 KiB line is checked against its own
    // staged bytes, not those 64 KiB earlier.
    assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, 65600u, &target[65600u], 20u) == NOAH_PROFILE_PEER_STORE_OK);
    while (noah_profile_peer_store_backend_next_offset(&fixture.peer) < REUSED_AT + REUSED) {
        assert(noah_profile_peer_store_backend_reuse(&fixture.peer, descriptor.generation, descriptor.payload_digest, REUSED_AT, &source, 150u, REUSED_AT + REUSED, 50u) == NOAH_PROFILE_PEER_STORE_OK);
    }
    for (offset = REUSED_AT + REUSED; offset < WIDE;) {
        uint8_t length = WIDE - offset < NOAH_PROFILE_SPLIT_V1_CHUNK_MAX ? (uint8_t)(WIDE - offset) : NOAH_PROFILE_SPLIT_V1_CHUNK_MAX;
        assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, offset, &target[offset], length) == NOAH_PROFILE_PEER_STORE_OK);
        offset += length;
    }
    assert(noah_profile_peer_store_backend_next_offset(&fixture.peer) == WIDE);
    assert(fixture.store.candidate_written == WIDE);
    assert(staged_equals(&fixture, target, WIDE));
    assert(noah_profile_crc32_finish(fixture.store.candidate_crc32_state) == descriptor.payload_crc32 && fixture.store.candidate_digest_state == descriptor.payload_digest);
    // Past the declared length is a range error, not a wrap.
    assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, WIDE, target, 1u) == NOAH_PROFILE_PEER_STORE_RANGE_ERROR);
}

// A schema 4 blob of one RGB domain filling `length` bytes: its envelope and
// checksums are sound, and the validator refuses the domain at its end, since
// RGB keeps 16-bit internal offsets (D-F28).
static void make_wide_rgb_blob(uint8_t *blob, uint32_t length) {
    memcpy(blob, (const uint8_t[]){'N', 'L', 'P', '1', NOAH_PROFILE_SCHEMA_MAJOR, 0u, 1u, 1u, 0x10u, NOAH_PROFILE_DOMAIN_VERSION_RGB}, 10u);
    blob[10] = (uint8_t)(length - 14u);
    blob[11] = (uint8_t)((length - 14u) >> 8u);
    blob[12] = (uint8_t)((length - 14u) >> 16u);
    blob[13] = 0u;
    for (uint32_t index = 14u; index < length; index++) blob[index] = (uint8_t)(index * 13u + 7u);
}

// The receiving half's real validator over a 70,000-byte copy (D-F28): one
// byte changed past 64 KiB fails the checksum; intact, the copy passes it and
// is refused at the end of its RGB domain, a location past 64 KiB.
static void test_validation_over_64_kib(void) {
    enum { WIDE = 70000u };
    static fixture_t                 fixture;
    static uint8_t                   blob[WIDE];
    static uint8_t                   sent[WIDE];
    noah_profile_split_descriptor_t  descriptor;
    noah_profile_peer_store_result_t result;

    make_wide_rgb_blob(blob, WIDE);
    for (uint8_t corrupt = 0u; corrupt < 2u; corrupt++) {
        uint32_t steps = 0u;

        fixture_init(&fixture);
        memcpy(sent, blob, WIDE);
        if (corrupt) sent[66000u] ^= 1u;
        descriptor                = descriptor_for(&fixture, 4u, 1u, NOAH_PROFILE_STORE_FLAG_OVERRIDE, NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB);
        descriptor.payload_length = WIDE;
        descriptor.payload_crc32  = payload_crc(blob, WIDE);
        descriptor.payload_digest = payload_digest(blob, WIDE);
        assert(noah_profile_peer_store_backend_begin(&fixture.peer, &descriptor) == NOAH_PROFILE_PEER_STORE_OK);
        for (uint32_t offset = 0u; offset < WIDE;) {
            uint8_t length = WIDE - offset < NOAH_PROFILE_SPLIT_V1_CHUNK_MAX ? (uint8_t)(WIDE - offset) : NOAH_PROFILE_SPLIT_V1_CHUNK_MAX;

            assert(noah_profile_peer_store_backend_write(&fixture.peer, descriptor.generation, descriptor.payload_digest, offset, &sent[offset], length) == NOAH_PROFILE_PEER_STORE_OK);
            offset += length;
        }
        result = noah_profile_peer_store_backend_commit_begin(&fixture.peer, &descriptor);
        while (result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS) {
            assert(++steps < 2u * WIDE / NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET + 64u);
            result = noah_profile_peer_store_backend_step(&fixture.peer, NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET);
        }
        assert(result == NOAH_PROFILE_PEER_STORE_VALIDATION_ERROR);
        if (corrupt) {
            assert(fixture.peer.validation_error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_CHECKSUM_MISMATCH);
        } else {
            assert(fixture.peer.validation_error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_VALIDATION_REJECTED);
            assert(fixture.peer.validation_error.domain_id == NOAH_PROFILE_DOMAIN_V1_RGB && fixture.peer.validation_error.byte_offset == WIDE);
        }
        assert(noah_profile_candidate_store_backend_admission_owner(&fixture.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
        assert(fixture.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
    }
}

// A record, a staged candidate and a split descriptor are one identity in
// three forms; each comparison checks every field of the tuple, and only a
// readable profile descriptor names a record or a candidate.
static void test_identity_forms_compare_one_tuple(void) {
    noah_profile_store_candidate_t  candidate = {
        .format_version          = NOAH_PROFILE_STORE_FORMAT_VERSION,
        .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
        .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
        .domain_mask             = 3u,
        .flags                   = NOAH_PROFILE_STORE_FLAG_OVERRIDE,
        .payload_length          = 40u,
        .generation              = 9u,
        .origin_half             = 1u,
        .payload_crc32           = 0x01020304u,
        .payload_digest          = 0x05060708u,
        .compiled_default_digest = 0x090a0b0cu,
        .action_abi_digest       = ACTION_ABI_DIGEST,
    };
    noah_profile_store_record_t     record = {
        .slot                    = NOAH_PROFILE_SLOT_B,
        .format_version          = candidate.format_version,
        .schema_major            = candidate.schema_major,
        .schema_minor            = candidate.schema_minor,
        .domain_mask             = candidate.domain_mask,
        .flags                   = candidate.flags,
        .payload_length          = candidate.payload_length,
        .generation              = candidate.generation,
        .origin_half             = candidate.origin_half,
        .payload_crc32           = candidate.payload_crc32,
        .payload_digest          = candidate.payload_digest,
        .compiled_default_digest = candidate.compiled_default_digest,
        .action_abi_digest       = candidate.action_abi_digest,
    };
    noah_profile_split_descriptor_t descriptor = {
        .generation              = candidate.generation,
        .payload_crc32           = candidate.payload_crc32,
        .payload_digest          = candidate.payload_digest,
        .compiled_default_digest = candidate.compiled_default_digest,
        .action_abi_digest       = candidate.action_abi_digest,
        .payload_length          = candidate.payload_length,
        .schema_major            = candidate.schema_major,
        .schema_minor            = candidate.schema_minor,
        .domain_mask             = candidate.domain_mask,
        .profile_flags           = candidate.flags,
        .origin_half             = candidate.origin_half,
        .readable                = true,
        .has_profile             = true,
    };
    noah_profile_store_record_t     other_slot = record;
    noah_profile_split_descriptor_t changed;
    noah_profile_store_record_t     changed_record;
    noah_profile_store_candidate_t  changed_candidate;

    assert(noah_profile_record_matches_descriptor(&record, &descriptor));
    assert(noah_profile_candidate_matches_descriptor(&candidate, &descriptor));
    assert(noah_profile_descriptor_equal(&descriptor, &descriptor));
    assert(noah_profile_record_equal(&record, &record) && noah_profile_candidate_equal(&candidate, &candidate));
    // The same saved profile in the other slot is the same profile, not the same record.
    other_slot.slot = NOAH_PROFILE_SLOT_A;
    assert(noah_profile_record_same_profile(&record, &other_slot) && !noah_profile_record_equal(&record, &other_slot));
    assert(noah_profile_record_matches_descriptor(&other_slot, &descriptor));
    // Nothing compares equal to nothing.
    assert(!noah_profile_record_equal(NULL, &record) && !noah_profile_record_same_profile(&record, NULL) && !noah_profile_candidate_equal(&candidate, NULL));
    assert(!noah_profile_descriptor_equal(NULL, &descriptor) && !noah_profile_record_matches_descriptor(NULL, &descriptor) && !noah_profile_candidate_matches_descriptor(&candidate, NULL));
    // A record that is in no slot, or in another store format, is named by no descriptor.
    changed_record      = record;
    changed_record.slot = NOAH_PROFILE_SLOT_NONE;
    assert(!noah_profile_record_matches_descriptor(&changed_record, &descriptor));
    changed_record                = record;
    changed_record.format_version = (uint8_t)(NOAH_PROFILE_STORE_FORMAT_VERSION + 1u);
    assert(!noah_profile_record_matches_descriptor(&changed_record, &descriptor) && !noah_profile_record_same_profile(&changed_record, &record));
    // An unreadable descriptor, or one that says no profile, names nothing,
    // and readability is part of descriptor equality.
    changed          = descriptor;
    changed.readable = false;
    assert(!noah_profile_record_matches_descriptor(&record, &changed) && !noah_profile_candidate_matches_descriptor(&candidate, &changed) && !noah_profile_descriptor_equal(&changed, &descriptor));
    changed             = descriptor;
    changed.has_profile = false;
    assert(!noah_profile_record_matches_descriptor(&record, &changed) && !noah_profile_candidate_matches_descriptor(&candidate, &changed) && !noah_profile_descriptor_equal(&changed, &descriptor));
    // Every field of the tuple counts.
    for (uint8_t field = 0u; field < 11u; field++) {
        changed           = descriptor;
        changed_record    = record;
        changed_candidate = candidate;
        switch (field) {
            case 0: changed.generation++; changed_record.generation++; changed_candidate.generation++; break;
            case 1: changed.payload_crc32 ^= 1u; changed_record.payload_crc32 ^= 1u; changed_candidate.payload_crc32 ^= 1u; break;
            case 2: changed.payload_digest ^= 1u; changed_record.payload_digest ^= 1u; changed_candidate.payload_digest ^= 1u; break;
            case 3: changed.compiled_default_digest ^= 1u; changed_record.compiled_default_digest ^= 1u; changed_candidate.compiled_default_digest ^= 1u; break;
            case 4: changed.action_abi_digest ^= 1u; changed_record.action_abi_digest ^= 1u; changed_candidate.action_abi_digest ^= 1u; break;
            case 5: changed.payload_length++; changed_record.payload_length++; changed_candidate.payload_length++; break;
            case 6: changed.schema_major++; changed_record.schema_major++; changed_candidate.schema_major++; break;
            case 7: changed.schema_minor++; changed_record.schema_minor++; changed_candidate.schema_minor++; break;
            case 8: changed.domain_mask ^= 4u; changed_record.domain_mask ^= 4u; changed_candidate.domain_mask ^= 4u; break;
            case 9: changed.profile_flags ^= 1u; changed_record.flags ^= 1u; changed_candidate.flags ^= 1u; break;
            default: changed.origin_half ^= 1u; changed_record.origin_half ^= 1u; changed_candidate.origin_half ^= 1u; break;
        }
        assert(!noah_profile_descriptor_equal(&changed, &descriptor));
        assert(!noah_profile_record_matches_descriptor(&record, &changed) && !noah_profile_candidate_matches_descriptor(&candidate, &changed));
        assert(!noah_profile_record_matches_descriptor(&changed_record, &descriptor) && !noah_profile_candidate_matches_descriptor(&changed_candidate, &descriptor));
        assert(!noah_profile_record_same_profile(&changed_record, &record) && !noah_profile_candidate_equal(&changed_candidate, &candidate));
        // The changed forms still agree with one another.
        assert(noah_profile_record_matches_descriptor(&changed_record, &changed) && noah_profile_candidate_matches_descriptor(&changed_candidate, &changed));
    }
}

int main(void) {
    test_identity_forms_compare_one_tuple();
    test_copy_over_64_kib();
    test_validation_over_64_kib();
    test_exact_identity_commit_reboot_and_idempotence();
    test_reset_record_preserves_zero_flags_without_activation();
    test_ordering_and_compatibility_reject_before_write();
    test_chunk_replay_gap_overlap_and_conflict();
    test_domain_mismatch_rejects_without_commit();
    test_durability_unknown_requires_boot_reconciliation();
    test_reuse_fills_ranges_from_the_active_profile();
    puts("profile peer store backend host tests passed");
    return 0;
}
