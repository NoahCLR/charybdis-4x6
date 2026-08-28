#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "users/noah/lib/profile/storage/profile_peer_store_backend.h"

enum {
    ACTION_ABI_DIGEST = UINT32_C(0x11223344),
};

typedef struct {
    uint8_t  bytes[NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE];
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

static const uint8_t empty_profile[] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};

static uint32_t always_safe(void *context) {
    (void)context;
    return 0u;
}

static bool memory_read(void *context, uint16_t address, uint8_t *target, uint16_t length) {
    memory_t *memory = context;

    memory->reads++;
    if (memory->fail_reads || !target || length == 0u || (uint32_t)address + length > sizeof(memory->bytes)) {
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

static void fixture_init(fixture_t *fixture) {
    noah_profile_reader_t                     reader = noah_profile_reader_from_memory(empty_profile, sizeof(empty_profile));
    noah_profile_validator_v1_profile_t       profile;
    noah_effective_profile_snapshot_t         compiled;
    noah_profile_validator_v1_compatibility_t compatibility = noah_profile_validator_v1_default_compatibility(ACTION_ABI_DIGEST);
    noah_profile_store_record_t               selected;

    memset(fixture, 0, sizeof(*fixture));
    memset(fixture->memory.bytes, 0xff, sizeof(fixture->memory.bytes));
    memset(&profile, 0, sizeof(profile));
    profile.byte_length       = sizeof(empty_profile);
    profile.crc32             = payload_crc(empty_profile, sizeof(empty_profile));
    profile.digest            = payload_digest(empty_profile, sizeof(empty_profile));
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

int main(void) {
    test_exact_identity_commit_reboot_and_idempotence();
    test_reset_record_preserves_zero_flags_without_activation();
    test_ordering_and_compatibility_reject_before_write();
    test_chunk_replay_gap_overlap_and_conflict();
    test_domain_mismatch_rejects_without_commit();
    test_durability_unknown_requires_boot_reconciliation();
    puts("profile peer store backend host tests passed");
    return 0;
}
