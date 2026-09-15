// ─────────────────────────────────────────────────────────────────────────────
// Canonical Live-Profile Blob And Semantic Actions — Profile Wire v1.0
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stddef.h>
#include <stdint.h>

enum {
    NOAH_PROFILE_BLOB_V1_HEADER_SIZE                 = 8u,
    NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE          = 4u,
    NOAH_PROFILE_BLOB_V1_ACTION_SIZE                 = 4u,
    NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR                = 1u,
    NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR                = 0u,
    NOAH_PROFILE_BLOB_V1_CANONICAL_FLAG              = 1u,
    NOAH_PROFILE_BLOB_V1_KNOWN_FLAGS                 = 1u,
    NOAH_PROFILE_BLOB_V1_MAX_SIZE                    = 4064u,
    NOAH_PROFILE_BLOB_V1_MAX_DOMAINS                 = 4u,
    NOAH_PROFILE_DOMAIN_V1_SETTINGS                  = 0x40u,
    NOAH_PROFILE_DOMAIN_V1_COMBOS                    = 0x30u,
    NOAH_PROFILE_DOMAIN_V1_COMBOS_VERSION            = 1u,
    NOAH_PROFILE_DOMAIN_V1_RGB                       = 0x10u,
    NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS             = 0x20u,
    NOAH_PROFILE_DOMAIN_V1_RGB_VERSION               = 1u,
    NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIOR_VERSION      = 1u,
    NOAH_PROFILE_ACTION_V1_MAX_LOGICAL_LAYERS        = 8u,
    NOAH_PROFILE_ACTION_V1_MAX_PD_MODES              = 6u,
    NOAH_PROFILE_ACTION_V1_MAX_VIA_MACRO_SLOTS       = 64u,
    NOAH_PROFILE_ACTION_V1_MAX_HARDCODED_MACRO_SLOTS = 16u,
};

typedef enum {
    NOAH_PROFILE_ACTION_V1_NONE              = 0u,
    NOAH_PROFILE_ACTION_V1_QMK_KEYCODE       = 1u,
    NOAH_PROFILE_ACTION_V1_LAYER_MOMENTARY   = 2u,
    NOAH_PROFILE_ACTION_V1_LAYER_LOCK        = 3u,
    NOAH_PROFILE_ACTION_V1_PD_MODE_MOMENTARY = 4u,
    NOAH_PROFILE_ACTION_V1_PD_MODE_LOCK      = 5u,
    NOAH_PROFILE_ACTION_V1_VIA_MACRO         = 6u,
    NOAH_PROFILE_ACTION_V1_HARDCODED_MACRO   = 7u,
} noah_profile_action_v1_kind_t;

typedef enum {
    NOAH_PROFILE_CODEC_V1_OK = 0u,
    NOAH_PROFILE_CODEC_V1_INVALID_ARGUMENT,
    NOAH_PROFILE_CODEC_V1_TRUNCATED,
    NOAH_PROFILE_CODEC_V1_CAPACITY_EXCEEDED,
    NOAH_PROFILE_CODEC_V1_OUTPUT_TOO_SMALL,
    NOAH_PROFILE_CODEC_V1_INVALID_MAGIC,
    NOAH_PROFILE_CODEC_V1_INCOMPATIBLE_SCHEMA,
    NOAH_PROFILE_CODEC_V1_RESERVED_FLAGS,
    NOAH_PROFILE_CODEC_V1_NONCANONICAL,
    NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN,
    NOAH_PROFILE_CODEC_V1_UNKNOWN_DOMAIN_VERSION,
    NOAH_PROFILE_CODEC_V1_DUPLICATE_DOMAIN,
    NOAH_PROFILE_CODEC_V1_DOMAIN_ORDER,
    NOAH_PROFILE_CODEC_V1_TRAILING_BYTES,
    NOAH_PROFILE_CODEC_V1_INVALID_LENGTH,
    NOAH_PROFILE_CODEC_V1_UNKNOWN_ACTION_KIND,
    NOAH_PROFILE_CODEC_V1_INVALID_OPERAND,
    NOAH_PROFILE_CODEC_V1_READ_ERROR,
    NOAH_PROFILE_CODEC_V1_RESERVED_FIELDS,
    NOAH_PROFILE_CODEC_V1_DUPLICATE_TARGET,
    NOAH_PROFILE_CODEC_V1_ROW_ORDER,
    NOAH_PROFILE_CODEC_V1_INVALID_TARGET,
    NOAH_PROFILE_CODEC_V1_DUPLICATE_STEP,
    NOAH_PROFILE_CODEC_V1_STEP_ORDER,
    NOAH_PROFILE_CODEC_V1_INVALID_TAP_INDEX,
    NOAH_PROFILE_CODEC_V1_EMPTY_STEP,
    NOAH_PROFILE_CODEC_V1_INVALID_ACTION,
    NOAH_PROFILE_CODEC_V1_INVALID_HOLD_MODE,
    NOAH_PROFILE_CODEC_V1_INVALID_REPEAT_RATE,
    NOAH_PROFILE_CODEC_V1_ROW_LENGTH,
    NOAH_PROFILE_CODEC_V1_COUNT_MISMATCH,
    NOAH_PROFILE_CODEC_V1_WRONG_DOMAIN,
} noah_profile_codec_v1_result_t;

