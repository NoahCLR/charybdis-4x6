// ────────────────────────────────────────────────────────────────────────────
// Action Lifecycle
// ────────────────────────────────────────────────────────────────────────────

#include "action_lifecycle.h"

#include "noah_keymap.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#ifdef VIA_ENABLE
#    include "dynamic_keymap.h"
#    include "send_string.h"
#endif

#include "action_dispatch.h"
#include "macro_dispatch.h"
#include "owned_keycode.h"
#include "synthetic_record.h"
#include "../pointing/pd_modes.h"
#include "../state/layer_ownership.h"
#include "../state/split_runtime_sync.h"

static bool noah_action_is_owned_momentary_layer(uint16_t action) {
    return IS_QK_MOMENTARY(action);
}

static void noah_action_log_unsupported_layer_action(uint16_t action) {
#ifdef CONSOLE_ENABLE
    uprintf("Unsupported raw QMK layer action 0x%04X; use LOCK_LAYER(...) for persistent changes or PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer)) for owned momentary holds\n", (unsigned int)action);
#else
    (void)action;
#endif
}

#ifdef VIA_ENABLE
#    ifndef DYNAMIC_KEYMAP_MACRO_DELAY
#        define DYNAMIC_KEYMAP_MACRO_DELAY TAP_CODE_DELAY
#    endif

static uint8_t noah_action_via_macro_read_byte(uint16_t offset) {
    uint8_t byte = 0;
    dynamic_keymap_macro_get_buffer(offset, 1, &byte);
    return byte;
}

// Forked from the current QMK/VIA dynamic macro sender on this userspace's
// fork so macro playback can route literal key presses through owned-keycode
// dispatch. If the VIA macro encoding changes upstream, re-validate this copy.
static void noah_action_via_macro_send(uint8_t id) {
    uint16_t size   = dynamic_keymap_macro_get_buffer_size();
    uint16_t offset = 0;

    if (id >= dynamic_keymap_macro_get_count() || size == 0) {
        return;
    }

    if (noah_action_via_macro_read_byte(size - 1) != 0) {
        return;
    }

    while (id > 0) {
        if (offset == size) {
            return;
        }
        if (noah_action_via_macro_read_byte(offset) == 0) {
            --id;
        }
        ++offset;
    }

    while (offset < size) {
        uint8_t code = noah_action_via_macro_read_byte(offset++);

        if (code == 0) {
            break;
        }

        if (code == SS_QMK_PREFIX) {
            if (offset >= size) {
                break;
            }

            code = noah_action_via_macro_read_byte(offset++);

            if (code == SS_TAP_CODE || code == SS_DOWN_CODE || code == SS_UP_CODE) {
                if (offset >= size) {
                    break;
                }

                uint8_t keycode = noah_action_via_macro_read_byte(offset++);

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
                    uint8_t delay_char = noah_action_via_macro_read_byte(offset++);

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

static bool noah_action_handle_one_shot_press(uint16_t action) {
    if (action_dispatch_is_layer_lock(action)) {
        layer_ownership_toggle_lock_state((uint8_t)(action - LAYER_LOCK_BASE));
        return true;
    }

    if (is_pd_mode_lock_action(action)) {
        const pd_mode_def_t *def = pd_mode_lock_action_lookup(action);
        if (def && pd_mode_toggle_lock_state(def->mode_flag)) {
            split_runtime_sync();
        }
        return true;
    }

#ifdef VIA_ENABLE
    if (IS_QK_MACRO(action)) {
        noah_action_via_macro_send((uint8_t)(action - QK_MACRO));
        return true;
    }
#endif

    return macro_dispatch(action);
}

noah_action_hold_kind_t noah_action_hold_kind(uint16_t action) {
    if (action_dispatch_is_layer_lock(action) || is_pd_mode_lock_action(action) || action_dispatch_is_macro(action)) {
        return NOAH_ACTION_HOLD_KIND_PRESS_ONLY;
    }

    if (noah_action_is_owned_momentary_layer(action)) {
        return NOAH_ACTION_HOLD_KIND_PER_KEY;
    }

    return NOAH_ACTION_HOLD_KIND_SHARED;
}

void noah_action_tap(uint16_t action) {
    if (noah_action_handle_one_shot_press(action)) {
        return;
    }

    if (action_dispatch_is_raw_qmk_layer_action(action)) {
        noah_action_log_unsupported_layer_action(action);
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

void noah_action_press(keypos_t key_pos, uint16_t action) {
    if (noah_action_handle_one_shot_press(action)) {
        return;
    }

    if (pd_mode_handle_keycode_press(action)) {
        return;
    }

    if (noah_action_is_owned_momentary_layer(action)) {
        layer_ownership_momentary_press(key_pos, QK_MOMENTARY_GET_LAYER(action));
        return;
    }

    if (action_dispatch_is_raw_qmk_layer_action(action)) {
        noah_action_log_unsupported_layer_action(action);
        return;
    }

    if (action_dispatch_is_qmk_behavior_keycode(action)) {
        noah_dispatch_synthetic_qmk_record(action, true, 0);
        return;
    }

    if (action >= NOAH_KEYMAP_SAFE_RANGE) {
        noah_dispatch_synthetic_record(action, true);
        return;
    }

    // Route literal keycodes, including QK_MODS such as S(KC_1), through the
    // shared owned-keycode contract so held actions and macro dispatch cannot
    // drift apart.
    if (owned_keycode_register(action)) {
        return;
    }

    register_code16(action);
}

void noah_action_release(keypos_t key_pos, uint16_t action) {
    if (noah_action_hold_kind(action) == NOAH_ACTION_HOLD_KIND_PRESS_ONLY) {
        return;
    }

    if (pd_mode_handle_keycode_release(action)) {
        return;
    }

    if (noah_action_is_owned_momentary_layer(action)) {
        layer_ownership_momentary_release(key_pos);
        return;
    }

    if (action_dispatch_is_raw_qmk_layer_action(action)) {
        noah_action_log_unsupported_layer_action(action);
        return;
    }

    if (action_dispatch_is_qmk_behavior_keycode(action)) {
        noah_dispatch_synthetic_qmk_record(action, false, 0);
        return;
    }

    if (action >= NOAH_KEYMAP_SAFE_RANGE) {
        noah_dispatch_synthetic_record(action, false);
        return;
    }

    if (owned_keycode_unregister(action)) {
        return;
    }

    unregister_code16(action);
}
