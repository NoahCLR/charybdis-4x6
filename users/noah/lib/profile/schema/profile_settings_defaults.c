#include "profile_settings_defaults.h"
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
#    include "noah_keymap_ids.h"
#    include "../../compat/qmk_factory_settings.h"
uint32_t noah_profile_settings_default(uint8_t id) {
    if (id >= NOAH_SETTING_DRAGSCROLL_DPI && id <= NOAH_SETTING_ARROW_DPI) return 0;
    switch (id) {
        case NOAH_SETTING_TAPPING_TERM:
            return TAPPING_TERM;
        case NOAH_SETTING_TAP_HOLD_TERM:
            return CUSTOM_TAP_HOLD_TERM;
        case NOAH_SETTING_LONG_HOLD_TERM:
            return CUSTOM_LONGER_HOLD_TERM;
        case NOAH_SETTING_MULTI_TAP_TERM:
            return CUSTOM_MULTI_TAP_TERM;
        case NOAH_SETTING_AUTO_MOUSE_ENABLED:
            return 1u;
        case NOAH_SETTING_AUTO_MOUSE_LAYER:
            return AUTO_MOUSE_DEFAULT_LAYER;
        case NOAH_SETTING_AUTO_MOUSE_TIMEOUT:
            return AUTO_MOUSE_TIME;
        case NOAH_SETTING_AUTO_MOUSE_DEBOUNCE:
            return AUTO_MOUSE_DEBOUNCE;
        case NOAH_SETTING_AUTO_SNIPING_ENABLED:
            return 1;
        case NOAH_SETTING_AUTO_SNIPING_LAYER:
            return CHARYBDIS_AUTO_SNIPING_LAYER;
        case NOAH_SETTING_FEEDBACK_PERIOD:
            return RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS;
        case NOAH_SETTING_AUTO_MOUSE_DEAD_TIME:
            return AUTOMOUSE_RGB_DEAD_TIME;
        case NOAH_SETTING_RGB_TIMEOUT:
            return 900000;
        case NOAH_SETTING_DEFAULT_DPI:
            return CHARYBDIS_MINIMUM_DEFAULT_DPI;
        case NOAH_SETTING_SNIPING_DPI:
            return CHARYBDIS_MINIMUM_SNIPING_DPI;
        case NOAH_SETTING_COMBOS_ENABLED:
            return 1u;
        case NOAH_SETTING_RGB_MODE:
            return NOAH_QMK_FACTORY_RGB_MODE;
        case NOAH_SETTING_RGB_COLOR:
            return NOAH_QMK_FACTORY_RGB_COLOR;
        case NOAH_SETTING_DEFAULT_LAYERS:
            return 1u;
        case NOAH_SETTING_KEYMAP_OPTIONS:
            return NOAH_QMK_FACTORY_KEYMAP_OPTIONS;
        case NOAH_SETTING_AUTO_MOUSE_DELAY:
            return 200;
        case NOAH_SETTING_AUTO_MOUSE_THRESHOLD:
            return 10;
        case NOAH_SETTING_COMBO_REFERENCES:
            return 0x76543210u;
        default:
            return 0;
    }
}
// The keymap's names fill a settings domain that no profile has stored.
_Static_assert((int)LAYER_COUNT == (int)NOAH_SETTINGS_LAYERS, "Settings name every one of the eight layers");
_Static_assert(NOAH_LAYER_NAME_SIZE == NOAH_SETTINGS_NAME_BYTES, "A layer name fills one settings name field");
_Static_assert(VIA_MACRO_SLOT_COUNT == NOAH_SETTINGS_MACRO_NAMES, "Settings name every VIA macro");
_Static_assert(NOAH_MACRO_NAME_SIZE == NOAH_SETTINGS_MACRO_NAME_ASCII_MAX + 1u, "A macro name is at most 20 characters");
_Static_assert(CUSTOM_KEY_SLOT_COUNT == NOAH_SETTINGS_CUSTOM_KEY_NAMES, "Settings name every custom key");
enum { LAYER_NAMES_OFFSET = 8u + NOAH_SETTINGS_COUNT * 4u };
// Name records after the fixed part: the macros' (v3), then the custom keys' (v5).
enum { NAME_RECORDS = NOAH_SETTINGS_MACRO_NAMES + NOAH_SETTINGS_CUSTOM_KEY_NAMES, CUSTOM_KEY_NAME_HEADER = NOAH_SETTINGS_CUSTOM_KEY_NAMES };
static uint8_t name_length(const char *name, uint8_t max) {
    uint8_t length = 0;
    while (length < max && name[length])
        length++;
    return length;
}
static const char *record_name(uint8_t record) {
    if (record >= NOAH_SETTINGS_MACRO_NAMES) return custom_key_names[record - NOAH_SETTINGS_MACRO_NAMES];
    return via_macro_names[record];
}
static uint8_t record_name_length(uint8_t record) {
    return name_length(record_name(record), NOAH_SETTINGS_MACRO_NAME_ASCII_MAX);
}
uint16_t noah_profile_settings_defaults_length(void) {
    uint16_t length = NOAH_SETTINGS_FIXED_SIZE + NAME_RECORDS;
    for (uint8_t record = 0; record < NAME_RECORDS; record++)
        length += record_name_length(record);
    return length;
}
uint8_t noah_profile_settings_defaults_byte(uint16_t offset) {
    if (offset >= 8u && offset < LAYER_NAMES_OFFSET) {
        uint8_t id = (offset - 8u) / 4u;
        return noah_profile_settings_default(id) >> (8u * ((offset - 8u) % 4u));
    }
    // No profile settings are live: the current version with the keymap's
    // layer, macro and custom-key names.
    const uint8_t header[8] = {NOAH_SETTINGS_VERSION, 8, NOAH_SETTINGS_COUNT, NOAH_SETTINGS_MACRO_NAMES, CUSTOM_KEY_NAME_HEADER, 0, 0, 0};
    if (offset < 8u) return header[offset];
    if (offset < NOAH_SETTINGS_FIXED_SIZE) {
        offset -= LAYER_NAMES_OFFSET;
        const char *name = layer_names[offset / NOAH_SETTINGS_NAME_BYTES];
        uint8_t     byte = offset % NOAH_SETTINGS_NAME_BYTES;
        // Zero padded after the name, whatever the array holds there.
        return byte < name_length(name, NOAH_SETTINGS_NAME_BYTES - 1u) ? (uint8_t)name[byte] : 0u;
    }
    offset -= NOAH_SETTINGS_FIXED_SIZE;
    // Reads run forward, so resume from the record the last byte was in; the
    // authored names never change, so the cursor stays valid across reads.
    static uint8_t  cursor_record;
    static uint16_t cursor_start;
    if (offset < cursor_start) cursor_record = 0u, cursor_start = 0u;
    for (uint8_t record = cursor_record; record < NAME_RECORDS; record++) {
        uint8_t length = record_name_length(record);
        if (offset - cursor_start <= length) {
            cursor_record = record;
            return offset == cursor_start ? length : (uint8_t)record_name(record)[offset - cursor_start - 1u];
        }
        cursor_start += length + 1u;
    }
    cursor_record = NAME_RECORDS;
    return 0u;
}
#endif
