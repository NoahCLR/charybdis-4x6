#include "profile_settings_defaults.h"
#include "profile_compiled_writer.h"
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
        case NOAH_SETTING_BEHAVIORS_ENABLED:
            return 1u;
        case NOAH_SETTING_LAYER_BEHAVIORS:
        case NOAH_SETTING_LAYER_COMBOS:
            // Every layer allows its behaviours and combos.
            return (UINT32_C(1) << NOAH_SETTINGS_LAYERS) - 1u;
        default:
            return 0;
    }
}
// The keymap's names fill a settings domain that no profile has stored.
_Static_assert((int)LAYER_COUNT == (int)NOAH_SETTINGS_LAYERS, "Settings name every layer of the bank");
_Static_assert(NOAH_LAYER_NAME_SIZE == NOAH_SETTINGS_NAME_MAX + 1u, "A layer name is at most 32 bytes");
_Static_assert(VIA_MACRO_SLOT_COUNT == NOAH_SETTINGS_MACRO_NAMES, "Settings name every VIA macro");
_Static_assert(NOAH_MACRO_NAME_SIZE == NOAH_SETTINGS_NAME_MAX + 1u, "A macro or custom-key name is at most 32 bytes");
_Static_assert(CUSTOM_KEY_SLOT_COUNT == NOAH_SETTINGS_CUSTOM_KEY_NAMES, "Settings name every custom key");
_Static_assert(MATRIX_ROWS * MATRIX_COLS <= NOAH_SETTINGS_PLACEMENT_MAX_POSITIONS, "A placement bitmap covers the whole matrix");
static uint8_t name_length(const char *name) {
    uint8_t length = 0;
    while (length < NOAH_SETTINGS_NAME_MAX && name[length])
        length++;
    return length;
}
// Name records in order: the layers', the macros', then the custom keys'.
static const char *record_name(uint16_t record) {
    if (record < NOAH_SETTINGS_LAYERS) return layer_names[record];
    record -= NOAH_SETTINGS_LAYERS;
    if (record < NOAH_SETTINGS_MACRO_NAMES) return via_macro_names[record];
    return custom_key_names[record - NOAH_SETTINGS_MACRO_NAMES];
}
uint16_t noah_profile_settings_defaults_length(void) {
    uint16_t length = NOAH_SETTINGS_MIN_SIZE;
    for (uint16_t record = 0; record < NOAH_SETTINGS_NAME_COUNT; record++)
        length += name_length(record_name(record));
    return length;
}
uint8_t noah_profile_settings_defaults_byte(uint16_t offset) {
    if (offset >= NOAH_SETTINGS_SCALARS_OFFSET && offset < NOAH_SETTINGS_LAYER_RECORDS_OFFSET) {
        uint8_t id = (offset - NOAH_SETTINGS_SCALARS_OFFSET) / 4u;
        return noah_profile_settings_default(id) >> (8u * ((offset - NOAH_SETTINGS_SCALARS_OFFSET) % 4u));
    }
    // No profile settings are live: the current version with the keymap's
    // layer, macro and custom-key names.
    const uint8_t header[NOAH_SETTINGS_HEADER_SIZE] = {NOAH_SETTINGS_VERSION, NOAH_SETTINGS_LAYERS, NOAH_SETTINGS_COUNT, NOAH_SETTINGS_MACRO_NAMES, NOAH_SETTINGS_CUSTOM_KEY_NAMES, 0, 0, 0};
    if (offset < NOAH_SETTINGS_HEADER_SIZE) return header[offset];
    if (offset < NOAH_SETTINGS_FIXED_SIZE) {
        // Each layer refers combos to itself and bypasses or excludes nothing.
        uint16_t record = offset - NOAH_SETTINGS_LAYER_RECORDS_OFFSET;
        return record % NOAH_SETTINGS_LAYER_RECORD_SIZE == NOAH_SETTINGS_LAYER_REFERENCE ? (uint8_t)(record / NOAH_SETTINGS_LAYER_RECORD_SIZE) : 0u;
    }
    offset -= NOAH_SETTINGS_FIXED_SIZE;
    // Reads run forward, so resume from the record the last byte was in; the
    // authored names never change, so the cursor stays valid across reads.
    static uint16_t cursor_record;
    static uint16_t cursor_start;
    if (offset < cursor_start) cursor_record = 0u, cursor_start = 0u;
    for (uint16_t record = cursor_record; record < NOAH_SETTINGS_NAME_COUNT; record++) {
        uint8_t length = name_length(record_name(record));
        if (offset - cursor_start <= length) {
            cursor_record = record;
            return offset == cursor_start ? length : (uint8_t)record_name(record)[offset - cursor_start - 1u];
        }
        cursor_start += length + 1u;
    }
    cursor_record = NOAH_SETTINGS_NAME_COUNT;
    return 0u;
}
#endif

// The factory settings domain: the bytes above, in order. A build without
// portable profiles has no settings domain.
noah_profile_compiled_v1_result_t noah_profile_settings_compiled_v1_write(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
    (void)error;
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
    uint16_t length = noah_profile_settings_defaults_length();
    for (uint16_t offset = 0; offset < length; offset++)
        if (!emit_u8(writer, noah_profile_settings_defaults_byte(offset))) return writer->result;
    return writer->result;
#else
    (void)writer;
    return NOAH_PROFILE_COMPILED_V1_OK;
#endif
}
