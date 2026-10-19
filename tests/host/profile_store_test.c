#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "users/noah/lib/profile/storage/profile_store.h"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            exit(1);                                                                        \
        }                                                                                   \
    } while (0)

enum {
    ACTION_ABI_DIGEST = UINT32_C(0x23456789),
    COMPILED_DIGEST   = UINT32_C(0x10203040),
};

typedef struct {
    uint8_t  bytes[NOAH_PROFILE_STORAGE_REGION_SIZE];
    uint32_t read_calls;
    uint32_t write_calls;
    uint32_t fail_write_call;
    uint16_t fail_partial_bytes;
    bool     fail_reads;
    uint32_t last_write_address;
    uint32_t last_write_length;
    uint32_t last_read_length;
    uint32_t largest_read_length;
    uint32_t read_bytes;
} fake_eeprom_t;

static fake_eeprom_t eeprom;
static fake_eeprom_t alternate_eeprom;
static uint8_t       eeprom_snapshot[NOAH_PROFILE_STORAGE_REGION_SIZE];

typedef struct {
    fake_eeprom_t      *memory;
    noah_profile_slot_t slot;
    uint32_t            begin_calls;
    uint32_t            end_calls;
    bool                deny_begin;
    bool                fail_end;
    bool                saw_pristine_marker;
} reuse_guard_state_t;

static const uint8_t empty_profile[]    = {'N', 'L', 'P', '1', 4u, 0u, 0u, 1u};
static const uint8_t rgb_profile[]      = {'N', 'L', 'P', '1', 4u, 0u, 1u, 1u, 0x10u, NOAH_PROFILE_DOMAIN_VERSION_RGB, 3u, 0u, 0u, 0u, 1u, 2u, 3u};
static const uint8_t behavior_profile[] = {'N', 'L', 'P', '1', 4u, 0u, 1u, 1u, 0x20u, NOAH_PROFILE_DOMAIN_VERSION_KEY_BEHAVIORS, 2u, 0u, 0u, 0u, 4u, 5u};

static bool fake_read(void *context, noah_profile_storage_address_t address, uint8_t *target, uint32_t length) {
    fake_eeprom_t *memory = context;

    memory->read_calls++;
    memory->last_read_length = length;
    memory->read_bytes += length;
    if (length > memory->largest_read_length) memory->largest_read_length = length;
    if (memory->fail_reads || length > NOAH_PROFILE_STORE_IO_CHUNK_MAX || (uint32_t)address + length > sizeof(memory->bytes)) {
        return false;
    }
    memcpy(target, &memory->bytes[address], length);
    return true;
}

static uint16_t partial_limit_for_commit_write(uint32_t write_call, uint16_t payload_length) {
    uint32_t payload_calls = (payload_length + 4u) / 5u;

    if (write_call == 1u || write_call == payload_calls + 4u) {
        return 2u;
    }
    if (write_call <= payload_calls + 1u) {
        uint32_t payload_call = write_call - 2u;
        uint16_t remaining    = (uint16_t)(payload_length - payload_call * 5u);
        return remaining < 5u ? remaining : 5u;
    }
    return write_call == payload_calls + 2u ? 20u : NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE - 1u - 20u;
}

static bool fake_write(void *context, noah_profile_storage_address_t address, const uint8_t *source, uint32_t length) {
    fake_eeprom_t *memory  = context;
    uint16_t       written = length;

    memory->write_calls++;
    memory->last_write_address = address;
    memory->last_write_length  = length;
    if ((uint32_t)address + length > sizeof(memory->bytes)) {
        return false;
    }
    if (memory->fail_write_call == memory->write_calls) {
        written = memory->fail_partial_bytes < length ? memory->fail_partial_bytes : length;
        memcpy(&memory->bytes[address], source, written);
        return false;
    }
    memcpy(&memory->bytes[address], source, length);
    return true;
}

static noah_profile_store_io_t io_for(fake_eeprom_t *memory) {
    return (noah_profile_store_io_t){.read = fake_read, .write = fake_write, .context = memory};
}

static noah_profile_store_compatibility_t compatibility(void) {
    return (noah_profile_store_compatibility_t){.schema_major = NOAH_PROFILE_SCHEMA_MAJOR, .schema_minor = 0u, .compiled_default_digest = COMPILED_DIGEST, .action_abi_digest = ACTION_ABI_DIGEST};
}

static bool reuse_begin(void *context, noah_profile_slot_t slot) {
    reuse_guard_state_t *state = context;
    noah_profile_storage_address_t start = slot == NOAH_PROFILE_SLOT_A ? NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR : NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR;

    state->begin_calls++;
    state->slot                = slot;
    state->saw_pristine_marker = state->memory->bytes[start + 34u] == 0xffu && state->memory->bytes[start + 35u] == 0xffu;
    return !state->deny_begin;
}

static bool reuse_end(void *context, noah_profile_slot_t slot) {
    reuse_guard_state_t *state = context;

    state->end_calls++;
    CHECK(slot == state->slot);
    return !state->fail_end;
}

static uint16_t load_u16(const uint8_t *bytes) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u);
}

static uint32_t load_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) | ((uint32_t)bytes[3] << 24u);
}

// Format 6 keys its header CRC16 with the format version (D-F28).
static uint16_t header_crc16(const uint8_t *header) {
    static const uint8_t format = NOAH_PROFILE_STORE_FORMAT_VERSION;
    return noah_profile_crc16_ccitt_update(noah_profile_crc16_ccitt_update(NOAH_PROFILE_CRC16_INITIAL, &format, 1u), header, 33u);
}

static void store_u32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8u);
    bytes[2] = (uint8_t)(value >> 16u);
    bytes[3] = (uint8_t)(value >> 24u);
}

static void store_u16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8u);
}

static void reset_eeprom(fake_eeprom_t *memory) {
    memset(memory, 0xFF, sizeof(*memory));
    memory->write_calls        = 0u;
    memory->fail_write_call    = 0u;
    memory->fail_partial_bytes = 0u;
    memory->fail_reads         = false;
}

static noah_profile_store_candidate_t candidate_for(const uint8_t *payload, uint32_t length, uint32_t generation, uint8_t origin_half) {
    uint32_t crc         = noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, payload, length);
    uint8_t  domain_mask = 0u;

    if (length >= 12u && payload[6] != 0u) {
        domain_mask = payload[8] == 0x10u ? 1u : payload[8] == 0x20u ? 2u : 0u;
    }

    return (noah_profile_store_candidate_t){
        .format_version          = NOAH_PROFILE_STORE_FORMAT_VERSION,
        .schema_major            = NOAH_PROFILE_SCHEMA_MAJOR,
        .schema_minor            = 0u,
        .domain_mask             = domain_mask,
        .flags                   = NOAH_PROFILE_STORE_FLAG_OVERRIDE,
        .payload_length          = length,
        .generation              = generation,
        .origin_half             = origin_half,
        .payload_crc32           = noah_profile_crc32_finish(crc),
        .payload_digest          = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, payload, length),
        .compiled_default_digest = COMPILED_DIGEST,
        .action_abi_digest       = ACTION_ABI_DIGEST,
    };
}

static void initialize_store(noah_profile_store_t *store, fake_eeprom_t *memory, noah_profile_store_result_t expected_boot) {
    noah_profile_store_record_t selected;

    noah_profile_store_init(store, io_for(memory), compatibility());
    CHECK(noah_profile_store_boot_select(store, &selected) == expected_boot);
}

static noah_profile_store_result_t commit_payload(noah_profile_store_t *store, const uint8_t *payload, uint32_t length, uint32_t generation, uint8_t origin_half, noah_profile_store_record_t *record) {
    noah_profile_store_candidate_t candidate = candidate_for(payload, length, generation, origin_half);
    uint32_t                       offset    = 0u;
    noah_profile_store_result_t    result;

    result = noah_profile_store_prepare_begin(store, &candidate);
    if (result != NOAH_PROFILE_STORE_OK) {
        return result;
    }
    while (offset < length) {
        uint32_t chunk = length - offset;

        if (chunk > 5u) {
            chunk = 5u;
        }
        result = noah_profile_store_prepare_write(store, offset, &payload[offset], chunk);
        if (result != NOAH_PROFILE_STORE_OK) {
            return result;
        }
        offset += chunk;
    }
    return noah_profile_store_prepare_commit(store, record);
}

static void stage_commit_to_phase(noah_profile_store_t *store, const uint8_t *payload, uint16_t length, noah_profile_store_commit_phase_t phase) {
    noah_profile_store_candidate_t candidate = candidate_for(payload, length, 1u, 0u);
    noah_profile_store_result_t    result;
    uint16_t                       steps = 0u;

    CHECK(noah_profile_store_prepare_begin(store, &candidate) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_write(store, 0u, payload, length) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_commit_begin(store) == NOAH_PROFILE_STORE_IN_PROGRESS);
    while (store->commit_phase != phase) {
        result = noah_profile_store_prepare_commit_step(store, 20u, NULL);
        CHECK(result == NOAH_PROFILE_STORE_IN_PROGRESS);
        CHECK(++steps < 64u);
    }
}

