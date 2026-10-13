#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/compat/qmk_profile_eeprom.h"
#include "users/noah/lib/profile/schema/profile_compiled_defaults_v1.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "users/noah/lib/profile/storage/profile_store_runtime.h"

enum {
    TEST_COMPILED_DIGEST = UINT32_C(0x10203040),
};

static uint8_t  eeprom_bytes[NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE];
static uint8_t  alternate_bytes[NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE];
static uint32_t qmk_read_calls;
static uint32_t qmk_write_calls;
static uint32_t current_compiled_digest = TEST_COMPILED_DIGEST;

static const uint8_t empty_profile[] = {'N', 'L', 'P', '1', 3u, 0u, 0u, 1u};
static const uint8_t rgb_profile[]   = {'N', 'L', 'P', '1', 3u, 0u, 1u, 1u, 0x10u, NOAH_PROFILE_DOMAIN_VERSION_RGB, 1u, 0u, 0xA5u};

noah_profile_compiled_v1_result_t noah_profile_compiled_v1_open(noah_profile_compiled_v1_t *profile, noah_profile_compiled_v1_error_t *error) {
    (void)error;
    assert(profile != NULL);
    memset(profile, 0, sizeof(*profile));
    profile->metadata.digest            = current_compiled_digest;
    profile->metadata.action_abi_digest = 0u;
    return NOAH_PROFILE_COMPILED_V1_OK;
}

// The store runtime now reads compiled defaults too. Stubbed rather than
// linked, because the real module is a virtual view over authored keymap
// tables and this test is about storage.
noah_profile_reader_t noah_profile_compiled_v1_reader(const noah_profile_compiled_v1_t *profile) {
    (void)profile;
    return (noah_profile_reader_t){0};
}

void eeprom_read_block(void *target, const void *source, size_t length) {
    uintptr_t address = (uintptr_t)source;

    assert(target != NULL);
    assert(address >= NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR);
    assert(address + length <= sizeof(eeprom_bytes));
    qmk_read_calls++;
    memcpy(target, &eeprom_bytes[address], length);
}

void eeprom_write_block(const void *source, void *target, size_t length) {
    uintptr_t address = (uintptr_t)target;

    assert(source != NULL);
    assert(address >= NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR);
    assert(address + length <= sizeof(eeprom_bytes));
    qmk_write_calls++;
    memcpy(&eeprom_bytes[address], source, length);
}

static bool memory_read(void *context, noah_profile_storage_address_t address, uint8_t *target, uint16_t length) {
    uint8_t *bytes = context;

    if (!target || (uint32_t)address + length > NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE) {
        return false;
    }
    memcpy(target, &bytes[address], length);
    return true;
}

