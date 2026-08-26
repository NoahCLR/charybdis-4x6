// ────────────────────────────────────────────────────────────────────────────
// Canonical Key-Behavior Domain — Profile Wire v1.0
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "profile_blob_v1.h"
#include "profile_reader.h"

enum {
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VERSION                  = 1u,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_HEADER_SIZE              = 4u,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_LENGTH_SIZE          = 2u,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FIXED_SIZE           = 12u,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HEADER_SIZE         = 2u,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_HOLD_SIZE                = 6u,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FLAG_AUTO_MOUSE      = 1u << 0,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_KNOWN_ROW_FLAGS          = 1u << 0,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP             = 1u << 0,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_HOLD            = 1u << 1,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_LONG_HOLD       = 1u << 2,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_KNOWN_STEP_MASK          = 0x07u,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS                 = 64u,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_POPULATED_STEPS      = 128u,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_TAP_STEPS_PER_ROW    = 5u,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_REPEAT_HZ            = 100u,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_PAYLOAD_SIZE         = NOAH_PROFILE_BLOB_V1_MAX_SIZE - NOAH_PROFILE_BLOB_V1_HEADER_SIZE - NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE,
    // An incremental validation step performs at most one reader call. The
    // current wire records need no more than the 12-byte fixed row read, but
    // the public budget remains 20 bytes to match the scan owner contract.
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_READ_MAX       = 20u,
    // Payload-independent 32-bit representation regression policy. This is
    // not a statement of RP2040 hardware capacity.
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_EMBEDDED_VALIDATION_BUDGET = 120u,
};

typedef enum {
    NOAH_KEY_BEHAVIOR_HOLD_V1_PRESS_AND_HOLD_UNTIL_RELEASE = 1u,
    NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_AT_THRESHOLD              = 2u,
    NOAH_KEY_BEHAVIOR_HOLD_V1_REPEAT_WHILE_HELD             = 3u,
    NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_ON_RELEASE_AFTER_HOLD     = 4u,
} noah_key_behavior_hold_v1_mode_t;

typedef enum {
    NOAH_KEY_BEHAVIOR_FIELD_V1_HEADER = 1u,
    NOAH_KEY_BEHAVIOR_FIELD_V1_ROW_LENGTH,
    NOAH_KEY_BEHAVIOR_FIELD_V1_TARGET,
    NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_HOLD_TERM,
    NOAH_KEY_BEHAVIOR_FIELD_V1_LONGER_HOLD_TERM,
    NOAH_KEY_BEHAVIOR_FIELD_V1_MULTI_TAP_TERM,
    NOAH_KEY_BEHAVIOR_FIELD_V1_ROW_FLAGS,
    NOAH_KEY_BEHAVIOR_FIELD_V1_ROW_STEP_COUNT,
    NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_INDEX,
    NOAH_KEY_BEHAVIOR_FIELD_V1_PRESENCE_MASK,
    NOAH_KEY_BEHAVIOR_FIELD_V1_TAP_ACTION,
    NOAH_KEY_BEHAVIOR_FIELD_V1_HOLD_MODE,
    NOAH_KEY_BEHAVIOR_FIELD_V1_HOLD_REPEAT,
    NOAH_KEY_BEHAVIOR_FIELD_V1_HOLD_ACTION,
    NOAH_KEY_BEHAVIOR_FIELD_V1_LONG_HOLD_MODE,
    NOAH_KEY_BEHAVIOR_FIELD_V1_LONG_HOLD_REPEAT,
    NOAH_KEY_BEHAVIOR_FIELD_V1_LONG_HOLD_ACTION,
} noah_key_behavior_field_v1_t;

typedef struct {
    uint8_t max_rows;
    uint8_t max_populated_steps;
    uint8_t max_tap_steps_per_row;
    uint8_t max_repeat_hz;
    uint16_t max_payload_size;
} noah_key_behavior_limits_v1_t;

typedef struct {
    uint8_t                  mode;
    uint8_t                  repeat_hz;
    noah_profile_action_v1_t action;
} noah_key_behavior_hold_v1_t;

