#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lib/profile/schema/profile_validator_v1.h"
#include "lib/profile/runtime/effective_pd_runtime.h"
#include "lib/profile/storage/profile_checksum.h"
#include "lib/profile/protocol/profile_candidate_v1.h"

static void test_candidate_capacity(void) {
    uint8_t frame[32] = {NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET, NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL, NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN, 1, 0, NOAH_PROFILE_SCHEMA_MAJOR, 0, 31};
    frame[23]         = NOAH_PROFILE_LOGICAL_STORE_VERSION;
    frame[24]         = 6;
    frame[28]         = 42;
    noah_profile_candidate_v1_command_t command;
    noah_profile_candidate_v1_frame_error_t error;
    // Schema 3 advertises 65,504 bytes: admission must accept the whole range,
    // including profiles above the old 5,088-byte ceiling (D-F14).
    const uint16_t lengths[] = {5088, 5089, 65504, 65505, 65535};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        frame[9] = (uint8_t)lengths[i];
        frame[10] = lengths[i] >> 8;
        assert(noah_profile_candidate_v1_decode(frame, sizeof(frame), &command, &error) ==
               (lengths[i] <= NOAH_PROFILE_PAYLOAD_MAX ? NOAH_PROFILE_CANDIDATE_V1_DECODE_OK : NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED));
    }
    memset(frame + 5, 0, sizeof(frame) - 5);
    frame[2] = NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK;
    frame[5] = (uint8_t)65503u;
    frame[6] = 65503u >> 8;
    frame[7] = 1;
    assert(noah_profile_candidate_v1_decode(frame, sizeof(frame), &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_OK);
    frame[7] = 2;
    assert(noah_profile_candidate_v1_decode(frame, sizeof(frame), &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED);
}

static uint8_t blob[12 + NOAH_PROFILE_PD_V1_MAX_SIZE + 1];
static size_t blob_length;
static size_t calls, largest, failed_offset = SIZE_MAX;
static bool read_bytes(void *context, size_t offset, uint8_t *target, size_t length) {
    (void)context;
    calls++;
    if (length > largest) largest = length;
    if ((offset <= failed_offset && failed_offset - offset < length) || offset > blob_length || length > blob_length - offset) return false;
    memcpy(target, blob + offset, length);
    return true;
}
static noah_profile_reader_t reader = {.read = read_bytes};
static noah_profile_validator_v1_profile_t profile;

static noah_profile_validator_v1_result_t validate(void) {
    noah_profile_validator_v1_t validator;
    noah_profile_validator_v1_declaration_t declaration = {
        .schema_major = NOAH_PROFILE_SCHEMA_MAJOR, .domain_mask = 16, .byte_length = blob_length, .action_abi_digest = 42,
        .crc32 = noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, blob, blob_length)),
        .digest = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, blob, blob_length),
    };
    noah_profile_validator_v1_compatibility_t compatibility = noah_profile_validator_v1_default_compatibility(42);
    noah_profile_validator_v1_result_t result = noah_profile_validator_v1_begin(&validator, &reader, 0, &declaration, &compatibility, NULL);
    largest = 0;
    size_t steps = 0;
    while (result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
        size_t before = calls;
        result = noah_profile_validator_v1_step(&validator, 20, NULL);
        assert(calls - before <= 1);
        assert(largest <= 20);
        assert(++steps < 1000);
    }
    if (result == NOAH_PROFILE_VALIDATOR_V1_VALID)
        assert(noah_profile_validator_v1_profile(&validator, &profile, NULL) == result);
    return result;
}

// Frames one PD domain payload as a whole schema-2 blob.
static size_t load_domain(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file);
    size_t length = fread(blob + 12, 1, sizeof(blob) - 12, file);
    assert(feof(file));
    fclose(file);
    memcpy(blob, (uint8_t[]){'N', 'L', 'P', '1', 3, 0, 1, 1, 0x50, NOAH_PROFILE_PD_V1_VERSION, (uint8_t)length, (uint8_t)(length >> 8)}, 12);
    blob_length   = 12 + length;
    reader.length = blob_length;
    return length;
}

