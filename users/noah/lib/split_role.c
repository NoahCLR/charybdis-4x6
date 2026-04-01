// ────────────────────────────────────────────────────────────────────────────
// Split-Role Override
// ────────────────────────────────────────────────────────────────────────────
//
// Needed when both halves have their own USB connection so they do not both
// detect USB and fight over who is master.  Build with FORCE_MASTER=yes or
// FORCE_SLAVE=yes in rules.mk to activate.
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

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
