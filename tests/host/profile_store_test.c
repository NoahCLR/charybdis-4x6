#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "users/noah/lib/profile/storage/profile_store.h"

#define CHECK(condition)                                                                                                                   \
    do {                                                                                                                                   \
        if (!(condition)) {                                                                                                                \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition);                                              \
            exit(1);                                                                                                                       \
        }                                                                                                                                  \
    } while (0)

enum {
    ACTION_ABI_DIGEST = UINT32_C(0x23456789),
    COMPILED_DIGEST   = UINT32_C(0x10203040),
};

typedef struct {
    uint8_t  bytes[NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE];
    uint32_t read_calls;
    uint32_t write_calls;
    uint32_t fail_write_call;
    uint16_t fail_partial_bytes;
    bool     fail_reads;
    uint16_t last_write_address;
    uint16_t last_write_length;
    uint16_t last_read_length;
} fake_eeprom_t;

static fake_eeprom_t eeprom;
static fake_eeprom_t alternate_eeprom;
static uint8_t       eeprom_snapshot[NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE];

typedef struct {
    fake_eeprom_t      *memory;
    noah_profile_slot_t slot;
    uint32_t            begin_calls;
    uint32_t            end_calls;
    bool                deny_begin;
    bool                fail_end;
    bool                saw_pristine_marker;
} reuse_guard_state_t;

static const uint8_t empty_profile[] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};
static const uint8_t rgb_profile[]   = {'N', 'L', 'P', '1', 1u, 0u, 1u, 1u, 0x10u, 1u, 3u, 0u, 1u, 2u, 3u};
static const uint8_t behavior_profile[] = {'N', 'L', 'P', '1', 1u, 0u, 1u, 1u, 0x20u, 1u, 2u, 0u, 4u, 5u};

static bool fake_read(void *context, uint16_t address, uint8_t *target, uint16_t length) {
    fake_eeprom_t *memory = context;

    memory->read_calls++;
    memory->last_read_length = length;
    if (memory->fail_reads || (uint32_t)address + length > sizeof(memory->bytes)) {
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
    return write_call == payload_calls + 2u ? 20u : 10u;
}

static bool fake_write(void *context, uint16_t address, const uint8_t *source, uint16_t length) {
    fake_eeprom_t *memory = context;
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
    return (noah_profile_store_compatibility_t){.schema_major = 1u, .schema_minor = 0u, .action_abi_digest = ACTION_ABI_DIGEST};
}

static bool reuse_begin(void *context, noah_profile_slot_t slot) {
    reuse_guard_state_t *state = context;
    uint16_t             start = slot == NOAH_PROFILE_SLOT_A ? NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR : NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR;

    state->begin_calls++;
    state->slot                = slot;
    state->saw_pristine_marker = state->memory->bytes[start + 30u] == 0xffu && state->memory->bytes[start + 31u] == 0xffu;
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

static void store_u16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8u);
}

static void reset_eeprom(fake_eeprom_t *memory) {
    memset(memory, 0xFF, sizeof(*memory));
    memory->write_calls      = 0u;
    memory->fail_write_call  = 0u;
    memory->fail_partial_bytes = 0u;
    memory->fail_reads       = false;
}

