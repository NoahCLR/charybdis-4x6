#include "profile_pd_v1.h"
#include <string.h>

// The record table in pd-mode-domain-v1.md, field by field: the one place a
// byte position is written down. The encoder, the validators below, the
// effective cache and the pointing engine all take positions from the
// structure, so none of them can disagree with another.
#define PD_FIELD_AT(type, member, offset) _Static_assert(offsetof(type, member) == (offset), "PD record field " #member " has moved")
PD_FIELD_AT(noah_pd_config_t, id, 0);
PD_FIELD_AT(noah_pd_config_t, kind, 1);
PD_FIELD_AT(noah_pd_config_t, pointer_layer, 2);
PD_FIELD_AT(noah_pd_config_t, axis, 3);
PD_FIELD_AT(noah_pd_config_t, dpi, 4);
PD_FIELD_AT(noah_pd_config_t, held_modifiers, 6);
PD_FIELD_AT(noah_pd_config_t, reserved, 7);
PD_FIELD_AT(noah_pd_config_t, name_length, NOAH_PROFILE_PD_V1_NAME_LENGTH);
PD_FIELD_AT(noah_pd_config_t, link_head_reserved, 9);
PD_FIELD_AT(noah_pd_config_t, link_ends, 11);
PD_FIELD_AT(noah_pd_config_t, linked_layers, 12);
PD_FIELD_AT(noah_pd_config_t, link_turns_on, 14);
PD_FIELD_AT(noah_pd_config_t, link_reserved, 16);
PD_FIELD_AT(noah_pd_config_t, threshold_x, 32);
PD_FIELD_AT(noah_pd_config_t, threshold_y, 34);
PD_FIELD_AT(noah_pd_config_t, directions, 36);
PD_FIELD_AT(noah_pd_config_t, retired_buttons, 52);
PD_FIELD_AT(noah_pd_config_t, scroll, 70);
PD_FIELD_AT(noah_pd_config_t, scroll.threshold_h, 70);
PD_FIELD_AT(noah_pd_config_t, scroll.threshold_v, 72);
PD_FIELD_AT(noah_pd_config_t, scroll.divisor_h, 74);
PD_FIELD_AT(noah_pd_config_t, scroll.divisor_v, 76);
PD_FIELD_AT(noah_pd_config_t, scroll.interval_ms, 78);
PD_FIELD_AT(noah_pd_config_t, scroll.expiry_ms, 80);
PD_FIELD_AT(noah_pd_config_t, scroll.axis_timeout_ms, 82);
PD_FIELD_AT(noah_pd_config_t, scroll_policy.start_num, 84);
PD_FIELD_AT(noah_pd_config_t, scroll_policy.start_den, 85);
PD_FIELD_AT(noah_pd_config_t, scroll_policy.sustain_num, 86);
PD_FIELD_AT(noah_pd_config_t, scroll_policy.sustain_den, 87);
PD_FIELD_AT(noah_pd_config_t, scroll_policy.decay, 88);
PD_FIELD_AT(noah_pd_config_t, scroll_policy.invert, 89);
PD_FIELD_AT(noah_pd_config_t, diagonals, 70);
PD_FIELD_AT(noah_pd_config_t, empty_direction, 86);
PD_FIELD_AT(noah_pd_config_t, direction_output, 87);
PD_FIELD_AT(noah_pd_config_t, diagonal_reserved, 88);
PD_FIELD_AT(noah_pd_config_t, tail_reserved, 90);
PD_FIELD_AT(noah_pd_config_t, name, NOAH_PROFILE_PD_V1_NAME_OFFSET);
PD_FIELD_AT(noah_pd_tap_t, keycode, 0);
PD_FIELD_AT(noah_pd_tap_t, policy, 2);
PD_FIELD_AT(noah_pd_tap_t, mask, 3);
_Static_assert(sizeof(noah_pd_tap_t) == 4, "a PD tap is 4 bytes");
_Static_assert(sizeof(noah_pd_config_t) == NOAH_PROFILE_PD_V1_RECORD_SIZE, "a PD record is 128 bytes");
// The cache holds the structure, so every 16-bit field reads aligned, and its
// bytes are the wire's only on a little-endian target (the RP2040 and every
// host that builds this).
_Static_assert(_Alignof(noah_pd_config_t) == _Alignof(uint16_t), "a PD record aligns its 16-bit fields");
_Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "PD records are little-endian");

