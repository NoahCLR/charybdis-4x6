#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lib/profile/schema/profile_validator_v1.h"
#include "lib/profile/runtime/effective_pd_runtime.h"
#include "lib/profile/storage/profile_checksum.h"
#include "lib/profile/protocol/profile_candidate_v1.h"

static void test_candidate_capacity(void) {
    uint8_t frame[32] = {NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET, NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL, NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN, 1, 0, 2, 0, 31};
    noah_profile_candidate_v1_command_t command;
    noah_profile_candidate_v1_frame_error_t error;
    // Schema 2 advertises 5,088 bytes: admission must accept the whole range,
    // including profiles above the old 4,064-byte ceiling.
    const uint16_t lengths[] = {4064, 4065, 5088, 5089};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        frame[9] = (uint8_t)lengths[i];
        frame[10] = lengths[i] >> 8;
        assert(noah_profile_candidate_v1_decode(frame, sizeof(frame), &command, &error) ==
               (lengths[i] <= NOAH_PROFILE_PAYLOAD_MAX ? NOAH_PROFILE_CANDIDATE_V1_DECODE_OK : NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED));
    }
    memset(frame + 5, 0, sizeof(frame) - 5);
    frame[2] = NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK;
    frame[5] = (uint8_t)5087;
    frame[6] = 5087 >> 8;
    frame[7] = 1;
    assert(noah_profile_candidate_v1_decode(frame, sizeof(frame), &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_OK);
    frame[7] = 2;
    assert(noah_profile_candidate_v1_decode(frame, sizeof(frame), &command, &error) == NOAH_PROFILE_CANDIDATE_V1_DECODE_MALFORMED);
}

static uint8_t blob[788];
static size_t calls, largest, failed_offset = SIZE_MAX;
static bool read_bytes(void *context, size_t offset, uint8_t *target, size_t length) {
    (void)context;
    calls++;
    if (length > largest) largest = length;
    if ((offset <= failed_offset && failed_offset - offset < length) || offset > sizeof(blob) || length > sizeof(blob) - offset) return false;
    memcpy(target, blob + offset, length);
    return true;
}
static noah_profile_reader_t reader = {.read = read_bytes, .length = sizeof(blob)};
static noah_profile_validator_v1_profile_t profile;

static noah_profile_validator_v1_result_t validate(void) {
    noah_profile_validator_v1_t validator;
    noah_profile_validator_v1_declaration_t declaration = {
        .schema_major = 2, .domain_mask = 16, .byte_length = sizeof(blob), .action_abi_digest = 42,
        .crc32 = noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, blob, sizeof(blob))),
        .digest = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, blob, sizeof(blob)),
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
        assert(++steps < 200);
    }
    if (result == NOAH_PROFILE_VALIDATOR_V1_VALID)
        assert(noah_profile_validator_v1_profile(&validator, &profile, NULL) == result);
    return result;
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
    noah_effective_pd_load_compiled_defaults(factory);
    assert(noah_effective_pd_ready());
    assert(noah_effective_pd_for_mask(1)[34] == 60);
    assert(!noah_effective_pd_for_mask(2));
    factory[0].threshold_y = 0;
    noah_effective_pd_load_compiled_defaults(factory);
    assert(!noah_effective_pd_ready());
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    memcpy(blob, (uint8_t[]){'N','L','P','1',2,0,1,1,0x50,1,8,3}, 12);
    assert(fread(blob + 12, 1, 776, file) == 776);
    fclose(file);
    assert(validate() == NOAH_PROFILE_VALIDATOR_V1_VALID);
    assert(profile.pd.offset == 12 && profile.pd.length == 776 && profile.domain_mask == 16);
    noah_effective_profile_snapshot_t snapshot = {.profile = profile, .reader = reader};
    noah_effective_profile_identity_t identity = {0};
    noah_effective_pd_invalidate(NULL, 0, identity, identity, &snapshot);
    assert(noah_effective_pd_ready());
    assert(noah_effective_pd_for_mask(1)[1] == 2);
    assert(noah_effective_pd_for_mask(16)[3] == 2);
    assert(!noah_effective_pd_for_mask(128));
    assert(!noah_effective_pd_for_mask(3));
    assert(!noah_effective_pd_record(8));
    // Every byte of the new domain is rejected on read failure; no stale or
    // partly copied settings become available after a failed publication.
    for (failed_offset = 12; failed_offset < sizeof(blob); failed_offset++) {
        noah_effective_pd_invalidate(NULL, 1, identity, identity, &snapshot);
        assert(!noah_effective_pd_ready());
    }
    failed_offset = SIZE_MAX;
    noah_effective_pd_invalidate(NULL, 2, identity, identity, &snapshot);
    assert(noah_effective_pd_ready());
    for (uint8_t slot = 0; slot < 8; slot++) {
        size_t offset = 20 + (size_t)slot * 96 + 90;
        blob[offset] = 1;
        assert(validate() == NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
        noah_effective_pd_invalidate(NULL, 3, identity, identity, &snapshot);
        assert(!noah_effective_pd_ready());
        blob[offset] = 0;
    }
    blob[12] = 2;
    assert(validate() == NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN);
    puts("configured PD profile validation and cache tests passed");
}
