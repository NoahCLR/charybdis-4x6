#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/profile/storage/profile_candidate_store_backend.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"

enum {
    ACTION_ABI_DIGEST = 0x11223344u,
};

static uint8_t eeprom_bytes[NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE];
static const uint8_t empty_profile[] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};
static const uint8_t behavior_profile[] =
    "\x4e\x4c\x50\x31\x01\x00\x01\x01\x20\x01\x3a\x00\x02\x03\x00\x00"
    "\x20\x00\x01\x00\x34\x12\x96\x00\x90\x01\xaf\x00\x01\x02\x00\x02"
    "\x03\x19\x01\x00\x28\x00\x02\x05\x06\x00\x0a\x00\x02\x00\x03\x00"
    "\x03\x00\x12\x00\x04\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x01"
    "\x01\x01\x07\x00\x02\x00";

static uint32_t always_safe(void *context) {
    (void)context;
    return 0u;
}

static bool memory_read(void *context, uint16_t address, uint8_t *target, uint16_t length) {
    uint8_t *bytes = context;

    if (!target || length == 0u || (uint32_t)address + length > NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE) {
        return false;
    }
    memcpy(target, &bytes[address], length);
    return true;
}

static bool memory_write(void *context, uint16_t address, const uint8_t *source, uint16_t length) {
    uint8_t *bytes = context;

    if (!source || length == 0u || (uint32_t)address + length > NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE) {
        return false;
    }
    memcpy(&bytes[address], source, length);
    return true;
}

static noah_profile_candidate_v1_metadata_t metadata_for(const uint8_t *payload, uint16_t length) {
    uint32_t crc = noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, payload, length);

    return (noah_profile_candidate_v1_metadata_t){
        .schema_major      = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR,
        .schema_minor      = NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR,
        .requested_domains = 0u,
        .flags             = 0u,
        .payload_length    = length,
        .crc32             = noah_profile_crc32_finish(crc),
        .digest            = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, payload, length),
        .action_abi_digest = ACTION_ABI_DIGEST,
    };
}

static void init_store(noah_profile_store_t *store) {
    noah_profile_store_record_t selected;

    noah_profile_store_init(store, (noah_profile_store_io_t){
                                         .read    = memory_read,
                                         .write   = memory_write,
                                         .context = eeprom_bytes,
                                     },
                            (noah_profile_store_compatibility_t){
                                .schema_major      = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
                                .schema_minor      = NOAH_PROFILE_STORE_SCHEMA_MINOR,
                                .action_abi_digest = ACTION_ABI_DIGEST,
                            });
    assert(noah_profile_store_boot_select(store, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
}

static uint32_t init_provider(noah_effective_profile_provider_t *provider) {
    noah_profile_reader_t               reader = noah_profile_reader_from_memory(empty_profile, sizeof(empty_profile));
    noah_profile_validator_v1_profile_t profile;
    noah_effective_profile_snapshot_t   compiled;
    uint32_t                            crc = noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, empty_profile, sizeof(empty_profile));

    memset(&profile, 0, sizeof(profile));
    profile.byte_length       = sizeof(empty_profile);
    profile.crc32             = noah_profile_crc32_finish(crc);
    profile.digest            = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, empty_profile, sizeof(empty_profile));
    profile.action_abi_digest = ACTION_ABI_DIGEST;
    assert(noah_effective_profile_snapshot_make_compiled(&profile, &reader, 0u, &compiled) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_init(provider, &compiled, always_safe, NULL, NULL, 0u) == NOAH_EFFECTIVE_PROFILE_OK);
    return compiled.identity.payload_digest;
}

static void init_backend(noah_profile_candidate_store_backend_t *backend, noah_profile_store_t *store, noah_effective_profile_provider_t *provider, uint32_t compiled_default_digest) {
    noah_profile_validator_v1_compatibility_t compatibility = noah_profile_validator_v1_default_compatibility(ACTION_ABI_DIGEST);

    compatibility.required_domain_mask = 0u;
    compatibility.allowed_domain_mask  = 0u;
    noah_profile_candidate_store_backend_init(backend, store, provider, &compatibility, compiled_default_digest, 1u);
    assert(backend->reuse_guard_installed);
}

static noah_profile_candidate_backend_result_t validate_to_completion(noah_profile_candidate_backend_t *interface, const noah_profile_candidate_v1_metadata_t *metadata, noah_profile_candidate_v1_error_t *error) {
    noah_profile_candidate_backend_result_t result = interface->validation_begin(interface->context, metadata, error);
    uint16_t                                steps  = 0u;

    while (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        assert(steps++ < 64u);
        result = interface->validation_step(interface->context, NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET, error);
    }
    return result;
}