static void test_checksums(void) {
    static const uint8_t canonical[] = "123456789";
    uint32_t             crc         = noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, canonical, sizeof(canonical) - 1u);
    uint32_t             fnv         = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, canonical, sizeof(canonical) - 1u);
    uint16_t             crc16       = noah_profile_crc16_ccitt_update(NOAH_PROFILE_CRC16_INITIAL, canonical, sizeof(canonical) - 1u);

    CHECK(noah_profile_crc32_finish(crc) == UINT32_C(0xCBF43926));
    CHECK(fnv == UINT32_C(0xBB86B11C));
    CHECK(crc16 == UINT16_C(0x29B1));
}

// The CRC-32 takes half bytes from a table; it must give the bit-at-a-time
// result for any bytes, however they are split across updates.
static uint32_t bitwise_crc32(uint32_t crc, const uint8_t *bytes, size_t length) {
    for (size_t index = 0u; index < length; index++) {
        crc ^= bytes[index];
        for (uint8_t bit = 0u; bit < 8u; bit++) {
            crc = (crc >> 1u) ^ (UINT32_C(0xEDB88320) & (uint32_t)-(int32_t)(crc & 1u));
        }
    }
    return crc;
}

static void test_crc32_matches_the_bitwise_crc(void) {
    uint8_t  bytes[1031];
    uint32_t seed = UINT32_C(0x12345678);

    for (size_t index = 0u; index < sizeof(bytes); index++) {
        seed         = seed * UINT32_C(1103515245) + 12345u;
        bytes[index] = (uint8_t)(seed >> 16u);
    }
    for (size_t split = 0u; split <= sizeof(bytes); split += 97u) {
        uint32_t crc = noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, split);

        crc = noah_profile_crc32_update(crc, &bytes[split], sizeof(bytes) - split);
        CHECK(crc == bitwise_crc32(NOAH_PROFILE_CRC32_INITIAL, bytes, sizeof(bytes)));
    }
    for (unsigned value = 0u; value < 256u; value++) {
        uint8_t byte = (uint8_t)value;

        CHECK(noah_profile_crc32_update(0u, &byte, 1u) == bitwise_crc32(0u, &byte, 1u));
    }
}

static void test_commit_and_boot_selection(void) {
    noah_profile_store_t           store;
    noah_profile_store_t           rebooted;
    noah_profile_store_record_t    record;
    noah_profile_store_candidate_t first_candidate;
    const uint8_t                 *header;
    uint32_t                       next;

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(noah_profile_store_next_generation(&store, &next));
    CHECK(next == 1u);
    CHECK(commit_payload(&store, empty_profile, sizeof(empty_profile), 1u, 0u, &record) == NOAH_PROFILE_STORE_OK);
    CHECK(record.slot == NOAH_PROFILE_SLOT_A && record.generation == 1u);
    CHECK(record.domain_mask == 0u);
    CHECK(eeprom.last_write_address == NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + 35u);
    CHECK(eeprom.last_write_length == NOAH_PROFILE_STORAGE_COMMIT_MARKER_SIZE);
    first_candidate = candidate_for(empty_profile, sizeof(empty_profile), 1u, 0u);
    header          = &eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR];
    // Format 6 (D-F28): the domain mask, a 24-bit payload length, then the
    // flags byte (origin half bit 0, override bit 1).
    CHECK(header[0] == 0x00u);
    CHECK(load_u32(&header[1]) == sizeof(empty_profile));
    CHECK(header[5] == 0x02u);
    CHECK(load_u32(&header[6]) == 1u);
    CHECK(load_u32(&header[10]) == first_candidate.payload_crc32);
    CHECK(load_u32(&header[14]) == COMPILED_DIGEST);
    CHECK(load_u32(&header[18]) == ACTION_ABI_DIGEST);
    for (uint8_t reserved = 22u; reserved < 33u; reserved++) CHECK(header[reserved] == 0u);
    CHECK(load_u16(&header[33]) == header_crc16(header));
    CHECK(header[35] == 0x18u);

    initialize_store(&rebooted, &eeprom, NOAH_PROFILE_STORE_OK);
    CHECK(rebooted.committed.slot == NOAH_PROFILE_SLOT_A && rebooted.committed.payload_length == sizeof(empty_profile));
    CHECK(commit_payload(&rebooted, rgb_profile, sizeof(rgb_profile), 7u, 1u, &record) == NOAH_PROFILE_STORE_OK);
    CHECK(record.slot == NOAH_PROFILE_SLOT_B && record.generation == 7u && record.origin_half == 1u);
    CHECK(record.domain_mask == 1u);

    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_OK);
    CHECK(store.committed.slot == NOAH_PROFILE_SLOT_B && store.committed.generation == 7u);
    CHECK(noah_profile_store_next_generation(&store, &next) && next == 8u);
}

// Bytes 22..32 are reserved and zero; 22..29 held the VIA binding until
// D-F31. A record round-trips without them, and compatibility still decides
// whether a committed slot is selected.
static void test_header_reserved_bytes_are_zero(void) {
    noah_profile_store_t           store;
    noah_profile_store_t           rebooted;
    noah_profile_store_record_t    record;
    noah_profile_store_candidate_t candidate;
    uint8_t                       *header;

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    candidate                = candidate_for(empty_profile, sizeof(empty_profile), 1u, 0u);
    candidate.format_version = NOAH_PROFILE_STORE_FORMAT_VERSION;
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_write(&store, 0u, empty_profile, sizeof(empty_profile)) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_commit(&store, &record) == NOAH_PROFILE_STORE_OK);
    CHECK(record.format_version == NOAH_PROFILE_STORE_FORMAT_VERSION);
    header = &eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR];
    CHECK(load_u32(&header[1]) == candidate.payload_length);
    CHECK(load_u32(&header[6]) == candidate.generation);
    CHECK(load_u32(&header[10]) == candidate.payload_crc32);
    CHECK(load_u32(&header[14]) == candidate.compiled_default_digest);
    CHECK(load_u32(&header[18]) == candidate.action_abi_digest);
    for (uint8_t reserved = 22u; reserved < 33u; reserved++) CHECK(header[reserved] == 0u);
    CHECK(load_u16(&header[33]) == header_crc16(header));
    CHECK(header[35] == 0x18u);

    initialize_store(&rebooted, &eeprom, NOAH_PROFILE_STORE_OK);
    CHECK(rebooted.committed.format_version == NOAH_PROFILE_STORE_FORMAT_VERSION);
    CHECK(rebooted.committed.payload_digest == candidate.payload_digest);
    {
        noah_profile_store_compatibility_t incompatible = compatibility();
        noah_profile_store_record_t        selected;
        incompatible.compiled_default_digest ^= 1u;
        noah_profile_store_init(&rebooted, io_for(&eeprom), incompatible);
        CHECK(noah_profile_store_boot_select(&rebooted, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
        incompatible = compatibility();
        incompatible.action_abi_digest ^= 1u;
        noah_profile_store_init(&rebooted, io_for(&eeprom), incompatible);
        CHECK(noah_profile_store_boot_select(&rebooted, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    }
    // A header still carrying a VIA binding is not this format's: refused,
    // even with its checksum repaired.
    store_u32(&header[22], 17u);
    store_u32(&header[26], UINT32_C(0x89ABCDEF));
    store_u16(&header[33], header_crc16(header));
    initialize_store(&rebooted, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
}

static void test_chunk_and_candidate_guards(void) {
    noah_profile_store_t           store;
    noah_profile_store_candidate_t candidate;
    uint8_t                        oversized[NOAH_PROFILE_STORE_IO_CHUNK_MAX + 1u] = {0};

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    candidate = candidate_for(empty_profile, sizeof(empty_profile), 1u, 0u);
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_PREPARE_IN_PROGRESS);
    CHECK(noah_profile_store_prepare_write(&store, 1u, empty_profile, 1u) == NOAH_PROFILE_STORE_CHUNK_OUT_OF_ORDER);
    CHECK(noah_profile_store_prepare_write(&store, 0u, oversized, sizeof(oversized)) == NOAH_PROFILE_STORE_CHUNK_TOO_LARGE);
    CHECK(noah_profile_store_prepare_commit(&store, NULL) == NOAH_PROFILE_STORE_PAYLOAD_INCOMPLETE);
    CHECK(noah_profile_store_prepare_abort(&store) == NOAH_PROFILE_STORE_OK);

    candidate.schema_major = 1u;
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_INCOMPATIBLE_SCHEMA);
    candidate.schema_major      = NOAH_PROFILE_SCHEMA_MAJOR;
    candidate.action_abi_digest = 0u;
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_INCOMPATIBLE_ACTION_ABI);
    candidate.action_abi_digest       = ACTION_ABI_DIGEST;
    candidate.compiled_default_digest = 0u;
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_INCOMPATIBLE_COMPILED_DEFAULT);
}

