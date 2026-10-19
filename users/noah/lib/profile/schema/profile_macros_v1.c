#include "profile_macros_v1.h"

enum {
    LEXEME_TEXT = 0u,
    LEXEME_CODE,       // after the prefix
    LEXEME_KEYCODE,    // after a tap, press or release code
    LEXEME_PROTECTION, // after the protection code
    LEXEME_DELAY,      // among a delay's digits
    LEXEME_UTF8,       // inside a multi-byte character
};

bool noah_profile_macros_v1_length_valid(uint32_t length, uint8_t slots) {
    return slots != 0u && slots <= NOAH_PROFILE_MACROS_V1_SLOTS && length > 0u && length <= NOAH_PROFILE_MACROS_V1_MAX_SIZE;
}

// The rules macro_payload_utf8_scalar applies when a macro plays: no overlong
// form, surrogate, value above U+10FFFF or C1 control.
static bool utf8_canonical(const uint8_t *bytes, uint8_t count) {
    uint32_t value = bytes[0] & (count == 2u ? 0x1Fu : count == 3u ? 0x0Fu : 0x07u);
    for (uint8_t i = 1u; i < count; i++) {
        if ((bytes[i] & 0xC0u) != 0x80u) return false;
        value = (value << 6u) | (bytes[i] & 0x3Fu);
    }
    return value >= 0xA0u && !(count == 3u && value < 0x800u) && !(count == 4u && value < 0x10000u) && !(value >= 0xD800u && value <= 0xDFFFu) && value <= 0x10FFFFu;
}

static bool end_macro(noah_profile_macros_v1_validation_t *state) {
    state->macros++;
    state->lexeme   = LEXEME_TEXT;
    state->position = 0u;
    return true;
}

static bool consume(noah_profile_macros_v1_validation_t *state, uint8_t byte) {
    switch (state->lexeme) {
        case LEXEME_TEXT:
            if (byte == 0u) return end_macro(state);
            if (byte == NOAH_PROFILE_MACROS_V1_PREFIX) {
                state->lexeme = LEXEME_CODE;
                return true;
            }
            if (byte < 0x80u) return true;
            state->utf8_need = byte >= 0xC2u && byte <= 0xDFu ? 2u : byte >= 0xE0u && byte <= 0xEFu ? 3u : byte >= 0xF0u && byte <= 0xF4u ? 4u : 0u;
            if (!state->utf8_need) return false;
            state->utf8[0]   = byte;
            state->utf8_have = 1u;
            state->lexeme    = LEXEME_UTF8;
            return true;
        case LEXEME_UTF8:
            state->utf8[state->utf8_have++] = byte;
            if (state->utf8_have < state->utf8_need) return (byte & 0xC0u) == 0x80u;
            state->lexeme = LEXEME_TEXT;
            return utf8_canonical(state->utf8, state->utf8_need);
        case LEXEME_CODE:
            switch (byte) {
                case NOAH_PROFILE_MACROS_V1_TAP:
                case NOAH_PROFILE_MACROS_V1_DOWN:
                case NOAH_PROFILE_MACROS_V1_UP:
                    state->lexeme = LEXEME_KEYCODE;
                    return true;
                case NOAH_PROFILE_MACROS_V1_DELAY:
                    state->lexeme = LEXEME_DELAY;
                    return true;
                case NOAH_PROFILE_MACROS_V1_PROTECT:
                    // Only as the macro's first two bytes.
                    state->lexeme = LEXEME_PROTECTION;
                    return state->position == 1u;
                default:
                    return false;
            }
        case LEXEME_KEYCODE:
            state->lexeme = LEXEME_TEXT;
            return byte != 0u;
        case LEXEME_PROTECTION:
            state->lexeme = LEXEME_TEXT;
            return byte == 1u || byte == 2u;
        case LEXEME_DELAY:
            if (byte >= '0' && byte <= '9') return true;
            if (byte == 0u) return end_macro(state);
            state->lexeme = LEXEME_TEXT;
            return true;
        default:
            return false;
    }
}

bool noah_profile_macros_v1_consume(noah_profile_macros_v1_validation_t *state, uint8_t byte, uint8_t slots) {
    if (!state) return false;
    // VIA banks can retain stale bytes after the final macro; their final
    // byte is still the zero validity sentinel. Playback never reads padding.
    state->last_byte = byte;
    if (state->macros == slots) return true;
    uint8_t macros = state->macros;
    if (!consume(state, byte)) return false;
    // A macro that just ended starts the next at zero.
    if (state->macros == macros && state->position < UINT8_MAX) state->position++;
    return true;
}

bool noah_profile_macros_v1_complete(const noah_profile_macros_v1_validation_t *state, uint8_t slots) {
    return state && state->macros == slots && state->lexeme == LEXEME_TEXT && state->position == 0u && state->last_byte == 0u;
}