static noah_profile_store_candidate_t candidate_for(const uint8_t *payload, uint16_t length, uint32_t generation, uint8_t origin_half) {
    uint32_t crc = noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, payload, length);

    return (noah_profile_store_candidate_t){
        .schema_major            = 1u,
        .schema_minor            = 0u,
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

static noah_profile_store_result_t commit_payload(noah_profile_store_t *store, const uint8_t *payload, uint16_t length, uint32_t generation, uint8_t origin_half, noah_profile_store_record_t *record) {
    noah_profile_store_candidate_t candidate = candidate_for(payload, length, generation, origin_half);
    uint16_t                       offset    = 0u;
    noah_profile_store_result_t    result;

    result = noah_profile_store_prepare_begin(store, &candidate);
    if (result != NOAH_PROFILE_STORE_OK) {
        return result;
    }
    while (offset < length) {
        uint16_t chunk = (uint16_t)(length - offset);

        if (chunk > 5u) {
            chunk = 5u;
        }
        result = noah_profile_store_prepare_write(store, offset, &payload[offset], chunk);
        if (result != NOAH_PROFILE_STORE_OK) {
            return result;
        }
        offset = (uint16_t)(offset + chunk);
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

static void test_commit_and_boot_selection(void) {
    noah_profile_store_t        store;
    noah_profile_store_t        rebooted;
    noah_profile_store_record_t record;
    noah_profile_store_candidate_t first_candidate;
    const uint8_t              *header;
    uint32_t                    next;

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(noah_profile_store_next_generation(&store, &next));
    CHECK(next == 1u);
    CHECK(commit_payload(&store, empty_profile, sizeof(empty_profile), 1u, 0u, &record) == NOAH_PROFILE_STORE_OK);
    CHECK(record.slot == NOAH_PROFILE_SLOT_A && record.generation == 1u);
    CHECK(eeprom.last_write_address == NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + 30u);
    CHECK(eeprom.last_write_length == NOAH_PROFILE_STORAGE_COMMIT_MARKER_SIZE);
    first_candidate = candidate_for(empty_profile, sizeof(empty_profile), 1u, 0u);
    header          = &eeprom.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR];
    CHECK(header[0] == 'N' && header[1] == 'P' && header[2] == NOAH_PROFILE_STORE_FORMAT_VERSION && header[3] == 0x10u);
    CHECK(load_u16(&header[4]) == sizeof(empty_profile));
    CHECK(load_u32(&header[6]) == 1u && header[10] == 0u && header[11] == NOAH_PROFILE_STORE_FLAG_OVERRIDE);
    CHECK(load_u32(&header[12]) == first_candidate.payload_crc32);
    CHECK(load_u32(&header[16]) == first_candidate.payload_digest);
    CHECK(load_u32(&header[20]) == COMPILED_DIGEST);
    CHECK(load_u32(&header[24]) == ACTION_ABI_DIGEST);
    CHECK(load_u16(&header[28]) == noah_profile_crc16_ccitt_update(NOAH_PROFILE_CRC16_INITIAL, header, 28u));
    CHECK(header[30] == 0xA5u && header[31] == 0x5Au);

    initialize_store(&rebooted, &eeprom, NOAH_PROFILE_STORE_OK);
    CHECK(rebooted.committed.slot == NOAH_PROFILE_SLOT_A && rebooted.committed.payload_length == sizeof(empty_profile));
    CHECK(commit_payload(&rebooted, rgb_profile, sizeof(rgb_profile), 7u, 1u, &record) == NOAH_PROFILE_STORE_OK);
    CHECK(record.slot == NOAH_PROFILE_SLOT_B && record.generation == 7u && record.origin_half == 1u);

    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_OK);
    CHECK(store.committed.slot == NOAH_PROFILE_SLOT_B && store.committed.generation == 7u);
    CHECK(noah_profile_store_next_generation(&store, &next) && next == 8u);
}

static void test_chunk_and_candidate_guards(void) {
    noah_profile_store_t             store;
    noah_profile_store_candidate_t  candidate;
    uint8_t                         oversized[NOAH_PROFILE_STORE_IO_CHUNK_MAX + 1u] = {0};

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    candidate = candidate_for(empty_profile, sizeof(empty_profile), 1u, 0u);
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_PREPARE_IN_PROGRESS);
    CHECK(noah_profile_store_prepare_write(&store, 1u, empty_profile, 1u) == NOAH_PROFILE_STORE_CHUNK_OUT_OF_ORDER);
    CHECK(noah_profile_store_prepare_write(&store, 0u, oversized, sizeof(oversized)) == NOAH_PROFILE_STORE_CHUNK_TOO_LARGE);
    CHECK(noah_profile_store_prepare_commit(&store, NULL) == NOAH_PROFILE_STORE_PAYLOAD_INCOMPLETE);
    CHECK(noah_profile_store_prepare_abort(&store) == NOAH_PROFILE_STORE_OK);

    candidate.schema_major = 2u;
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_INCOMPATIBLE_SCHEMA);
    candidate.schema_major      = 1u;
    candidate.action_abi_digest = 0u;
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_INCOMPATIBLE_ACTION_ABI);
}

