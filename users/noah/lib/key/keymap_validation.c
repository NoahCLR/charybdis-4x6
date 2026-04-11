// ────────────────────────────────────────────────────────────────────────────
// Keymap Validation
// ────────────────────────────────────────────────────────────────────────────

#include "noah_keymap_ids.h"
#include "keymap_introspection.h" // QMK

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "../action/action_dispatch.h"
#include "key_behavior_lookup.h"
#include "keymap_validation.h"

static bool keymap_layer_action_supported(uint16_t keycode) {
    if (!action_dispatch_is_raw_qmk_layer_action(keycode)) {
        return true;
    }

    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

static void log_invalid_keymap_layer_action(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode) {
#ifdef CONSOLE_ENABLE
    uprintf("Unsupported keymaps[%u][%u][%u] raw layer action 0x%04X; use MO()/LT() for momentary access or LOCK_LAYER(...) for persistent layer changes\n", (unsigned int)layer, (unsigned int)row, (unsigned int)col, (unsigned int)keycode);
#else
    (void)layer;
    (void)row;
    (void)col;
    (void)keycode;
#endif
}

static void log_invalid_combo_output(uint8_t combo_index, uint16_t keycode) {
#ifdef CONSOLE_ENABLE
    uprintf("Unsupported COMBOS(COMBO) output[%u] raw layer action 0x%04X; combo taps bypass userspace layer ownership. Use LOCK_LAYER(...) for toggles or route the behavior through key_behaviors[]\n", (unsigned int)combo_index, (unsigned int)keycode);
#else
    (void)combo_index;
    (void)keycode;
#endif
}

static void log_unreachable_key_behavior(uint8_t index, uint16_t keycode) {
#ifdef CONSOLE_ENABLE
    uprintf("Unreachable key_behaviors[%u].keycode 0x%04X; row is not referenced by keymaps[][] or COMBOS(COMBO)\n", (unsigned int)index, (unsigned int)keycode);
#else
    (void)index;
    (void)keycode;
#endif
}

static bool keycode_present_in_keymaps(uint16_t keycode) {
    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                if (keycode_at_keymap_location(layer, row, col) == keycode) {
                    return true;
                }
            }
        }
    }

    return false;
}

static bool keycode_present_in_combo_outputs(uint16_t keycode) {
    for (uint8_t combo_index = 0; combo_index < noah_combo_output_count; combo_index++) {
        if (noah_combo_output_keycodes && noah_combo_output_keycodes[combo_index] == keycode) {
            return true;
        }
    }

    return false;
}

static void validate_authored_keymap_layer_actions(void) {
    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                uint16_t keycode = keycode_at_keymap_location(layer, row, col);

                if (!keymap_layer_action_supported(keycode)) {
                    log_invalid_keymap_layer_action(layer, row, col, keycode);
                }
            }
        }
    }
}

static void validate_combo_outputs(void) {
    for (uint8_t combo_index = 0; combo_index < noah_combo_output_count; combo_index++) {
        uint16_t keycode = noah_combo_output_keycodes[combo_index];

        if (action_dispatch_is_raw_qmk_layer_action(keycode)) {
            log_invalid_combo_output(combo_index, keycode);
        }
    }
}

static void validate_key_behavior_reachability(void) {
    for (uint8_t index = 0; index < key_behavior_count; index++) {
        uint16_t keycode = key_behaviors[index].keycode;

        if (!keycode_present_in_keymaps(keycode) && !keycode_present_in_combo_outputs(keycode)) {
            log_unreachable_key_behavior(index, keycode);
        }
    }
}

void noah_keymap_validate(void) {
    key_behavior_validate_all();
    validate_authored_keymap_layer_actions();
    validate_combo_outputs();
    validate_key_behavior_reachability();
}