typedef struct {
    uint8_t                     tap_index;
    uint8_t                     presence_mask;
    noah_profile_action_v1_t    tap;
    noah_key_behavior_hold_v1_t hold;
    noah_key_behavior_hold_v1_t long_hold;
} noah_key_behavior_step_v1_t;

typedef struct {
    noah_profile_action_v1_t       target;
    uint16_t                       tap_hold_term;
    uint16_t                       longer_hold_term;
    uint16_t                       multi_tap_term;
    uint8_t                        flags;
    const noah_key_behavior_step_v1_t *steps;
    size_t                         step_count;
} noah_key_behavior_row_v1_t;

// A validated reader-backed domain. It retains no payload-sized buffer and no
// native row/step arrays. Row and step accessors copy one resolved record at a
// time through the bounded reader.
typedef struct {
    noah_profile_reader_t          reader;
    size_t                         base_offset;
    size_t                         byte_length;
    uint8_t                        row_count;
    uint8_t                        populated_step_count;
    noah_key_behavior_limits_v1_t  limits;
    noah_profile_action_v1_limits_t action_limits;
} noah_key_behavior_domain_v1_t;

typedef struct {
    uint8_t                  row_index;
    uint8_t                  target_bytes[NOAH_PROFILE_BLOB_V1_ACTION_SIZE];
    noah_profile_action_v1_t target;
    uint16_t                 tap_hold_term;
    uint16_t                 longer_hold_term;
    uint16_t                 multi_tap_term;
    uint8_t                  flags;
    uint8_t                  step_count;
    size_t                   row_offset;
    size_t                   steps_offset;
    size_t                   row_end;
} noah_key_behavior_row_v1_view_t;

typedef enum {
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_IN_PROGRESS = 0u,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_VALID,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_REJECTED,
} noah_key_behavior_domain_v1_validation_result_t;

// Public for deterministic scan-owner tests and diagnostics. Callers should
// otherwise treat phases as an implementation detail.
typedef enum {
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_PHASE_UNINITIALIZED = 0u,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_PHASE_HEADER,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_PHASE_ROW_LENGTH,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_PHASE_ROW_FIXED,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_PHASE_STEP_HEADER,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_PHASE_STEP_TAP,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_PHASE_STEP_HOLD_FIELDS,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_PHASE_STEP_HOLD_ACTION,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_PHASE_STEP_LONG_HOLD_FIELDS,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_PHASE_STEP_LONG_HOLD_ACTION,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_PHASE_VALID,
    NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_PHASE_REJECTED,
} noah_key_behavior_domain_v1_validation_phase_t;

// Published for the single action decoded by the most recent validation
// step. Offsets are relative to the start of the behavior payload. A whole-
// profile caller can record the first invalid reference from these events and
// defer reporting it until structural validation completes, preserving the
// existing domain-error-before-reference-error precedence without rereading.
typedef struct {
    noah_profile_action_v1_t action;
    size_t                   offset;
    uint8_t                  row_index;
    uint8_t                  step_index;
    uint8_t                  field_id;
} noah_key_behavior_domain_v1_action_event_t;

// Caller-owned, payload-independent incremental validation state. Fields are
// public only so firmware can allocate the object statically.
typedef struct {
    noah_key_behavior_domain_v1_validation_phase_t phase;
    noah_profile_codec_v1_result_t                  terminal_result;
    noah_profile_codec_v1_error_t                   terminal_error;
    noah_key_behavior_domain_v1_t                   candidate;
    noah_key_behavior_domain_v1_action_event_t      action_event;
    noah_profile_action_v1_t                        current_target;
    size_t                                          row_offset;
    size_t                                          row_end;
    size_t                                          step_offset;
    size_t                                          branch_offset;
    size_t                                          actual_steps;
    int16_t                                         previous_tap_index;
    uint8_t                                         previous_target[NOAH_PROFILE_BLOB_V1_ACTION_SIZE];
    uint8_t                                         current_target_bytes[NOAH_PROFILE_BLOB_V1_ACTION_SIZE];
    uint8_t                                         row_index;
    uint8_t                                         step_index;
    uint8_t                                         current_step_count;
    uint8_t                                         current_tap_index;
    uint8_t                                         current_presence_mask;
    bool                                            have_previous_target;
    bool                                            action_event_available;
} noah_key_behavior_domain_v1_validation_t;