static void test_commit_state_machine_is_scan_bounded(void) {
    noah_profile_store_t            store;
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
        prior_reads  = eeprom.read_calls;
        prior_writes = eeprom.write_calls;
        result       = noah_profile_store_prepare_commit_step(&store, 1u, &record);
        CHECK(eeprom.read_calls + eeprom.write_calls == prior_reads + prior_writes + 1u);
        if (eeprom.read_calls != prior_reads) {
            CHECK(eeprom.last_read_length <= 1u);
        } else {
            CHECK(eeprom.last_write_length <= 1u);
        }
        CHECK(++steps < 160u);
    } while (result == NOAH_PROFILE_STORE_IN_PROGRESS);

    CHECK(result == NOAH_PROFILE_STORE_OK);
    CHECK(record.slot == NOAH_PROFILE_SLOT_A && record.generation == 1u);
    CHECK(!store.prepare_active && store.commit_phase == NOAH_PROFILE_STORE_COMMIT_IDLE);
}

static void test_destructive_reuse_guard_brackets_every_prepare(void) {
    noah_profile_store_t            store;
    noah_profile_store_candidate_t candidate;
    reuse_guard_state_t             state;
    noah_profile_store_reuse_guard_t guard;
    uint32_t                        writes_before;

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    memset(&state, 0, sizeof(state));
    state.memory = &eeprom;
    guard = (noah_profile_store_reuse_guard_t){.begin = reuse_begin, .end = reuse_end, .context = &state};
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
    noah_profile_store_t            store;
    noah_profile_store_t            rebooted;
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
    CHECK(!store.prepare_active);
    eeprom.fail_write_call = 0u;
    initialize_store(&rebooted, &eeprom, NOAH_PROFILE_STORE_OK);
    CHECK(rebooted.committed.generation == 1u);

    reset_eeprom(&eeprom);
    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    stage_commit_to_phase(&store, empty_profile, sizeof(empty_profile), NOAH_PROFILE_STORE_COMMIT_MARKER_READBACK);
    eeprom.fail_reads = true;
    CHECK(noah_profile_store_prepare_commit_step(&store, 20u, NULL) == NOAH_PROFILE_STORE_DURABILITY_UNKNOWN);
    eeprom.fail_reads = false;
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
            eeprom.write_calls       = 0u;
            eeprom.fail_write_call   = fail_call;
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
    slot_b_header[20] ^= 0x01u;
    store_u16(&slot_b_header[28], noah_profile_crc16_ccitt_update(NOAH_PROFILE_CRC16_INITIAL, slot_b_header, 28u));

    initialize_store(&store, &eeprom, NOAH_PROFILE_STORE_GENERATION_CONFLICT);
    CHECK(store.conflict);
}

static void test_io_failures_are_not_treated_as_empty(void) {
    noah_profile_store_t        store;
    noah_profile_store_record_t selected;
    noah_profile_store_candidate_t candidate;

    reset_eeprom(&eeprom);
    eeprom.fail_reads = true;
    noah_profile_store_init(&store, io_for(&eeprom), compatibility());
    CHECK(noah_profile_store_boot_select(&store, &selected) == NOAH_PROFILE_STORE_IO_ERROR);
    candidate = candidate_for(empty_profile, sizeof(empty_profile), 1u, 0u);
    CHECK(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_INVALID_ARGUMENT);
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

int main(void) {
    test_checksums();
    test_commit_and_boot_selection();
    test_chunk_and_candidate_guards();
    test_commit_state_machine_is_scan_bounded();
    test_destructive_reuse_guard_brackets_every_prepare();
    test_payload_and_header_validation();
    test_checksum_mismatch_never_commits();
    test_final_marker_io_failure_is_durability_unknown();
    test_power_loss_preserves_last_known_good();
    test_first_commit_power_loss_falls_back_to_compiled_defaults();
    test_generation_conflict_and_rollover();
    test_equal_generation_metadata_divergence_conflicts();
    test_io_failures_are_not_treated_as_empty();
    test_boot_selection_supports_read_only_discovery();
    puts("profile store host tests passed");
    return 0;
}
