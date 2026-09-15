#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/protocol/profile_wire_v1.h"

static const noah_profile_wire_v1_read_service_t service = {
    .capabilities =
        {
            .protocol_major               = 1u,
            .schema_major                 = 1u,
            .candidate_chunk_max          = 0u,
            .feature_flags                = NOAH_PROFILE_FEATURE_READ_SURFACE | NOAH_PROFILE_FEATURE_STORAGE_LAYOUT | NOAH_PROFILE_FEATURE_RGB_SCHEMA | NOAH_PROFILE_FEATURE_KEY_BEHAVIOR_SCHEMA | NOAH_PROFILE_FEATURE_SPLIT_KEYBOARD | NOAH_PROFILE_FEATURE_ACTION_ABI_DIGEST | NOAH_PROFILE_FEATURE_COMPILED_PROFILE_HASH,
            .action_abi_digest            = UINT32_C(0x12345678),
            .firmware_version             = UINT32_C(0x00010000),
            .compiled_default_digest      = UINT32_C(0x89ABCDEF),
            .compiled_layer_count         = 5u,
            .max_logical_layers           = 8u,
            .max_behavior_rows            = 64u,
            .max_tap_steps_per_behavior   = 5u,
            .max_populated_behavior_steps = 128u,
            .max_combos                   = 32u,
            .max_keys_per_combo           = 4u,
            .max_reusable_rgb_groups      = 16u,
            .max_rgb_stage_group_rows     = 32u,
            .physical_led_count           = 58u,
            .led_bitmap_size              = 8u,
            .hardcoded_macro_slots        = 16u,
            .via_macro_slots              = 64u,
            .max_profile_payload          = 4064u,
            .profile_slot_payload         = 4064u,
            .profile_slot_size            = 4096u,
            .via_macro_bytes              = 7551u,
            .supported_domain_mask        = 3u,
        },
    .status =
        {
            .state_flags                   = UINT16_C(0x00FF),
            .source_digest                 = UINT32_C(0x01020304),
            .compiled_default_digest       = UINT32_C(0x11121314),
            .active_digest                 = UINT32_C(0x21222324),
            .pending_digest                = UINT32_C(0x31323334),
            .committed_digest              = UINT32_C(0x41424344),
            .active_kind                   = NOAH_PROFILE_ACTIVE_PREVIEW,
            .active_generation             = UINT32_C(0x51525354),
            .active_origin_half            = 1u,
            .committed_generation          = UINT32_C(0x61626364),
            .committed_origin_half         = 0u,
            .peer_generation               = UINT32_C(0x71727374),
            .peer_origin_half              = 1u,
            .candidate_transaction_id      = UINT16_C(0x8182),
            .last_committed_transaction_id = UINT16_C(0x9192),
            .conflict_count                = UINT16_C(0xA1A2),
            .validation_state              = 0xB1u,
            .last_error                    = 0xC1u,
        },
};

static const char *fixture_path;

static uint8_t hex_nibble(char value) {
    if (value >= '0' && value <= '9') {
        return (uint8_t)(value - '0');
    }
    value = (char)tolower((unsigned char)value);
    assert(value >= 'a' && value <= 'f');
    return (uint8_t)(value - 'a' + 10);
}

static void load_fixture(const char *label, uint8_t expected[NOAH_PROFILE_WIRE_V1_REPORT_SIZE]) {
    FILE  *fixture = fopen(fixture_path, "r");
    char   line[192];
    size_t label_length = strlen(label);
    bool   found        = false;

    assert(fixture != NULL);
    while (fgets(line, sizeof(line), fixture)) {
        if (strncmp(line, label, label_length) != 0 || line[label_length] != '=') {
            continue;
        }
        const char *hex = &line[label_length + 1u];
        for (uint8_t index = 0u; index < NOAH_PROFILE_WIRE_V1_REPORT_SIZE; index++) {
            assert(isxdigit((unsigned char)hex[index * 2u]));
            assert(isxdigit((unsigned char)hex[index * 2u + 1u]));
            expected[index] = (uint8_t)((hex_nibble(hex[index * 2u]) << 4u) | hex_nibble(hex[index * 2u + 1u]));
        }
        assert(hex[NOAH_PROFILE_WIRE_V1_REPORT_SIZE * 2u] == '\n' || hex[NOAH_PROFILE_WIRE_V1_REPORT_SIZE * 2u] == '\0');
        found = true;
        break;
    }
    fclose(fixture);
    assert(found);
}

static void assert_golden(const char *label, const uint8_t actual[NOAH_PROFILE_WIRE_V1_REPORT_SIZE]) {
    uint8_t expected[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];
    load_fixture(label, expected);
    assert(memcmp(actual, expected, sizeof(expected)) == 0);
}

static void request(uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE], uint8_t value, uint8_t request_id, uint8_t page) {
    memset(frame, 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE);
    frame[0] = NOAH_PROFILE_WIRE_V1_COMMAND_GET;
    frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    frame[2] = value;
    frame[3] = request_id;
    frame[4] = page;
}

