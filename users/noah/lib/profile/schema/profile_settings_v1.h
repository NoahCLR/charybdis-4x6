#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "profile_reader.h"
#include "profile_versions.h"
#include "profile_name_v1.h"
#include "../../action/custom_key_shortcut.h"

// Settings v8 (D-F14, D-F25, D-F26) stores, in order: an eight-byte header,
// the scalar policy, one fixed record per layer (its combo reference layer and
// its two placement bitmaps), one two-byte shortcut per custom key, then every
// name counted: a length byte and at most 32 bytes of UTF-8, the layers'
// first, then the macros', then the custom keys', then the keyboard's own
// name. The fixed part comes first so its fields sit at known offsets.
enum {
    NOAH_SETTINGS_VERSION              = NOAH_PROFILE_SETTINGS_VERSION,
    NOAH_SETTINGS_HEADER_SIZE          = 8,
    // Feature bit 30 uses header byte 5; zero retains generated behaviours.
    NOAH_SETTINGS_FLAGS_OFFSET         = 5,
    NOAH_SETTINGS_FLAG_COMBO_OUTPUT_BYPASS = 1u << 0,
    NOAH_SETTINGS_KNOWN_FLAGS          = NOAH_SETTINGS_FLAG_COMBO_OUTPUT_BYPASS,
    NOAH_SETTINGS_COUNT                = 31,
    NOAH_SETTINGS_LAYERS               = 16,
    NOAH_SETTINGS_MACRO_NAMES          = 128,
    NOAH_SETTINGS_CUSTOM_KEY_NAMES     = 128,
    // Header byte 6 counts the custom-key shortcut records (D-F26).
    NOAH_SETTINGS_SHORTCUT_COUNT_OFFSET = 6,
    NOAH_SETTINGS_CUSTOM_KEY_SHORTCUTS = 128,
    // The keyboard name the USB product string carries; empty means the
    // firmware's built-in name (D-F25).
    NOAH_SETTINGS_KEYBOARD_NAMES       = 1,
    NOAH_SETTINGS_NAME_MAX             = NOAH_PROFILE_NAME_MAX,
    // A placement bitmap covers up to 64 matrix positions; bits past the
    // firmware's rows × columns are zero.
    NOAH_SETTINGS_PLACEMENT_MAX_POSITIONS = 64,
    NOAH_SETTINGS_PLACEMENT_BYTES      = 8,
    NOAH_SETTINGS_LAYER_RECORD_SIZE    = 1 + 2 * NOAH_SETTINGS_PLACEMENT_BYTES,
    NOAH_SETTINGS_SCALARS_OFFSET       = NOAH_SETTINGS_HEADER_SIZE,
    NOAH_SETTINGS_LAYER_RECORDS_OFFSET = NOAH_SETTINGS_SCALARS_OFFSET + NOAH_SETTINGS_COUNT * 4,
    NOAH_SETTINGS_SHORTCUTS_OFFSET     = NOAH_SETTINGS_LAYER_RECORDS_OFFSET + NOAH_SETTINGS_LAYERS * NOAH_SETTINGS_LAYER_RECORD_SIZE,
    NOAH_SETTINGS_SHORTCUT_SIZE        = NOAH_CUSTOM_KEY_SHORTCUT_SIZE,
    NOAH_SETTINGS_FIXED_SIZE           = NOAH_SETTINGS_SHORTCUTS_OFFSET + NOAH_SETTINGS_CUSTOM_KEY_SHORTCUTS * NOAH_SETTINGS_SHORTCUT_SIZE,
    NOAH_SETTINGS_NAME_COUNT           = NOAH_SETTINGS_LAYERS + NOAH_SETTINGS_MACRO_NAMES + NOAH_SETTINGS_CUSTOM_KEY_NAMES + NOAH_SETTINGS_KEYBOARD_NAMES,
    NOAH_SETTINGS_KEYBOARD_NAME_RECORD = NOAH_SETTINGS_NAME_COUNT - 1,
    NOAH_SETTINGS_MIN_SIZE             = NOAH_SETTINGS_FIXED_SIZE + NOAH_SETTINGS_NAME_COUNT,
    NOAH_SETTINGS_MAX_SIZE             = NOAH_SETTINGS_FIXED_SIZE + NOAH_SETTINGS_NAME_COUNT * (1 + NOAH_SETTINGS_NAME_MAX),
};
// Offsets inside one layer record.
enum {
    NOAH_SETTINGS_LAYER_REFERENCE = 0,
    NOAH_SETTINGS_LAYER_BYPASS    = 1,
    NOAH_SETTINGS_LAYER_EXCLUDE   = 1 + NOAH_SETTINGS_PLACEMENT_BYTES,
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
    // Host settings (feature bit 21): bits 0..1 select Auto/macOS/Windows/
    // Linux and bit 8 enables Unicode entry; zero is Auto with Unicode off.
    NOAH_SETTING_UNICODE_HOST_MODE,
    // Participation (participation-policy.md): the behaviour master, then one
    // bit per layer for its behaviours and for its combos.
    NOAH_SETTING_BEHAVIORS_ENABLED,
    NOAH_SETTING_LAYER_BEHAVIORS,
    NOAH_SETTING_LAYER_COMBOS,
} noah_setting_id_t;


typedef struct {
    uint16_t               offset;
    uint16_t               name; // the name being read, 0..NOAH_SETTINGS_NAME_COUNT
    bool                   in_name;
    noah_profile_name_v1_t current;
    uint32_t               value, timeout, dead_time;
} noah_profile_settings_v1_validation_t;
// One byte per call, permitting the whole-profile owner to retain its 20-byte
// per-step I/O ceiling. False rejects the candidate before publication.
bool noah_profile_settings_v1_consume(noah_profile_settings_v1_validation_t *state, uint8_t byte, uint32_t length, uint8_t layers, uint8_t positions);
bool noah_profile_settings_v1_complete(const noah_profile_settings_v1_validation_t *state, uint32_t length);
bool noah_profile_setting_v1_valid(uint8_t id, uint32_t value, uint8_t layers);

// The keyboard's own name, the domain's last name record (D-F25). Empty means
// the firmware's built-in name.
typedef struct {
    uint8_t length;
    uint8_t bytes[NOAH_SETTINGS_NAME_MAX];
} noah_profile_keyboard_name_t;
// Reads the keyboard name from a current settings domain of `length` bytes at
// `offset`. False, with the name left empty, when the bytes cannot be read or
// are not a current domain whose last record is a valid name ending the domain.
bool noah_profile_settings_v1_keyboard_name(const noah_profile_reader_t *reader, size_t offset, size_t length, noah_profile_keyboard_name_t *name);
bool noah_profile_keyboard_name_equal(const noah_profile_keyboard_name_t *a, const noah_profile_keyboard_name_t *b);
