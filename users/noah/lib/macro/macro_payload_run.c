#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "send_string.h"

#include "../action/owned_keycode.h"
#include "macro_payload_internal.h"

static void macro_payload_wait_interval(void) {
    wait_ms(TAP_CODE_DELAY);
}

static bool macro_payload_run_command(const macro_payload_command_t *command) {
    switch (command->kind) {
        case MACRO_PAYLOAD_COMMAND_DELAY:
            wait_ms(command->delay_ms);
            macro_payload_wait_interval();
            return true;
        case MACRO_PAYLOAD_COMMAND_KEY_DOWN:
            (void)owned_keycode_register(command->keycode);
            macro_payload_wait_interval();
            return true;
        case MACRO_PAYLOAD_COMMAND_KEY_UP:
            (void)owned_keycode_unregister(command->keycode);
            macro_payload_wait_interval();
            return true;
        case MACRO_PAYLOAD_COMMAND_TAP_LIST:
            for (uint8_t i = 0; i + 1 < command->tap_list.count; i++) {
                (void)owned_keycode_register(command->tap_list.keycodes[i]);
            }

            (void)owned_keycode_tap(command->tap_list.keycodes[command->tap_list.count - 1]);

            for (uint8_t i = command->tap_list.count - 1; i > 0; i--) {
                (void)owned_keycode_unregister(command->tap_list.keycodes[i - 1]);
            }

            macro_payload_wait_interval();
            return true;
    }

    return false;
}

static bool macro_payload_visit_text_send_char(char c, void *context) {
    (void)context;
    send_char(c);
    return true;
}

static bool macro_payload_visit_command_run(const macro_payload_command_t *command, void *context) {
    (void)context;
    return macro_payload_run_command(command);
}

bool macro_payload_run(const char *payload) {
    return macro_payload_visit(payload, macro_payload_visit_text_send_char, macro_payload_visit_command_run, NULL);
}