// File-local: where a field sits, and its bytes in any (unaligned) buffer.
#define AT(member)     offsetof(noah_pd_config_t, member)
#define SIZE(member)   sizeof(((const noah_pd_config_t *)0)->member)
#define U8(p, member)  ((p)[AT(member)])
#define U16(p, member) u16((p) + AT(member))
// Every byte of a field, or from one field up to (not including) another.
#define ZERO(p, member)           zero((p) + AT(member), SIZE(member))
#define ZERO_UNTIL(p, first, end) zero((p) + AT(first), AT(end) - AT(first))
#define TAP_AT(member) offsetof(noah_pd_tap_t, member)

void noah_profile_pd_v1_encode_record(const noah_pd_config_t *config, uint8_t output[NOAH_PROFILE_PD_V1_RECORD_SIZE]) {
    uint8_t length = 0;
    while (length < NOAH_PROFILE_PD_V1_NAME_MAX && config->name[length]) length++;
    memcpy(output, config, NOAH_PROFILE_PD_V1_RECORD_SIZE);
    output[AT(name_length)] = length;
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

// The shared counted rule: the length in name_length, the UTF-8 in name, zero
// after it.
static bool name_valid(const uint8_t *record) {
    noah_profile_name_v1_t name;
    uint8_t                length = U8(record, name_length);
    if (!noah_profile_name_v1_begin(&name, length)) return false;
    for (uint8_t i = 0; i < length; i++)
        if (!noah_profile_name_v1_byte(&name, record[AT(name) + i])) return false;
    return noah_profile_name_v1_complete(&name) && zero(record + AT(name) + length, SIZE(name) - length);
}

bool noah_profile_pd_v1_tap_key_valid(uint16_t keycode) {
    if (keycode >= 4 && keycode <= 0xc2) return true;
    // Plain mouse buttons 1-8 click (feature bit 28); movement, wheel and
    // acceleration stay out.
    if (keycode >= 0xd1 && keycode <= 0xd8) return true;
    if (keycode >= 0x7700 && keycode <= 0x777f) return true; // macros 0-127
    uint8_t mods = (uint8_t)(keycode >> 8);
    uint8_t key  = (uint8_t)keycode;
    return mods <= 0x1f && (mods & 0x0f) != 0 && key >= 4 && key <= 0xa4;
}

// One noah_pd_tap_t's bytes. Macro triggers inherit the held modifiers.
static bool tap_valid(const uint8_t *tap) {
    uint16_t key = u16(tap + TAP_AT(keycode));
    if (!key) return zero(tap, sizeof(noah_pd_tap_t));
    if (!noah_profile_pd_v1_tap_key_valid(key)) return false;
    if (key >= 0x7700 && key <= 0x777f) return tap[TAP_AT(policy)] == NOAH_PD_TAP_INHERIT && tap[TAP_AT(mask)] == 0;
    return tap[TAP_AT(policy)] == NOAH_PD_TAP_MASK ? tap[TAP_AT(mask)] != 0 : (tap[TAP_AT(policy)] == NOAH_PD_TAP_INHERIT || tap[TAP_AT(policy)] == NOAH_PD_TAP_EXACT) && tap[TAP_AT(mask)] == 0;
}

noah_profile_pd_v1_result_t noah_profile_pd_v1_validate_record(const uint8_t *p, size_t length, uint8_t slot, noah_profile_pd_v1_error_t *error) {
    if (!p || slot >= NOAH_PROFILE_PD_V1_SLOT_COUNT) return fail(error, NOAH_PROFILE_PD_V1_INVALID_ARGUMENT, 0);
    if (length != NOAH_PROFILE_PD_V1_RECORD_SIZE) return fail(error, NOAH_PROFILE_PD_V1_INVALID_LENGTH, 0);
    if (U8(p, id) != slot) return fail(error, NOAH_PROFILE_PD_V1_INVALID_ID, AT(id));
    if (U8(p, reserved)) return fail(error, NOAH_PROFILE_PD_V1_RESERVED, AT(reserved));
    if (!ZERO(p, link_head_reserved)) return fail(error, NOAH_PROFILE_PD_V1_RESERVED, AT(link_head_reserved));
    if (!ZERO(p, link_reserved)) return fail(error, NOAH_PROFILE_PD_V1_RESERVED, AT(link_reserved));
    if (!ZERO(p, retired_buttons)) return fail(error, NOAH_PROFILE_PD_V1_RESERVED, AT(retired_buttons));
    if (!ZERO(p, tail_reserved)) return fail(error, NOAH_PROFILE_PD_V1_RESERVED, AT(tail_reserved));
    if (!name_valid(p)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_NAME, AT(name_length));
    if (U8(p, kind) > NOAH_PD_KIND_SCROLLING || U8(p, pointer_layer) > NOAH_PD_POINTER_LAYER_PREFER_TYPING || U8(p, axis) > NOAH_PD_AXIS_EIGHT) return fail(error, NOAH_PROFILE_PD_V1_INVALID_POLICY, AT(kind));
    if (U8(p, link_ends) > NOAH_PD_LINK_ENDS_OTHER_LAYER) return fail(error, NOAH_PROFILE_PD_V1_INVALID_POLICY, AT(link_ends));
    if (U8(p, kind) == NOAH_PD_KIND_DISABLED) {
        // Everything but the ID and the name is zero: an empty slot has no link.
        if (!ZERO_UNTIL(p, pointer_layer, name_length) || !ZERO_UNTIL(p, link_ends, link_reserved) || !ZERO_UNTIL(p, threshold_x, tail_reserved)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, AT(pointer_layer));
        return fail(error, NOAH_PROFILE_PD_V1_OK, 0);
    }
    if (!U8(p, name_length)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_NAME, AT(name_length));
    // What ends a mode its linked layers started needs linked layers.
    if (U8(p, link_ends) != NOAH_PD_LINK_ENDS_NOTHING && !U16(p, linked_layers)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, AT(link_ends));
    for (size_t i = AT(directions); i < AT(directions) + SIZE(directions); i += sizeof(noah_pd_tap_t)) {
        if (!tap_valid(p + i)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_ACTION, i);
    }
    if (U8(p, kind) == NOAH_PD_KIND_DIRECTIONAL && U8(p, axis) == NOAH_PD_AXIS_EIGHT) {
        // Both axes are read, so both need a threshold; the diagonals sit
        // beside what an empty direction does and how often a direction sends.
        if (U8(p, held_modifiers)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, AT(held_modifiers));
        if (!U16(p, threshold_x) || !U16(p, threshold_y)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, AT(threshold_x));
        for (size_t i = AT(diagonals); i < AT(diagonals) + SIZE(diagonals); i += sizeof(noah_pd_tap_t)) {
            if (!tap_valid(p + i)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_ACTION, i);
        }
        if (U8(p, empty_direction) > NOAH_PD_EMPTY_DIRECTION_NOTHING) return fail(error, NOAH_PROFILE_PD_V1_INVALID_POLICY, AT(empty_direction));
        if (U8(p, direction_output) > NOAH_PD_DIRECTION_OUTPUT_ONCE) return fail(error, NOAH_PROFILE_PD_V1_INVALID_POLICY, AT(direction_output));
        if (!ZERO(p, diagonal_reserved)) return fail(error, NOAH_PROFILE_PD_V1_RESERVED, AT(diagonal_reserved));
    } else if (U8(p, kind) == NOAH_PD_KIND_DIRECTIONAL) {
        // No diagonals; an empty direction's policy and how often a direction
        // sends still apply. A single axis has no threshold or taps on the
        // other.
        if (U8(p, held_modifiers) || !ZERO(p, diagonals)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, AT(held_modifiers));
        if (U8(p, empty_direction) > NOAH_PD_EMPTY_DIRECTION_NOTHING) return fail(error, NOAH_PROFILE_PD_V1_INVALID_POLICY, AT(empty_direction));
        if (U8(p, direction_output) > NOAH_PD_DIRECTION_OUTPUT_ONCE) return fail(error, NOAH_PROFILE_PD_V1_INVALID_POLICY, AT(direction_output));
        if (!ZERO(p, diagonal_reserved)) return fail(error, NOAH_PROFILE_PD_V1_RESERVED, AT(diagonal_reserved));
        if ((U8(p, axis) != NOAH_PD_AXIS_VERTICAL && !U16(p, threshold_x)) || (U8(p, axis) != NOAH_PD_AXIS_HORIZONTAL && !U16(p, threshold_y))) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, AT(threshold_x));
        if ((U8(p, axis) == NOAH_PD_AXIS_VERTICAL && (U16(p, threshold_x) || !ZERO_UNTIL(p, directions[NOAH_PD_DIRECTION_LEFT], directions[NOAH_PD_DIRECTION_UP]))) ||
            (U8(p, axis) == NOAH_PD_AXIS_HORIZONTAL && (U16(p, threshold_y) || !ZERO_UNTIL(p, directions[NOAH_PD_DIRECTION_UP], retired_buttons)))) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, AT(threshold_x));
    } else {
        // The axis is which axes it scrolls; the directional fields are zero.
        if (U8(p, axis) > NOAH_PD_SCROLL_VERTICAL || !ZERO_UNTIL(p, threshold_x, retired_buttons)) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, AT(axis));
        if (!U16(p, scroll.threshold_h) || !U16(p, scroll.threshold_v) || !U16(p, scroll.divisor_h) || !U16(p, scroll.divisor_v) ||
            !U16(p, scroll.axis_timeout_ms) || U16(p, scroll.expiry_ms) < U16(p, scroll.axis_timeout_ms) ||
            !U8(p, scroll_policy.start_num) || !U8(p, scroll_policy.start_den) || !U8(p, scroll_policy.sustain_num) || !U8(p, scroll_policy.sustain_den) ||
            U8(p, scroll_policy.start_num) < U8(p, scroll_policy.start_den) || U8(p, scroll_policy.sustain_num) < U8(p, scroll_policy.sustain_den) ||
            (uint32_t)U8(p, scroll_policy.sustain_num) * U8(p, scroll_policy.start_den) > (uint32_t)U8(p, scroll_policy.start_num) * U8(p, scroll_policy.sustain_den) ||
            U8(p, scroll_policy.decay) < 2 || U8(p, scroll_policy.invert) > 3) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, AT(scroll));
    }
    return fail(error, NOAH_PROFILE_PD_V1_OK, 0);
}

