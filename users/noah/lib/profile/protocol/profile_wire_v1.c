// ────────────────────────────────────────────────────────────────────────────
// Live Profile Wire V1 Read Surface
// ───────────────────────────────────────────────────────────────────────────

#include "profile_wire_v1.h"

#include <stddef.h>
#include <string.h>

enum {
    WIRE_COMMAND       = 0u,
    WIRE_CHANNEL       = 1u,
    WIRE_VALUE         = 2u,
    WIRE_REQUEST_ID    = 3u,
    WIRE_PAGE          = 4u,
    WIRE_STATUS        = 5u,
    WIRE_PAYLOAD_SIZE  = 6u,
    WIRE_PAYLOAD       = 7u,
    WIRE_REQUEST_FIXED = 5u,
};

static void write_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
}

static void write_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
    target[2] = (uint8_t)(value >> 16u);
    target[3] = (uint8_t)(value >> 24u);
}

static bool reserved_request_bytes_are_zero(const uint8_t *frame) {
    for (uint8_t index = WIRE_STATUS; index < NOAH_PROFILE_WIRE_V1_REPORT_SIZE; index++) {
        if (frame[index] != 0u) {
            return false;
        }
    }
    return true;
}

static void begin_response(uint8_t *frame, noah_profile_wire_v1_status_code_t status, uint8_t payload_size) {
    memset(&frame[WIRE_STATUS], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - WIRE_STATUS);
    frame[WIRE_STATUS]       = (uint8_t)status;
    frame[WIRE_PAYLOAD_SIZE] = payload_size;
}

static void encode_capabilities_page_zero(uint8_t *payload, const noah_profile_wire_v1_capabilities_t *capabilities) {
    payload[0] = 1u; // response layout version
    payload[1] = NOAH_PROFILE_WIRE_V1_CAPABILITY_PAGES;
    payload[2] = capabilities->protocol_major;
    payload[3] = capabilities->protocol_minor;
    payload[4] = capabilities->schema_major;
    payload[5] = capabilities->schema_minor;
    payload[6] = NOAH_PROFILE_WIRE_V1_REPORT_SIZE;
    payload[7] = capabilities->candidate_chunk_max;
    payload[8] = NOAH_PROFILE_WIRE_V1_STATUS_PAGES;
    write_u32(&payload[9], capabilities->feature_flags);
    write_u32(&payload[13], capabilities->action_abi_digest);
    write_u32(&payload[17], capabilities->firmware_version);
    write_u32(&payload[21], capabilities->compiled_default_digest);
}

static void encode_capabilities_page_one(uint8_t *payload, const noah_profile_wire_v1_capabilities_t *capabilities) {
    payload[0]  = capabilities->compiled_layer_count;
    payload[1]  = capabilities->max_logical_layers;
    payload[2]  = capabilities->max_behavior_rows;
    payload[3]  = capabilities->max_tap_steps_per_behavior;
    payload[4]  = capabilities->max_populated_behavior_steps;
    payload[5]  = capabilities->max_combos;
    payload[6]  = capabilities->max_keys_per_combo;
    payload[7]  = capabilities->max_reusable_rgb_groups;
    payload[8]  = capabilities->max_rgb_stage_group_rows;
    payload[9]  = capabilities->physical_led_count;
    payload[10] = capabilities->led_bitmap_size;
    payload[11] = capabilities->hardcoded_macro_slots;
    payload[12] = capabilities->via_macro_slots;
    write_u16(&payload[13], capabilities->max_profile_payload);
    write_u16(&payload[15], capabilities->profile_slot_payload);
    write_u16(&payload[17], capabilities->profile_slot_size);
    write_u16(&payload[19], capabilities->via_macro_bytes);
    payload[21] = capabilities->supported_domain_mask;
    // Bytes 22..24 remain canonical zero padding.
}

static void encode_status_page_zero(uint8_t *payload, const noah_profile_wire_v1_device_status_t *status) {
    payload[0] = 1u; // response layout version
    payload[1] = NOAH_PROFILE_WIRE_V1_STATUS_PAGES;
    write_u16(&payload[2], status->state_flags);
    write_u32(&payload[4], status->source_digest);
    write_u32(&payload[8], status->compiled_default_digest);
    write_u32(&payload[12], status->active_digest);
    write_u32(&payload[16], status->pending_digest);
    write_u32(&payload[20], status->committed_digest);
    payload[24] = status->active_kind;
}

static void encode_status_page_one(uint8_t *payload, const noah_profile_wire_v1_device_status_t *status) {
    write_u32(&payload[0], status->active_generation);
    payload[4] = status->active_origin_half;
    write_u32(&payload[5], status->committed_generation);
    payload[9] = status->committed_origin_half;
    write_u32(&payload[10], status->peer_generation);
    payload[14] = status->peer_origin_half;
    write_u16(&payload[15], status->candidate_transaction_id);
    write_u16(&payload[17], status->last_committed_transaction_id);
    write_u16(&payload[19], status->conflict_count);
    payload[21] = status->validation_state;
    payload[22] = status->last_error;
    // Bytes 23..24 remain canonical zero padding.
}

bool noah_profile_wire_v1_handle_get(const noah_profile_wire_v1_read_service_t *service, uint8_t *frame, uint8_t length) {
    uint8_t page;
    uint8_t value;

    if (!service || !frame || length != NOAH_PROFILE_WIRE_V1_REPORT_SIZE) {
        return false;
    }
    if (frame[WIRE_COMMAND] != NOAH_PROFILE_WIRE_V1_COMMAND_GET || frame[WIRE_CHANNEL] != NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL) {
        return false;
    }

    value = frame[WIRE_VALUE];
    if (value != NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY && value != NOAH_PROFILE_WIRE_V1_VALUE_STATUS) {
        return false;
    }
    if (frame[WIRE_REQUEST_ID] == 0u || !reserved_request_bytes_are_zero(frame)) {
        begin_response(frame, NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED, 0u);
        return true;
    }

    page = frame[WIRE_PAGE];
    if ((value == NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY && page >= NOAH_PROFILE_WIRE_V1_CAPABILITY_PAGES) || (value == NOAH_PROFILE_WIRE_V1_VALUE_STATUS && page >= NOAH_PROFILE_WIRE_V1_STATUS_PAGES)) {
        begin_response(frame, NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE, 0u);
        return true;
    }

    begin_response(frame, NOAH_PROFILE_WIRE_V1_STATUS_OK, NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE);
    if (value == NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY) {
        if (page == 0u) {
            encode_capabilities_page_zero(&frame[WIRE_PAYLOAD], &service->capabilities);
        } else {
            encode_capabilities_page_one(&frame[WIRE_PAYLOAD], &service->capabilities);
        }
    } else if (page == 0u) {
        encode_status_page_zero(&frame[WIRE_PAYLOAD], &service->status);
    } else {
        encode_status_page_one(&frame[WIRE_PAYLOAD], &service->status);
    }
    return true;
}

_Static_assert(WIRE_REQUEST_FIXED == WIRE_STATUS, "Profile Wire request header offsets drifted");
_Static_assert(WIRE_PAYLOAD + NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE == NOAH_PROFILE_WIRE_V1_REPORT_SIZE, "Profile Wire payload must fill one 32-byte report exactly");
