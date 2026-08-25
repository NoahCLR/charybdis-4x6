// ────────────────────────────────────────────────────────────────────────────
// Canonical RGB Domain — Profile Wire v1.0
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "profile_blob_v1.h"
#include "profile_reader.h"

enum {
    NOAH_PROFILE_RGB_V1_DOMAIN_ID               = NOAH_PROFILE_DOMAIN_V1_RGB,
    NOAH_PROFILE_RGB_V1_DOMAIN_VERSION          = NOAH_PROFILE_DOMAIN_V1_RGB_VERSION,
    NOAH_PROFILE_RGB_V1_FORMAT_VERSION          = 1u,
    NOAH_PROFILE_RGB_V1_HEADER_SIZE             = 16u,
    NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT      = 58u,
    NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE         = 8u,
    NOAH_PROFILE_RGB_V1_MAX_GROUPS              = 16u,
    NOAH_PROFILE_RGB_V1_MAX_LOGICAL_LAYERS      = 8u,
    NOAH_PROFILE_RGB_V1_MAX_PD_MODES            = 6u,
    NOAH_PROFILE_RGB_V1_MAX_STAGE_GROUP_ROWS    = 32u,
    NOAH_PROFILE_RGB_V1_MAX_TAP_BRANCH_COLORS   = 4u,
    NOAH_PROFILE_RGB_V1_MAX_PAYLOAD_SIZE        = NOAH_PROFILE_BLOB_V1_MAX_SIZE - NOAH_PROFILE_BLOB_V1_HEADER_SIZE - NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE,
    NOAH_PROFILE_RGB_V1_SELECTOR_ALL            = 0xffu,
    NOAH_PROFILE_RGB_V1_STAGE_LAYER             = 1u << 0,
    NOAH_PROFILE_RGB_V1_STAGE_AUTOMOUSE         = 1u << 1,
    NOAH_PROFILE_RGB_V1_STAGE_PD_MODE           = 1u << 2,
    NOAH_PROFILE_RGB_V1_STAGE_COMBO             = 1u << 3,
    NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR      = 1u << 4,
    NOAH_PROFILE_RGB_V1_STAGE_MASK_ALL          = 0x1fu,
    NOAH_PROFILE_RGB_V1_PD_MODE_MASK_ALL        = 0x3fu,
    // Validation performs at most one reader call per step. The v1 header is
    // the largest record; the remaining records are at most 11 bytes.
    NOAH_PROFILE_RGB_V1_VALIDATION_READ_MAX     = 20u,
    // Payload-independent 32-bit representation regression policies. These
    // are not statements of RP2040 hardware capacity.
    NOAH_PROFILE_RGB_V1_EMBEDDED_VIEW_BUDGET    = 40u,
    NOAH_PROFILE_RGB_V1_EMBEDDED_VALIDATION_BUDGET = 72u,
};

typedef enum {
    NOAH_PROFILE_RGB_V1_OK = 0u,
    NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT,
    NOAH_PROFILE_RGB_V1_TRUNCATED,
    NOAH_PROFILE_RGB_V1_TRAILING_BYTES,
    NOAH_PROFILE_RGB_V1_READ_ERROR,
    NOAH_PROFILE_RGB_V1_CAPACITY_EXCEEDED,
    NOAH_PROFILE_RGB_V1_INVALID_VERSION,
    NOAH_PROFILE_RGB_V1_RESERVED_BITS,
    NOAH_PROFILE_RGB_V1_INCOMPATIBLE_GEOMETRY,
    NOAH_PROFILE_RGB_V1_NONCANONICAL_ORDER,
    NOAH_PROFILE_RGB_V1_DUPLICATE_BITMAP,
    NOAH_PROFILE_RGB_V1_INVALID_REFERENCE,
    NOAH_PROFILE_RGB_V1_INVALID_SELECTOR,
    NOAH_PROFILE_RGB_V1_INVALID_ENUM,
    NOAH_PROFILE_RGB_V1_BRIGHTNESS_EXCEEDED,
    NOAH_PROFILE_RGB_V1_INVALID_ID,
    NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE,
    NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE_DATA,
    NOAH_PROFILE_RGB_V1_INCOMPLETE_SURFACE,
} noah_profile_rgb_v1_result_t;

typedef enum {
    NOAH_PROFILE_RGB_V1_TABLE_HEADER = 0u,
    NOAH_PROFILE_RGB_V1_TABLE_GROUPS,
    NOAH_PROFILE_RGB_V1_TABLE_LAYER_COLORS,
    NOAH_PROFILE_RGB_V1_TABLE_LAYER_GROUPS,
    NOAH_PROFILE_RGB_V1_TABLE_AUTOMOUSE,
    NOAH_PROFILE_RGB_V1_TABLE_PD_COLORS,
    NOAH_PROFILE_RGB_V1_TABLE_PD_GROUPS,
    NOAH_PROFILE_RGB_V1_TABLE_COMBO,
    NOAH_PROFILE_RGB_V1_TABLE_COMBO_GROUPS,
    NOAH_PROFILE_RGB_V1_TABLE_TAP_BRANCH_COLORS,
    NOAH_PROFILE_RGB_V1_TABLE_KEY_FEEDBACK,
    NOAH_PROFILE_RGB_V1_TABLE_KEY_GROUPS,
} noah_profile_rgb_v1_table_t;