bool noah_profile_pd_v1_record_present(const uint8_t *record) {
    return record && (U8(record, kind) != NOAH_PD_KIND_DISABLED || U8(record, name_length) != 0u);
}

noah_profile_pd_v1_result_t noah_profile_pd_v1_validate_header(const uint8_t header[8], size_t payload_length, uint8_t *record_count, noah_profile_pd_v1_error_t *error) {
    static const uint8_t expected[3] = {NOAH_PROFILE_PD_V1_VERSION, NOAH_PROFILE_PD_V1_SLOT_COUNT, NOAH_PROFILE_PD_V1_RECORD_SIZE};
    if (!header) return fail(error, NOAH_PROFILE_PD_V1_INVALID_ARGUMENT, 0);
    for (size_t i = 0; i < sizeof(expected); i++) {
        if (header[i] != expected[i]) return fail(error, NOAH_PROFILE_PD_V1_INVALID_HEADER, i);
    }
    if (header[3] > NOAH_PROFILE_PD_V1_SLOT_COUNT) return fail(error, NOAH_PROFILE_PD_V1_INVALID_HEADER, 3);
    for (size_t i = 4; i < NOAH_PROFILE_PD_V1_HEADER_SIZE; i++) {
        if (header[i]) return fail(error, NOAH_PROFILE_PD_V1_RESERVED, i);
    }
    if (payload_length != NOAH_PROFILE_PD_V1_HEADER_SIZE + (size_t)header[3] * NOAH_PROFILE_PD_V1_RECORD_SIZE) return fail(error, NOAH_PROFILE_PD_V1_INVALID_LENGTH, 0);
    if (record_count) *record_count = header[3];
    return fail(error, NOAH_PROFILE_PD_V1_OK, 0);
}

