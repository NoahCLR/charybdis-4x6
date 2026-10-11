#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "send_string.h"

#include "macro_payload_internal.h"

static bool macro_payload_ir_write_byte(macro_payload_ir_t *ir, uint8_t byte) {
    if (!ir || ir->length >= sizeof(ir->bytes)) {
        return false;
    }

    ir->bytes[ir->length++] = byte;
    return true;
}

static bool macro_payload_ir_write_delay(macro_payload_ir_t *ir, uint16_t delay_ms) {
    return macro_payload_ir_write_byte(ir, MACRO_PAYLOAD_IR_OP_DELAY) && macro_payload_ir_write_byte(ir, (uint8_t)(delay_ms & 0xFFu)) && macro_payload_ir_write_byte(ir, (uint8_t)(delay_ms >> 8));
}

static bool macro_payload_ir_write_key_action(macro_payload_ir_t *ir, macro_payload_hold_balance_t *balance, macro_payload_ir_opcode_t opcode, uint8_t keycode) {
    if (opcode == MACRO_PAYLOAD_IR_OP_KEY_DOWN && !macro_payload_hold_balance_note_down(balance, keycode)) {
        return false;
    }
    if (opcode == MACRO_PAYLOAD_IR_OP_KEY_UP && !macro_payload_hold_balance_note_up(balance, keycode)) {
        return false;
    }

    return macro_payload_ir_write_byte(ir, opcode) && macro_payload_ir_write_byte(ir, keycode);
}

static bool macro_payload_ir_write_tap_list(macro_payload_ir_t *ir, const uint8_t *keycodes, uint8_t count) {
    if (!keycodes || count == 0 || count > MACRO_PAYLOAD_MAX_TAP_KEYS) {
        return false;
    }

    if (!macro_payload_ir_write_byte(ir, MACRO_PAYLOAD_IR_OP_TAP_LIST) || !macro_payload_ir_write_byte(ir, count)) {
        return false;
    }

    for (uint8_t i = 0; i < count; i++) {
        if (!macro_payload_ir_write_byte(ir, keycodes[i])) {
            return false;
        }
    }

    return true;
}

static bool macro_payload_ir_write_single_tap(macro_payload_ir_t *ir, uint8_t keycode) {
    return macro_payload_ir_write_tap_list(ir, &keycode, 1);
}

typedef struct {
    bool     active;
    uint16_t length_index;
} macro_payload_text_chunk_t;

static bool macro_payload_ir_append_text_byte(macro_payload_ir_t *ir, macro_payload_text_chunk_t *chunk, uint8_t byte) {
    if (!ir || !chunk) {
        return false;
    }

    if (!chunk->active || ir->bytes[chunk->length_index] == UINT8_MAX) {
        if (!macro_payload_ir_write_byte(ir, MACRO_PAYLOAD_IR_OP_TEXT) || !macro_payload_ir_write_byte(ir, 0)) {
            return false;
        }

        chunk->active       = true;
        chunk->length_index = ir->length - 1u;
    }

    if (!macro_payload_ir_write_byte(ir, byte)) {
        return false;
    }

    ir->bytes[chunk->length_index]++;
    return true;
}

static bool macro_payload_flush_pending_qmk_sequence(macro_payload_ir_t *ir, macro_payload_hold_balance_t *balance, const uint8_t *pending_downs, uint8_t pending_down_count, bool pending_tap, uint8_t pending_tap_key, uint8_t matched_up_count) {
    for (uint8_t i = 0; i < pending_down_count; i++) {
        if (!macro_payload_ir_write_key_action(ir, balance, MACRO_PAYLOAD_IR_OP_KEY_DOWN, pending_downs[i])) {
            return false;
        }
    }

    if (pending_tap && !macro_payload_ir_write_single_tap(ir, pending_tap_key)) {
        return false;
    }

    for (uint8_t i = 0; i < matched_up_count; i++) {
        uint8_t keycode = pending_downs[pending_down_count - 1u - i];

        if (!macro_payload_ir_write_key_action(ir, balance, MACRO_PAYLOAD_IR_OP_KEY_UP, keycode)) {
            return false;
        }
    }

    return true;
}

// A clean point sits between whole steps, with no key press or tap pending.
// The most one span between clean points writes is under 72 bytes (sixteen
// presses, a tap and fifteen matched releases, then one more step), so a window
// that fails this close to full may only have run out of room. It ends at its
// last clean point; a real error then fails the next window at its start.
#define MACRO_PAYLOAD_QMK_SPAN_MAX_BYTES 128u