typedef enum {
    NOAH_PROFILE_RGB_V1_FIELD_HEADER = 0u,
    NOAH_PROFILE_RGB_V1_FIELD_FORMAT_VERSION,
    NOAH_PROFILE_RGB_V1_FIELD_RESERVED,
    NOAH_PROFILE_RGB_V1_FIELD_STAGE_MASK,
    NOAH_PROFILE_RGB_V1_FIELD_COUNT,
    NOAH_PROFILE_RGB_V1_FIELD_GEOMETRY,
    NOAH_PROFILE_RGB_V1_FIELD_ID,
    NOAH_PROFILE_RGB_V1_FIELD_BITMAP,
    NOAH_PROFILE_RGB_V1_FIELD_COLOR,
    NOAH_PROFILE_RGB_V1_FIELD_MODE,
    NOAH_PROFILE_RGB_V1_FIELD_LOCALITY,
    NOAH_PROFILE_RGB_V1_FIELD_SELECTOR,
    NOAH_PROFILE_RGB_V1_FIELD_GROUP_ID,
    NOAH_PROFILE_RGB_V1_FIELD_SEMANTIC,
} noah_profile_rgb_v1_field_t;

typedef struct {
    noah_profile_rgb_v1_result_t code;
    size_t                       offset;
    uint8_t                      table;
    uint8_t                      row;
    uint8_t                      field;
} noah_profile_rgb_v1_error_t;

typedef struct {
    uint16_t compiled_stage_mask;
    uint16_t max_payload_size;
    uint8_t  logical_layer_count;
    uint8_t  max_logical_layers;
    uint8_t  maximum_brightness;
    uint8_t  tap_branch_color_count;
    uint8_t  supported_pd_mode_mask;
} noah_profile_rgb_v1_limits_t;

typedef struct {
    uint8_t h;
    uint8_t s;
    uint8_t v;
} noah_profile_rgb_v1_hsv_t;

typedef struct {
    uint8_t id;
    uint8_t bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
} noah_profile_rgb_v1_group_t;

typedef struct {
    uint8_t                   layer_id;
    noah_profile_rgb_v1_hsv_t color;
    uint8_t                   mode;
} noah_profile_rgb_v1_layer_color_t;

typedef struct {
    uint8_t                   selector;
    noah_profile_rgb_v1_hsv_t color;
    uint8_t                   group_id;
} noah_profile_rgb_v1_group_row_t;

typedef struct {
    uint8_t                   mode;
    noah_profile_rgb_v1_hsv_t end_color;
} noah_profile_rgb_v1_automouse_t;

typedef struct {
    uint8_t                   pd_mode_id;
    noah_profile_rgb_v1_hsv_t color;
    uint8_t                   locality;
} noah_profile_rgb_v1_pd_color_t;

typedef struct {
    noah_profile_rgb_v1_hsv_t color;
    uint8_t                   locality;
} noah_profile_rgb_v1_feedback_t;

typedef struct {
    noah_profile_rgb_v1_hsv_t color;
    uint8_t                   group_id;
} noah_profile_rgb_v1_combo_group_row_t;

typedef struct {
    noah_profile_rgb_v1_hsv_t tap_committed_color;
    noah_profile_rgb_v1_hsv_t hold_active_color;
    noah_profile_rgb_v1_hsv_t long_hold_active_color;
    uint8_t                   tap_commit_mode;
    uint8_t                   locality;
} noah_profile_rgb_v1_key_feedback_t;

// A validated reader-backed view. It retains no payload-sized buffer, group
// dictionary, or row array. On a 32-bit firmware target it is bounded by
// NOAH_PROFILE_RGB_V1_EMBEDDED_VIEW_BUDGET; host pointers are intentionally
// wider. Accessors copy only one fixed-size record at a time.
typedef struct {
    noah_profile_reader_t         reader;
    size_t                        base_offset;
    uint16_t                      byte_length;
    uint16_t                      stage_enable_mask;
    uint8_t                       group_count;
    uint8_t                       layer_color_count;
    uint8_t                       layer_group_count;
    uint8_t                       pd_color_count;
    uint8_t                       pd_group_count;
    uint8_t                       combo_group_count;
    uint8_t                       tap_branch_color_count;
    uint8_t                       key_group_count;
    noah_profile_rgb_v1_limits_t  limits;
} noah_profile_rgb_v1_view_t;

typedef enum {
    NOAH_PROFILE_RGB_V1_VALIDATION_IN_PROGRESS = 0u,
    NOAH_PROFILE_RGB_V1_VALIDATION_VALID,
    NOAH_PROFILE_RGB_V1_VALIDATION_REJECTED,
} noah_profile_rgb_v1_validation_result_t;