static void test_commit_state_machine_is_scan_bounded(void) {
    noah_profile_store_t           store;
    noah_profile_store_candidate_t candidate;
    noah_profile_store_record_t    record;
    noah_profile_store_result_t    result;
    uint32_t                       prior_reads;
    uint32_t                       prior_writes;
    uint16_t                       steps = 0u;

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    candidate = candidate_for(rgb_profile, sizeof(rgb_profile), 1u, 0u);
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_write(&store, 0u, rgb_profile, sizeof(rgb_profile)) == NOAH_PROFILE_STORE_OK);
    prior_reads  = eeprom.read_calls;
    prior_writes = eeprom.write_calls;
    CHECK(noah_profile_store_prepare_commit_begin(&store) == NOAH_PROFILE_STORE_IN_PROGRESS);
    CHECK(eeprom.read_calls == prior_reads && eeprom.write_calls == prior_writes);
    CHECK(noah_profile_store_prepare_write(&store, 0u, rgb_profile, 1u) == NOAH_PROFILE_STORE_PREPARE_IN_PROGRESS);
    CHECK(noah_profile_store_prepare_commit_step(&store, 0u, NULL) == NOAH_PROFILE_STORE_INVALID_ARGUMENT);
    CHECK(noah_profile_store_prepare_commit_step(&store, 21u, NULL) == NOAH_PROFILE_STORE_INVALID_ARGUMENT);
    CHECK(eeprom.read_calls == prior_reads && eeprom.write_calls == prior_writes);

    do {
        noah_profile_store_commit_phase_t phase      = store.commit_phase;
        uint32_t                          prior_read = eeprom.read_bytes;

        prior_reads                = eeprom.read_calls;
        prior_writes               = eeprom.write_calls;
        eeprom.largest_read_length = 0u;
        result                     = noah_profile_store_prepare_commit_step(&store, 1u, &record);
        if (eeprom.write_calls != prior_writes) {
            CHECK(eeprom.write_calls == prior_writes + 1u && eeprom.read_calls == prior_reads && eeprom.last_write_length <= 1u);
        } else if (phase == NOAH_PROFILE_STORE_COMMIT_PAYLOAD_READBACK) {
            // The payload readback only reads: up to a flash page a step, in
            // reads of the budget (D-F29, "Pacing").
            CHECK(eeprom.read_calls > prior_reads && eeprom.largest_read_length <= 1u && eeprom.read_bytes - prior_read <= NOAH_PROFILE_STORAGE_PAGE_SIZE);
        } else {
            CHECK(eeprom.read_calls == prior_reads + 1u && eeprom.last_read_length <= 1u);
        }
        CHECK(++steps < 160u);
    } while (result == NOAH_PROFILE_STORE_IN_PROGRESS);

    CHECK(result == NOAH_PROFILE_STORE_OK);
    CHECK(record.slot == NOAH_PROFILE_SLOT_A && record.generation == 1u);
    CHECK(!store.prepare_active && store.commit_phase == NOAH_PROFILE_STORE_COMMIT_IDLE);
}

// A long payload's readback takes a flash page a step, in reads of the
// budget, and covers every byte once.
static void test_payload_readback_reads_a_page_a_step(void) {
    enum { BODY = 600u };
    static uint8_t                 payload[14u + BODY];
    noah_profile_store_t           store;
    noah_profile_store_candidate_t candidate;
    noah_profile_store_record_t    record;
    noah_profile_store_result_t    result;
    uint32_t                       readback_steps = 0u;
    uint32_t                       readback_bytes = 0u;

    memcpy(payload, rgb_profile, 14u);
    payload[10] = (uint8_t)BODY;
    payload[11] = (uint8_t)(BODY >> 8u);
    for (uint32_t index = 0u; index < BODY; index++) payload[14u + index] = (uint8_t)(index * 7u + 1u);
    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    candidate = candidate_for(payload, sizeof(payload), 1u, 0u);
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
    for (uint32_t offset = 0u; offset < sizeof(payload); offset += NOAH_PROFILE_STORE_IO_CHUNK_MAX) {
        uint32_t length = sizeof(payload) - offset < NOAH_PROFILE_STORE_IO_CHUNK_MAX ? sizeof(payload) - offset : NOAH_PROFILE_STORE_IO_CHUNK_MAX;
        CHECK(noah_profile_store_prepare_write(&store, offset, &payload[offset], length) == NOAH_PROFILE_STORE_OK);
    }
    CHECK(noah_profile_store_prepare_commit_begin(&store) == NOAH_PROFILE_STORE_IN_PROGRESS);
    do {
        bool     readback = store.commit_phase == NOAH_PROFILE_STORE_COMMIT_PAYLOAD_READBACK;
        uint32_t before   = eeprom.read_bytes;

        eeprom.largest_read_length = 0u;
        result                     = noah_profile_store_prepare_commit_step(&store, 20u, &record);
        if (readback) {
            CHECK(eeprom.largest_read_length <= 20u && eeprom.read_bytes - before <= NOAH_PROFILE_STORAGE_PAGE_SIZE);
            readback_steps++;
            readback_bytes += eeprom.read_bytes - before;
        }
    } while (result == NOAH_PROFILE_STORE_IN_PROGRESS);
    CHECK(result == NOAH_PROFILE_STORE_OK && record.payload_length == sizeof(payload));
    CHECK(readback_bytes == sizeof(payload));
    CHECK(readback_steps == (sizeof(payload) + NOAH_PROFILE_STORAGE_PAGE_SIZE - 1u) / NOAH_PROFILE_STORAGE_PAGE_SIZE);
}

static void test_durable_prepare_is_not_authority_until_decided(void) {
    noah_profile_store_t        store;
    noah_profile_store_t        rebooted;
    noah_profile_store_record_t record;
    noah_profile_store_result_t result;

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    {
        noah_profile_store_candidate_t candidate = candidate_for(empty_profile, sizeof(empty_profile), 1u, 0u);
        candidate.format_version                 = NOAH_PROFILE_STORE_FORMAT_VERSION;
        CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
        CHECK(noah_profile_store_prepare_write(&store, 0u, empty_profile, sizeof(empty_profile)) == NOAH_PROFILE_STORE_OK);
    }
    CHECK(noah_profile_store_prepare_durable_begin(&store) == NOAH_PROFILE_STORE_IN_PROGRESS);
    do {
        result = noah_profile_store_prepare_durable_step(&store, 5u, &record);
    } while (result == NOAH_PROFILE_STORE_IN_PROGRESS);
    CHECK(result == NOAH_PROFILE_STORE_OK);
    CHECK(store.prepare_active && store.prepared_durable);
    CHECK(eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + 35u] == 0x3Cu);

    // A reset before the decision marker retains the previous authority. On a
    // first transaction that means compiled defaults remain authoritative.
    initialize_store(&rebooted, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);

    CHECK(noah_profile_store_prepared_commit_begin(&store) == NOAH_PROFILE_STORE_IN_PROGRESS);
    do {
        result = noah_profile_store_prepared_commit_step(&store, 1u, &record);
    } while (result == NOAH_PROFILE_STORE_IN_PROGRESS);
    CHECK(result == NOAH_PROFILE_STORE_OK);
    CHECK(!store.prepare_active && !store.prepared_durable);
    CHECK(record.generation == 1u && record.format_version == NOAH_PROFILE_STORE_FORMAT_VERSION);
    CHECK(eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + 35u] == 0x18u);
    initialize_store(&rebooted, &eeprom, NOAH_PROFILE_STORE_OK);
    CHECK(rebooted.committed.generation == 1u);
}

