#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "send_string.h"

#include "../action/owned_keycode.h"
#include "../state/diagnostics/runtime_diag.h"
#include "macro_payload_internal.h"

static void macro_payload_wait_ms(uint16_t delay_ms) {
    static const uint16_t macro_payload_wait_slice_ms = 50u;

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

static bool macro_payload_send_text_char(char c, macro_payload_text_output_t text_output, uint8_t interval) {
    if (text_output == MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED) {
        send_char_with_delay(c, interval);
        return true;
    }

    send_char(c);
    return true;
}

static void macro_payload_release_balanced_holds(macro_payload_hold_balance_t *balance) {
    if (!balance) {
        return;
    }

    while (balance->count > 0u) {
        uint8_t keycode = balance->keycodes[balance->count - 1u];

        (void)owned_keycode_unregister(keycode);
        balance->count--;
    }
}

bool macro_payload_play_ir_with_text_output(const macro_payload_ir_t *ir, macro_payload_text_output_t text_output, uint8_t interval) {
    const uint8_t                *cursor;
    const uint8_t                *end;
    macro_payload_hold_balance_t  balance = {0};
    bool                          ok      = false;

    if (!ir) {
        return false;
    }

    cursor = ir->bytes;
    end    = ir->bytes + ir->length;
    macro_payload_hold_balance_reset(&balance);

    while (cursor < end) {
        macro_payload_ir_opcode_t opcode = (macro_payload_ir_opcode_t)(*cursor++);

        switch (opcode) {
            case MACRO_PAYLOAD_IR_OP_TEXT: {
                uint8_t text_length = 0;

                if (cursor >= end) {
                    goto finish;
                }

                text_length = *cursor++;
                if ((size_t)(end - cursor) < text_length) {
                    goto finish;
                }

                for (uint8_t i = 0; i < text_length; i++) {
                    if (!macro_payload_send_text_char((char)cursor[i], text_output, interval)) {
                        goto finish;
                    }
                }

                cursor += text_length;
                break;
            }
            case MACRO_PAYLOAD_IR_OP_DELAY:
                if ((size_t)(end - cursor) < 2u) {
                    goto finish;
                }
                if (!macro_payload_run_delay((uint16_t)cursor[0] | ((uint16_t)cursor[1] << 8))) {
                    goto finish;
                }
                cursor += 2;
                break;
            case MACRO_PAYLOAD_IR_OP_KEY_DOWN:
                if (cursor >= end || !macro_payload_hold_balance_note_down(&balance, *cursor) || !macro_payload_run_key_down(*cursor)) {
                    goto finish;
                }
                cursor++;
                break;
            case MACRO_PAYLOAD_IR_OP_KEY_UP:
                if (cursor >= end || !macro_payload_run_key_up(*cursor)) {
                    goto finish;
                }
                (void)macro_payload_hold_balance_note_up(&balance, *cursor);
                cursor++;
                break;
            case MACRO_PAYLOAD_IR_OP_TAP_LIST: {
                uint8_t count = 0;

                if (cursor >= end) {
                    goto finish;
                }

                count = *cursor++;
                if ((size_t)(end - cursor) < count || !macro_payload_run_tap_list(cursor, count)) {
                    goto finish;
                }

                cursor += count;
                break;
            }
            default:
                goto finish;
        }
    }

    ok = cursor == end && macro_payload_hold_balance_is_clear(&balance);

finish:
    macro_payload_release_balanced_holds(&balance);
    return ok;
}

bool macro_payload_play_ir(const macro_payload_ir_t *ir) {
    return macro_payload_play_ir_with_text_output(ir, MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN, 0);
}