typedef enum {
    NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_UNINITIALIZED = 0u,
    NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_HEADER,
    NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_GROUPS,
    NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_LAYER_COLORS,
    NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_LAYER_GROUPS,
    NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_AUTOMOUSE,
    NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_PD_COLORS,
    NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_PD_GROUPS,
    NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_COMBO,
    NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_COMBO_GROUPS,
    NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_TAP_BRANCH_COLORS,
    NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_KEY_FEEDBACK,
    NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_KEY_GROUPS,
    NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_COMPLETE,
    NOAH_PROFILE_RGB_V1_VALIDATION_PHASE_REJECTED,
} noah_profile_rgb_v1_validation_phase_t;

// Caller-owned, payload-independent validation state. Treat fields as opaque;
// they are public only so firmware owners can allocate the state without a
// heap. begin() performs no reader I/O. Each step() performs at most one read
// of at most NOAH_PROFILE_RGB_V1_VALIDATION_READ_MAX bytes.
typedef struct {
    noah_profile_rgb_v1_view_t  candidate;
    noah_profile_rgb_v1_error_t rejection;
    uint8_t                     previous_bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
    uint8_t                     phase;
    uint8_t                     row;
    uint8_t                     have_previous_bitmap;
    uint8_t                     observed_pd_mask;
    int8_t                      previous_pd_id;
    uint8_t                     result;
} noah_profile_rgb_v1_validation_t;

noah_profile_rgb_v1_limits_t noah_profile_rgb_v1_default_limits(void);

noah_profile_rgb_v1_validation_result_t noah_profile_rgb_v1_validation_begin(noah_profile_rgb_v1_validation_t *validation, const noah_profile_reader_t *reader, size_t base_offset, size_t length, const noah_profile_rgb_v1_limits_t *limits, noah_profile_rgb_v1_error_t *error);
noah_profile_rgb_v1_validation_result_t noah_profile_rgb_v1_validation_step(noah_profile_rgb_v1_validation_t *validation, noah_profile_rgb_v1_error_t *error);
noah_profile_rgb_v1_validation_result_t noah_profile_rgb_v1_validation_view(const noah_profile_rgb_v1_validation_t *validation, noah_profile_rgb_v1_view_t *view, noah_profile_rgb_v1_error_t *error);

// Compatibility wrappers. decode_reader() drives the incremental validator to
// completion; scan-context owners should call begin()/step()/view() directly.
noah_profile_rgb_v1_result_t noah_profile_rgb_v1_decode_reader(const noah_profile_reader_t *reader, size_t base_offset, size_t length, const noah_profile_rgb_v1_limits_t *limits, noah_profile_rgb_v1_view_t *view, noah_profile_rgb_v1_error_t *error);
noah_profile_rgb_v1_result_t noah_profile_rgb_v1_decode(const uint8_t *bytes, size_t length, const noah_profile_rgb_v1_limits_t *limits, noah_profile_rgb_v1_view_t *view, noah_profile_rgb_v1_error_t *error);

noah_profile_rgb_v1_result_t noah_profile_rgb_v1_group_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_group_t *group, noah_profile_rgb_v1_error_t *error);
noah_profile_rgb_v1_result_t noah_profile_rgb_v1_layer_color_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_layer_color_t *row, noah_profile_rgb_v1_error_t *error);
noah_profile_rgb_v1_result_t noah_profile_rgb_v1_layer_group_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_group_row_t *row, noah_profile_rgb_v1_error_t *error);
noah_profile_rgb_v1_result_t noah_profile_rgb_v1_automouse(const noah_profile_rgb_v1_view_t *view, noah_profile_rgb_v1_automouse_t *value, noah_profile_rgb_v1_error_t *error);
noah_profile_rgb_v1_result_t noah_profile_rgb_v1_pd_color_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_pd_color_t *row, noah_profile_rgb_v1_error_t *error);
noah_profile_rgb_v1_result_t noah_profile_rgb_v1_pd_group_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_group_row_t *row, noah_profile_rgb_v1_error_t *error);
noah_profile_rgb_v1_result_t noah_profile_rgb_v1_combo_feedback(const noah_profile_rgb_v1_view_t *view, noah_profile_rgb_v1_feedback_t *value, noah_profile_rgb_v1_error_t *error);
noah_profile_rgb_v1_result_t noah_profile_rgb_v1_combo_group_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_combo_group_row_t *row, noah_profile_rgb_v1_error_t *error);
noah_profile_rgb_v1_result_t noah_profile_rgb_v1_tap_branch_color_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_hsv_t *color, noah_profile_rgb_v1_error_t *error);
noah_profile_rgb_v1_result_t noah_profile_rgb_v1_key_feedback(const noah_profile_rgb_v1_view_t *view, noah_profile_rgb_v1_key_feedback_t *value, noah_profile_rgb_v1_error_t *error);
noah_profile_rgb_v1_result_t noah_profile_rgb_v1_key_group_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_group_row_t *row, noah_profile_rgb_v1_error_t *error);
