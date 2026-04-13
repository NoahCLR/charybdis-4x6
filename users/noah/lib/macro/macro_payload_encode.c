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

static bool macro_payload_visit_text_write_byte(char c, void *context) {
    macro_payload_writer_t *state = (macro_payload_writer_t *)context;

    return macro_payload_writer_write_byte(state, (uint8_t)c);
}

static bool macro_payload_visit_command_encode(const macro_payload_command_t *command, void *context) {
    macro_payload_writer_t *state = (macro_payload_writer_t *)context;

    return macro_payload_encode_command(command, state);
}

bool macro_payload_encode_write(const char *payload, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written) {
    macro_payload_writer_t state = {
        .write_byte = write_byte,
        .context    = context,
        .length     = 0,
    };

    if (written) {
        *written = 0;
    }

    if (!write_byte) {
        return false;
    }

    if (!macro_payload_visit(payload, macro_payload_visit_text_write_byte, macro_payload_visit_command_encode, &state)) {
        return false;
    }

    if (written) {
        *written = state.length;
    }

    return true;
}

bool macro_payload_encode(const char *payload, uint8_t *buffer, uint16_t capacity, uint16_t *written) {
    macro_payload_buffer_sink_t sink = {
        .buffer   = buffer,
        .capacity = capacity,
    };

    return macro_payload_encode_write(payload, macro_payload_buffer_sink_write_byte, &sink, written);
}