typedef struct {
    noah_profile_codec_v1_result_t code;
    size_t                         offset;
    uint8_t                        domain_index;
    uint8_t                        domain_id;
    uint8_t                        table_id;
    uint8_t                        row_index;
    uint8_t                        step_index;
    uint8_t                        field_id;
} noah_profile_codec_v1_error_t;

typedef struct {
    uint8_t        id;
    uint8_t        version;
    const uint8_t *payload;
    size_t         payload_length;
} noah_profile_domain_v1_t;

typedef struct {
    uint8_t                  schema_major;
    uint8_t                  schema_minor;
    uint8_t                  flags;
    uint8_t                  domain_count;
    size_t                   byte_length;
    uint32_t                 digest;
    uint32_t                 crc32;
    noah_profile_domain_v1_t domains[NOAH_PROFILE_BLOB_V1_MAX_DOMAINS];
} noah_profile_blob_v1_t;

typedef struct {
    uint8_t  kind;
    uint8_t  flags;
    uint16_t operand;
} noah_profile_action_v1_t;

// Counts, not largest ids. A zero count makes the corresponding action kind
// unrepresentable. QMK keycodes remain a complete u16 space here; their exact
// support is checked by the separate action-ABI compatibility layer.
typedef struct {
    uint32_t max_logical_layers;
    uint32_t max_pd_modes;
    uint32_t max_via_macro_slots;
    uint32_t max_hardcoded_macro_slots;
} noah_profile_action_v1_limits_t;

noah_profile_action_v1_limits_t noah_profile_action_v1_default_limits(void);

noah_profile_codec_v1_result_t noah_profile_blob_v1_encode(const noah_profile_domain_v1_t *domains, size_t domain_count, uint8_t *output, size_t output_capacity, size_t *written, noah_profile_codec_v1_error_t *error);
noah_profile_codec_v1_result_t noah_profile_blob_v1_decode(const uint8_t *bytes, size_t length, noah_profile_blob_v1_t *blob, noah_profile_codec_v1_error_t *error);

noah_profile_codec_v1_result_t noah_profile_domain_v1_encode(const noah_profile_domain_v1_t *domain, uint8_t *output, size_t output_capacity, size_t *written, noah_profile_codec_v1_error_t *error);
noah_profile_codec_v1_result_t noah_profile_domain_v1_decode(const uint8_t *bytes, size_t length, noah_profile_domain_v1_t *domain, noah_profile_codec_v1_error_t *error);
noah_profile_codec_v1_result_t noah_profile_domain_v1_read(const uint8_t *bytes, size_t length, size_t offset, noah_profile_domain_v1_t *domain, size_t *next_offset, noah_profile_codec_v1_error_t *error);

noah_profile_codec_v1_result_t noah_profile_action_v1_encode(const noah_profile_action_v1_t *action, const noah_profile_action_v1_limits_t *limits, uint8_t output[NOAH_PROFILE_BLOB_V1_ACTION_SIZE], noah_profile_codec_v1_error_t *error);
noah_profile_codec_v1_result_t noah_profile_action_v1_decode(const uint8_t *bytes, size_t length, const noah_profile_action_v1_limits_t *limits, noah_profile_action_v1_t *action, noah_profile_codec_v1_error_t *error);
noah_profile_codec_v1_result_t noah_profile_action_v1_read(const uint8_t *bytes, size_t length, size_t offset, const noah_profile_action_v1_limits_t *limits, noah_profile_action_v1_t *action, size_t *next_offset, noah_profile_codec_v1_error_t *error);