static void test_durable_prepare_can_be_aborted_before_decision(void) {
    noah_profile_store_t        store;
    noah_profile_store_t        rebooted;
    noah_profile_store_result_t result;

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    {
        noah_profile_store_candidate_t candidate = candidate_for(rgb_profile, sizeof(rgb_profile), 1u, 1u);
        CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
        CHECK(noah_profile_store_prepare_write(&store, 0u, rgb_profile, sizeof(rgb_profile)) == NOAH_PROFILE_STORE_OK);
    }
    CHECK(noah_profile_store_prepare_durable_begin(&store) == NOAH_PROFILE_STORE_IN_PROGRESS);
    do {
        result = noah_profile_store_prepare_durable_step(&store, 20u, NULL);
    } while (result == NOAH_PROFILE_STORE_IN_PROGRESS);
    CHECK(result == NOAH_PROFILE_STORE_OK && store.prepared_durable);
    CHECK(noah_profile_store_prepare_abort(&store) == NOAH_PROFILE_STORE_OK);
    CHECK(!store.prepare_active && !store.prepared_durable);
    initialize_store(&rebooted, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
}

static void test_destructive_reuse_guard_brackets_every_prepare(void) {
    noah_profile_store_t             store;
    noah_profile_store_candidate_t   candidate;
    reuse_guard_state_t              state;
    noah_profile_store_reuse_guard_t guard;
    uint32_t                         writes_before;

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    memset(&state, 0, sizeof(state));
    state.memory = &eeprom;
    guard        = (noah_profile_store_reuse_guard_t){.begin = reuse_begin, .end = reuse_end, .context = &state};
    CHECK(!noah_profile_store_set_reuse_guard(&store, &(noah_profile_store_reuse_guard_t){.begin = reuse_begin}));
    CHECK(noah_profile_store_set_reuse_guard(&store, &guard));
    candidate = candidate_for(empty_profile, sizeof(empty_profile), 1u, 0u);

    state.deny_begin = true;
    writes_before    = eeprom.write_calls;
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_BACKING_REUSE_DENIED);
    CHECK(state.begin_calls == 1u && state.end_calls == 0u);
    CHECK(eeprom.write_calls == writes_before && state.saw_pristine_marker);
    CHECK(!store.prepare_active && !store.reuse_active);

    state.deny_begin = false;
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
    CHECK(state.begin_calls == 2u && state.end_calls == 0u && state.slot == NOAH_PROFILE_SLOT_A);
    CHECK(store.prepare_active && store.reuse_active);
    CHECK(!noah_profile_store_set_reuse_guard(&store, NULL));
    CHECK(noah_profile_store_prepare_abort(&store) == NOAH_PROFILE_STORE_OK);
    CHECK(state.end_calls == 1u && !store.prepare_active && !store.reuse_active);

    eeprom.fail_write_call = eeprom.write_calls + 1u;
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_IO_ERROR);
    CHECK(state.begin_calls == 3u && state.end_calls == 2u);
    CHECK(!store.prepare_active && !store.reuse_active);
    eeprom.fail_write_call = 0u;

    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
    eeprom.fail_write_call = eeprom.write_calls + 1u;
    CHECK(noah_profile_store_prepare_write(&store, 0u, empty_profile, sizeof(empty_profile)) == NOAH_PROFILE_STORE_IO_ERROR);
    CHECK(state.begin_calls == 4u && state.end_calls == 3u && !store.reuse_active);
    eeprom.fail_write_call = 0u;

    candidate.payload_crc32 ^= 1u;
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_write(&store, 0u, empty_profile, sizeof(empty_profile)) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_commit(&store, NULL) == NOAH_PROFILE_STORE_CHECKSUM_MISMATCH);
    CHECK(state.begin_calls == 5u && state.end_calls == 4u && !store.reuse_active);
    candidate.payload_crc32 ^= 1u;

    CHECK(commit_payload(&store, empty_profile, sizeof(empty_profile), 1u, 0u, NULL) == NOAH_PROFILE_STORE_OK);
    CHECK(state.begin_calls == 6u && state.end_calls == 5u && !store.reuse_active);

    candidate.generation = 2u;
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
    state.fail_end = true;
    CHECK(noah_profile_store_prepare_abort(&store) == NOAH_PROFILE_STORE_BACKING_REUSE_RELEASE_FAILED);
    CHECK(state.begin_calls == 7u && state.end_calls == 6u && !store.prepare_active && store.reuse_active);
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_BACKING_REUSE_RELEASE_FAILED);
}

static void test_payload_and_header_validation(void) {
    noah_profile_store_t        store;
    noah_profile_store_t        rebooted;
    noah_profile_store_record_t record;
    uint8_t                     malformed[sizeof(empty_profile)];

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    memcpy(malformed, empty_profile, sizeof(malformed));
    malformed[7] = 0u;
    CHECK(commit_payload(&store, malformed, sizeof(malformed), 1u, 0u, NULL) == NOAH_PROFILE_STORE_INVALID_PAYLOAD);
    initialize_store(&rebooted, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);

    CHECK(commit_payload(&rebooted, empty_profile, sizeof(empty_profile), 1u, 0u, &record) == NOAH_PROFILE_STORE_OK);
    eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE] ^= 0x01u;
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(commit_payload(&store, empty_profile, sizeof(empty_profile), 1u, 0u, NULL) == NOAH_PROFILE_STORE_OK);
    CHECK(commit_payload(&store, rgb_profile, sizeof(rgb_profile), 2u, 0u, NULL) == NOAH_PROFILE_STORE_OK);
    eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE + 1u] ^= 0x80u;
    initialize_store(&rebooted, &eeprom, NOAH_PROFILE_STORE_OK);
    CHECK(rebooted.committed.slot == NOAH_PROFILE_SLOT_A && rebooted.committed.generation == 1u);

    eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + 28u] ^= 0x01u;
    initialize_store(&rebooted, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
}

static void test_checksum_mismatch_never_commits(void) {
    noah_profile_store_t           store;
    noah_profile_store_t           rebooted;
    noah_profile_store_candidate_t candidate;

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    candidate = candidate_for(empty_profile, sizeof(empty_profile), 1u, 0u);
    candidate.payload_crc32 ^= 1u;
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_write(&store, 0u, empty_profile, sizeof(empty_profile)) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_commit(&store, NULL) == NOAH_PROFILE_STORE_CHECKSUM_MISMATCH);
    initialize_store(&rebooted, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
}

static void test_final_marker_io_failure_is_durability_unknown(void) {
    noah_profile_store_t        store;
    noah_profile_store_t        rebooted;
    noah_profile_store_record_t selected;

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    stage_commit_to_phase(&store, empty_profile, sizeof(empty_profile), NOAH_PROFILE_STORE_COMMIT_MARKER_WRITE);
    eeprom.fail_write_call    = eeprom.write_calls + 1u;
    eeprom.fail_partial_bytes = NOAH_PROFILE_STORAGE_COMMIT_MARKER_SIZE;
    CHECK(noah_profile_store_prepare_commit_step(&store, 20u, NULL) == NOAH_PROFILE_STORE_DURABILITY_UNKNOWN);
    CHECK(!store.prepare_active && store.reconciliation_required);
    CHECK(noah_profile_store_prepare_begin(&store, &(noah_profile_store_candidate_t){0}) == NOAH_PROFILE_STORE_DURABILITY_UNKNOWN);
    eeprom.fail_write_call = 0u;
    initialize_store(&rebooted, &eeprom, NOAH_PROFILE_STORE_OK);
    CHECK(rebooted.committed.generation == 1u);

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    stage_commit_to_phase(&store, empty_profile, sizeof(empty_profile), NOAH_PROFILE_STORE_COMMIT_MARKER_READBACK);
    eeprom.fail_reads = true;
    CHECK(noah_profile_store_prepare_commit_step(&store, 20u, NULL) == NOAH_PROFILE_STORE_DURABILITY_UNKNOWN);
    CHECK(store.reconciliation_required);
    eeprom.fail_reads = false;
    CHECK(noah_profile_store_boot_select(&store, &selected) == NOAH_PROFILE_STORE_OK);
    CHECK(!store.reconciliation_required);
    noah_profile_store_init(&rebooted, io_for(&eeprom), compatibility());
    CHECK(noah_profile_store_boot_select(&rebooted, &selected) == NOAH_PROFILE_STORE_OK);
    CHECK(selected.generation == 1u);
}

static void test_power_loss_preserves_last_known_good(void) {
    noah_profile_store_t store;
    uint32_t             fail_call;

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(commit_payload(&store, empty_profile, sizeof(empty_profile), 1u, 0u, NULL) == NOAH_PROFILE_STORE_OK);
    memcpy(eeprom_snapshot, eeprom.bytes, sizeof(eeprom_snapshot));

    for (fail_call = 1u; fail_call <= 7u; fail_call++) {
        uint16_t partial_limit = partial_limit_for_commit_write(fail_call, sizeof(rgb_profile));
        uint16_t partial;

        for (partial = 0u; partial < partial_limit; partial++) {
            noah_profile_store_result_t result;

            memcpy(eeprom.bytes, eeprom_snapshot, sizeof(eeprom.bytes));
            eeprom.write_calls        = 0u;
            eeprom.fail_write_call    = fail_call;
            eeprom.fail_partial_bytes = partial;
            initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_OK);
            result = commit_payload(&store, rgb_profile, sizeof(rgb_profile), 2u, 1u, NULL);
            CHECK(result != NOAH_PROFILE_STORE_OK);

            eeprom.fail_write_call = 0u;
            initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_OK);
            CHECK(store.committed.slot == NOAH_PROFILE_SLOT_A && store.committed.generation == 1u);
        }
    }
}

static void test_first_commit_power_loss_falls_back_to_compiled_defaults(void) {
    noah_profile_store_t store;
    uint32_t             fail_call;

    for (fail_call = 1u; fail_call <= 6u; fail_call++) {
        uint16_t partial_limit = partial_limit_for_commit_write(fail_call, sizeof(empty_profile));
        uint16_t partial;

        for (partial = 0u; partial < partial_limit; partial++) {
            reset_eeprom(&eeprom);
            initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
            eeprom.fail_write_call    = fail_call;
            eeprom.fail_partial_bytes = partial;
            CHECK(commit_payload(&store, empty_profile, sizeof(empty_profile), 1u, 0u, NULL) != NOAH_PROFILE_STORE_OK);

            eeprom.fail_write_call = 0u;
            initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
        }
    }

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(commit_payload(&store, empty_profile, sizeof(empty_profile), 1u, 0u, NULL) == NOAH_PROFILE_STORE_OK);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_OK);
    CHECK(store.committed.generation == 1u);
}