noah_profile_pd_v1_result_t noah_profile_pd_v1_validate_entry(const uint8_t *p, size_t length, uint8_t minimum_slot, noah_profile_pd_v1_error_t *error) {
    if (!p) return fail(error, NOAH_PROFILE_PD_V1_INVALID_ARGUMENT, 0);
    if (length != NOAH_PROFILE_PD_V1_RECORD_SIZE) return fail(error, NOAH_PROFILE_PD_V1_INVALID_LENGTH, 0);
    if (U8(p, id) >= NOAH_PROFILE_PD_V1_SLOT_COUNT || U8(p, id) < minimum_slot) return fail(error, NOAH_PROFILE_PD_V1_INVALID_ID, AT(id));
    noah_profile_pd_v1_result_t result = noah_profile_pd_v1_validate_record(p, length, U8(p, id), error);
    if (result != NOAH_PROFILE_PD_V1_OK) return result;
    if (!noah_profile_pd_v1_record_present(p)) return fail(error, NOAH_PROFILE_PD_V1_NONCANONICAL, AT(kind));
    return fail(error, NOAH_PROFILE_PD_V1_OK, 0);
}

noah_profile_pd_v1_result_t noah_profile_pd_v1_cursor_begin(noah_profile_pd_v1_cursor_t *cursor, const uint8_t header[8], size_t length, noah_profile_pd_v1_error_t *error) {
    if (!cursor) return fail(error, NOAH_PROFILE_PD_V1_INVALID_ARGUMENT, 0);
    memset(cursor, 0, sizeof(*cursor));
    return noah_profile_pd_v1_validate_header(header, length, &cursor->count, error);
}

