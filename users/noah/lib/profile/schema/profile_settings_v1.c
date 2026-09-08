#include "profile_settings_v1.h"
#include <string.h>

bool noah_profile_setting_v1_valid(uint8_t id, uint32_t v, uint8_t layers) {
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
static bool key(uint8_t v) {
    return (v >= 4 && v <= 0xa4) || (v >= 0xe0 && v <= 0xe7);
}
static bool ir_byte(noah_profile_settings_v1_validation_t *s, uint8_t b) {
    if (!s->opcode) {
        if (b < 1 || b > 5) return false;
        s->opcode    = b;
        s->remaining = b == 2 ? 2 : 0;
        s->tap_count = 0;
        return true;
    }
    if (s->opcode == 2) {
        if (!--s->remaining) s->opcode = 0;
        return true;
    }
    if (s->opcode == 3 || s->opcode == 4) {
        if (!key(b)) return false;
        uint8_t i = 0;
        while (i < s->held_count && s->held[i] != b)
            i++;
        if (s->opcode == 3) {
            if (i != s->held_count || s->held_count == 16) return false;
            s->held[s->held_count++] = b;
        } else {
            if (i == s->held_count) return false;
            memmove(&s->held[i], &s->held[i + 1], s->held_count - i - 1);
            s->held_count--;
        }
        s->opcode = 0;
        return true;
    }
    if (!s->remaining) {
        if (!b || (s->opcode == 5 && b > 16)) return false;
        s->remaining = b;
        return true;
    }
    if (s->opcode == 1) {
        if (!(b == 9 || b == 10 || (b >= 32 && b <= 126))) return false;
    } else {
        if (!key(b)) return false;
        for (uint8_t i = 0; i < s->tap_count; i++)
            if (s->tap[i] == b) return false;
        for (uint8_t i = 0; i < s->held_count; i++)
            if (s->held[i] == b) return false;
        s->tap[s->tap_count++] = b;
    }
    if (!--s->remaining) s->opcode = 0;
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
    if (!s || length < NOAH_SETTINGS_FIXED_SIZE + 32 || length > NOAH_SETTINGS_MAX_SIZE || s->offset >= length) return false;
    uint16_t offset = s->offset++;
    if (offset < 8) {
        static const uint8_t header[8] = {1, 8, 28, 16, 0, 0, 0, 0};
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
    if (s->slot >= 16) return false;
    if (s->macro_offset < 2) {
        if (!s->macro_offset)
            s->macro_length = b;
        else {
            s->macro_length |= (uint16_t)b << 8;
            s->macro_total += s->macro_length;
            if (s->macro_length > 512 || s->macro_total > 1024) return false;
        }
        s->macro_offset++;
    } else {
        if (!ir_byte(s, b)) return false;
        s->macro_offset++;
    }
    if (s->macro_offset == s->macro_length + 2) {
        if (s->opcode || s->held_count) return false;
        s->slot++;
        s->macro_offset = 0;
    }
    return true;
}
bool noah_profile_settings_v1_complete(const noah_profile_settings_v1_validation_t *s, uint16_t length) {
    return s && s->offset == length && s->slot == 16 && !s->macro_offset;
}