static void test_generation_conflict_and_rollover(void) {
    noah_profile_store_t store;
    noah_profile_store_t alternate_store;
    uint32_t             next;

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(commit_payload(&store, empty_profile, sizeof(empty_profile), UINT32_MAX, 0u, NULL) == NOAH_PROFILE_STORE_OK);
    CHECK(!noah_profile_store_next_generation(&store, &next));
    CHECK(commit_payload(&store, rgb_profile, sizeof(rgb_profile), 1u, 1u, NULL) == NOAH_PROFILE_STORE_GENERATION_NOT_NEWER);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_OK);
    CHECK(store.committed.generation == UINT32_MAX);

    reset_eeprom(&eeprom);
    reset_eeprom(&alternate_eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    initialize_store(&alternate_store, &alternate_eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(commit_payload(&store, empty_profile, sizeof(empty_profile), 9u, 0u, NULL) == NOAH_PROFILE_STORE_OK);
    CHECK(commit_payload(&alternate_store, behavior_profile, sizeof(behavior_profile), 9u, 0u, NULL) == NOAH_PROFILE_STORE_OK);
    memcpy(&eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR], &alternate_eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR], NOAH_PROFILE_STORAGE_SLOT_A_SIZE);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_GENERATION_CONFLICT);
    CHECK(store.conflict);
    CHECK(!noah_profile_store_next_generation(&store, &next));
}

static void test_equal_generation_metadata_divergence_conflicts(void) {
    noah_profile_store_t store;
    uint8_t             *slot_b_header;

    reset_eeprom(&eeprom);
    reset_eeprom(&alternate_eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(commit_payload(&store, empty_profile, sizeof(empty_profile), 11u, 0u, NULL) == NOAH_PROFILE_STORE_OK);
    initialize_store(&store, &alternate_eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(commit_payload(&store, empty_profile, sizeof(empty_profile), 11u, 0u, NULL) == NOAH_PROFILE_STORE_OK);
    memcpy(&eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR], &alternate_eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR], NOAH_PROFILE_STORAGE_SLOT_A_SIZE);

    slot_b_header = &eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR];
    slot_b_header[5] ^= 1u << 1u; // the override flag
    store_u16(&slot_b_header[33], header_crc16(slot_b_header));

    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_GENERATION_CONFLICT);
    CHECK(store.conflict);
}

static void test_io_failures_are_not_treated_as_empty(void) {
    noah_profile_store_t           store;
    noah_profile_store_record_t    selected;
    noah_profile_store_candidate_t candidate;

    reset_eeprom(&eeprom);
    eeprom.fail_reads = true;
    noah_profile_store_init(&store, io_for(&eeprom), compatibility());
    CHECK(noah_profile_store_boot_select(&store, &selected) == NOAH_PROFILE_STORE_IO_ERROR);
    candidate = candidate_for(empty_profile, sizeof(empty_profile), 1u, 0u);
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_INVALID_ARGUMENT);
}

