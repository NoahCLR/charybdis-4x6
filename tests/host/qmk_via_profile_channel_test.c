#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "via.h"

#include "users/noah/lib/profile/protocol/profile_wire_v1.h"
#include "users/noah/lib/profile/storage/profile_storage_layout.h"
#include "users/noah/lib/profile/storage/profile_store_runtime.h"

void via_custom_value_command_kb(uint8_t *data, uint8_t length);

enum {
    WIRE_PAYLOAD = 7u,
};

static noah_profile_store_runtime_state_t store_runtime_state;
static noah_profile_store_record_t        store_runtime_record;
static bool                               store_runtime_has_committed;

noah_profile_store_runtime_state_t noah_profile_store_runtime_state(void) {
    return store_runtime_state;
}

const noah_profile_store_record_t *noah_profile_store_runtime_committed(void) {
    return store_runtime_has_committed ? &store_runtime_record : NULL;
}

static uint16_t read_u16(const uint8_t *source) {
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8u);
}

static uint32_t read_u32(const uint8_t *source) {
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8u) | ((uint32_t)source[2] << 16u) | ((uint32_t)source[3] << 24u);
}

static void make_request(uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE], uint8_t value, uint8_t request_id, uint8_t page) {
    memset(frame, 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE);
    frame[0] = id_custom_get_value;
    frame[1] = id_custom_channel;
    frame[2] = value;
    frame[3] = request_id;
    frame[4] = page;
}

static uint32_t expected_feature_flags(void) {
    uint32_t flags = NOAH_PROFILE_FEATURE_READ_SURFACE | NOAH_PROFILE_FEATURE_STORAGE_LAYOUT;
#ifdef SPLIT_KEYBOARD
    flags |= NOAH_PROFILE_FEATURE_SPLIT_KEYBOARD;
#endif
    return flags;
}

static void test_capability_hook(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY, 0x31u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[0] == id_custom_get_value);
    assert(frame[1] == id_custom_channel);
    assert(frame[2] == NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY);
    assert(frame[3] == 0x31u);
    assert(frame[4] == 0u);
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(frame[6] == NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE);
    assert(frame[WIRE_PAYLOAD + 6u] == NOAH_PROFILE_WIRE_V1_REPORT_SIZE);
    assert(frame[WIRE_PAYLOAD + 7u] == 0u);
    assert(read_u32(&frame[WIRE_PAYLOAD + 9u]) == expected_feature_flags());
    assert((read_u32(&frame[WIRE_PAYLOAD + 9u]) & (NOAH_PROFILE_FEATURE_RGB_SCHEMA | NOAH_PROFILE_FEATURE_KEY_BEHAVIOR_SCHEMA | NOAH_PROFILE_FEATURE_CANDIDATE_WRITE | NOAH_PROFILE_FEATURE_PERSISTENT_COMMIT)) == 0u);
    assert(read_u32(&frame[WIRE_PAYLOAD + 17u]) == VIA_FIRMWARE_VERSION);

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY, 0x32u, 1u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(frame[WIRE_PAYLOAD] == DYNAMIC_KEYMAP_LAYER_COUNT);
#ifdef RGB_MATRIX_ENABLE
    assert(frame[WIRE_PAYLOAD + 9u] == RGB_MATRIX_LED_COUNT);
#else
    assert(frame[WIRE_PAYLOAD + 9u] == 0u);
#endif
    assert(frame[WIRE_PAYLOAD + 21u] == 0u);
}

static void test_status_surfaces_read_only_store_discovery(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    store_runtime_state         = NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING;
    store_runtime_has_committed = false;
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x41u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(read_u16(&frame[WIRE_PAYLOAD + 2u]) == (NOAH_PROFILE_STATE_ACTIVE_IS_COMPILED_DEFAULT | NOAH_PROFILE_STATE_DIGESTS_UNAVAILABLE));
    assert(read_u32(&frame[WIRE_PAYLOAD + 20u]) == 0u);
    assert(frame[WIRE_PAYLOAD + 24u] == NOAH_PROFILE_ACTIVE_COMPILED_ONLY);

    store_runtime_state         = NOAH_PROFILE_STORE_RUNTIME_COMMITTED_FOUND;
    store_runtime_has_committed = true;
    store_runtime_record        = (noah_profile_store_record_t){
        .payload_digest = UINT32_C(0x78563412),
        .generation     = UINT32_C(0x01020304),
        .origin_half    = 1u,
    };
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x42u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(read_u16(&frame[WIRE_PAYLOAD + 2u]) == (NOAH_PROFILE_STATE_ACTIVE_IS_COMPILED_DEFAULT | NOAH_PROFILE_STATE_COMMITTED_VALID | NOAH_PROFILE_STATE_DIGESTS_UNAVAILABLE));
    assert(read_u32(&frame[WIRE_PAYLOAD + 20u]) == UINT32_C(0x78563412));
    assert(frame[WIRE_PAYLOAD + 24u] == NOAH_PROFILE_ACTIVE_COMPILED_ONLY);

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x43u, 1u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(read_u32(&frame[WIRE_PAYLOAD + 5u]) == UINT32_C(0x01020304));
    assert(frame[WIRE_PAYLOAD + 9u] == 1u);

    store_runtime_state         = NOAH_PROFILE_STORE_RUNTIME_GENERATION_CONFLICT;
    store_runtime_has_committed = false;
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x44u, 1u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(read_u16(&frame[WIRE_PAYLOAD + 19u]) == 1u);
    assert(read_u32(&frame[WIRE_PAYLOAD + 5u]) == 0u);
}

static void test_hook_rejects_unsupported_or_malformed_commands(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY, 0u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[0] == id_custom_get_value);
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED);

    make_request(frame, 0x7Fu, 1u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[0] == id_unhandled);

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY, 1u, 0u);
    via_custom_value_command_kb(frame, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - 1u);
    assert(frame[0] == id_unhandled);

    memset(frame, 0, sizeof(frame));
    frame[0] = id_custom_set_value;
    frame[1] = id_custom_channel;
    frame[2] = NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY;
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[0] == id_unhandled);

    via_custom_value_command_kb(NULL, 0u);
}

int main(void) {
    test_capability_hook();
    test_status_surfaces_read_only_store_discovery();
    test_hook_rejects_unsupported_or_malformed_commands();
    puts("qmk VIA Profile Wire channel tests passed");
    return 0;
}
