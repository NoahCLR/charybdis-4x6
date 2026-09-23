#include "profile_pd_v1.h"
#include <string.h>

_Static_assert(sizeof(noah_pd_config_t) == 96 && offsetof(noah_pd_config_t, scroll) == 70 && offsetof(noah_pd_config_t, directions) == 36 && offsetof(noah_pd_config_t, diagonals) == 70 && offsetof(noah_pd_config_t, empty_diagonal) == 86, "PD native structure layout drift");

static void write_u16(uint8_t *p, uint16_t value) { p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8); }
void noah_profile_pd_v1_encode_record(const noah_pd_config_t *config, uint8_t output[96]) {
    memcpy(output, config, 96);
    write_u16(output + 4, config->dpi);
    write_u16(output + 32, config->threshold_x);
    write_u16(output + 34, config->threshold_y);
    for (uint8_t i = 0; i < 4; i++) write_u16(output + 36 + i * 4, config->directions[i].keycode);
    for (uint8_t i = 0; i < 3; i++) write_u16(output + 54 + i * 6, config->buttons[i].tap.keycode);
    if (config->kind == 2u) {
        for (uint8_t i = 0; i < 7; i++) write_u16(output + 70 + i * 2, config->scroll[i]);
    } else {
        for (uint8_t i = 0; i < 4; i++) write_u16(output + 70 + i * 4, config->diagonals[i].keycode);
    }
}

static noah_profile_pd_v1_result_t fail(noah_profile_pd_v1_error_t *error, noah_profile_pd_v1_result_t code, size_t offset) {
    if (error) *error = (noah_profile_pd_v1_error_t){code, offset};
    return code;
}

static uint16_t u16(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}

static bool zero(const uint8_t *p, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (p[i]) return false;
    }
    return true;
}

static bool name_valid(const uint8_t *p) {
    uint8_t remaining = 0, minimum = 0x80, maximum = 0xbf;
    for (size_t i = 0; i < NOAH_PROFILE_PD_V1_NAME_SIZE; i++) {
        uint8_t b = p[i];
        if (remaining) {
            if (b < minimum || b > maximum) return false;
            remaining--;
            minimum = 0x80;
            maximum = 0xbf;
        } else if (b == 0) {
            return zero(p + i, NOAH_PROFILE_PD_V1_NAME_SIZE - i);
        } else if (b < 0x20 || b == 0x7f) {
            return false;
        } else if (b >= 0x80) {
            if (b >= 0xc2 && b <= 0xdf) {
                remaining = 1;
            } else if (b >= 0xe0 && b <= 0xef) {
                remaining = 2;
                if (b == 0xe0) minimum = 0xa0;
                if (b == 0xed) maximum = 0x9f;
            } else if (b >= 0xf0 && b <= 0xf4) {
                remaining = 3;
                if (b == 0xf0) minimum = 0x90;
                if (b == 0xf4) maximum = 0x8f;
            } else {
                return false;
            }
        }
    }
    return false; // Includes unterminated names and partial UTF-8 at the boundary.
}

bool noah_profile_pd_v1_tap_key_valid(uint16_t keycode) {
    if (keycode >= 4 && keycode <= 0xc2) return true;
    uint8_t mods = (uint8_t)(keycode >> 8);
    uint8_t key  = (uint8_t)keycode;
    return mods <= 0x1f && (mods & 0x0f) != 0 && key >= 4 && key <= 0xa4;
}

static bool tap_valid(const uint8_t *p) {
    uint16_t key = u16(p);
    if (!key) return zero(p, 4);
    if (!noah_profile_pd_v1_tap_key_valid(key)) return false;
    return p[2] == 1 ? p[3] != 0 : (p[2] == 0 || p[2] == 2) && p[3] == 0;
}

