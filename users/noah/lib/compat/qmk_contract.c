// ────────────────────────────────────────────────────────────────────────────
// QMK Contract Compatibility
// ────────────────────────────────────────────────────────────────────────────

#include "qmk_via_playback_contract.h"

#ifdef VIA_ENABLE
#    include "dynamic_keymap.h"
#    include "send_string.h"

#    include "../action/owned_keycode.h"

#    ifndef DYNAMIC_KEYMAP_MACRO_DELAY
#        define DYNAMIC_KEYMAP_MACRO_DELAY TAP_CODE_DELAY
#    endif

static uint8_t qmk_contract_via_macro_read_byte(uint16_t offset) {
    uint8_t byte = 0;
    dynamic_keymap_macro_get_buffer(offset, 1, &byte);
    return byte;
}

// Forked from the current QMK/VIA dynamic macro sender on this userspace's
// fork so macro playback can route literal key presses through owned-keycode
// dispatch. If the VIA macro encoding changes upstream, re-validate this copy.
static void qmk_contract_play_via_macro(uint8_t id) {
    uint16_t size   = dynamic_keymap_macro_get_buffer_size();
    uint16_t offset = 0;

    if (id >= dynamic_keymap_macro_get_count() || size == 0) {
        return;
    }

    if (qmk_contract_via_macro_read_byte(size - 1) != 0) {
        return;
    }

    while (id > 0) {
        if (offset == size) {
            return;
        }
        if (qmk_contract_via_macro_read_byte(offset) == 0) {
            --id;
        }
        ++offset;
    }

    while (offset < size) {
        uint8_t code = qmk_contract_via_macro_read_byte(offset++);

        if (code == 0) {
            break;
        }

        if (code == SS_QMK_PREFIX) {
            if (offset >= size) {
                break;
            }

            code = qmk_contract_via_macro_read_byte(offset++);

            if (code == SS_TAP_CODE || code == SS_DOWN_CODE || code == SS_UP_CODE) {
                if (offset >= size) {
                    break;
                }

                uint8_t keycode = qmk_contract_via_macro_read_byte(offset++);

                if (code == SS_TAP_CODE) {
                    (void)owned_keycode_tap(keycode);
                } else if (code == SS_DOWN_CODE) {
                    (void)owned_keycode_register(keycode);
                } else {
                    (void)owned_keycode_unregister(keycode);
                }
            } else if (code == SS_DELAY_CODE) {
                int delay_ms = 0;

                while (offset < size) {
                    uint8_t delay_char = qmk_contract_via_macro_read_byte(offset++);

                    if (delay_char < '0' || delay_char > '9') {
                        code = delay_char;
                        break;
                    }

                    delay_ms *= 10;
                    delay_ms += delay_char - '0';
                }

                wait_ms((uint16_t)delay_ms);
            }

            wait_ms(DYNAMIC_KEYMAP_MACRO_DELAY);

            if (code == 0) {
                break;
            }

            continue;
        }

        send_char_with_delay((char)code, DYNAMIC_KEYMAP_MACRO_DELAY);
    }
}
#endif

bool noah_qmk_contract_try_play_via_macro(uint16_t action) {
#ifdef VIA_ENABLE
    if (IS_QK_MACRO(action)) {
        qmk_contract_play_via_macro((uint8_t)(action - QK_MACRO));
        return true;
    }
#else
    (void)action;
#endif

    return false;
}
