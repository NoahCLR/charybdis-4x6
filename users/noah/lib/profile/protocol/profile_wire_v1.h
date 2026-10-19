// ───────────────────────────────────────────────────────────────────────────
// Live Profile Wire V1 Read Surface
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    NOAH_PROFILE_WIRE_V1_REPORT_SIZE      = 32u,
    NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE     = 25u,
    NOAH_PROFILE_WIRE_V1_COMMAND_GET      = 0x08u,
    // The firmware answers every Raw HID report itself (D-F31). A report no
    // Profile Wire handler takes comes back with this command byte.
    NOAH_PROFILE_WIRE_V1_COMMAND_UNHANDLED = 0xFFu,
    NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL   = 0x00u,
    NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY = 0x01u,
    NOAH_PROFILE_WIRE_V1_VALUE_STATUS     = 0x02u,
    // 0x03 is the performance cadence recorder, present only in diagnostic
    // builds; see users/noah/lib/state/diagnostics/runtime_diag.h.
    NOAH_PROFILE_WIRE_V1_VALUE_PAYLOAD = 0x04u,
    // The compiled defaults the firmware was built with, served through the
    // same page layout. A keyboard with nothing committed is still running
    // something, and this is it.
    NOAH_PROFILE_WIRE_V1_VALUE_COMPILED = 0x05u,
    NOAH_PROFILE_WIRE_V1_VALUE_COMBOS   = 0x06u,
    // Page 0 of a payload read is metadata; pages 1..N carry raw payload
    // bytes, a full report payload each.
    NOAH_PROFILE_WIRE_V1_PAYLOAD_METADATA_PAGE = 0u,
    // Profile Wire 2.0 opens the new base (D-F28): 32-bit profile lengths
    // and offsets everywhere, a feature word of build differences only.
    NOAH_PROFILE_WIRE_V1_PROTOCOL_MAJOR = 2u,
    NOAH_PROFILE_WIRE_V1_PROTOCOL_MINOR = 0u,
    // Capability layout 3 (D-F28): page 1 the byte-wide ceilings, page 2 the
    // wider ones, payload capacities 32 bits.
    NOAH_PROFILE_WIRE_V1_CAPABILITY_LAYOUT     = 3u,
    NOAH_PROFILE_WIRE_V1_CAPABILITY_PAGES      = 3u,
    NOAH_PROFILE_WIRE_V1_STATUS_PAGES          = 2u,
};

typedef enum {
    NOAH_PROFILE_WIRE_V1_STATUS_OK           = 0u,
    NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED    = 1u,
    NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE = 2u,
    NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE  = 3u,
} noah_profile_wire_v1_status_code_t;

// The feature word holds only what a build genuinely varies by (D-F28).
// Everything this base does is implied by its protocol and schema versions;
// what each bit of the earlier word meant is recorded in profile-wire-v1.md.
// No firmware behaviour depends on a bit: the word only describes the build.
enum {
    // One half of a split keyboard: a save copies the profile to the other
    // half, and status reports the peer.
    NOAH_PROFILE_FEATURE_SPLIT_KEYBOARD = 1u << 0,
    // The build saves profiles: candidate writes (with reuse and streaming
    // chunks), durable commit and activation. Set once the live-profile
    // owner reports; the candidate chunk size is zero exactly without it.
    NOAH_PROFILE_FEATURE_SAVES = 1u << 1,
    // The build has a pointing device, so pointing keys and the pointing
    // domain mean something on it.
    NOAH_PROFILE_FEATURE_POINTING = 1u << 2,
    NOAH_PROFILE_FEATURES_KNOWN = NOAH_PROFILE_FEATURE_SPLIT_KEYBOARD | NOAH_PROFILE_FEATURE_SAVES | NOAH_PROFILE_FEATURE_POINTING,
};

// A wide-page request: [command, channel, value, request id, page low, page
// high], then 26 reserved zero bytes. The response echoes bytes 0..4 only,
// since byte 5 carries its status; the request id correlates it.
enum { NOAH_PROFILE_WIRE_V1_WIDE_REQUEST_FIXED = 6u };
static inline uint16_t noah_profile_wire_v1_wide_page(const uint8_t *frame) {
    return (uint16_t)(frame[4] | ((uint16_t)frame[5] << 8u));
}

