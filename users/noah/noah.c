// ────────────────────────────────────────────────────────────────────────────
// Noah Userspace Runtime
// ────────────────────────────────────────────────────────────────────────────

#include "noah.h"

// ─── Split-Role Override ────────────────────────────────────────────────────
//
// Needed when both halves have their own USB connection so they do not both
// detect USB and fight over who is master.
#if defined(FORCE_SLAVE)
#    include "usb_util.h" // QMK
#endif

#if defined(FORCE_MASTER)
bool is_keyboard_master_impl(void) {
    return true;
}
#elif defined(FORCE_SLAVE)
bool is_keyboard_master_impl(void) {
    usb_disconnect();
    return false;
}
#endif

// ─── Tap/Hold Behavior ─────────────────────────────────────────────────────

bool get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record) {
    (void)record;

    switch (keycode) {
        case MT(MOD_LSFT, KC_CAPS):
            return true;
        default:
            return false;
    }
}
