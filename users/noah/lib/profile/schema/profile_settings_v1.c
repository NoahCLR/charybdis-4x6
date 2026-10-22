#include "profile_settings_v1.h"
#include <string.h>

#include "../../compat/qmk_host_setting.h"

bool noah_profile_setting_v1_valid(uint8_t id, uint32_t v, uint8_t layers) {
    if (id >= NOAH_SETTING_DRAGSCROLL_DPI && id <= NOAH_SETTING_ARROW_DPI) return v == 0;
    uint32_t bank = layers >= 32u ? UINT32_MAX : (UINT32_C(1) << layers) - 1u;
    switch (id) {
        case NOAH_SETTING_UNICODE_HOST_MODE:
            return (v & ~NOAH_HOST_SETTING_BITS) == 0u && noah_host_layout_id(v) < NOAH_HOST_LAYOUT_LIMIT;
        case NOAH_SETTING_FEEDBACK_PERIOD:
            return v > 0 && v <= 65535;
        case NOAH_SETTING_AUTO_MOUSE_ENABLED:
        case NOAH_SETTING_AUTO_SNIPING_ENABLED:
        case NOAH_SETTING_COMBOS_ENABLED:
        case NOAH_SETTING_BEHAVIORS_ENABLED:
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
            return v && !(v & ~bank);
        case NOAH_SETTING_LAYER_BEHAVIORS:
        case NOAH_SETTING_LAYER_COMBOS:
            return !(v & ~bank);

        case NOAH_SETTING_DEFAULT_DPI:
            return v >= 400 && v <= 3400 && v % 200 == 0;
        case NOAH_SETTING_SNIPING_DPI:
            return v >= 100 && v <= 400 && v % 100 == 0;
        default:
            return id < NOAH_SETTINGS_COUNT && v <= 65535;
    }
}

static bool layer_record_byte(uint8_t b, uint16_t offset, uint8_t layers, uint8_t positions) {
    uint8_t field = (uint8_t)((offset - NOAH_SETTINGS_LAYER_RECORDS_OFFSET) % NOAH_SETTINGS_LAYER_RECORD_SIZE);
    if (field == NOAH_SETTINGS_LAYER_REFERENCE) return b < layers;
    uint8_t bitmap_byte = (uint8_t)((field - NOAH_SETTINGS_LAYER_BYPASS) % NOAH_SETTINGS_PLACEMENT_BYTES);
    uint8_t first_bit   = (uint8_t)(bitmap_byte * 8u);
    if (first_bit >= positions) return b == 0;
    uint8_t used = positions - first_bit;
    return used >= 8u || !(b >> used);
}

// A shortcut record is its report modifier mask, any eight bits, then its
// key, from the shortcut vocabulary (custom_key_shortcut.h).
static bool shortcut_byte(uint8_t b, uint16_t offset) {
    return (offset - NOAH_SETTINGS_SHORTCUTS_OFFSET) % NOAH_SETTINGS_SHORTCUT_SIZE != NOAH_CUSTOM_KEY_SHORTCUT_KEY || noah_custom_key_shortcut_key_valid(b);
}

static bool name_byte(noah_profile_settings_v1_validation_t *s, uint8_t b) {
    if (s->name >= NOAH_SETTINGS_NAME_COUNT) return false;
    if (!s->in_name) {
        if (!noah_profile_name_v1_begin(&s->current, b)) return false;
        if (b)
            s->in_name = true;
        else
            s->name++;
        return true;
    }
    if (!noah_profile_name_v1_byte(&s->current, b)) return false;
    if (!s->current.remaining) {
        if (!noah_profile_name_v1_complete(&s->current)) return false;
        s->in_name = false;
        s->name++;
    }
    return true;
}