static void assert_success(const uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE], uint8_t value, uint8_t request_id, uint8_t page) {
    assert(frame[0] == NOAH_PROFILE_WIRE_V1_COMMAND_GET);
    assert(frame[1] == NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL);
    assert(frame[2] == value);
    assert(frame[3] == request_id);
    assert(frame[4] == page);
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(frame[6] == NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE);
}

static void test_capability_pages(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    request(frame, NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY, 0x41u, 0u);
    assert_golden("capabilities-page-0-request", frame);
    assert(noah_profile_wire_v1_handle_get(&service, frame, sizeof(frame)));
    assert_golden("capabilities-page-0-response", frame);
    assert_success(frame, NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY, 0x41u, 0u);
    assert(frame[7] == 1u && frame[8] == 2u);
    assert(frame[9] == 1u && frame[11] == 1u);
    assert(frame[13] == 32u && frame[14] == 0u && frame[15] == 2u);
    assert(frame[16] == 0x1Fu && frame[17] == 0x0Cu && frame[18] == 0u && frame[19] == 0u);

    request(frame, NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY, 0x42u, 1u);
    assert_golden("capabilities-page-1-request", frame);
    assert(noah_profile_wire_v1_handle_get(&service, frame, sizeof(frame)));
    assert_golden("capabilities-page-1-response", frame);
    assert_success(frame, NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY, 0x42u, 1u);
    assert(frame[7] == 5u && frame[8] == 8u && frame[9] == 64u && frame[10] == 5u);
    assert(frame[20] == 0xE0u && frame[21] == 0x0Fu);
    assert(frame[24] == 0x00u && frame[25] == 0x10u);
    assert(frame[26] == 0x7Fu && frame[27] == 0x1Du);
    assert(frame[28] == 3u && frame[29] == 0u && frame[30] == 0u && frame[31] == 0u);
}

static void test_status_pages(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x43u, 0u);
    assert_golden("status-page-0-request", frame);
    assert(noah_profile_wire_v1_handle_get(&service, frame, sizeof(frame)));
    assert_golden("status-page-0-response", frame);
    assert_success(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x43u, 0u);
    assert(frame[7] == 1u && frame[8] == 2u);
    assert(frame[9] == 0xFFu && frame[10] == 0u);
    assert(frame[31] == NOAH_PROFILE_ACTIVE_PREVIEW);

    request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x44u, 1u);
    assert_golden("status-page-1-request", frame);
    assert(noah_profile_wire_v1_handle_get(&service, frame, sizeof(frame)));
    assert_golden("status-page-1-response", frame);
    assert_success(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x44u, 1u);
    assert(frame[11] == 1u);
    assert(frame[16] == 0u);
    assert(frame[21] == 1u);
    assert(frame[28] == 0xB1u && frame[29] == 0xC1u && frame[30] == 0u && frame[31] == 0u);
}

static void test_malformed_requests(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE + 1u];

    request(frame, NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY, 0u, 0u);
    assert(noah_profile_wire_v1_handle_get(&service, frame, NOAH_PROFILE_WIRE_V1_REPORT_SIZE));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED && frame[6] == 0u);

    request(frame, NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY, 1u, 0u);
    frame[31] = 1u;
    assert(noah_profile_wire_v1_handle_get(&service, frame, NOAH_PROFILE_WIRE_V1_REPORT_SIZE));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED && frame[31] == 0u);

    request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 1u, 2u);
    assert(noah_profile_wire_v1_handle_get(&service, frame, NOAH_PROFILE_WIRE_V1_REPORT_SIZE));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE && frame[6] == 0u);

    request(frame, 0x7Fu, 1u, 0u);
    assert(!noah_profile_wire_v1_handle_get(&service, frame, NOAH_PROFILE_WIRE_V1_REPORT_SIZE));
    request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 1u, 0u);
    frame[1] = 1u;
    assert(!noah_profile_wire_v1_handle_get(&service, frame, NOAH_PROFILE_WIRE_V1_REPORT_SIZE));
    request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 1u, 0u);
    assert(!noah_profile_wire_v1_handle_get(&service, frame, 31u));
    assert(!noah_profile_wire_v1_handle_get(&service, frame, 33u));
    assert(!noah_profile_wire_v1_handle_get(NULL, frame, NOAH_PROFILE_WIRE_V1_REPORT_SIZE));
    assert(!noah_profile_wire_v1_handle_get(&service, NULL, NOAH_PROFILE_WIRE_V1_REPORT_SIZE));
}

int main(int argc, char **argv) {
    assert(argc == 2);
    fixture_path = argv[1];
    test_capability_pages();
    test_status_pages();
    test_malformed_requests();
    puts("profile wire v1 host tests passed");
    return 0;
}