static void publish(noah_effective_profile_snapshot_t *snapshot) {
    noah_effective_profile_identity_t identity = {0};
    snapshot->profile = profile;
    snapshot->reader  = reader;
    noah_effective_pd_invalidate(NULL, 0, identity, identity, snapshot);
}

// Stored slots appear in the cache as stored; every omitted slot reads as
// disabled with an empty name, and none of them can be activated.
static void assert_cache_matches_domain(void) {
    uint8_t count = blob[12 + 3];
    uint8_t next  = 0;
    for (uint8_t index = 0; index <= count; index++) {
        uint8_t stored = index < count ? blob[12 + 8 + (size_t)index * NOAH_PROFILE_PD_V1_RECORD_SIZE] : NOAH_PROFILE_PD_V1_SLOT_COUNT;
        for (; next < stored; next++) {
            const uint8_t *record = noah_effective_pd_record(next);
            assert(record && record[0] == next);
            for (size_t i = 1; i < NOAH_PROFILE_PD_V1_RECORD_SIZE; i++) assert(record[i] == 0);
            assert(!noah_effective_pd_for_mask(UINT32_C(1) << next));
        }
        if (index < count) {
            assert(memcmp(noah_effective_pd_record(stored), blob + 12 + 8 + (size_t)index * NOAH_PROFILE_PD_V1_RECORD_SIZE, NOAH_PROFILE_PD_V1_RECORD_SIZE) == 0);
            next = (uint8_t)(stored + 1u);
        }
    }
    assert(!noah_effective_pd_record(NOAH_PROFILE_PD_V1_SLOT_COUNT));
}

