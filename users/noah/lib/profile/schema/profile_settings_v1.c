#include "profile_settings_v1.h"
#include <string.h>

bool noah_profile_setting_v1_valid(uint8_t id, uint32_t v, uint8_t layers) {
    if (id >= NOAH_SETTING_DRAGSCROLL_DPI && id <= NOAH_SETTING_ARROW_DPI) return v == 0;
    switch (id) {
        case NOAH_SETTING_FEEDBACK_PERIOD:
            return v > 0 && v <= 65535;
        case NOAH_SETTING_AUTO_MOUSE_ENABLED:
        case NOAH_SETTING_AUTO_SNIPING_ENABLED:
        case NOAH_SETTING_COMBOS_ENABLED:
            return v <= 1;
        case NOAH_SETTING_AUTO_MOUSE_LAYER:
        case NOAH_SETTING_AUTO_SNIPING_LAYER:
            return v < layers;
        case NOAH_SETTING_AUTO_MOUSE_DEBOUNCE:
            return v <= 255;
        case NOAH_SETTING_RGB_TIMEOUT:
            return v <= 86400000;
        case NOAH_SETTING_RGB_MODE:
            return (v & 0xff) <= 1;
        case NOAH_SETTING_RGB_COLOR:
            return !(v >> 24);
        case NOAH_SETTING_DEFAULT_LAYERS:
            return v && !(v >> layers);
        case NOAH_SETTING_COMBO_REFERENCES:
            for (uint8_t i = 0; i < 8; i++)
                if (((v >> (i * 4)) & 15) >= layers) return false;
            return true;
        case NOAH_SETTING_DEFAULT_DPI:
            return v >= 400 && v <= 3400 && v % 200 == 0;
        case NOAH_SETTING_SNIPING_DPI:
            return v >= 100 && v <= 400 && v % 100 == 0;
        default:
            return id < NOAH_SETTINGS_COUNT && v <= 65535;
    }
}
static bool macro_name_byte(noah_profile_settings_v1_validation_t *s, uint8_t b) {
    if (s->slot >= NOAH_SETTINGS_MACRO_NAMES + NOAH_SETTINGS_CUSTOM_KEY_NAMES) return false;
    if (!s->macro_offset) {
        if (b > NOAH_SETTINGS_MACRO_NAME_ASCII_MAX) return false;
        s->macro_length = b;
        if (b)
            s->macro_offset = 1;
        else
            s->slot++;
        return true;
    }
    if (b < 0x20 || b > 0x7e) return false;
    if (s->macro_offset++ == s->macro_length) {
        s->slot++;
        s->macro_offset = 0;
    }
    return true;
}
static bool name_byte(noah_profile_settings_v1_validation_t *s, uint8_t b, uint8_t column) {
    if (!column) {
        s->name_ended     = false;
        s->utf8_remaining = 0;
    }
    if (s->name_ended) return b == 0;
    if (s->utf8_remaining) {
        if (b < s->utf8_min || b > s->utf8_max) return false;
        s->utf8_remaining--;
        s->utf8_min = 0x80;
        s->utf8_max = 0xbf;
    } else if (!b)
        s->name_ended = true;
    else if (b < 0x20 || b == 0x7f)
        return false;
    else if (b >= 0x80) {
        s->utf8_min = 0x80;
        s->utf8_max = 0xbf;
        if (b >= 0xc2 && b <= 0xdf)
            s->utf8_remaining = 1;
        else if (b >= 0xe0 && b <= 0xef) {
            s->utf8_remaining = 2;
            if (b == 0xe0) s->utf8_min = 0xa0;
            if (b == 0xed) s->utf8_max = 0x9f;
        } else if (b >= 0xf0 && b <= 0xf4) {
            s->utf8_remaining = 3;
            if (b == 0xf0) s->utf8_min = 0x90;
            if (b == 0xf4) s->utf8_max = 0x8f;
        } else
            return false;
    }
    return column != 23 || s->name_ended;
}
bool noah_profile_settings_v1_consume(noah_profile_settings_v1_validation_t *s, uint8_t b, uint16_t length, uint8_t layers) {
    if (!s || length < NOAH_SETTINGS_FIXED_SIZE + NOAH_SETTINGS_MACRO_NAMES + NOAH_SETTINGS_CUSTOM_KEY_NAMES || length > NOAH_SETTINGS_MAX_SIZE || s->offset >= length) return false;
    uint16_t offset = s->offset++;
    if (offset < 8) {
        static const uint8_t header[8] = {0, 8, 28, 0, 0, 0, 0, 0};
        if (!offset) {
            if (!NOAH_PROFILE_SETTINGS_VERSION_ACCEPTED(b) || (s->expected_version && b != s->expected_version)) return false;
            s->version = b;
            return true;
        }
        if (offset == 3) return b == NOAH_SETTINGS_MACRO_NAMES;
        if (offset == 4) return b == NOAH_SETTINGS_CUSTOM_KEY_NAMES;
        return b == header[offset];
    }
    if (offset < 8 + 28 * 4) {
        uint8_t id = (offset - 8) / 4, part = (offset - 8) % 4;
        if (!part) s->value = 0;
        s->value |= (uint32_t)b << (part * 8);
        if (part != 3) return true;
        if (!noah_profile_setting_v1_valid(id, s->value, layers)) return false;
        if (id == NOAH_SETTING_AUTO_MOUSE_TIMEOUT) s->timeout = s->value;
        if (id == NOAH_SETTING_AUTO_MOUSE_DEAD_TIME) {
            s->dead_time = s->value;
            if (!s->timeout || s->dead_time >= s->timeout) return false;
        }
        return true;
    }
    if (offset < NOAH_SETTINGS_FIXED_SIZE) return name_byte(s, b, (offset - 120) % 24);
    return macro_name_byte(s, b);
}
bool noah_profile_settings_v1_complete(const noah_profile_settings_v1_validation_t *s, uint16_t length) {
    return s && s->offset == length && s->slot == NOAH_SETTINGS_MACRO_NAMES + NOAH_SETTINGS_CUSTOM_KEY_NAMES && !s->macro_offset;
}