static void test_boot_rejects_old_compiled_default_identity(void) {
    noah_profile_store_t               store;
    noah_profile_store_record_t        selected;
    noah_profile_store_candidate_t     candidate;
    noah_profile_store_compatibility_t old_compatibility = compatibility();

    reset_eeprom(&eeprom);
    old_compatibility.compiled_default_digest ^= 1u;
    noah_profile_store_init(&store, io_for(&eeprom), old_compatibility);
    CHECK(noah_profile_store_boot_select(&store, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    candidate                         = candidate_for(empty_profile, sizeof(empty_profile), 1u, 0u);
    candidate.compiled_default_digest = old_compatibility.compiled_default_digest;
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_write(&store, 0u, empty_profile, sizeof(empty_profile)) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_commit(&store, &selected) == NOAH_PROFILE_STORE_OK);

    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
}

static void test_boot_selection_supports_read_only_discovery(void) {
    noah_profile_store_t        store;
    noah_profile_store_t        read_only_store;
    noah_profile_store_io_t     read_only_io;
    noah_profile_store_record_t selected;

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(commit_payload(&store, empty_profile, sizeof(empty_profile), 1u, 0u, NULL) == NOAH_PROFILE_STORE_OK);

    read_only_io       = io_for(&eeprom);
    read_only_io.write = NULL;
    noah_profile_store_init(&read_only_store, read_only_io, compatibility());
    CHECK(noah_profile_store_validate_slot(&read_only_store, NOAH_PROFILE_SLOT_A, true, &selected) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_boot_select(&read_only_store, &selected) == NOAH_PROFILE_STORE_OK);
    CHECK(selected.slot == NOAH_PROFILE_SLOT_A && selected.generation == 1u);
    CHECK(noah_profile_store_prepare_begin(&read_only_store, &(noah_profile_store_candidate_t){0}) == NOAH_PROFILE_STORE_INVALID_ARGUMENT);
}

static void test_boot_selection_state_machine_is_scan_bounded(void) {
    noah_profile_store_t        store;
    noah_profile_store_record_t selected;
    noah_profile_store_result_t result;
    uint32_t                    steps = 0u;

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(commit_payload(&store, behavior_profile, sizeof(behavior_profile), 4u, 1u, NULL) == NOAH_PROFILE_STORE_OK);

    noah_profile_store_init(&store, io_for(&eeprom), compatibility());
    eeprom.read_calls = 0u;
    result            = noah_profile_store_boot_select_begin(&store);
    CHECK(result == NOAH_PROFILE_STORE_IN_PROGRESS);
    CHECK(eeprom.read_calls == 0u);
    while (result == NOAH_PROFILE_STORE_IN_PROGRESS) {
        uint32_t reads_before = eeprom.read_calls;

        CHECK(steps++ < 64u);
        result = noah_profile_store_boot_select_step(&store, 5u, &selected);
        CHECK(eeprom.read_calls == reads_before || eeprom.read_calls == reads_before + 1u);
        if (eeprom.read_calls != reads_before) {
            CHECK(eeprom.last_read_length <= NOAH_PROFILE_STORE_IO_CHUNK_MAX);
        }
    }
    CHECK(result == NOAH_PROFILE_STORE_OK);
    CHECK(selected.slot == NOAH_PROFILE_SLOT_A && selected.generation == 4u && selected.origin_half == 1u);
    CHECK(store.boot_scanned && !store.conflict);

    {
        uint32_t                    reads_before = eeprom.read_calls;
        noah_profile_store_record_t repeated     = {0};

        CHECK(noah_profile_store_boot_select_step(&store, 1u, &repeated) == NOAH_PROFILE_STORE_OK);
        CHECK(eeprom.read_calls == reads_before);
        CHECK(repeated.slot == selected.slot && repeated.generation == selected.generation && repeated.payload_digest == selected.payload_digest);
    }
}

// These are storage-envelope fixtures: domain bodies are opaque to this layer.
// The schema validator remains responsible for semantic payload validation.
static uint16_t pd_payload(uint8_t *payload, uint8_t mask) {
    const uint8_t header[] = {'N', 'L', 'P', '1', 4u, 0u, 0u, 1u};
    uint16_t length = sizeof(header);
    memcpy(payload, header, sizeof(header));
    for (uint8_t domain = 0u; domain < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT; domain++) {
        if (!(mask & (1u << domain))) continue;
        payload[6]++;
        payload[length++] = (uint8_t)((domain + 1u) * 0x10u);
        // Every domain at the one version the current schema accepts.
        payload[length++] = noah_profile_domain_at(domain)->version;
        payload[length++] = 1u;
        payload[length++] = 0u;
        payload[length++] = 0u;
        payload[length++] = 0u;
        payload[length++] = (uint8_t)(0x80u + domain);
    }
    return length;
}

static noah_profile_store_compatibility_t pd_compatibility(void) {
    noah_profile_store_compatibility_t value = compatibility();
    value.schema_major = NOAH_PROFILE_SCHEMA_MAJOR;
    return value;
}

static void initialize_pd_store(noah_profile_store_t *store) {
    noah_profile_store_record_t record;
    noah_profile_store_init(store, io_for(&eeprom), pd_compatibility());
    noah_profile_store_result_t result = noah_profile_store_boot_select(store, &record);
    CHECK(result == NOAH_PROFILE_STORE_OK || result == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
}

static noah_profile_store_candidate_t pd_candidate(const uint8_t *payload, uint16_t length, uint8_t mask, uint32_t generation, uint8_t origin, uint8_t flags) {
    noah_profile_store_candidate_t value = candidate_for(payload, length, generation, origin);
    value.schema_major = NOAH_PROFILE_SCHEMA_MAJOR;
    value.format_version                 = NOAH_PROFILE_STORE_FORMAT_VERSION;
    value.domain_mask = mask;
    value.flags = flags;
    return value;
}

static noah_profile_store_result_t stage_pd(noah_profile_store_t *store, const uint8_t *payload, const noah_profile_store_candidate_t *candidate) {
    noah_profile_store_result_t result = noah_profile_store_prepare_begin(store, candidate);
    if (result != NOAH_PROFILE_STORE_OK) return result;
    for (uint16_t at = 0u; at < candidate->payload_length;) {
        uint16_t length = candidate->payload_length - at;
        if (length > 5u) length = 5u;
        result = noah_profile_store_prepare_write(store, at, &payload[at], length);
        if (result != NOAH_PROFILE_STORE_OK) return result;
        at += length;
    }
    return NOAH_PROFILE_STORE_OK;
}

static noah_profile_store_result_t commit_pd(noah_profile_store_t *store, const uint8_t *payload, const noah_profile_store_candidate_t *candidate) {
    noah_profile_store_result_t result = stage_pd(store, payload, candidate);
    return result == NOAH_PROFILE_STORE_OK ? noah_profile_store_prepare_commit(store, NULL) : result;
}

// The store's shape check accepts every settings version the validator does:
// a profile with named macros (settings v4) once passed validation on both
// halves and was then refused here on every try.
static void test_pd_store_accepts_every_settings_version_the_validator_does(void) {
    for (uint8_t version = 1u; version <= 8u; version++) {
        uint8_t                        payload[48];
        uint16_t                       length = pd_payload(payload, 1u << 3u);
        noah_profile_store_t           store;
        noah_profile_store_candidate_t candidate;

        CHECK(payload[8] == 0x40u);
        payload[9] = version;
        candidate  = pd_candidate(payload, length, 1u << 3u, 1u, 0u, 0u);
        reset_eeprom(&eeprom);
        initialize_pd_store(&store);
        noah_profile_store_result_t result = commit_pd(&store, payload, &candidate);
        CHECK((result == NOAH_PROFILE_STORE_OK) == (version == NOAH_PROFILE_DOMAIN_VERSION_SETTINGS));
        CHECK(NOAH_PROFILE_DOMAIN_VERSION_SETTINGS == 8u);
    }
}

// Combo versions too: the keyboard's combo domain became v2 (one default
// window), the validator passed it, and this check refused every copy to the
// other half until it used the validator's rule.
static void test_pd_store_accepts_every_combo_version_the_validator_does(void) {
    for (uint8_t version = 0u; version <= 4u; version++) {
        uint8_t                        payload[48];
        uint16_t                       length = pd_payload(payload, 1u << 2u);
        noah_profile_store_t           store;
        noah_profile_store_candidate_t candidate;
        noah_profile_store_record_t    record;

        CHECK(payload[8] == 0x30u);
        payload[9] = version;
        candidate  = pd_candidate(payload, length, 1u << 2u, 1u, 0u, 0u);
        reset_eeprom(&eeprom);
        initialize_pd_store(&store);
        noah_profile_store_result_t result = commit_pd(&store, payload, &candidate);
        CHECK((result == NOAH_PROFILE_STORE_OK) == (version == NOAH_PROFILE_DOMAIN_VERSION_COMBOS));
        CHECK(NOAH_PROFILE_DOMAIN_VERSION_COMBOS == 3u);
        if (result != NOAH_PROFILE_STORE_OK) continue;
        // ...and a stored current table is selected again at boot.
        noah_profile_store_init(&store, io_for(&eeprom), pd_compatibility());
        CHECK(noah_profile_store_boot_select(&store, &record) == NOAH_PROFILE_STORE_OK && record.generation == 1u);
    }
}

// The 32-slot domains: the store keeps only RGB v3 and PD v2, the versions
// the validator accepts. An eight-slot RGB v2 or PD v1 never reaches it.
static void test_pd_store_accepts_only_32_slot_rgb_and_pd_versions(void) {
    const struct { uint8_t mask, id; } domains[] = {{1u << 0u, 0x10u}, {1u << 4u, 0x50u}};
    for (size_t d = 0u; d < sizeof(domains) / sizeof(domains[0]); d++) {
        for (uint8_t version = 0u; version <= 5u; version++) {
            uint8_t                        payload[48];
            uint16_t                       length = pd_payload(payload, domains[d].mask);
            noah_profile_store_t           store;
            noah_profile_store_candidate_t candidate;
            bool accepted = version == (domains[d].id == 0x10u ? NOAH_PROFILE_DOMAIN_VERSION_RGB : NOAH_PROFILE_DOMAIN_VERSION_PD);

            CHECK(payload[8] == domains[d].id);
            payload[9] = version;
            candidate  = pd_candidate(payload, length, domains[d].mask, 1u, 0u, 0u);
            reset_eeprom(&eeprom);
            initialize_pd_store(&store);
            CHECK((commit_pd(&store, payload, &candidate) == NOAH_PROFILE_STORE_OK) == accepted);
            CHECK(accepted == (version == (domains[d].id == 0x10u ? 4u : 3u)));
        }
    }
}

static void test_pd_header_all_identity_bits_and_boot_paths(void) {
    for (uint8_t origin = 0u; origin < 2u; origin++) {
        for (uint8_t flags = 0u; flags < 2u; flags++) {
            for (uint8_t mask = 0u; mask <= NOAH_PROFILE_DOMAIN_MASK_ALL; mask++) {
                uint8_t payload[64];
                uint16_t length = pd_payload(payload, mask);
                noah_profile_store_t store, rebooted;
                noah_profile_store_record_t record;
                noah_profile_store_candidate_t candidate = pd_candidate(payload, length, mask, 1u, origin, flags);
                reset_eeprom(&eeprom);
                initialize_pd_store(&store);
                CHECK(commit_pd(&store, payload, &candidate) == NOAH_PROFILE_STORE_OK);
                const uint8_t *header = &eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR];
                CHECK(header[0] == mask && header[5] == (uint8_t)(origin | (flags << 1u)));
                CHECK(load_u16(&header[33]) == header_crc16(header));
                CHECK(header[35] == 0x18u);
                CHECK(noah_profile_store_validate_slot(&store, NOAH_PROFILE_SLOT_A, true, &record) == NOAH_PROFILE_STORE_OK);
                CHECK(record.domain_mask == mask && record.origin_half == origin && record.flags == flags);
                CHECK(record.payload_digest == candidate.payload_digest && record.schema_major == NOAH_PROFILE_SCHEMA_MAJOR);
                noah_profile_store_init(&rebooted, io_for(&eeprom), pd_compatibility());
                CHECK(noah_profile_store_boot_select_begin(&rebooted) == NOAH_PROFILE_STORE_IN_PROGRESS);
                noah_profile_store_result_t result;
                do {
                    uint32_t reads = eeprom.read_calls;
                    result = noah_profile_store_boot_select_step(&rebooted, 1u, &record);
                    CHECK(eeprom.read_calls <= reads + 1u);
                    CHECK(eeprom.last_read_length <= NOAH_PROFILE_STORE_IO_CHUNK_MAX);
                } while (result == NOAH_PROFILE_STORE_IN_PROGRESS);
                CHECK(result == NOAH_PROFILE_STORE_OK);
                CHECK(record.domain_mask == mask && record.origin_half == origin && record.flags == flags);
                // A reader with a different schema must refuse the current record.
                noah_profile_store_compatibility_t incompatible = compatibility();
                incompatible.schema_major                       = 1u;
                noah_profile_store_init(&rebooted, io_for(&eeprom), incompatible);
                CHECK(noah_profile_store_boot_select(&rebooted, &record) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
                CHECK(noah_profile_store_validate_slot(&rebooted, NOAH_PROFILE_SLOT_A, true, &record) == NOAH_PROFILE_STORE_INCOMPATIBLE_SCHEMA);
            }
        }
    }
}

// Even a valid prior-format header at a slot boundary must not be adopted:
// the format-5 magic and commit marker make it an earlier format, whatever
// its other bytes say.
static void test_previous_layout_store_header_is_refused(void) {
    uint8_t payload[64];
    uint16_t length = pd_payload(payload, 31u);
    noah_profile_store_t store;
    noah_profile_store_record_t record;
    noah_profile_store_candidate_t candidate = pd_candidate(payload, length, 31u, 1u, 0u, 0u);
    reset_eeprom(&eeprom);
    initialize_pd_store(&store);
    CHECK(commit_pd(&store, payload, &candidate) == NOAH_PROFILE_STORE_OK);
    uint8_t *header = &eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR];
    header[0]  = 'N';
    header[1]  = 'T';
    header[31] = 0xA5u;
    store_u16(&header[33], noah_profile_crc16_ccitt_update(NOAH_PROFILE_CRC16_INITIAL, header, 33u));
    noah_profile_store_init(&store, io_for(&eeprom), pd_compatibility());
    CHECK(noah_profile_store_boot_select(&store, &record) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE && store.earlier_format);
    CHECK(noah_profile_store_validate_slot(&store, NOAH_PROFILE_SLOT_A, true, &record) == NOAH_PROFILE_STORE_EARLIER_FORMAT);
}

static void test_pd_header_rejects_bad_contracts_before_writing(void) {
    uint8_t payload[64];
    uint16_t length = pd_payload(payload, 31u);
    noah_profile_store_t store;
    noah_profile_store_candidate_t valid = pd_candidate(payload, length, 31u, 1u, 1u, 1u), candidate;
    reset_eeprom(&eeprom);
    initialize_pd_store(&store);
    for (uint8_t invalid = 0u; invalid < 10u; invalid++) {
        if (invalid == 1u || invalid == 2u) continue; // were the retired VIA binding (D-F31)
        candidate = valid;
        switch (invalid) {
            case 0: candidate.domain_mask = 0x80u; break;
            case 3: candidate.origin_half = 2u; break;
            case 4: candidate.flags = 2u; break;
            case 5:
                candidate.format_version = 2u;
                break;
            case 6: candidate.schema_major = 1u; break;
            case 7: candidate.schema_minor = 1u; break;
            case 8:
                candidate.format_version = 0u;
                break;
            case 9:
                candidate.format_version = 1u;
                break;
        }
        CHECK(noah_profile_store_prepare_begin(&store, &candidate) != NOAH_PROFILE_STORE_OK);
        CHECK(eeprom.write_calls == 0u);
    }
    CHECK(commit_pd(&store, payload, &valid) == NOAH_PROFILE_STORE_OK);
    uint8_t *header = &eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR];
    noah_profile_store_record_t record;
    // Reserved header bytes and flag bits remain invalid with repaired CRCs.
    for (uint8_t reserved = 22; reserved <= 32; reserved++) {
        header[reserved] = 1;
        store_u16(&header[33], header_crc16(header));
        CHECK(noah_profile_store_validate_slot(&store, NOAH_PROFILE_SLOT_A, true, &record) == (reserved < 30u ? NOAH_PROFILE_STORE_EARLIER_FORMAT : NOAH_PROFILE_STORE_INVALID_HEADER));
        header[reserved] = 0;
    }
    header[5] ^= 4;
    store_u16(&header[33], header_crc16(header));
    CHECK(noah_profile_store_validate_slot(&store, NOAH_PROFILE_SLOT_A, true, &record) == NOAH_PROFILE_STORE_INVALID_HEADER);
    header[5] ^= 4;
    // Reserved bits and mask/payload disagreement stay invalid even with a valid CRC.
    header[0] |= 0x80u;
    store_u16(&header[33], header_crc16(header));
    CHECK(noah_profile_store_validate_slot(&store, NOAH_PROFILE_SLOT_A, true, &record) == NOAH_PROFILE_STORE_INVALID_HEADER);
    header[0] = 0x0fu; // Drops PD from the mask.
    store_u16(&header[33], header_crc16(header));
    CHECK(noah_profile_store_validate_slot(&store, NOAH_PROFILE_SLOT_A, true, &record) == NOAH_PROFILE_STORE_INVALID_PAYLOAD);
    initialize_pd_store(&store);
    CHECK(noah_profile_store_boot_select(&store, &record) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);

    for (uint8_t domain = 0u; domain < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT; domain++) {
        reset_eeprom(&eeprom);
        initialize_pd_store(&store);
        length = pd_payload(payload, NOAH_PROFILE_DOMAIN_MASK_ALL);
        // The version before each domain's current one is retired.
        payload[9u + 7u * domain] = (uint8_t)(noah_profile_domain_at(domain)->version - 1u);
        candidate = pd_candidate(payload, length, NOAH_PROFILE_DOMAIN_MASK_ALL, 1u, 0u, 1u);
        CHECK(commit_pd(&store, payload, &candidate) == NOAH_PROFILE_STORE_INVALID_PAYLOAD);
        CHECK(noah_profile_store_boot_select(&store, &record) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    }
    // Mask bits beyond the seven current domains are refused before writing.
    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    candidate = candidate_for(empty_profile, sizeof(empty_profile), 1u, 0u);
    candidate.format_version = NOAH_PROFILE_STORE_FORMAT_VERSION;
    candidate.domain_mask    = 128u;
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_INVALID_ARGUMENT);
    CHECK(eeprom.write_calls == 0u);
}

