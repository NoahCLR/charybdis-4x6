#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "send_string.h"

#include "../action/owned_keycode.h"
#include "../state/runtime/runtime_diag.h"
#include "macro_payload_internal.h"

static void macro_payload_wait_ms(uint16_t delay_ms) {
    static const uint16_t macro_payload_wait_slice_ms = 100u;

    while (delay_ms > macro_payload_wait_slice_ms) {
        wait_ms(macro_payload_wait_slice_ms);
        noah_runtime_diag_heartbeat();
        delay_ms = (uint16_t)(delay_ms - macro_payload_wait_slice_ms);
    }

    if (delay_ms > 0u) {
        wait_ms(delay_ms);
        noah_runtime_diag_heartbeat();
    }
}

static void macro_payload_wait_interval(void) {
    macro_payload_wait_ms(TAP_CODE_DELAY);
}

static bool macro_payload_run_delay(uint16_t delay_ms) {
    macro_payload_wait_ms(delay_ms);
    macro_payload_wait_interval();
    return true;
}

static bool macro_payload_run_key_down(uint8_t keycode) {
    (void)owned_keycode_register(keycode);
    macro_payload_wait_interval();
    return true;
}

static bool macro_payload_run_key_up(uint8_t keycode) {
    (void)owned_keycode_unregister(keycode);
    macro_payload_wait_interval();
    return true;
}

static bool macro_payload_run_tap_list(const uint8_t *keycodes, uint8_t count) {
    if (!keycodes || count == 0) {
        return false;
    }

    for (uint8_t i = 0; i + 1 < count; i++) {
        (void)owned_keycode_register(keycodes[i]);
    }

    (void)owned_keycode_tap(keycodes[count - 1]);

    for (uint8_t i = count - 1; i > 0; i--) {
        (void)owned_keycode_unregister(keycodes[i - 1]);
    }

    macro_payload_wait_interval();
    return true;
}

static bool macro_payload_run_command(const macro_payload_command_t *command) {
    switch (command->kind) {
        case MACRO_PAYLOAD_COMMAND_DELAY:
            return macro_payload_run_delay(command->delay_ms);
        case MACRO_PAYLOAD_COMMAND_KEY_DOWN:
            return macro_payload_run_key_down(command->keycode);
        case MACRO_PAYLOAD_COMMAND_KEY_UP:
            return macro_payload_run_key_up(command->keycode);
        case MACRO_PAYLOAD_COMMAND_TAP_LIST:
            return macro_payload_run_tap_list(command->tap_list.keycodes, command->tap_list.count);
    }

    return false;
}

static bool macro_payload_send_text_char(char c, macro_payload_text_output_t text_output, uint8_t interval) {
    if (text_output == MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED) {
        send_char_with_delay(c, interval);
        return true;
    }

    send_char(c);
    return true;
}

static bool macro_payload_visit_text_send_char(char c, void *context) {
    macro_payload_text_output_t text_output = context ? *(macro_payload_text_output_t *)context : MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN;

    return macro_payload_send_text_char(c, text_output, TAP_CODE_DELAY);
}

static bool macro_payload_visit_command_run(const macro_payload_command_t *command, void *context) {
    (void)context;
    return macro_payload_run_command(command);
}

bool macro_payload_run(const char *payload) {
    macro_payload_text_output_t text_output = MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN;

    return macro_payload_visit(payload, macro_payload_visit_text_send_char, macro_payload_visit_command_run, &text_output);
}

bool macro_payload_play_ir_with_text_output(const macro_payload_ir_t *ir, macro_payload_text_output_t text_output, uint8_t interval) {
    const uint8_t *cursor;
    const uint8_t *end;

    if (!ir) {
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
                    if (!macro_payload_send_text_char((char)cursor[i], text_output, interval)) {
                        return false;
                    }
                }

                cursor += text_length;
                break;
            }
            case MACRO_PAYLOAD_IR_OP_DELAY:
                if ((size_t)(end - cursor) < 2u) {
                    return false;
                }
                if (!macro_payload_run_delay((uint16_t)cursor[0] | ((uint16_t)cursor[1] << 8))) {
                    return false;
                }
                cursor += 2;
                break;
            case MACRO_PAYLOAD_IR_OP_KEY_DOWN:
                if (cursor >= end || !macro_payload_run_key_down(*cursor++)) {
                    return false;
                }
                break;
            case MACRO_PAYLOAD_IR_OP_KEY_UP:
                if (cursor >= end || !macro_payload_run_key_up(*cursor++)) {
                    return false;
                }
                break;
            case MACRO_PAYLOAD_IR_OP_TAP_LIST: {
                uint8_t count = 0;

                if (cursor >= end) {
                    return false;
                }

                count = *cursor++;
                if ((size_t)(end - cursor) < count || !macro_payload_run_tap_list(cursor, count)) {
                    return false;
                }

                cursor += count;
                break;
            }
            default:
                return false;
        }
    }

    return cursor == end;
}

bool macro_payload_play_ir(const macro_payload_ir_t *ir) {
    return macro_payload_play_ir_with_text_output(ir, MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN, 0);
}