enum {
    NOAH_PROFILE_STATE_ACTIVE_IS_COMPILED_DEFAULT = 1u << 0,
    NOAH_PROFILE_STATE_COMMITTED_VALID            = 1u << 1,
    NOAH_PROFILE_STATE_CANDIDATE_PENDING          = 1u << 2,
    NOAH_PROFILE_STATE_PREVIEW_ACTIVE             = 1u << 3,
    NOAH_PROFILE_STATE_PEER_KNOWN                 = 1u << 4,
    NOAH_PROFILE_STATE_PEER_CONVERGED             = 1u << 5,
    NOAH_PROFILE_STATE_WAITING_SAFE_BOUNDARY      = 1u << 6,
    NOAH_PROFILE_STATE_DIGESTS_UNAVAILABLE        = 1u << 7,
    // A cancelled save's peer ABORT was never acknowledged; see
    // NOAH_PROFILE_SPLIT_PREPARED_ABORT_TIMEOUT_MS. No new save starts until
    // the peer confirms, and a peer that never does needs a restart.
    NOAH_PROFILE_STATE_PEER_CLEANUP_PENDING = 1u << 8,
    // A slot holds a profile in an earlier store format. It is not read or
    // converted (D-F28), so the keyboard runs its compiled defaults until a
    // save; the flag clears once a profile is committed.
    NOAH_PROFILE_STATE_STORED_EARLIER_FORMAT = 1u << 9,
};

typedef enum {
    NOAH_PROFILE_ACTIVE_COMPILED_ONLY = 0u,
    NOAH_PROFILE_ACTIVE_COMMITTED     = 1u,
    NOAH_PROFILE_ACTIVE_PREVIEW       = 2u,
    NOAH_PROFILE_ACTIVE_PENDING       = 3u,
} noah_profile_active_kind_t;

typedef struct {
    uint8_t  protocol_major;
    uint8_t  protocol_minor;
    uint8_t  schema_major;
    uint8_t  schema_minor;
    uint8_t  candidate_chunk_max;
    uint32_t feature_flags;
    uint32_t action_abi_digest;
    uint32_t firmware_version;
    uint32_t compiled_default_digest;
    uint8_t  compiled_layer_count;
    uint8_t  max_logical_layers;
    uint8_t  max_behavior_rows;
    uint8_t  max_tap_steps_per_behavior;
    uint16_t max_populated_behavior_steps;
    uint8_t  max_combos;
    uint8_t  max_keys_per_combo;
    uint8_t  max_reusable_rgb_groups;
    uint8_t  max_rgb_stage_group_rows;
    uint8_t  physical_led_count;
    uint8_t  led_bitmap_size;
    uint8_t  custom_key_slots;
    uint8_t  macro_slots;
    uint32_t max_profile_payload;
    uint32_t profile_slot_payload;
    uint32_t profile_slot_size;
    uint32_t macro_bytes;
    uint8_t  supported_domain_mask;
    uint8_t  max_name_bytes;
    uint8_t  layer_mask_bits;
    uint8_t  placement_positions;
} noah_profile_wire_v1_capabilities_t;

typedef struct {
    uint16_t state_flags;
    uint32_t source_digest;
    uint32_t compiled_default_digest;
    uint32_t active_digest;
    uint32_t pending_digest;
    uint32_t committed_digest;
    uint8_t  active_kind;
    uint32_t active_generation;
    uint8_t  active_origin_half;
    uint32_t committed_generation;
    uint8_t  committed_origin_half;
    uint32_t peer_generation;
    uint8_t  peer_origin_half;
    uint16_t candidate_transaction_id;
    uint16_t last_committed_transaction_id;
    uint16_t conflict_count;
    uint8_t  validation_state;
    uint8_t  last_error;
} noah_profile_wire_v1_device_status_t;

typedef struct {
    noah_profile_wire_v1_capabilities_t  capabilities;
    noah_profile_wire_v1_device_status_t status;
} noah_profile_wire_v1_read_service_t;

// Handles only Profile Wire v1 custom-get requests. The request header is:
// [command, channel, value, nonzero request id, page], followed by zeroed
// reserved bytes. A handled response preserves those five correlation bytes,
// then returns [status, payload length, payload...].
bool noah_profile_wire_v1_handle_get(const noah_profile_wire_v1_read_service_t *service, uint8_t *frame, uint8_t length);
