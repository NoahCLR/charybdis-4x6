// ────────────────────────────────────────────────────────────────────────────
// Action Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#ifdef VIA_ENABLE
#    include "dynamic_keymap.h"
#    include "send_string.h"
#endif

#include "noah_keymap.h"
#include "synthetic_record.h"
#include "../pointing/pointing_device_modes.h"
#include "../state/keyboard_mod_ownership.h"
#include "../state/layer_ownership.h"
#include "../state/runtime_shared_state.h"
#include "action_dispatch.h"

bool action_dispatch_is_layer_lock(uint16_t action) {
    return action >= LAYER_LOCK_BASE && action < LAYER_LOCK_BASE + LAYER_COUNT;
}

bool action_dispatch_is_raw_qmk_layer_action(uint16_t action) {
    return IS_QK_TO(action) || IS_QK_MOMENTARY(action) || IS_QK_DEF_LAYER(action) || IS_QK_TOGGLE_LAYER(action) || IS_QK_ONE_SHOT_LAYER(action) || IS_QK_LAYER_TAP_TOGGLE(action) || IS_QK_LAYER_MOD(action) || IS_QK_LAYER_TAP(action);
}

bool action_dispatch_is_layer_action(uint16_t action) {
    return action_dispatch_is_layer_lock(action) || action_dispatch_is_raw_qmk_layer_action(action);
}

bool action_dispatch_is_macro(uint16_t action) {
    return (action >= MACRO_0 && action <= MACRO_15) || IS_QK_MACRO(action);
}

bool action_dispatch_is_qmk_behavior_keycode(uint16_t action) {
    return IS_QK_ONE_SHOT_MOD(action) || IS_QK_MOD_TAP(action);
}

bool action_dispatch_layer_is_locked(uint8_t layer) {
    return layer_ownership_is_locked(layer);
}

static void action_dispatch_log_unsupported_layer_action(uint16_t action) {
#ifdef CONSOLE_ENABLE
    uprintf("Unsupported authored raw QMK layer action 0x%04X; use LOCK_LAYER(...) or PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer)) instead\n", (unsigned int)action);
#else
    (void)action;
#endif
}

static void action_dispatch_register_keycode(uint8_t keycode) {
    if (IS_MODIFIER_KEYCODE(keycode)) {
        keyboard_mod_ownership_register(keycode);
        return;
    }

    register_code(keycode);
}

static void action_dispatch_unregister_keycode(uint8_t keycode) {
    if (IS_MODIFIER_KEYCODE(keycode)) {
        keyboard_mod_ownership_unregister(keycode);
        return;
    }

    unregister_code(keycode);
}

static void action_dispatch_tap_keycode(uint8_t keycode) {
    if (IS_MODIFIER_KEYCODE(keycode)) {
        uint16_t delay = (keycode == KC_CAPS_LOCK) ? TAP_HOLD_CAPS_DELAY : TAP_CODE_DELAY;

        keyboard_mod_ownership_register(keycode);
        wait_ms(delay);
        keyboard_mod_ownership_unregister(keycode);
        return;
    }

    tap_code(keycode);
}

#ifdef VIA_ENABLE
#    ifndef DYNAMIC_KEYMAP_MACRO_DELAY
#        define DYNAMIC_KEYMAP_MACRO_DELAY TAP_CODE_DELAY
#    endif

static uint8_t action_dispatch_via_macro_read_byte(uint16_t offset) {
    uint8_t byte = 0;
    dynamic_keymap_macro_get_buffer(offset, 1, &byte);
    return byte;
}

static void action_dispatch_via_macro_send(uint8_t id) {
    uint16_t size = dynamic_keymap_macro_get_buffer_size();
    uint16_t offset = 0;

    if (id >= dynamic_keymap_macro_get_count() || size == 0) {
        return;
    }

    if (action_dispatch_via_macro_read_byte(size - 1) != 0) {
        return;
    }

    while (id > 0) {
        if (offset == size) {
            return;
        }
        if (action_dispatch_via_macro_read_byte(offset) == 0) {
            --id;
        }
        ++offset;
    }

    while (offset < size) {
        uint8_t code = action_dispatch_via_macro_read_byte(offset++);

        if (code == 0) {
            break;
        }

        if (code == SS_QMK_PREFIX) {
            if (offset >= size) {
                break;
            }

            code = action_dispatch_via_macro_read_byte(offset++);

            if (code == SS_TAP_CODE || code == SS_DOWN_CODE || code == SS_UP_CODE) {
                if (offset >= size) {
                    break;
                }

                uint8_t keycode = action_dispatch_via_macro_read_byte(offset++);

                if (code == SS_TAP_CODE) {
                    action_dispatch_tap_keycode(keycode);
                } else if (code == SS_DOWN_CODE) {
                    action_dispatch_register_keycode(keycode);
                } else {
                    action_dispatch_unregister_keycode(keycode);
                }
            } else if (code == SS_DELAY_CODE) {
                int delay_ms = 0;

                while (offset < size) {
                    uint8_t delay_char = action_dispatch_via_macro_read_byte(offset++);

                    if (delay_char < '0' || delay_char > '9') {
                        code = delay_char;
                        break;
                    }

                    delay_ms *= 10;
                    delay_ms += delay_char - '0';
                }

                wait_ms(delay_ms);
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

void action_dispatch(uint16_t action) {
    if (action_dispatch_is_layer_lock(action)) {
        uint8_t layer = action - LAYER_LOCK_BASE;

        layer_ownership_toggle_lock_state(layer);
        return;
    }

    if (is_pd_mode_lock_action(action)) {
        const pd_mode_def_t *def = pd_mode_lock_action_lookup(action);
        if (def && pd_mode_toggle_lock_state(def->mode_flag)) {
            runtime_shared_state_sync();
        }
        return;
    }

#ifdef VIA_ENABLE
    if (IS_QK_MACRO(action)) {
        action_dispatch_via_macro_send((uint8_t)(action - QK_MACRO));
        return;
    }
#endif

    if (macro_dispatch(action)) {
        return;
    }

    if (action_dispatch_is_raw_qmk_layer_action(action)) {
        action_dispatch_log_unsupported_layer_action(action);
        return;
    }

    if (action >= NOAH_KEYMAP_SAFE_RANGE) {
        noah_dispatch_synthetic_tap(action);
        return;
    }

    if (action_dispatch_is_qmk_behavior_keycode(action)) {
        noah_dispatch_synthetic_qmk_tap(action);
        return;
    }

    tap_code16(action);
}
