#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/compat/qmk_profile_eeprom.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "users/noah/lib/profile/storage/profile_store_runtime.h"

static uint8_t  eeprom_bytes[NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE];
static uint8_t  alternate_bytes[NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE];
static uint32_t qmk_read_calls;
static uint32_t qmk_write_calls;

static const uint8_t empty_profile[] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};
static const uint8_t rgb_profile[]   = {'N', 'L', 'P', '1', 1u, 0u, 1u, 1u, 0x10u, 1u, 1u, 0u, 0xA5u};

void eeprom_read_block(void *target, const void *source, size_t length) {
    uintptr_t address = (uintptr_t)source;

    assert(target != NULL);
    assert(address >= NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR);
    assert(address + length <= sizeof(eeprom_bytes));
    qmk_read_calls++;
    memcpy(target, &eeprom_bytes[address], length);
}

static bool memory_read(void *context, uint16_t address, uint8_t *target, uint16_t length) {
    uint8_t *bytes = context;

    if (!target || (uint32_t)address + length > NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE) {
        return false;
    }
    memcpy(target, &bytes[address], length);
    return true;
}

static bool memory_write(void *context, uint16_t address, const uint8_t *source, uint16_t length) {
    uint8_t *bytes = context;

    if (!source || (uint32_t)address + length > NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE) {
        return false;
    }
    qmk_write_calls++;
    memcpy(&bytes[address], source, length);
    return true;
}

static noah_profile_store_candidate_t candidate_for(const uint8_t *payload, uint16_t length, uint32_t generation) {
    uint32_t crc = noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, payload, length);

    return (noah_profile_store_candidate_t){
        .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
        .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
        .flags                   = NOAH_PROFILE_STORE_FLAG_OVERRIDE,
        .payload_length          = length,
        .generation              = generation,
        .origin_half             = 0u,
        .payload_crc32           = noah_profile_crc32_finish(crc),
        .payload_digest          = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, payload, length),
        .compiled_default_digest = UINT32_C(0x10203040),
        .action_abi_digest       = 0u,
    };
}

static void commit_to(uint8_t *bytes, const uint8_t *payload, uint16_t length, uint32_t generation) {
    noah_profile_store_t           store;
    noah_profile_store_record_t    selected;
    noah_profile_store_candidate_t candidate = candidate_for(payload, length, generation);
    noah_profile_store_io_t        io        = {
        .read    = memory_read,
        .write   = memory_write,
        .context = bytes,
    };

    noah_profile_store_init(&store, io, (noah_profile_store_compatibility_t){
                                           .schema_major      = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
                                           .schema_minor      = NOAH_PROFILE_STORE_SCHEMA_MINOR,
                                           .action_abi_digest = 0u,
                                       });
    assert(noah_profile_store_boot_select(&store, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    assert(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
    assert(noah_profile_store_prepare_write(&store, 0u, payload, length) == NOAH_PROFILE_STORE_OK);
    assert(noah_profile_store_prepare_commit(&store, &selected) == NOAH_PROFILE_STORE_OK);
}

static void reset_eeprom(uint8_t *bytes) {
    memset(bytes, 0xFF, NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE);
}

static void test_qmk_adapter_is_slot_bounded_and_read_only(void) {
    noah_profile_store_io_t io = noah_qmk_profile_eeprom_read_only_io();
    uint8_t                 byte;

    reset_eeprom(eeprom_bytes);
    qmk_read_calls = 0u;
    assert(io.read != NULL);
    assert(io.write == NULL);
    assert(!io.read(io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR - 1u, &byte, 1u));
    assert(!io.read(io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, NULL, 1u));
    assert(!io.read(io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, &byte, 0u));
    assert(io.read(io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, &byte, 1u));
    assert(io.read(io.context, NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE - 1u, &byte, 1u));
    assert(!io.read(io.context, NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE - 1u, &byte, 2u));
    assert(qmk_read_calls == 2u);
}

static void test_boot_discovery_runs_once_from_scan(void) {
    uint32_t reads_after_discovery;

    reset_eeprom(eeprom_bytes);
    qmk_read_calls  = 0u;
    qmk_write_calls = 0u;
    noah_profile_store_runtime_init();
    assert(noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING);
    assert(qmk_read_calls == 0u);
    noah_profile_store_runtime_matrix_scan();
    assert(noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_COMPILED_FALLBACK);
    assert(noah_profile_store_runtime_discovery_result() == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    assert(noah_profile_store_runtime_committed() == NULL);
    assert(qmk_write_calls == 0u);
    reads_after_discovery = qmk_read_calls;
    noah_profile_store_runtime_matrix_scan();
    assert(qmk_read_calls == reads_after_discovery);
}

static void test_committed_metadata_is_discovered_without_activation(void) {
    const noah_profile_store_record_t *record;
    noah_profile_store_candidate_t     candidate;

    reset_eeprom(eeprom_bytes);
    qmk_write_calls = 0u;
    commit_to(eeprom_bytes, empty_profile, sizeof(empty_profile), 7u);
    candidate       = candidate_for(empty_profile, sizeof(empty_profile), 7u);
    qmk_read_calls  = 0u;
    qmk_write_calls = 0u;

    noah_profile_store_runtime_init();
    noah_profile_store_runtime_matrix_scan();
    assert(noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_COMMITTED_FOUND);
    assert(noah_profile_store_runtime_discovery_result() == NOAH_PROFILE_STORE_OK);
    record = noah_profile_store_runtime_committed();
    assert(record != NULL);
    assert(record->slot == NOAH_PROFILE_SLOT_A);
    assert(record->generation == 7u);
    assert(record->payload_digest == candidate.payload_digest);
    assert(qmk_read_calls > 0u);
    assert(qmk_write_calls == 0u);
}

static void test_equal_generation_divergence_is_reported(void) {
    reset_eeprom(eeprom_bytes);
    reset_eeprom(alternate_bytes);
    commit_to(eeprom_bytes, empty_profile, sizeof(empty_profile), 9u);
    commit_to(alternate_bytes, rgb_profile, sizeof(rgb_profile), 9u);
    memcpy(&eeprom_bytes[NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR], &alternate_bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR], NOAH_PROFILE_STORAGE_SLOT_A_SIZE);
    qmk_read_calls  = 0u;
    qmk_write_calls = 0u;

    noah_profile_store_runtime_init();
    noah_profile_store_runtime_matrix_scan();
    assert(noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_GENERATION_CONFLICT);
    assert(noah_profile_store_runtime_discovery_result() == NOAH_PROFILE_STORE_GENERATION_CONFLICT);
    assert(noah_profile_store_runtime_committed() == NULL);
    assert(qmk_write_calls == 0u);
}

int main(void) {
    test_qmk_adapter_is_slot_bounded_and_read_only();
    test_boot_discovery_runs_once_from_scan();
    test_committed_metadata_is_discovered_without_activation();
    test_equal_generation_divergence_is_reported();
    puts("profile store runtime host tests passed");
    return 0;
}