noah_profile_pd_v1_result_t noah_profile_pd_v1_cursor_next(noah_profile_pd_v1_cursor_t *cursor, const uint8_t record[NOAH_PROFILE_PD_V1_RECORD_SIZE], noah_profile_pd_v1_error_t *error) {
    if (!cursor || cursor->index >= cursor->count) return fail(error, NOAH_PROFILE_PD_V1_INVALID_LENGTH, 0);
    noah_profile_pd_v1_result_t result = noah_profile_pd_v1_validate_entry(record, NOAH_PROFILE_PD_V1_RECORD_SIZE, cursor->minimum, error);
    if (result != NOAH_PROFILE_PD_V1_OK) return result;
    // No layer starts two modes (D-F62). The layers keys turn on may repeat:
    // a key source beats a layer source and a mode its layers started owns no
    // layer, so no link can start another or keep itself on.
    uint16_t linked = U16(record, linked_layers);
    if (cursor->starting_layers & linked) return fail(error, NOAH_PROFILE_PD_V1_INVALID_PARAMETER, AT(linked_layers));
    cursor->starting_layers |= linked;
    cursor->minimum = (uint8_t)(U8(record, id) + 1u);
    cursor->index++;
    return result;
}

uint8_t noah_profile_pd_v1_default_record_count(const noah_pd_config_t defaults[NOAH_PROFILE_PD_V1_SLOT_COUNT]) {
    uint8_t count = 0;
    if (defaults) for (uint8_t slot = 0; slot < NOAH_PROFILE_PD_V1_SLOT_COUNT; slot++) {
        if (defaults[slot].kind || defaults[slot].name[0]) count++;
    }
    return count;
}