static void test_pd_header_power_loss_keeps_complete_generation(void) {
    uint8_t payload[64];
    uint16_t length = pd_payload(payload, 31u);
    noah_profile_store_t store, rebooted;
    noah_profile_store_record_t record;
    noah_profile_store_candidate_t first = pd_candidate(payload, length, 31u, 1u, 0u, 1u);
    noah_profile_store_candidate_t second = pd_candidate(payload, length, 31u, 2u, 1u, 0u);
    reset_eeprom(&eeprom);
    initialize_pd_store(&store);
    CHECK(commit_pd(&store, payload, &first) == NOAH_PROFILE_STORE_OK);
    memcpy(eeprom_snapshot, eeprom.bytes, sizeof(eeprom_snapshot));
    uint32_t before = eeprom.write_calls;
    CHECK(commit_pd(&store, payload, &second) == NOAH_PROFILE_STORE_OK);
    uint32_t writes = eeprom.write_calls - before;
    for (uint32_t cut = 1u; cut <= writes; cut++) {
        for (uint16_t partial = 0u; partial <= NOAH_PROFILE_STORE_IO_CHUNK_MAX; partial++) {
            reset_eeprom(&eeprom);
            memcpy(eeprom.bytes, eeprom_snapshot, sizeof(eeprom_snapshot));
            initialize_pd_store(&store);
            CHECK(noah_profile_store_boot_select(&store, &record) == NOAH_PROFILE_STORE_OK);
            eeprom.fail_write_call = cut;
            eeprom.fail_partial_bytes = partial;
            CHECK(commit_pd(&store, payload, &second) != NOAH_PROFILE_STORE_OK);
            eeprom.fail_write_call = 0u;
            noah_profile_store_init(&rebooted, io_for(&eeprom), pd_compatibility());
            CHECK(noah_profile_store_boot_select(&rebooted, &record) == NOAH_PROFILE_STORE_OK);
            CHECK(record.generation == 1u || record.generation == 2u);
            CHECK(record.domain_mask == 31u && record.payload_digest == first.payload_digest);
            CHECK(record.origin_half == (record.generation == 1u ? 0u : 1u));
            CHECK(record.flags == (record.generation == 1u ? 1u : 0u));
        }
    }
    // A durable prepared NR record is inspectable but never selected at boot.
    reset_eeprom(&eeprom);
    memcpy(eeprom.bytes, eeprom_snapshot, sizeof(eeprom_snapshot));
    initialize_pd_store(&store);
    CHECK(noah_profile_store_boot_select(&store, &record) == NOAH_PROFILE_STORE_OK);
    CHECK(stage_pd(&store, payload, &second) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_durable_begin(&store) == NOAH_PROFILE_STORE_IN_PROGRESS);
    noah_profile_store_result_t result;
    do { result = noah_profile_store_prepare_durable_step(&store, 1u, &record); } while (result == NOAH_PROFILE_STORE_IN_PROGRESS);
    CHECK(result == NOAH_PROFILE_STORE_OK && store.prepared_durable);
    CHECK(noah_profile_store_validate_slot(&store, NOAH_PROFILE_SLOT_B, false, &record) == NOAH_PROFILE_STORE_OK);
    CHECK(record.generation == 2u);
    noah_profile_store_init(&rebooted, io_for(&eeprom), pd_compatibility());
    CHECK(noah_profile_store_boot_select(&rebooted, &record) == NOAH_PROFILE_STORE_OK && record.generation == 1u);
    CHECK(noah_profile_store_prepare_abort(&store) == NOAH_PROFILE_STORE_OK);
}

