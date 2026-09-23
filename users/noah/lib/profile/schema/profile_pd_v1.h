// Eight-slot PD domain contract, advertised by schema-2 owner firmware.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    NOAH_PROFILE_PD_V1_DOMAIN_ID   = 0x50,
    NOAH_PROFILE_PD_V1_VERSION     = 1,
    NOAH_PROFILE_PD_V1_SLOT_COUNT  = 8,
    NOAH_PROFILE_PD_V1_HEADER_SIZE = 8,
    NOAH_PROFILE_PD_V1_RECORD_SIZE = 96,
    NOAH_PROFILE_PD_V1_NAME_SIZE   = 24,
    NOAH_PROFILE_PD_V1_SIZE        = 776,
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
// What a directional mode does with motion toward a direction that has no
// shortcut (byte 86, every directional record).
enum {
    NOAH_PD_EMPTY_DIRECTION_NEAREST = 0, // its neighbours take its share
    NOAH_PD_EMPTY_DIRECTION_BOTH,        // a diagonal taps both straight directions; a straight one acts as nearest
    NOAH_PD_EMPTY_DIRECTION_NOTHING,     // a dead zone
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
            uint8_t       diagonal_reserved[3];
        };
    };
    uint8_t tail_reserved[6];
} noah_pd_config_t;
extern const noah_pd_config_t noah_pd_defaults[8];
void noah_profile_pd_v1_encode_record(const noah_pd_config_t *config, uint8_t output[96]);

// Vocabulary is the audited QMK 0.0.8 native ABI, not arbitrary uint16 actions.
bool noah_profile_pd_v1_tap_key_valid(uint16_t keycode);

// Cold byte validators; they allocate no profile/cache state. The record API
// permits future bounded reader integration without reading the entire domain.
noah_profile_pd_v1_result_t noah_profile_pd_v1_validate_record(const uint8_t *record, size_t length, uint8_t slot, noah_profile_pd_v1_error_t *error);
noah_profile_pd_v1_result_t noah_profile_pd_v1_validate(const uint8_t *bytes, size_t length, noah_profile_pd_v1_error_t *error);
