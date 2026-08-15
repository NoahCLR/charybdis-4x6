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

typedef struct {
    macro_payload_ir_opcode_t opcode;
    const uint8_t            *bytes;
    uint16_t                  value;
    uint8_t                   length;
} macro_payload_ir_step_t;

static bool macro_payload_ir_next(const uint8_t **cursor, const uint8_t *end, macro_payload_ir_step_t *step) {
    const uint8_t *current;

    if (!cursor || !*cursor || !end || !step || *cursor >= end) {
        return false;
    }

    current = *cursor;
    *step   = (macro_payload_ir_step_t){.opcode = (macro_payload_ir_opcode_t)(*current++)};

    switch (step->opcode) {
        case MACRO_PAYLOAD_IR_OP_TEXT:
            if (current >= end) {
                return false;
            }

            step->length = *current++;
            if (step->length == 0u || (size_t)(end - current) < step->length) {
                return false;
            }
            for (uint8_t index = 0; index < step->length; index++) {
                if (!macro_payload_text_byte_is_supported(current[index])) {
                    return false;
                }
            }

            step->bytes = current;
            current += step->length;
            break;
        case MACRO_PAYLOAD_IR_OP_DELAY:
            if ((size_t)(end - current) < 2u) {
                return false;
            }

            step->value = (uint16_t)current[0] | ((uint16_t)current[1] << 8);
            current += 2;
            break;
        case MACRO_PAYLOAD_IR_OP_KEY_DOWN:
        case MACRO_PAYLOAD_IR_OP_KEY_UP:
            if (current >= end) {
                return false;
            }

            step->value = *current++;
            break;
        case MACRO_PAYLOAD_IR_OP_TAP_LIST:
            if (current >= end) {
                return false;
            }

            step->length = *current++;
            if (step->length == 0u || step->length > MACRO_PAYLOAD_MAX_TAP_KEYS || (size_t)(end - current) < step->length) {
                return false;
            }

            step->bytes = current;
            current += step->length;
            break;
        default:
            return false;
    }

    *cursor = current;
    return true;
}

static bool macro_payload_ir_preflight(const macro_payload_ir_t *ir) {
    const uint8_t                *cursor;
    const uint8_t                *end;
    macro_payload_hold_balance_t  balance = {0};

    if (!ir || ir->length > sizeof(ir->bytes)) {
        return false;
    }

    cursor = ir->bytes;
    end    = ir->bytes + ir->length;
    macro_payload_hold_balance_reset(&balance);

    while (cursor < end) {
        macro_payload_ir_step_t step;

        if (!macro_payload_ir_next(&cursor, end, &step)) {
            return false;
        }

        if (step.opcode == MACRO_PAYLOAD_IR_OP_KEY_DOWN && !macro_payload_hold_balance_note_down(&balance, (uint8_t)step.value)) {
            return false;
        }
        if (step.opcode == MACRO_PAYLOAD_IR_OP_KEY_UP && !macro_payload_hold_balance_note_up(&balance, (uint8_t)step.value)) {
            return false;
        }
    }

    return cursor == end && macro_payload_hold_balance_is_clear(&balance);
}

bool macro_payload_play_ir_with_text_output(const macro_payload_ir_t *ir, macro_payload_text_output_t text_output, uint8_t interval) {
    const uint8_t                *cursor;
    const uint8_t                *end;
    macro_payload_hold_balance_t  balance = {0};
    bool                          ok      = false;

    if (!macro_payload_ir_preflight(ir)) {
        return false;
    }

    cursor = ir->bytes;
    end    = ir->bytes + ir->length;
    macro_payload_hold_balance_reset(&balance);

    while (cursor < end) {
        macro_payload_ir_step_t step;

        if (!macro_payload_ir_next(&cursor, end, &step)) {
            goto finish;
        }

        switch (step.opcode) {
            case MACRO_PAYLOAD_IR_OP_TEXT:
                for (uint8_t i = 0; i < step.length; i++) {
                    if (!macro_payload_send_text_char((char)step.bytes[i], text_output, interval)) {
                        goto finish;
                    }
                }
                break;
            case MACRO_PAYLOAD_IR_OP_DELAY:
                if (!macro_payload_run_delay(step.value)) {
                    goto finish;
                }
                break;
            case MACRO_PAYLOAD_IR_OP_KEY_DOWN:
                if (!macro_payload_hold_balance_note_down(&balance, (uint8_t)step.value) || !macro_payload_run_key_down((uint8_t)step.value)) {
                    goto finish;
                }
                break;
            case MACRO_PAYLOAD_IR_OP_KEY_UP:
                if (!macro_payload_run_key_up((uint8_t)step.value)) {
                    goto finish;
                }
                (void)macro_payload_hold_balance_note_up(&balance, (uint8_t)step.value);
                break;
            case MACRO_PAYLOAD_IR_OP_TAP_LIST:
                if (!macro_payload_run_tap_list(step.bytes, step.length)) {
                    goto finish;
                }
                break;
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