// A slot in an earlier store format is refused before payload decoding in
// synchronous and bounded discovery, reported as such, and never displaces
// or becomes a current record (D-F28). Format 5 had the `NT` magic, a
// 16-bit length at byte 3, an unkeyed header CRC and markers 0xA5/0x5A. A
// header CRC keyed with another format version is refused too.
static void write_format5_header(uint8_t *header, uint8_t marker) {
    memset(header, 0, NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
    header[0] = 'N';
    header[1] = 'T';
    header[2] = 0x00u;
    store_u16(&header[3], sizeof(empty_profile));
    header[5] = 1u;
    store_u16(&header[29], noah_profile_crc16_ccitt_update(NOAH_PROFILE_CRC16_INITIAL, header, 29u));
    header[31] = marker;
}

// An unreleased format-6 build (before the keymap and macro domains) wrote a
// 32-byte header: identity byte (mask, origin << 5, override << 6), then the
// same fields one byte earlier and the keyed CRC16 at 29. Through today's
// addresses its page-0 bytes 31 to 34 read erased while the marker cells still
// read committed. Bytes 30 to 32 must be zero, so it is refused even when the
// CRC happens to match (forced_crc), and reported as a retired test format.
static void write_32_byte_format6_header(uint8_t *header, bool forced_crc) {
    static const uint8_t format = NOAH_PROFILE_STORE_FORMAT_VERSION;
    uint8_t              old[30];

    memcpy(old, header, sizeof(old));
    memset(header, 0, NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
    header[0] = old[0];
    memcpy(&header[1], &old[1], 4u);
    memcpy(&header[5], &old[6], 24u);
    store_u16(&header[29], noah_profile_crc16_ccitt_update(noah_profile_crc16_ccitt_update(NOAH_PROFILE_CRC16_INITIAL, &format, 1u), header, 29u));
    memset(&header[31], 0xFF, 4u);
    if (forced_crc) store_u16(&header[33], header_crc16(header));
    header[35] = 0x18u;
}

static void test_retired_store_headers_never_become_authority(void) {
    for (uint8_t variant = 0u; variant < 6u; variant++) {
        noah_profile_store_t        store, rebooted;
        noah_profile_store_record_t selected;
        uint8_t                     before[NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE];
        uint8_t                    *header = &eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR];
        bool                        earlier = variant != 2u;
        noah_profile_store_result_t refused = earlier ? NOAH_PROFILE_STORE_EARLIER_FORMAT : NOAH_PROFILE_STORE_CHECKSUM_MISMATCH;

        reset_eeprom(&eeprom);
        initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
        CHECK(commit_payload(&store, empty_profile, sizeof(empty_profile), 1u, 0u, &selected) == NOAH_PROFILE_STORE_OK);
        if (variant < 2u) {
            write_format5_header(header, variant == 0u ? 0xA5u : 0x5Au);
        } else if (variant == 5u) {
            // The immediately preceding test build bound a VIA generation
            // and digest at 22..29, with the same format and valid CRC.
            store_u32(&header[22], 6u);
            store_u32(&header[26], UINT32_C(0xabcdef01));
            store_u16(&header[33], header_crc16(header));
        } else if (variant >= 3u) {
            write_32_byte_format6_header(header, variant == 4u);
        } else {
            // Today's layout, its CRC keyed with format 7 rather than 6.
            static const uint8_t other_format = NOAH_PROFILE_STORE_FORMAT_VERSION + 1u;
            store_u16(&header[33], noah_profile_crc16_ccitt_update(noah_profile_crc16_ccitt_update(NOAH_PROFILE_CRC16_INITIAL, &other_format, 1u), header, 33u));
        }
        memcpy(before, header, sizeof(before));
        CHECK(noah_profile_store_validate_slot(&store, NOAH_PROFILE_SLOT_A, true, &selected) == refused);
        CHECK(noah_profile_store_validate_slot(&store, NOAH_PROFILE_SLOT_A, false, &selected) == refused);
        for (uint8_t bounded = 0u; bounded < 2u; bounded++) {
            noah_profile_store_init(&rebooted, io_for(&eeprom), compatibility());
            noah_profile_store_result_t result;
            if (bounded) {
                result = noah_profile_store_boot_select_begin(&rebooted);
                while (result == NOAH_PROFILE_STORE_IN_PROGRESS)
                    result = noah_profile_store_boot_select_step(&rebooted, 1u, &selected);
            } else
                result = noah_profile_store_boot_select(&rebooted, &selected);
            CHECK(result == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
            // Only an earlier format says why nothing was adopted.
            CHECK(rebooted.earlier_format == earlier);
        }
        // Save to B without overwriting the refused bytes in A.
        CHECK(commit_payload(&store, rgb_profile, sizeof(rgb_profile), 2u, 1u, &selected) == NOAH_PROFILE_STORE_OK);
        CHECK(selected.slot == NOAH_PROFILE_SLOT_B);
        noah_profile_store_init(&rebooted, io_for(&eeprom), compatibility());
        CHECK(noah_profile_store_boot_select(&rebooted, &selected) == NOAH_PROFILE_STORE_OK && selected.generation == 2u);
        CHECK(!rebooted.earlier_format);
        CHECK(memcmp(header, before, sizeof(before)) == 0);
    }
}

// A span longer than one store read is read in bounded pieces, in order, and
// a failed piece fails the whole span.
static void test_read_span_reads_within_the_io_bound(void) {
    noah_profile_store_t store;
    uint8_t              span[3u * NOAH_PROFILE_STORE_IO_CHUNK_MAX + 5u];
    const noah_profile_storage_address_t start = NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + 7u;

    reset_eeprom(&eeprom);
    for (uint16_t index = 0u; index < sizeof(span); index++) {
        eeprom.bytes[start + index] = (uint8_t)(index * 7u + 1u);
    }
    noah_profile_store_init(&store, io_for(&eeprom), pd_compatibility());
    eeprom.read_calls = 0u;
    CHECK(noah_profile_store_read_span(&store, start, span, sizeof(span)));
    CHECK(eeprom.read_calls == 4u && eeprom.last_read_length == 5u);
    CHECK(memcmp(span, &eeprom.bytes[start], sizeof(span)) == 0);
    CHECK(!noah_profile_store_read_span(&store, NOAH_PROFILE_STORAGE_REGION_SIZE - 4u, span, 5u));
    eeprom.fail_reads = true;
    CHECK(!noah_profile_store_read_span(&store, start, span, sizeof(span)));
    eeprom.fail_reads = false;
}

// Format 6's 32-bit payload length (D-F28), past 65,535 bytes in a 128 KiB
// test slot: a 70,000-byte blob is written, committed, selected at boot by
// the bounded scan and read back across the 64 KiB line.
static void test_payload_over_64_kib_round_trips(void) {
    enum { WIDE = 70000u };
    static uint8_t                 payload[WIDE];
    noah_profile_store_t           store, rebooted;
    noah_profile_store_record_t    record;
    noah_profile_storage_address_t payload_start = NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE;
    uint8_t                        span[40];
    noah_profile_store_result_t    result;

    memcpy(payload, (const uint8_t[]){'N', 'L', 'P', '1', 4u, 0u, 1u, 1u, 0x10u, NOAH_PROFILE_DOMAIN_VERSION_RGB}, 10u);
    payload[10] = (uint8_t)(WIDE - 14u);
    payload[11] = (uint8_t)((WIDE - 14u) >> 8u);
    payload[12] = (uint8_t)((WIDE - 14u) >> 16u);
    payload[13] = 0u;
    for (uint32_t index = 14u; index < WIDE; index++) payload[index] = (uint8_t)(index * 13u + 7u);

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(commit_payload(&store, payload, WIDE, 1u, 0u, &record) == NOAH_PROFILE_STORE_OK);
    CHECK(record.payload_length == WIDE && record.domain_mask == 1u);
    CHECK(load_u32(&eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + 1u]) == WIDE);
    CHECK(memcmp(&eeprom.bytes[payload_start], payload, WIDE) == 0);

    noah_profile_store_init(&rebooted, io_for(&eeprom), compatibility());
    result = noah_profile_store_boot_select_begin(&rebooted);
    while (result == NOAH_PROFILE_STORE_IN_PROGRESS) result = noah_profile_store_boot_select_step(&rebooted, NOAH_PROFILE_STORE_IO_CHUNK_MAX, &record);
    CHECK(result == NOAH_PROFILE_STORE_OK && record.payload_length == WIDE);
    CHECK(record.payload_digest == noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, payload, WIDE));
    CHECK(noah_profile_store_validate_slot(&rebooted, NOAH_PROFILE_SLOT_A, true, &record) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_read_span(&rebooted, payload_start + 65520u, span, sizeof(span)));
    CHECK(memcmp(span, &payload[65520u], sizeof(span)) == 0);

    // A write past the declared length, or one that skips ahead past 64 KiB,
    // is refused rather than wrapped.
    CHECK(noah_profile_store_prepare_begin(&rebooted, &(noah_profile_store_candidate_t){0}) == NOAH_PROFILE_STORE_INVALID_ARGUMENT);
    noah_profile_store_candidate_t next = candidate_for(payload, WIDE, 2u, 1u);
    CHECK(noah_profile_store_prepare_begin(&rebooted, &next) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_write(&rebooted, 65536u, payload, 4u) == NOAH_PROFILE_STORE_CHUNK_OUT_OF_ORDER);
    CHECK(noah_profile_store_prepare_abort(&rebooted) == NOAH_PROFILE_STORE_OK);
}

int main(void) {
    test_payload_over_64_kib_round_trips();
    test_read_span_reads_within_the_io_bound();
    test_retired_store_headers_never_become_authority();
    test_pd_store_accepts_every_settings_version_the_validator_does();
    test_pd_store_accepts_every_combo_version_the_validator_does();
    test_pd_store_accepts_only_32_slot_rgb_and_pd_versions();
    test_pd_header_all_identity_bits_and_boot_paths();
    test_previous_layout_store_header_is_refused();
    test_pd_header_rejects_bad_contracts_before_writing();
    test_pd_header_power_loss_keeps_complete_generation();
    test_checksums();
    test_crc32_matches_the_bitwise_crc();
    test_commit_and_boot_selection();
    test_header_reserved_bytes_are_zero();
    test_chunk_and_candidate_guards();
    test_commit_state_machine_is_scan_bounded();
    test_payload_readback_reads_a_page_a_step();
    test_durable_prepare_is_not_authority_until_decided();
    test_durable_prepare_can_be_aborted_before_decision();
    test_destructive_reuse_guard_brackets_every_prepare();
    test_payload_and_header_validation();
    test_checksum_mismatch_never_commits();
    test_final_marker_io_failure_is_durability_unknown();
    test_power_loss_preserves_last_known_good();
    test_first_commit_power_loss_falls_back_to_compiled_defaults();
    test_generation_conflict_and_rollover();
    test_equal_generation_metadata_divergence_conflicts();
    test_io_failures_are_not_treated_as_empty();
    test_boot_rejects_old_compiled_default_identity();
    test_boot_selection_supports_read_only_discovery();
    test_boot_selection_state_machine_is_scan_bounded();
    puts("profile store host tests passed");
    return 0;
}
