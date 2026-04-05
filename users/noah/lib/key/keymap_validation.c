// ────────────────────────────────────────────────────────────────────────────
// Keymap Validation
// ────────────────────────────────────────────────────────────────────────────

#include "noah_keymap.h"
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

void noah_keymap_validate(void) {
    key_behavior_validate_all();
    validate_authored_keymap_layer_actions();
}