int main(int argc, char **argv) {
    test_candidate_capacity();
    noah_pd_config_t factory[NOAH_PROFILE_PD_V1_SLOT_COUNT] = {0};
    for (uint8_t slot = 0; slot < NOAH_PROFILE_PD_V1_SLOT_COUNT; slot++) factory[slot].id = slot;
    factory[0].kind = 1;
    memcpy(factory[0].name, "Factory", sizeof("Factory"));
    factory[0].threshold_y = 60;
    factory[0].directions[2].keycode = 4;
    factory[0].directions[3].keycode = 5;
    factory[31] = factory[0];
    factory[31].id = 31;
    noah_effective_pd_load_compiled_defaults(factory);
    assert(noah_effective_pd_ready());
    assert(noah_effective_pd_for_mask(1)[34] == 60);
    assert(!noah_effective_pd_for_mask(2));
    // Slot 31 is reached through the mask's top bit.
    assert(noah_effective_pd_for_mask(UINT32_C(0x80000000)) == noah_effective_pd_record(31));
    assert(!noah_effective_pd_for_mask(UINT32_C(0x80000001)));
    factory[0].threshold_y = 0;
    noah_effective_pd_load_compiled_defaults(factory);
    assert(!noah_effective_pd_ready());
    assert(argc == 3);

    noah_effective_profile_snapshot_t snapshot = {0};
    noah_effective_profile_identity_t identity = {0};
    // The presets: seven stored records, slots 7..31 omitted.
    size_t presets_length = load_domain(argv[1]);
    assert(presets_length == 8 + 7 * NOAH_PROFILE_PD_V1_RECORD_SIZE);
    assert(validate() == NOAH_PROFILE_VALIDATOR_V1_VALID);
    noah_profile_domain_range_t pd;
    assert(noah_profile_blob_v1_find_domain(&reader, 0, reader.length, NOAH_PROFILE_DOMAIN_V1_PD, &pd) && pd.offset == 12 && pd.length == presets_length && profile.domain_mask == 16);
    publish(&snapshot);
    assert(noah_effective_pd_ready());
    assert(noah_effective_pd_for_mask(1)[1] == 2);
    assert(noah_effective_pd_for_mask(16)[3] == 2);
    assert(noah_effective_pd_for_mask(64)[3] == 1);
    assert(!noah_effective_pd_for_mask(128));
    assert(!noah_effective_pd_for_mask(3));
    assert_cache_matches_domain();
    // Every byte of the domain is rejected on read failure; no stale or
    // partly copied settings become available after a failed publication.
    for (failed_offset = 12; failed_offset < blob_length; failed_offset++) {
        noah_effective_pd_invalidate(NULL, 1, identity, identity, &snapshot);
        assert(!noah_effective_pd_ready());
    }
    failed_offset = SIZE_MAX;
    noah_effective_pd_invalidate(NULL, 2, identity, identity, &snapshot);
    assert(noah_effective_pd_ready());
    for (uint8_t slot = 0; slot < 7; slot++) {
        size_t offset = 20 + (size_t)slot * NOAH_PROFILE_PD_V1_RECORD_SIZE + 90;
        blob[offset] = 1;
        assert(validate() == NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
        noah_effective_pd_invalidate(NULL, 3, identity, identity, &snapshot);
        assert(!noah_effective_pd_ready());
        blob[offset] = 0;
    }
    // The sparse rules hold in the incremental validator and the cache alike:
    // out-of-order or repeated IDs, an ID past 31, and a stored record that is
    // disabled with no name.
    const struct { size_t offset; uint8_t value; } mutations[] = {
        {20 + 128, 0},          // slot 1's record names slot 0 again
        {20 + 2 * 128, 1},      // slot 2's record repeats slot 1
        {20 + 6 * 128, 32},     // past the last slot
        {20 + 6 * 128 + 1, 0},  // slot 6 disabled ...
    };
    for (size_t i = 0; i < sizeof(mutations) / sizeof(mutations[0]); i++) {
        uint8_t saved = blob[mutations[i].offset];
        blob[mutations[i].offset] = mutations[i].value;
        if (i == 3) memset(blob + 20 + 6 * 128 + 2, 0, 126); // ... with no name
        assert(validate() == NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
        noah_effective_pd_invalidate(NULL, 4, identity, identity, &snapshot);
        assert(!noah_effective_pd_ready());
        blob[mutations[i].offset] = saved;
        if (i == 3) load_domain(argv[1]);
    }
    assert(validate() == NOAH_PROFILE_VALIDATOR_V1_VALID);
    // A header naming more records than the payload holds.
    blob[12 + 3] = 8;
    assert(validate() == NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    blob[12 + 3] = 7;
    blob[12] = 1;
    assert(validate() == NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    blob[12] = 2;
    blob[9] = 1;
    assert(validate() == NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    blob[9] = 2;

    // Every slot stored: the largest domain still validates in bounded
    // reads and fills the whole cache.
    assert(load_domain(argv[2]) == NOAH_PROFILE_PD_V1_MAX_SIZE);
    assert(validate() == NOAH_PROFILE_VALIDATOR_V1_VALID);
    publish(&snapshot);
    assert(noah_effective_pd_ready());
    assert_cache_matches_domain();
    assert(noah_effective_pd_record(31)[8] != 0);

    // No slot stored: all 32 disabled, none activatable.
    memcpy(blob + 12, (uint8_t[]){NOAH_PROFILE_PD_V1_VERSION, 32, NOAH_PROFILE_PD_V1_RECORD_SIZE, 0, 0, 0, 0, 0}, 8);
    blob[10] = 8;
    blob[11] = 0;
    blob_length = reader.length = 20;
    assert(validate() == NOAH_PROFILE_VALIDATOR_V1_VALID);
    publish(&snapshot);
    assert(noah_effective_pd_ready());
    assert_cache_matches_domain();
    puts("configured PD profile validation and cache tests passed");
}