static void deploy_empty(noah_profile_candidate_store_backend_t *backend, noah_profile_candidate_backend_t *interface, noah_effective_profile_provider_t *provider, const noah_profile_candidate_v1_metadata_t *metadata) {
    noah_profile_candidate_v1_error_t error = noah_profile_candidate_v1_no_error();

    assert(interface->begin(interface->context, metadata) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(interface->write(interface->context, 0u, empty_profile, sizeof(empty_profile)) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(validate_to_completion(interface, metadata, &error) == NOAH_PROFILE_CANDIDATE_BACKEND_VALID);
    assert(noah_profile_candidate_store_backend_commit(backend, NULL) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(noah_profile_candidate_store_backend_request_activation(backend) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(noah_effective_profile_provider_poll(provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
}

static void test_stage_validate_and_commit(void) {
    noah_profile_store_t                   store;
    noah_profile_candidate_store_backend_t backend;
    noah_effective_profile_provider_t      provider;
    noah_profile_candidate_backend_t       interface;
    noah_profile_candidate_v1_metadata_t   metadata = metadata_for(empty_profile, sizeof(empty_profile));
    noah_profile_candidate_v1_error_t      error    = noah_profile_candidate_v1_no_error();
    noah_profile_store_record_t            committed;
    noah_profile_store_t                   rebooted;
    noah_profile_store_record_t            selected;
    uint8_t                                staged[sizeof(empty_profile)];
    uint32_t                               compiled_default_digest;
    noah_effective_profile_status_t        provider_status;

    memset(eeprom_bytes, 0xff, sizeof(eeprom_bytes));
    init_store(&store);
    compiled_default_digest = init_provider(&provider);
    init_backend(&backend, &store, &provider, compiled_default_digest);
    interface = noah_profile_candidate_store_backend_interface(&backend);

    assert(interface.begin(interface.context, &metadata) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(interface.write(interface.context, 0u, empty_profile, 3u) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(interface.write(interface.context, 3u, &empty_profile[3], sizeof(empty_profile) - 3u) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(interface.read(interface.context, 0u, staged, sizeof(staged)) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(memcmp(staged, empty_profile, sizeof(staged)) == 0);
    assert(validate_to_completion(&interface, &metadata, &error) == NOAH_PROFILE_CANDIDATE_BACKEND_VALID);
    assert(noah_profile_candidate_store_backend_validated_profile(&backend) != NULL);
    assert(noah_profile_candidate_store_backend_validated_profile(&backend)->domain_mask == 0u);
    assert(noah_profile_candidate_store_backend_commit(&backend, &committed) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(committed.generation == 1u);
    assert(committed.origin_half == 1u);
    assert(committed.payload_digest == metadata.digest);
    assert(committed.compiled_default_digest == compiled_default_digest);
    assert(noah_profile_candidate_store_backend_validated_profile(&backend) != NULL);
    backend.committed_record.payload_crc32 ^= 1u;
    assert(noah_profile_candidate_store_backend_request_activation(&backend) == NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED);
    backend.committed_record.payload_crc32 ^= 1u;
    assert(noah_profile_candidate_store_backend_request_activation(&backend) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
    assert(noah_effective_profile_provider_status(&provider, &provider_status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(provider_status.active.generation == 1u && provider_status.active.payload_digest == metadata.digest);

    noah_profile_store_init(&rebooted, store.io, store.compatibility);
    assert(noah_profile_store_boot_select(&rebooted, &selected) == NOAH_PROFILE_STORE_OK);
    assert(selected.generation == committed.generation);
    assert(selected.payload_digest == committed.payload_digest);
}

static void test_checksum_rejection_and_abort_preserve_last_known_good(void) {
    noah_profile_store_t                   store;
    noah_profile_candidate_store_backend_t backend;
    noah_effective_profile_provider_t      provider;
    noah_profile_candidate_backend_t       interface;
    noah_profile_candidate_v1_metadata_t   metadata = metadata_for(empty_profile, sizeof(empty_profile));
    noah_profile_candidate_v1_error_t      error    = noah_profile_candidate_v1_no_error();
    noah_profile_store_record_t            selected;

    memset(eeprom_bytes, 0xff, sizeof(eeprom_bytes));
    init_store(&store);
    init_backend(&backend, &store, &provider, init_provider(&provider));
    interface = noah_profile_candidate_store_backend_interface(&backend);

    metadata.crc32 ^= 1u;
    assert(interface.begin(interface.context, &metadata) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(interface.write(interface.context, 0u, empty_profile, sizeof(empty_profile)) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(validate_to_completion(&interface, &metadata, &error) == NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED);
    assert(error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_CHECKSUM_MISMATCH);
    assert(noah_profile_candidate_store_backend_commit(&backend, &selected) == NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED);
    assert(interface.abort(interface.context) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(noah_profile_candidate_store_backend_validated_profile(&backend) == NULL);

    noah_profile_store_init(&store, store.io, store.compatibility);
    assert(noah_profile_store_boot_select(&store, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
}

static void test_committed_view_keeps_its_slot_when_next_candidate_starts(void) {
    noah_profile_store_t                     store;
    noah_profile_candidate_store_backend_t   backend;
    noah_effective_profile_provider_t        provider;
    noah_profile_candidate_backend_t         interface;
    noah_profile_candidate_v1_metadata_t     first_metadata = metadata_for(behavior_profile, sizeof(behavior_profile) - 1u);
    noah_profile_candidate_v1_metadata_t     next_metadata  = metadata_for(empty_profile, sizeof(empty_profile));
    noah_profile_candidate_v1_error_t        error          = noah_profile_candidate_v1_no_error();
    noah_profile_validator_v1_profile_t      retained;
    noah_key_behavior_row_v1_view_t          row;
    noah_profile_codec_v1_error_t            codec_error;

    memset(eeprom_bytes, 0xff, sizeof(eeprom_bytes));
    init_store(&store);
    init_backend(&backend, &store, &provider, init_provider(&provider));
    backend.compatibility.allowed_domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS;
    interface = noah_profile_candidate_store_backend_interface(&backend);
    first_metadata.requested_domains = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS;

    assert(interface.begin(interface.context, &first_metadata) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    for (uint16_t offset = 0u; offset < first_metadata.payload_length;) {
        uint8_t length = (uint8_t)(first_metadata.payload_length - offset);
        if (length > NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX) length = NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX;
        assert(interface.write(interface.context, offset, &behavior_profile[offset], length) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
        offset = (uint16_t)(offset + length);
    }
    assert(validate_to_completion(&interface, &first_metadata, &error) == NOAH_PROFILE_CANDIDATE_BACKEND_VALID);
    retained = *noah_profile_candidate_store_backend_validated_profile(&backend);
    assert(noah_profile_candidate_store_backend_commit(&backend, NULL) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(noah_profile_candidate_store_backend_request_activation(&backend) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
    assert(noah_key_behavior_domain_v1_row_at(&retained.key_behaviors, 0u, &row, &codec_error) == NOAH_PROFILE_CODEC_V1_OK);
    assert(row.target.kind == NOAH_PROFILE_ACTION_V1_QMK_KEYCODE && row.target.operand == 0x1234u);

    assert(interface.begin(interface.context, &next_metadata) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(provider.backing_reuse_in_progress);
    assert(interface.write(interface.context, 0u, empty_profile, sizeof(empty_profile)) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(noah_key_behavior_domain_v1_row_at(&retained.key_behaviors, 0u, &row, &codec_error) == NOAH_PROFILE_CODEC_V1_OK);
    assert(row.target.kind == NOAH_PROFILE_ACTION_V1_QMK_KEYCODE && row.target.operand == 0x1234u);
    assert(interface.abort(interface.context) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(!provider.backing_reuse_in_progress);
}

static void test_runtime_rollback_pins_store_target_until_active_backing_moves(void) {
    noah_profile_store_t                   store;
    noah_profile_candidate_store_backend_t backend;
    noah_profile_candidate_backend_t       interface;
    noah_effective_profile_provider_t      provider;
    noah_profile_candidate_v1_metadata_t   metadata = metadata_for(empty_profile, sizeof(empty_profile));
    noah_effective_profile_status_t        status;
    uint8_t                                marker_before[NOAH_PROFILE_STORAGE_COMMIT_MARKER_SIZE];

    memset(eeprom_bytes, 0xff, sizeof(eeprom_bytes));
    init_store(&store);
    init_backend(&backend, &store, &provider, init_provider(&provider));
    interface = noah_profile_candidate_store_backend_interface(&backend);

    deploy_empty(&backend, &interface, &provider, &metadata);
    assert(store.committed.slot == NOAH_PROFILE_SLOT_A && store.committed.generation == 1u);
    deploy_empty(&backend, &interface, &provider, &metadata);
    assert(store.committed.slot == NOAH_PROFILE_SLOT_B && store.committed.generation == 2u);

    assert(noah_effective_profile_provider_request_rollback(&provider) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
    assert(noah_effective_profile_provider_status(&provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(status.active.generation == 1u);

    memcpy(marker_before, &eeprom_bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE - NOAH_PROFILE_STORAGE_COMMIT_MARKER_SIZE], sizeof(marker_before));
    assert(interface.begin(interface.context, &metadata) == NOAH_PROFILE_CANDIDATE_BACKEND_IO_ERROR);
    assert(memcmp(marker_before, &eeprom_bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE - NOAH_PROFILE_STORAGE_COMMIT_MARKER_SIZE], sizeof(marker_before)) == 0);
    assert(!store.prepare_active && !store.reuse_active);
    assert(noah_effective_profile_provider_status(&provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(status.active.generation == 1u && !status.backing_reuse_in_progress);

    // The provider's rollback bank is the still-durable generation 2 in slot
    // B. Move active authority back there, then slot A can be discarded and
    // reserved for generation 3 without touching active bytes.
    assert(noah_effective_profile_provider_request_rollback(&provider) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
    assert(interface.begin(interface.context, &metadata) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
    assert(!provider.rollback_available && provider.backing_reuse_in_progress);
    assert(interface.abort(interface.context) == NOAH_PROFILE_CANDIDATE_BACKEND_OK);
}

int main(void) {
    test_stage_validate_and_commit();
    test_checksum_rejection_and_abort_preserve_last_known_good();
    test_committed_view_keeps_its_slot_when_next_candidate_starts();
    test_runtime_rollback_pins_store_target_until_active_backing_moves();
    puts("profile candidate store backend tests passed");
    return 0;
}
