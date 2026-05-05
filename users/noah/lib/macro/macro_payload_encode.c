#include <stddef.h>

#include "send_string.h"

#include "macro_payload_internal.h"

typedef struct {
    macro_payload_write_byte_fn write_byte;
    void                       *context;
    uint16_t                    length;
} macro_payload_writer_t;

typedef struct {
    uint8_t *buffer;
    uint16_t capacity;
} macro_payload_buffer_sink_t;

static bool macro_payload_buffer_sink_write_byte(uint8_t byte, void *context) {
    macro_payload_buffer_sink_t *sink = (macro_payload_buffer_sink_t *)context;

    if (sink->capacity == 0) {
        return false;
    }

    *sink->buffer++ = byte;
    sink->capacity--;
    return true;
}

static bool macro_payload_writer_write_byte(macro_payload_writer_t *state, uint8_t byte) {
    if (!state->write_byte || !state->write_byte(byte, state->context)) {
        return false;
    }

    state->length++;
    return true;
}

static bool macro_payload_writer_write_delay(macro_payload_writer_t *state, uint16_t delay_ms) {
    char   digits[5];
    size_t digit_count = 0;

    if (!macro_payload_writer_write_byte(state, SS_QMK_PREFIX) || !macro_payload_writer_write_byte(state, SS_DELAY_CODE)) {
        return false;
    }

    do {
        digits[digit_count++] = (char)('0' + (delay_ms % 10));
        delay_ms /= 10;
    } while (delay_ms > 0 && digit_count < sizeof(digits));

    while (digit_count > 0) {
        digit_count--;
        if (!macro_payload_writer_write_byte(state, (uint8_t)digits[digit_count])) {
            return false;
        }
    }

    return macro_payload_writer_write_byte(state, (uint8_t)'|');
}

static bool macro_payload_writer_write_key_action(macro_payload_writer_t *state, uint8_t action, uint8_t keycode) {
    return macro_payload_writer_write_byte(state, SS_QMK_PREFIX) && macro_payload_writer_write_byte(state, action) && macro_payload_writer_write_byte(state, keycode);
}

static bool macro_payload_encode_command(const macro_payload_command_t *command, macro_payload_writer_t *state) {
    switch (command->kind) {
        case MACRO_PAYLOAD_COMMAND_DELAY:
            return macro_payload_writer_write_delay(state, command->delay_ms);
        case MACRO_PAYLOAD_COMMAND_KEY_DOWN:
            return macro_payload_writer_write_key_action(state, SS_DOWN_CODE, command->keycode);
        case MACRO_PAYLOAD_COMMAND_KEY_UP:
            return macro_payload_writer_write_key_action(state, SS_UP_CODE, command->keycode);
        case MACRO_PAYLOAD_COMMAND_TAP_LIST:
            for (uint8_t i = 0; i + 1 < command->tap_list.count; i++) {
                if (!macro_payload_writer_write_key_action(state, SS_DOWN_CODE, command->tap_list.keycodes[i])) {
                    return false;
                }
            }

            if (!macro_payload_writer_write_key_action(state, SS_TAP_CODE, command->tap_list.keycodes[command->tap_list.count - 1])) {
                return false;
            }

            for (uint8_t i = command->tap_list.count - 1; i > 0; i--) {
                if (!macro_payload_writer_write_key_action(state, SS_UP_CODE, command->tap_list.keycodes[i - 1])) {
                    return false;
                }
            }

            return true;
    }

    return false;
}

