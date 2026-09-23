#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "profile_reader.h"
#include "profile_versions.h"

// Portable settings are data; the macro instruction vocabulary is fixed by
// the firmware ABI. Layer names are UTF-8, zero terminated and zero padded.
//
// After the fixed part, v1 and v2 carry 16 user-macro IR records (u16 length
// plus instructions). v3 instead names the 64 VIA macros: 64 records of a u8
// length (0..23) and that many UTF-8 bytes. The size ceiling is unchanged, so
// the names share the space the user macros had.
enum {
    NOAH_SETTINGS_VERSION     = NOAH_PROFILE_SETTINGS_VERSION,
    NOAH_SETTINGS_COUNT       = 28,
    NOAH_SETTINGS_LAYERS      = 8,
    NOAH_SETTINGS_NAME_BYTES  = 24,
    NOAH_SETTINGS_MACROS      = 16,
    NOAH_SETTINGS_MACRO_BYTES = 1024,
    NOAH_SETTINGS_MACRO_NAMES = 64,
    NOAH_SETTINGS_MACRO_NAME_MAX = 23,
    NOAH_SETTINGS_FIXED_SIZE  = 8 + 28 * 4 + 8 * 24,
    NOAH_SETTINGS_MAX_SIZE    = NOAH_SETTINGS_FIXED_SIZE + 16 * 2 + 1024,
};
typedef enum {
    NOAH_SETTING_TAPPING_TERM,
    NOAH_SETTING_TAP_HOLD_TERM,
    NOAH_SETTING_LONG_HOLD_TERM,
    NOAH_SETTING_MULTI_TAP_TERM,
    NOAH_SETTING_AUTO_MOUSE_ENABLED,
    NOAH_SETTING_AUTO_MOUSE_LAYER,
    NOAH_SETTING_AUTO_MOUSE_TIMEOUT,
    NOAH_SETTING_AUTO_MOUSE_DEBOUNCE,
    NOAH_SETTING_AUTO_SNIPING_ENABLED,
    NOAH_SETTING_AUTO_SNIPING_LAYER,
    NOAH_SETTING_DRAGSCROLL_DPI,
    NOAH_SETTING_VOLUME_DPI,
    NOAH_SETTING_BRIGHTNESS_DPI,
    NOAH_SETTING_ZOOM_DPI,
    NOAH_SETTING_ARROW_DPI,
    NOAH_SETTING_FEEDBACK_PERIOD,
    NOAH_SETTING_AUTO_MOUSE_DEAD_TIME,
    NOAH_SETTING_RGB_TIMEOUT,
    NOAH_SETTING_DEFAULT_DPI,
    NOAH_SETTING_SNIPING_DPI,
    NOAH_SETTING_COMBOS_ENABLED,
    NOAH_SETTING_RGB_MODE,
    NOAH_SETTING_RGB_COLOR,
    NOAH_SETTING_DEFAULT_LAYERS,
    NOAH_SETTING_KEYMAP_OPTIONS,
    NOAH_SETTING_AUTO_MOUSE_DELAY,
    NOAH_SETTING_AUTO_MOUSE_THRESHOLD,
    NOAH_SETTING_COMBO_REFERENCES,
} noah_setting_id_t;

typedef struct {
    uint16_t offset;
    uint16_t length;
} noah_profile_settings_v1_view_t;
typedef struct {
    uint16_t offset, macro_length, macro_offset, macro_total;
    uint32_t value, timeout, dead_time;
    uint8_t  slot, opcode, remaining, held[16], held_count, tap[16], tap_count;
    uint8_t  utf8_remaining, utf8_min, utf8_max;
    uint8_t  version, expected_version; // expected_version: the envelope's, 0 when unknown
    bool     name_ended;
} noah_profile_settings_v1_validation_t;
// One byte per call, permitting the whole-profile owner to retain its 20-byte
// per-step I/O ceiling. False rejects the candidate before publication.
bool noah_profile_settings_v1_consume(noah_profile_settings_v1_validation_t *state, uint8_t byte, uint16_t length, uint8_t layers);
bool noah_profile_settings_v1_complete(const noah_profile_settings_v1_validation_t *state, uint16_t length);
bool noah_profile_setting_v1_valid(uint8_t id, uint32_t value, uint8_t layers);