bool noah_profile_pd_v1_iterator_complete(const noah_profile_pd_v1_iterator_t *it) {
    return it && it->header && it->cursor.index == it->cursor.count;
}

noah_profile_pd_v1_iteration_t noah_profile_pd_v1_iterator_step(noah_profile_pd_v1_iterator_t *it, const noah_profile_reader_t *reader, size_t base, size_t length, noah_profile_pd_v1_error_t *error) {
    noah_profile_pd_v1_error_t detail = {NOAH_PROFILE_PD_V1_OK, 0};
    if (!it || !reader || base > SIZE_MAX - length) {
        fail(error, NOAH_PROFILE_PD_V1_INVALID_ARGUMENT, 0);
        return NOAH_PROFILE_PD_V1_REJECTED;
    }
    if (length < NOAH_PROFILE_PD_V1_HEADER_SIZE) {
        fail(error, NOAH_PROFILE_PD_V1_INVALID_LENGTH, 0);
        return NOAH_PROFILE_PD_V1_REJECTED;
    }
    if (noah_profile_pd_v1_iterator_complete(it)) return NOAH_PROFILE_PD_V1_COMPLETE;
    size_t offset = it->header ? NOAH_PROFILE_PD_V1_HEADER_SIZE + (size_t)it->cursor.index * NOAH_PROFILE_PD_V1_RECORD_SIZE : 0;
    size_t count = it->header ? NOAH_PROFILE_PD_V1_RECORD_SIZE - it->used : NOAH_PROFILE_PD_V1_HEADER_SIZE;
    if (count > NOAH_PROFILE_PD_V1_STEP_READ_MAX) count = NOAH_PROFILE_PD_V1_STEP_READ_MAX;
    if (!noah_profile_reader_read(reader, base + offset + it->used, it->bytes + it->used, count)) {
        fail(error, NOAH_PROFILE_PD_V1_READ_ERROR, offset + it->used);
        return NOAH_PROFILE_PD_V1_REJECTED;
    }
    if (!it->header) {
        if (noah_profile_pd_v1_cursor_begin(&it->cursor, it->bytes, length, &detail) == NOAH_PROFILE_PD_V1_OK) {
            it->header = true;
            return noah_profile_pd_v1_iterator_complete(it) ? NOAH_PROFILE_PD_V1_COMPLETE : NOAH_PROFILE_PD_V1_ITERATING;
        }
    } else {
        it->used += (uint8_t)count;
        if (it->used != NOAH_PROFILE_PD_V1_RECORD_SIZE) return NOAH_PROFILE_PD_V1_ITERATING;
        if (noah_profile_pd_v1_cursor_next(&it->cursor, it->bytes, &detail) == NOAH_PROFILE_PD_V1_OK) {
            it->used = 0;
            return NOAH_PROFILE_PD_V1_RECORD;
        }
        detail.offset += offset;
    }
    if (error) *error = detail;
    return NOAH_PROFILE_PD_V1_REJECTED;
}

noah_profile_pd_v1_result_t noah_profile_pd_v1_validate(const uint8_t *bytes, size_t length, noah_profile_pd_v1_error_t *error) {
    if (!bytes) return fail(error, NOAH_PROFILE_PD_V1_INVALID_ARGUMENT, 0);
    noah_profile_reader_t reader = noah_profile_reader_from_memory(bytes, length);
    noah_profile_pd_v1_iterator_t iterator = {0};
    noah_profile_pd_v1_error_t detail;
    do {
        if (noah_profile_pd_v1_iterator_step(&iterator, &reader, 0, length, &detail) == NOAH_PROFILE_PD_V1_REJECTED) {
            if (error) *error = detail;
            return detail.code;
        }
    } while (!noah_profile_pd_v1_iterator_complete(&iterator));
    return fail(error, NOAH_PROFILE_PD_V1_OK, 0);
}