bool noah_profile_settings_v1_consume(noah_profile_settings_v1_validation_t *s, uint8_t b, uint32_t length, uint8_t layers, uint8_t positions) {
    if (!s || layers != NOAH_SETTINGS_LAYERS || positions > NOAH_SETTINGS_PLACEMENT_MAX_POSITIONS || length < NOAH_SETTINGS_MIN_SIZE || length > NOAH_SETTINGS_MAX_SIZE || s->offset >= length) return false;
    uint16_t offset = s->offset++;
    if (offset < NOAH_SETTINGS_HEADER_SIZE) {
        static const uint8_t header[NOAH_SETTINGS_HEADER_SIZE] = {NOAH_SETTINGS_VERSION, NOAH_SETTINGS_LAYERS, NOAH_SETTINGS_COUNT, NOAH_SETTINGS_MACRO_NAMES, NOAH_SETTINGS_CUSTOM_KEY_NAMES, 0, NOAH_SETTINGS_CUSTOM_KEY_SHORTCUTS, 0};
        return offset == NOAH_SETTINGS_FLAGS_OFFSET ? !(b & (uint8_t)~NOAH_SETTINGS_KNOWN_FLAGS) : b == header[offset];
    }
    uint8_t id, part;
    if (noah_profile_settings_v1_scalar_at(offset, &id, &part)) {
        if (!part) s->value = 0;
        s->value |= noah_profile_settings_v1_scalar_bits(b, part);
        if (part != NOAH_SETTINGS_SCALAR_SIZE - 1) return true;
        if (!noah_profile_setting_v1_valid(id, s->value, layers)) return false;
        if (id == NOAH_SETTING_AUTO_MOUSE_TIMEOUT) s->timeout = s->value;
        if (id == NOAH_SETTING_AUTO_MOUSE_DEAD_TIME) {
            s->dead_time = s->value;
            if (!s->timeout || s->dead_time >= s->timeout) return false;
        }
        return true;
    }
    if (offset < NOAH_SETTINGS_SHORTCUTS_OFFSET) return layer_record_byte(b, offset, layers, positions);
    if (offset < NOAH_SETTINGS_FIXED_SIZE) return shortcut_byte(b, offset);
    return name_byte(s, b);
}

bool noah_profile_settings_v1_complete(const noah_profile_settings_v1_validation_t *s, uint32_t length) {
    return s && s->offset == length && s->name == NOAH_SETTINGS_NAME_COUNT && !s->in_name;
}

bool noah_profile_settings_v1_keyboard_name(const noah_profile_reader_t *reader, size_t offset, size_t length, noah_profile_keyboard_name_t *name) {
    if (!name) return false;
    memset(name, 0, sizeof(*name));
    uint8_t byte;
    if (length < NOAH_SETTINGS_MIN_SIZE || length > NOAH_SETTINGS_MAX_SIZE || !noah_profile_reader_read(reader, offset, &byte, 1) || byte != NOAH_SETTINGS_VERSION) return false;
    // Skip every name before the keyboard's: each is its length byte and bytes.
    size_t at = NOAH_SETTINGS_FIXED_SIZE;
    for (uint16_t record = 0; record < NOAH_SETTINGS_KEYBOARD_NAME_RECORD; record++) {
        if (at >= length || !noah_profile_reader_read(reader, offset + at, &byte, 1)) return false;
        at += 1u + byte;
    }
    noah_profile_name_v1_t rule;
    uint8_t                bytes[NOAH_SETTINGS_NAME_MAX];
    if (at >= length || !noah_profile_reader_read(reader, offset + at, &byte, 1) || !noah_profile_name_v1_begin(&rule, byte) || at + 1u + byte != length || !noah_profile_reader_read(reader, offset + at + 1u, bytes, byte)) return false;
    for (uint8_t i = 0; i < byte; i++)
        if (!noah_profile_name_v1_byte(&rule, bytes[i])) return false;
    if (!noah_profile_name_v1_complete(&rule)) return false;
    name->length = byte;
    memcpy(name->bytes, bytes, byte);
    return true;
}

bool noah_profile_keyboard_name_equal(const noah_profile_keyboard_name_t *a, const noah_profile_keyboard_name_t *b) {
    return a && b && a->length == b->length && !memcmp(a->bytes, b->bytes, a->length);
}