noah_key_behavior_limits_v1_t noah_key_behavior_domain_v1_default_limits(void);

// begin validates arguments and capacities but performs no reader I/O. Each
// successful step performs at most one reader call and reads no more than
// NOAH_KEY_BEHAVIOR_DOMAIN_V1_VALIDATION_READ_MAX newly observed bytes.
noah_key_behavior_domain_v1_validation_result_t noah_key_behavior_domain_v1_validation_begin(noah_key_behavior_domain_v1_validation_t *validation, const noah_profile_reader_t *reader, size_t base_offset, size_t length, const noah_key_behavior_limits_v1_t *limits, const noah_profile_action_v1_limits_t *action_limits, noah_profile_codec_v1_error_t *error);
noah_key_behavior_domain_v1_validation_result_t noah_key_behavior_domain_v1_validation_step(noah_key_behavior_domain_v1_validation_t *validation, noah_profile_codec_v1_error_t *error);
noah_key_behavior_domain_v1_validation_result_t noah_key_behavior_domain_v1_validation_view(const noah_key_behavior_domain_v1_validation_t *validation, noah_key_behavior_domain_v1_t *domain, noah_profile_codec_v1_error_t *error);

// Returns true and copies the action decoded by the most recent step. The
// event remains available until the next call to validation_step.
bool noah_key_behavior_domain_v1_validation_action_event(const noah_key_behavior_domain_v1_validation_t *validation, noah_key_behavior_domain_v1_action_event_t *event);

noah_profile_codec_v1_result_t noah_key_behavior_domain_v1_encode(const noah_key_behavior_row_v1_t *rows, size_t row_count, const noah_key_behavior_limits_v1_t *limits, const noah_profile_action_v1_limits_t *action_limits, uint8_t *output, size_t output_capacity, size_t *written, noah_profile_codec_v1_error_t *error);

noah_profile_codec_v1_result_t noah_key_behavior_domain_v1_decode_reader(const noah_profile_reader_t *reader, size_t base_offset, size_t length, const noah_key_behavior_limits_v1_t *limits, const noah_profile_action_v1_limits_t *action_limits, noah_key_behavior_domain_v1_t *domain, noah_profile_codec_v1_error_t *error);
noah_profile_codec_v1_result_t noah_key_behavior_domain_v1_decode(const uint8_t *bytes, size_t length, const noah_key_behavior_limits_v1_t *limits, const noah_profile_action_v1_limits_t *action_limits, noah_key_behavior_domain_v1_t *domain, noah_profile_codec_v1_error_t *error);

noah_profile_codec_v1_result_t noah_key_behavior_domain_v1_row_at(const noah_key_behavior_domain_v1_t *domain, uint8_t row_index, noah_key_behavior_row_v1_view_t *row, noah_profile_codec_v1_error_t *error);
// Finds one canonical semantic target in a single ordered row pass. A missing
// target is a successful query with *found == false; reader/shape failures are
// still reported as codec errors.
noah_profile_codec_v1_result_t noah_key_behavior_domain_v1_find_target(const noah_key_behavior_domain_v1_t *domain, const noah_profile_action_v1_t *target, noah_key_behavior_row_v1_view_t *row, bool *found, noah_profile_codec_v1_error_t *error);
// Reads one step from a row view returned by row_at/find_target without
// rescanning preceding rows. The row metadata is re-resolved at its exact
// offset before use, so a forged caller view cannot escape domain bounds.
noah_profile_codec_v1_result_t noah_key_behavior_domain_v1_step_in_row(const noah_key_behavior_domain_v1_t *domain, const noah_key_behavior_row_v1_view_t *row, uint8_t step_index, noah_key_behavior_step_v1_t *step, noah_profile_codec_v1_error_t *error);
// Re-resolves row_index from the validated domain rather than trusting caller-
// supplied row offsets. This keeps the reader bounds authoritative.
noah_profile_codec_v1_result_t noah_key_behavior_domain_v1_step_at(const noah_key_behavior_domain_v1_t *domain, uint8_t row_index, uint8_t step_index, noah_key_behavior_step_v1_t *step, noah_profile_codec_v1_error_t *error);
