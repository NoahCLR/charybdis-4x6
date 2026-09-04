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
    NOAH_PROFILE_WIRE_V1_VALUE_PAYLOAD    = 0x04u,
    // Page 0 of a payload read is metadata; pages 1..N carry raw payload
    // bytes, a full report payload each.
    NOAH_PROFILE_WIRE_V1_PAYLOAD_METADATA_PAGE = 0u,
    NOAH_PROFILE_WIRE_V1_CAPABILITY_PAGES = 2u,
    NOAH_PROFILE_WIRE_V1_STATUS_PAGES     = 2u,
};

typedef enum {
    NOAH_PROFILE_WIRE_V1_STATUS_OK             = 0u,
    NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED      = 1u,
    NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE   = 2u,
    NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE    = 3u,
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
};

enum {
    NOAH_PROFILE_STATE_ACTIVE_IS_COMPILED_DEFAULT = 1u << 0,
    NOAH_PROFILE_STATE_COMMITTED_VALID            = 1u << 1,
    NOAH_PROFILE_STATE_CANDIDATE_PENDING           = 1u << 2,
    NOAH_PROFILE_STATE_PREVIEW_ACTIVE              = 1u << 3,
    NOAH_PROFILE_STATE_PEER_KNOWN                  = 1u << 4,
    NOAH_PROFILE_STATE_PEER_CONVERGED              = 1u << 5,
    NOAH_PROFILE_STATE_WAITING_SAFE_BOUNDARY       = 1u << 6,
    NOAH_PROFILE_STATE_DIGESTS_UNAVAILABLE         = 1u << 7,
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
    uint8_t  max_populated_behavior_steps;
    uint8_t  max_combos;
    uint8_t  max_keys_per_combo;
    uint8_t  max_reusable_rgb_groups;
    uint8_t  max_rgb_stage_group_rows;
    uint8_t  physical_led_count;
    uint8_t  led_bitmap_size;
    uint8_t  hardcoded_macro_slots;
    uint8_t  via_macro_slots;
    uint16_t max_profile_payload;
    uint16_t profile_slot_payload;
    uint16_t profile_slot_size;
    uint16_t via_macro_bytes;
    uint8_t  supported_domain_mask;
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
    noah_profile_wire_v1_capabilities_t capabilities;
    noah_profile_wire_v1_device_status_t status;
} noah_profile_wire_v1_read_service_t;

// Handles only Profile Wire v1 custom-get requests. The request header is:
// [VIA command, channel, value, nonzero request id, page], followed by zeroed
// reserved bytes. A handled response preserves those five correlation bytes,
// then returns [status, payload length, payload...].
bool noah_profile_wire_v1_handle_get(const noah_profile_wire_v1_read_service_t *service, uint8_t *frame, uint8_t length);