bool macro_payload_encode_ir_write(const macro_payload_ir_t *ir, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written) {
    macro_payload_writer_t state = {
        .write_byte = write_byte,
        .context    = context,
        .length     = 0,
    };
    const uint8_t *cursor;
    const uint8_t *end;

    if (written) {
        *written = 0;
    }

    if (!ir || !write_byte) {
        return false;
    }

    cursor = ir->bytes;
    end    = ir->bytes + ir->length;

    while (cursor < end) {
        macro_payload_ir_opcode_t opcode = (macro_payload_ir_opcode_t)(*cursor++);

        switch (opcode) {
            case MACRO_PAYLOAD_IR_OP_TEXT: {
                uint8_t text_length = 0;

                if (cursor >= end) {
                    return false;
                }

                text_length = *cursor++;
                if ((size_t)(end - cursor) < text_length) {
                    return false;
                }

                for (uint8_t i = 0; i < text_length; i++) {
                    if (!macro_payload_writer_write_byte(&state, cursor[i])) {
                        return false;
                    }
                }

                cursor += text_length;
                break;
            }
            case MACRO_PAYLOAD_IR_OP_DELAY:
            case MACRO_PAYLOAD_IR_OP_KEY_DOWN:
            case MACRO_PAYLOAD_IR_OP_KEY_UP:
            case MACRO_PAYLOAD_IR_OP_TAP_LIST: {
                macro_payload_command_t command = {0};

                switch (opcode) {
                    case MACRO_PAYLOAD_IR_OP_DELAY:
                        if ((size_t)(end - cursor) < 2u) {
                            return false;
                        }
                        command.kind     = MACRO_PAYLOAD_COMMAND_DELAY;
                        command.delay_ms = (uint16_t)cursor[0] | ((uint16_t)cursor[1] << 8);
                        cursor += 2;
                        break;
                    case MACRO_PAYLOAD_IR_OP_KEY_DOWN:
                        if (cursor >= end) {
                            return false;
                        }
                        command.kind    = MACRO_PAYLOAD_COMMAND_KEY_DOWN;
                        command.keycode = *cursor++;
                        break;
                    case MACRO_PAYLOAD_IR_OP_KEY_UP:
                        if (cursor >= end) {
                            return false;
                        }
                        command.kind    = MACRO_PAYLOAD_COMMAND_KEY_UP;
                        command.keycode = *cursor++;
                        break;
                    case MACRO_PAYLOAD_IR_OP_TAP_LIST:
                        if (cursor >= end) {
                            return false;
                        }
                        command.kind           = MACRO_PAYLOAD_COMMAND_TAP_LIST;
                        command.tap_list.count = *cursor++;
                        if (command.tap_list.count == 0 || command.tap_list.count > MACRO_PAYLOAD_MAX_TAP_KEYS || (size_t)(end - cursor) < command.tap_list.count) {
                            return false;
                        }
                        for (uint8_t i = 0; i < command.tap_list.count; i++) {
                            command.tap_list.keycodes[i] = *cursor++;
                        }
                        break;
                    default:
                        return false;
                }

                if (!macro_payload_encode_command(&command, &state)) {
                    return false;
                }
                break;
            }
            default:
                return false;
        }
    }

    if (written) {
        *written = state.length;
    }

    return cursor == end;
}

bool macro_payload_encode_ir(const macro_payload_ir_t *ir, uint8_t *buffer, uint16_t capacity, uint16_t *written) {
    macro_payload_buffer_sink_t sink = {
        .buffer   = buffer,
        .capacity = capacity,
    };

    return macro_payload_encode_ir_write(ir, macro_payload_buffer_sink_write_byte, &sink, written);
}

bool macro_payload_encode_write(const char *payload, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written) {
    macro_payload_ir_t ir = {0};

    if (written) {
        *written = 0;
    }

    if (!macro_payload_compile(payload, &ir)) {
        return false;
    }

    return macro_payload_encode_ir_write(&ir, write_byte, context, written);
}

bool macro_payload_encode(const char *payload, uint8_t *buffer, uint16_t capacity, uint16_t *written) {
    macro_payload_ir_t ir = {0};

    if (written) {
        *written = 0;
    }

    if (!macro_payload_compile(payload, &ir)) {
        return false;
    }

    return macro_payload_encode_ir(&ir, buffer, capacity, written);
}
