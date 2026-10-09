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
    // Capability layout 2 (D-F14) adds page 2 for fields wider than a byte.
    NOAH_PROFILE_WIRE_V1_CAPABILITY_LAYOUT     = 2u,
    NOAH_PROFILE_WIRE_V1_CAPABILITY_PAGES      = 3u,
    NOAH_PROFILE_WIRE_V1_STATUS_PAGES          = 2u,
};

typedef enum {
    NOAH_PROFILE_WIRE_V1_STATUS_OK           = 0u,
    NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED    = 1u,
    NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE = 2u,
    NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE  = 3u,
} noah_profile_wire_v1_status_code_t;

enum {
    // The firmware understands the read surface and frozen storage/schema
    // bounds. Candidate writes, activation, preview, and peer reconciliation
    // get their own bits only when those implementations actually land.
    NOAH_PROFILE_FEATURE_READ_SURFACE          = 1u << 0,
    NOAH_PROFILE_FEATURE_STORAGE_LAYOUT        = 1u << 1,
    NOAH_PROFILE_FEATURE_RGB_SCHEMA            = 1u << 2,
    NOAH_PROFILE_FEATURE_KEY_BEHAVIOR_SCHEMA   = 1u << 3,
    NOAH_PROFILE_FEATURE_SPLIT_KEYBOARD        = 1u << 4,
    NOAH_PROFILE_FEATURE_CANDIDATE_WRITE       = 1u << 5,
    NOAH_PROFILE_FEATURE_PERSISTENT_COMMIT     = 1u << 6,
    NOAH_PROFILE_FEATURE_RGB_PREVIEW           = 1u << 7,
    NOAH_PROFILE_FEATURE_RUNTIME_ACTIVATION    = 1u << 8,
    NOAH_PROFILE_FEATURE_PEER_RECONCILIATION   = 1u << 9,
    NOAH_PROFILE_FEATURE_ACTION_ABI_DIGEST     = 1u << 10,
    NOAH_PROFILE_FEATURE_COMPILED_PROFILE_HASH = 1u << 11,
    NOAH_PROFILE_FEATURE_ATOMIC_LOGICAL_APPLY  = 1u << 12,
    // Bit 13 is retired; the legacy PD readback page is unsupported.
    // TG(), TO(), TT() and OSL() act through userspace layer ownership, so a
    // host may offer them in behaviours and combos where they can run.
    NOAH_PROFILE_FEATURE_OWNED_LAYER_TOGGLES = 1u << 14,
    // Behaviours send QMK and keyboard functions (DPI_MOD, RGB Matrix, Magic,
    // QK_BOOT…) through QMK's key processing, so a host may offer them in a
    // behaviour's target, tap and hold.
    NOAH_PROFILE_FEATURE_BEHAVIOR_QMK_FUNCTIONS = 1u << 15,
    // Userspace keycodes sit in fixed blocks (pointing holds 0x7e80, locks
    // 0x7ea0, layer locks 0x7ec0, custom keys 0x7f00 since D-F14); action
    // kind 7 is a custom key and the settings domain names the custom keys.
    NOAH_PROFILE_FEATURE_CUSTOM_KEYS = 1u << 16,
    // Physical key timestamps survive QMK buffering; authored LT rows own tapping.
    NOAH_PROFILE_FEATURE_PHYSICAL_GESTURE_TIMING = 1u << 17,
    NOAH_PROFILE_FEATURE_OWNED_TAPPING           = 1u << 18,
    // Payload, compiled and settings readbacks (GET 0x04, 0x05, 0x07) take a
    // 16-bit page: request byte 4 is its low byte and byte 5 its high byte.
    NOAH_PROFILE_FEATURE_WIDE_PAGES = 1u << 19,
    // Behaviour and combo participation controls (participation-policy.md).
    NOAH_PROFILE_FEATURE_PARTICIPATION = 1u << 20,
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
    uint8_t  via_macro_slots;
    uint16_t max_profile_payload;
    uint16_t profile_slot_payload;
    uint32_t profile_slot_size;
    uint16_t via_macro_bytes;
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
// [VIA command, channel, value, nonzero request id, page], followed by zeroed
// reserved bytes. A handled response preserves those five correlation bytes,
// then returns [status, payload length, payload...].
bool noah_profile_wire_v1_handle_get(const noah_profile_wire_v1_read_service_t *service, uint8_t *frame, uint8_t length);