noah_profile_pd_v1_result_t noah_profile_pd_v1_validate_record(const uint8_t *p, size_t length, uint8_t slot, noah_profile_pd_v1_error_t *error) {
    if (!p || slot >= NOAH_PROFILE_PD_V1_SLOT_COUNT) return fail(error, NOAH_PROFILE_PD_V1_INVALID_ARGUMENT, 0);
    if (length != NOAH_PROFILE_PD_V1_RECORD_SIZE) return fail(error, NOAH_PROFILE_PD_V1_INVALID_LENGTH, 0);
    if (p[0] != slot) return fail(error, NOAH_PROFILE_PD_V1_INVALID_ID, 0);
    if (p[7] || !zero(p + 90, 6)) return fail(error, NOAH_PROFILE_PD_V1_RESERVED, p[7] ? 7 : 90);
    if (!name_valid(p + 8)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_NAME, 8);
    if (p[1] > 2 || p[2] > 1 || p[3] > NOAH_PD_AXIS_EIGHT) return fail(error, NOAH_PROFILE_PD_V1_INVALID_POLICY, 1);
    if (p[1] == 0) {
        if (!zero(p + 2, 6) || !zero(p + 32, 64)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, 2);
        return fail(error, NOAH_PROFILE_PD_V1_OK, 0);
    }
    if (!p[8]) return fail(error, NOAH_PROFILE_PD_V1_INVALID_NAME, 8);
    for (size_t i = 36; i < 52; i += 4) {
        if (!tap_valid(p + i)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_ACTION, i);
    }
    for (size_t i = 52; i < 70; i += 6) {
        uint8_t kind = p[i];
        if (kind > 3 || (kind <= 1 && !zero(p + i + 1, 5)) ||
            (kind == 2 && (p[i + 1] || !u16(p + i + 2) || !tap_valid(p + i + 2))) ||
            (kind == 3 && (!p[i + 1] || !zero(p + i + 2, 4)))) {
            return fail(error, NOAH_PROFILE_PD_V1_INVALID_ACTION, i);
        }
    }
    if (p[1] == 1 && p[3] == NOAH_PD_AXIS_EIGHT) {
        // Both axes are read, so both need a threshold; bytes 70..86 carry
        // the diagonals and byte 86 what an empty one does.
        if (p[6]) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, 6);
        if (!u16(p + 32) || !u16(p + 34)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, 32);
        for (size_t i = 70; i < 86; i += 4) {
            if (!tap_valid(p + i)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_ACTION, i);
        }
        if (p[86] > NOAH_PD_EMPTY_DIAGONAL_NOTHING) return fail(error, NOAH_PROFILE_PD_V1_INVALID_POLICY, 86);
        if (!zero(p + 87, 3)) return fail(error, NOAH_PROFILE_PD_V1_RESERVED, 87);
    } else if (p[1] == 1) {
        if (p[6] || !zero(p + 70, 20)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, 6);
        if ((p[3] != 0 && !u16(p + 32)) || (p[3] != 1 && !u16(p + 34))) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, 32);
        if ((p[3] == 0 && (u16(p + 32) || !zero(p + 36, 8))) ||
            (p[3] == 1 && (u16(p + 34) || !zero(p + 44, 8)))) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, 32);
    } else {
        if (p[3] || !zero(p + 32, 20)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, 3);
        if (!u16(p + 70) || !u16(p + 72) || !u16(p + 74) || !u16(p + 76) ||
            !u16(p + 82) || u16(p + 80) < u16(p + 82) || !p[84] || !p[85] || !p[86] || !p[87] ||
            p[84] < p[85] || p[86] < p[87] || (uint32_t)p[86] * p[85] > (uint32_t)p[84] * p[87] ||
            p[88] < 2 || p[89] > 3) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, 70);
    }
    return fail(error, NOAH_PROFILE_PD_V1_OK, 0);
}

noah_profile_pd_v1_result_t noah_profile_pd_v1_validate(const uint8_t *bytes, size_t length, noah_profile_pd_v1_error_t *error) {
    static const uint8_t header[8] = {1, 8, 96, 0, 0, 0, 0, 0};
    if (!bytes) return fail(error, NOAH_PROFILE_PD_V1_INVALID_ARGUMENT, 0);
    if (length != NOAH_PROFILE_PD_V1_SIZE) return fail(error, NOAH_PROFILE_PD_V1_INVALID_LENGTH, 0);
    for (size_t i = 0; i < sizeof(header); i++) {
        if (bytes[i] != header[i]) return fail(error, NOAH_PROFILE_PD_V1_INVALID_HEADER, i);
    }
    for (uint8_t slot = 0; slot < NOAH_PROFILE_PD_V1_SLOT_COUNT; slot++) {
        size_t offset = NOAH_PROFILE_PD_V1_HEADER_SIZE + slot * NOAH_PROFILE_PD_V1_RECORD_SIZE;
        noah_profile_pd_v1_result_t result = noah_profile_pd_v1_validate_record(bytes + offset, NOAH_PROFILE_PD_V1_RECORD_SIZE, slot, error);
        if (result != NOAH_PROFILE_PD_V1_OK) {
            if (error) error->offset += offset;
            return result;
        }
    }
    return fail(error, NOAH_PROFILE_PD_V1_OK, 0);
}