typedef struct {
    uint16_t                     offset;
    uint16_t                     ir_length;
    macro_payload_hold_balance_t balance;
} macro_payload_qmk_clean_point_t;

bool macro_payload_decode_qmk_window(macro_payload_ir_t *ir, macro_payload_stream_cursor_t *cursor, uint16_t length, macro_payload_read_byte_fn read_byte, void *context) {
    macro_payload_text_chunk_t      text_chunk                                = {0};
    macro_payload_hold_balance_t    balance                                   = {0};
    macro_payload_qmk_clean_point_t clean                                     = {0};
    uint32_t                        delay_ms                                  = 0;
    uint8_t                         pending_downs[MACRO_PAYLOAD_MAX_TAP_KEYS] = {0};
    uint8_t                         pending_down_count                        = 0;
    bool                            pending_tap                               = false;
    uint8_t                         pending_tap_key                           = 0;
    uint8_t                         matched_up_count                          = 0;

    if (!ir || !cursor || !read_byte) {
        return false;
    }

    ir->length     = 0;
    ir->more       = false;
    ir->protection = cursor->protection;
    balance        = cursor->balance;

    for (uint16_t offset = cursor->offset; offset < length;) {
        uint8_t byte = 0;

        if (pending_down_count == 0u && !pending_tap) {
            clean = (macro_payload_qmk_clean_point_t){.offset = offset, .ir_length = ir->length, .balance = balance};
        }

        if (!read_byte(offset++, &byte, context)) {
            goto fail;
        }

        if (byte == 0) {
            if (!macro_payload_flush_pending_qmk_sequence(ir, &balance, pending_downs, pending_down_count, pending_tap, pending_tap_key, matched_up_count) || !macro_payload_hold_balance_is_clear(&balance)) {
                goto fail;
            }
            return true;
        }

        if (byte != SS_QMK_PREFIX) {
            uint8_t utf8[4] = {byte};
            uint8_t width = 1u;
            uint32_t scalar = byte;
            if (byte >= 0x80u) {
                uint8_t expected = byte >= 0xC2u && byte <= 0xDFu ? 2u : byte >= 0xE0u && byte <= 0xEFu ? 3u : byte >= 0xF0u && byte <= 0xF4u ? 4u : 0u;
                if (!expected || offset + expected - 1u > length) goto fail;
                for (uint8_t i = 1u; i < expected; i++) {
                    if (!read_byte(offset++, &utf8[i], context)) goto fail;
                }
                if (!macro_payload_utf8_scalar(utf8, expected, &scalar, &width)) goto fail;
            }
            if (width == 1u && !macro_payload_text_byte_is_supported(byte)) {
                goto fail;
            }

            if (!macro_payload_flush_pending_qmk_sequence(ir, &balance, pending_downs, pending_down_count, pending_tap, pending_tap_key, matched_up_count)) {
                goto fail;
            }
            pending_down_count = 0;
            pending_tap        = false;
            pending_tap_key    = 0;
            matched_up_count   = 0;

            if (width > 1u) text_chunk.active = false;
            if (!(width > 1u ? macro_payload_ir_append_unicode(ir, scalar) : macro_payload_ir_append_text_byte(ir, &text_chunk, byte))) {
                goto fail;
            }
            continue;
        }

        text_chunk.active = false;

        if (offset >= length || !read_byte(offset++, &byte, context)) {
            goto fail;
        }

        switch (byte) {
            case 5u:
                // A single optional policy prefix, before any executable
                // content. Zero is represented by no prefix, canonically.
                if (offset != 2u || offset >= length || !read_byte(offset++, &byte, context) || byte < 1u || byte > 2u) {
                    goto fail;
                }
                ir->protection = byte;
                break;
            case SS_TAP_CODE: {
                uint8_t keycode = 0;

                if (offset >= length || !read_byte(offset++, &keycode, context)) {
                    goto fail;
                }

                if (pending_tap) {
                    if (!macro_payload_flush_pending_qmk_sequence(ir, &balance, pending_downs, pending_down_count, pending_tap, pending_tap_key, matched_up_count)) {
                        goto fail;
                    }
                    // The flushed tap is written; left pending, a later
                    // release would play it again.
                    pending_down_count = 0;
                    pending_tap        = false;
                    pending_tap_key    = 0;
                    matched_up_count   = 0;
                }

                if (pending_down_count == 0) {
                    if (!macro_payload_ir_write_single_tap(ir, keycode)) {
                        goto fail;
                    }
                    break;
                }

                pending_tap      = true;
                pending_tap_key  = keycode;
                matched_up_count = 0;
                break;
            }
            case SS_DOWN_CODE:
                if (offset >= length || !read_byte(offset++, &byte, context)) {
                    goto fail;
                }

                if (pending_tap) {
                    if (!macro_payload_flush_pending_qmk_sequence(ir, &balance, pending_downs, pending_down_count, pending_tap, pending_tap_key, matched_up_count)) {
                        goto fail;
                    }
                    pending_down_count = 0;
                    pending_tap        = false;
                    pending_tap_key    = 0;
                    matched_up_count   = 0;
                }

                if (pending_down_count >= MACRO_PAYLOAD_MAX_TAP_KEYS) {
                    goto fail;
                }

                pending_downs[pending_down_count++] = byte;
                break;
            case SS_UP_CODE: {
                uint8_t keycode = 0;

                if (offset >= length || !read_byte(offset++, &keycode, context)) {
                    goto fail;
                }

                if (pending_tap && matched_up_count < pending_down_count && keycode == pending_downs[pending_down_count - 1u - matched_up_count]) {
                    matched_up_count++;
                    if (matched_up_count == pending_down_count) {
                        uint8_t tap_list[MACRO_PAYLOAD_MAX_TAP_KEYS + 1] = {0};

                        for (uint8_t i = 0; i < pending_down_count; i++) {
                            tap_list[i] = pending_downs[i];
                        }
                        tap_list[pending_down_count] = pending_tap_key;

                        if (!macro_payload_ir_write_tap_list(ir, tap_list, (uint8_t)(pending_down_count + 1u))) {
                            goto fail;
                        }

                        pending_down_count = 0;
                        pending_tap        = false;
                        pending_tap_key    = 0;
                        matched_up_count   = 0;
                    }
                    break;
                }

                if (!macro_payload_flush_pending_qmk_sequence(ir, &balance, pending_downs, pending_down_count, pending_tap, pending_tap_key, matched_up_count) || !macro_payload_ir_write_key_action(ir, &balance, MACRO_PAYLOAD_IR_OP_KEY_UP, keycode)) {
                    goto fail;
                }

                pending_down_count = 0;
                pending_tap        = false;
                pending_tap_key    = 0;
                matched_up_count   = 0;
                break;
            }
            case SS_DELAY_CODE:
                if (!macro_payload_flush_pending_qmk_sequence(ir, &balance, pending_downs, pending_down_count, pending_tap, pending_tap_key, matched_up_count)) {
                    goto fail;
                }
                pending_down_count = 0;
                pending_tap        = false;
                pending_tap_key    = 0;
                matched_up_count   = 0;
                delay_ms           = 0;
                {
                    bool terminated = false;

                    while (offset < length) {
                        uint8_t delay_char = 0;

                        if (!read_byte(offset++, &delay_char, context)) {
                            goto fail;
                        }

                        if (delay_char < '0' || delay_char > '9') {
                            if (!macro_payload_ir_write_delay(ir, (uint16_t)delay_ms)) {
                                goto fail;
                            }
                            if (delay_char == 0) {
                                if (!macro_payload_hold_balance_is_clear(&balance)) {
                                    goto fail;
                                }
                                return true;
                            }
                            terminated = true;
                            break;
                        }

                        delay_ms = (delay_ms * 10u) + (uint32_t)(delay_char - '0');
                        if (delay_ms > UINT16_MAX) {
                            delay_ms = UINT16_MAX;
                        }
                    }

                    if (!terminated) {
                        goto fail;
                    }
                }
                break;
            default:
                goto fail;
        }
    }

fail:
    if (clean.ir_length != 0u && ir->length + MACRO_PAYLOAD_QMK_SPAN_MAX_BYTES > sizeof(ir->bytes)) {
        ir->length         = clean.ir_length;
        ir->more           = true;
        cursor->offset     = clean.offset;
        cursor->protection = ir->protection;
        cursor->balance    = clean.balance;
        return true;
    }
    ir->length = 0;
    ir->more   = false;
    return false;
}

bool macro_payload_decode_qmk_stream(macro_payload_ir_t *ir, uint16_t length, macro_payload_read_byte_fn read_byte, void *context) {
    macro_payload_stream_cursor_t cursor = {0};

    if (!macro_payload_decode_qmk_window(ir, &cursor, length, read_byte, context)) {
        return false;
    }
    if (ir->more) {
        ir->length = 0;
        ir->more   = false;
        return false;
    }
    return true;
}