static bool memory_write(void *context, noah_profile_storage_address_t address, const uint8_t *source, uint16_t length) {
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
        .format_version          = NOAH_PROFILE_LOGICAL_STORE_VERSION,
        .via_generation          = 6u,
        .via_digest              = UINT32_C(0xabcdef01),
        .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
        .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
        .domain_mask             = payload[6] == 0u ? 0u : 1u,
        .flags                   = NOAH_PROFILE_STORE_FLAG_OVERRIDE,
        .payload_length          = length,
        .generation              = generation,
        .origin_half             = 0u,
        .payload_crc32           = noah_profile_crc32_finish(crc),
        .payload_digest          = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, payload, length),
        .compiled_default_digest = current_compiled_digest,
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

    noah_profile_store_init(&store, io,
                            (noah_profile_store_compatibility_t){
                                .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
                                .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
                                .compiled_default_digest = current_compiled_digest,
                                .action_abi_digest       = 0u,
                            });
    assert(noah_profile_store_boot_select(&store, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    assert(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
    assert(noah_profile_store_prepare_write(&store, 0u, payload, length) == NOAH_PROFILE_STORE_OK);
    assert(noah_profile_store_prepare_commit(&store, &selected) == NOAH_PROFILE_STORE_OK);
}

static void reset_eeprom(uint8_t *bytes) {
    memset(bytes, 0xFF, NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE);
}

// A profile holding only a settings domain whose keyboard name is `name`;
// the reader needs only the domain's shape, so every other byte is zero.
static uint16_t settings_profile(uint8_t *blob, const char *name) {
    uint8_t  name_length = (uint8_t)strlen(name);
    uint16_t length      = (uint16_t)(NOAH_SETTINGS_MIN_SIZE + name_length);
    uint8_t *payload     = blob + 12;

    memcpy(blob, (const uint8_t[]){'N', 'L', 'P', '1', 3u, 0u, 1u, 1u, NOAH_PROFILE_DOMAIN_V1_SETTINGS, NOAH_SETTINGS_VERSION, (uint8_t)length, (uint8_t)(length >> 8)}, 12);
    memset(payload, 0, length);
    memcpy(payload, (const uint8_t[]){NOAH_SETTINGS_VERSION, NOAH_SETTINGS_LAYERS, NOAH_SETTINGS_COUNT, NOAH_SETTINGS_MACRO_NAMES, NOAH_SETTINGS_CUSTOM_KEY_NAMES}, 5);
    payload[length - 1u - name_length] = name_length;
    memcpy(payload + length - name_length, name, name_length);
    return (uint16_t)(12u + length);
}

static void commit_settings(uint8_t *bytes, const char *name, uint32_t generation) {
    static uint8_t                 blob[12 + NOAH_SETTINGS_MAX_SIZE];
    uint16_t                       length = settings_profile(blob, name);
    noah_profile_store_t           store;
    noah_profile_store_record_t    selected;
    noah_profile_store_candidate_t candidate = candidate_for(blob, length, generation);
    noah_profile_store_io_t        io        = {.read = memory_read, .write = memory_write, .context = bytes};

    candidate.domain_mask = NOAH_PROFILE_DOMAIN_MASK_SETTINGS;
    noah_profile_store_init(&store, io,
                            (noah_profile_store_compatibility_t){
                                .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
                                .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
                                .compiled_default_digest = current_compiled_digest,
                                .action_abi_digest       = 0u,
                            });
    (void)noah_profile_store_boot_select(&store, &selected);
    assert(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
    for (uint16_t offset = 0u; offset < length; offset += NOAH_PROFILE_STORE_IO_CHUNK_MAX) {
        uint16_t piece = length - offset < NOAH_PROFILE_STORE_IO_CHUNK_MAX ? length - offset : NOAH_PROFILE_STORE_IO_CHUNK_MAX;
        assert(noah_profile_store_prepare_write(&store, offset, blob + offset, piece) == NOAH_PROFILE_STORE_OK);
    }
    assert(noah_profile_store_prepare_commit(&store, &selected) == NOAH_PROFILE_STORE_OK);
}

static bool read_name(uint8_t *bytes, noah_profile_keyboard_name_t *name) {
    noah_profile_store_t store;

    noah_profile_store_init(&store, (noah_profile_store_io_t){.read = memory_read, .context = bytes},
                            (noah_profile_store_compatibility_t){
                                .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
                                .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
                                .compiled_default_digest = TEST_COMPILED_DIGEST,
                                .action_abi_digest       = 0u,
                            });
    return noah_profile_store_runtime_read_keyboard_name(&store, name);
}

// D-F25: the committed keyboard name, read before USB starts.
static void test_keyboard_name_of_the_committed_profile(void) {
    noah_profile_keyboard_name_t name = {.length = 9u};

    reset_eeprom(alternate_bytes);
    assert(!read_name(alternate_bytes, &name) && name.length == 0u);
    commit_settings(alternate_bytes, "Desk \xc3\xa9", 3u);
    assert(read_name(alternate_bytes, &name) && name.length == 7u && !memcmp(name.bytes, "Desk \xc3\xa9", 7u));
    // The newer generation, in the other slot, wins.
    commit_settings(alternate_bytes, "Travel", 4u);
    assert(read_name(alternate_bytes, &name) && name.length == 6u && !memcmp(name.bytes, "Travel", 6u));
    // A profile without settings, or one for other compiled defaults, has none.
    reset_eeprom(alternate_bytes);
    commit_to(alternate_bytes, rgb_profile, sizeof(rgb_profile), 5u);
    assert(!read_name(alternate_bytes, &name) && name.length == 0u);
    reset_eeprom(alternate_bytes);
    current_compiled_digest = TEST_COMPILED_DIGEST ^ 1u;
    commit_settings(alternate_bytes, "Desk", 1u);
    current_compiled_digest = TEST_COMPILED_DIGEST;
    assert(!read_name(alternate_bytes, &name) && name.length == 0u);
    // A checksum failure has no name either.
    reset_eeprom(alternate_bytes);
    commit_settings(alternate_bytes, "Desk", 1u);
    alternate_bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE + 40u] ^= 1u;
    assert(!read_name(alternate_bytes, &name) && name.length == 0u);
    assert(!noah_profile_store_runtime_read_keyboard_name(NULL, &name));
}

// The boot read borrows the runtime's store only before the runtime starts,
// reads through the QMK adapter and writes nothing.
static void test_boot_keyboard_name_before_the_runtime_starts(void) {
    noah_profile_keyboard_name_t name;

    reset_eeprom(eeprom_bytes);
    assert(!noah_profile_store_runtime_boot_keyboard_name(&name) && name.length == 0u);
    commit_settings(eeprom_bytes, "Desk", 2u);
    qmk_read_calls  = 0u;
    qmk_write_calls = 0u;
    assert(noah_profile_store_runtime_boot_keyboard_name(&name) && name.length == 4u && !memcmp(name.bytes, "Desk", 4u));
    assert(qmk_read_calls > 0u && qmk_write_calls == 0u);
    assert(!noah_profile_store_runtime_boot_keyboard_name(NULL));
    noah_profile_store_runtime_init();
    assert(!noah_profile_store_runtime_boot_keyboard_name(&name) && name.length == 0u);
    // The borrowed store was left as the runtime expects to find it.
    noah_profile_store_runtime_matrix_scan();
    assert(noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_COMMITTED_FOUND);
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
    assert(!io.read(io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, &byte, NOAH_PROFILE_STORE_IO_CHUNK_MAX + 1u));
    assert(io.read(io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, &byte, 1u));
    assert(io.read(io.context, NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE - 1u, &byte, 1u));
    assert(!io.read(io.context, NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE - 1u, &byte, 2u));
    assert(qmk_read_calls == 2u);
}

static void test_qmk_write_adapter_is_slot_bounded(void) {
    noah_profile_store_io_t io                    = noah_qmk_profile_eeprom_io();
    const uint8_t           bytes[]               = {0x12u, 0x34u};
    uint8_t                 result[sizeof(bytes)] = {0u};

    reset_eeprom(eeprom_bytes);
    qmk_read_calls  = 0u;
    qmk_write_calls = 0u;
    assert(io.read != NULL);
    assert(io.write != NULL);
    assert(!io.write(io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR - 1u, bytes, 1u));
    assert(!io.write(io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, NULL, 1u));
    assert(!io.write(io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, bytes, 0u));
    assert(!io.write(io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, bytes, NOAH_PROFILE_STORE_IO_CHUNK_MAX + 1u));
    assert(io.write(io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, bytes, sizeof(bytes)));
    assert(io.read(io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, result, sizeof(result)));
    assert(memcmp(result, bytes, sizeof(bytes)) == 0);
    assert(io.write(io.context, NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE - sizeof(bytes), bytes, sizeof(bytes)));
    assert(!io.write(io.context, NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE - 1u, bytes, sizeof(bytes)));
    assert(qmk_write_calls == 2u);
    assert(qmk_read_calls == 1u);
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

static void test_old_compiled_default_record_falls_back(void) {
    reset_eeprom(eeprom_bytes);
    current_compiled_digest = TEST_COMPILED_DIGEST ^ 1u;
    commit_to(eeprom_bytes, empty_profile, sizeof(empty_profile), 1u);
    current_compiled_digest = TEST_COMPILED_DIGEST;
    qmk_read_calls          = 0u;
    qmk_write_calls         = 0u;

    noah_profile_store_runtime_init();
    noah_profile_store_runtime_matrix_scan();
    assert(noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_COMPILED_FALLBACK);
    assert(noah_profile_store_runtime_discovery_result() == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    assert(noah_profile_store_runtime_committed() == NULL);
    assert(qmk_write_calls == 0u);
}

int main(void) {
    // Boot order: before anything initializes the runtime.
    test_boot_keyboard_name_before_the_runtime_starts();
    test_keyboard_name_of_the_committed_profile();
    test_qmk_adapter_is_slot_bounded_and_read_only();
    test_qmk_write_adapter_is_slot_bounded();
    test_boot_discovery_runs_once_from_scan();
    test_committed_metadata_is_discovered_without_activation();
    test_equal_generation_divergence_is_reported();
    test_old_compiled_default_record_falls_back();
    puts("profile store runtime host tests passed");
    return 0;
}
