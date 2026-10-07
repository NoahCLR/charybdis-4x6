// PD domain contract, advertised by schema-2 owner firmware: 32 slots, stored
// sparsely (version 2). The record layout is version 1's, unchanged.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    NOAH_PROFILE_PD_V1_DOMAIN_ID   = 0x50,
    NOAH_PROFILE_PD_V1_VERSION     = 2,
    NOAH_PROFILE_PD_V1_SLOT_COUNT  = 32,
    NOAH_PROFILE_PD_V1_HEADER_SIZE = 8,
    NOAH_PROFILE_PD_V1_RECORD_SIZE = 96,
    NOAH_PROFILE_PD_V1_NAME_SIZE   = 24,
    // Every slot present: the largest payload a version-2 domain can have.
    NOAH_PROFILE_PD_V1_MAX_SIZE = NOAH_PROFILE_PD_V1_HEADER_SIZE + NOAH_PROFILE_PD_V1_SLOT_COUNT * NOAH_PROFILE_PD_V1_RECORD_SIZE,
};

// Directional axis policies (record byte 3): which directions exist. Vertical
// and horizontal have two, dominant axis the four straight ones, eight
// directions adds the diagonals, whose shortcuts live in bytes 70..85 (zero
// in every other directional record). One engine runs them all.
enum {
    NOAH_PD_AXIS_VERTICAL = 0,
    NOAH_PD_AXIS_HORIZONTAL,
    NOAH_PD_AXIS_DOMINANT,
    NOAH_PD_AXIS_EIGHT,
};
// Which axes a scrolling mode scrolls (byte 3 of a scrolling record).
enum {
    NOAH_PD_SCROLL_BOTH = 0,
    NOAH_PD_SCROLL_HORIZONTAL,
    NOAH_PD_SCROLL_VERTICAL,
};
// What a directional mode does with motion toward a direction that has no
// shortcut (byte 86, every directional record).
enum {
    NOAH_PD_EMPTY_DIRECTION_NEAREST = 0, // its neighbours take its share
    NOAH_PD_EMPTY_DIRECTION_BOTH,        // both compass neighbours (eight directions; elsewhere as nearest)
    NOAH_PD_EMPTY_DIRECTION_NOTHING,     // a dead zone
};
// How often a directional mode sends (byte 87, every directional record).
enum {
    NOAH_PD_DIRECTION_OUTPUT_REPEAT = 0, // one output per threshold step
    NOAH_PD_DIRECTION_OUTPUT_ONCE,       // one output per movement
};

typedef enum {
    NOAH_PROFILE_PD_V1_OK = 0,
    NOAH_PROFILE_PD_V1_INVALID_ARGUMENT,
    NOAH_PROFILE_PD_V1_INVALID_LENGTH,
    NOAH_PROFILE_PD_V1_INVALID_HEADER,
    NOAH_PROFILE_PD_V1_INVALID_ID,
    NOAH_PROFILE_PD_V1_RESERVED,
    NOAH_PROFILE_PD_V1_INVALID_NAME,
    NOAH_PROFILE_PD_V1_INVALID_POLICY,
    NOAH_PROFILE_PD_V1_INVALID_ACTION,
    NOAH_PROFILE_PD_V1_INVALID_PARAMETER,
    // A present version-2 record that says nothing: disabled with no name.
    // Such a slot is stored by leaving it out.
    NOAH_PROFILE_PD_V1_NONCANONICAL,
} noah_profile_pd_v1_result_t;

typedef struct {
    noah_profile_pd_v1_result_t code;
    size_t offset;
} noah_profile_pd_v1_error_t;

// Native authored data. Serialization explicitly writes little-endian words.
typedef struct { uint16_t keycode; uint8_t policy, mask; } noah_pd_tap_t;
typedef struct { uint8_t kind, modifiers; noah_pd_tap_t tap; } noah_pd_button_t;
typedef struct {
    uint8_t id, kind, pointer_layer, axis;
    uint16_t dpi;
    uint8_t held_modifiers, reserved;
    char name[24];
    uint16_t threshold_x, threshold_y;
    noah_pd_tap_t directions[4]; // left, right, up, down
    noah_pd_button_t buttons[3];
    union {
        struct {
            uint16_t scroll[7];       // H/V threshold, H/V divisor, interval, expiry, timeout
            uint8_t  scroll_policy[6]; // start N/D, sustain N/D, decay, inversion bits
        };
        struct {
            noah_pd_tap_t diagonals[4]; // eight directions: up-left, up-right, down-left, down-right
            uint8_t       empty_direction;
            uint8_t       direction_output;
            uint8_t       diagonal_reserved[2];
        };
    };
    uint8_t tail_reserved[6];
} noah_pd_config_t;
extern const noah_pd_config_t noah_pd_defaults[NOAH_PROFILE_PD_V1_SLOT_COUNT];
void noah_profile_pd_v1_encode_record(const noah_pd_config_t *config, uint8_t output[96]);
// Whether a version-2 domain stores the slot: it is configured, or disabled
// with a name. Record bytes 1 (kind) and 8 (first name byte) decide it.
bool noah_profile_pd_v1_record_present(const uint8_t *record);

// Vocabulary is the audited QMK 0.0.8 native ABI, not arbitrary uint16 actions.
bool noah_profile_pd_v1_tap_key_valid(uint16_t keycode);

// Cold byte validators; they allocate no profile/cache state. The header and
// entry APIs let a bounded reader validate a version-2 domain one record at a
// time without holding the whole payload.
noah_profile_pd_v1_result_t noah_profile_pd_v1_validate_record(const uint8_t *record, size_t length, uint8_t slot, noah_profile_pd_v1_error_t *error);
// Version 2: the 8-byte header against the whole payload length. On success
// *record_count is the number of records that follow.
noah_profile_pd_v1_result_t noah_profile_pd_v1_validate_header(const uint8_t header[8], size_t payload_length, uint8_t *record_count, noah_profile_pd_v1_error_t *error);
// Version 2: one present record, whose slot ID (byte 0) must be at least
// minimum_slot (zero for the first record, the previous ID plus one after) and
// below the slot count, and which must not be a disabled record without a name.
noah_profile_pd_v1_result_t noah_profile_pd_v1_validate_entry(const uint8_t *record, size_t length, uint8_t minimum_slot, noah_profile_pd_v1_error_t *error);
// A whole version-2 payload.
noah_profile_pd_v1_result_t noah_profile_pd_v1_validate(const uint8_t *bytes, size_t length, noah_profile_pd_v1_error_t *error);
// A whole retired version-1 payload: header 01 08 60 00.., eight records.
