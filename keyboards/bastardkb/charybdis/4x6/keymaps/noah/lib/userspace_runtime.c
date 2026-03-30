// ────────────────────────────────────────────────────────────────────────────
// Noah's Charybdis 4x6 userspace glue
// ────────────────────────────────────────────────────────────────────────────
//
// This file owns the small amount of shared compile-time glue that does not
// naturally belong to the key, pointing-device, or RGB runtime modules.
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#include "../keymap_defs.h"

#ifdef VIA_ENABLE
_Static_assert(LAYER_COUNT == DYNAMIC_KEYMAP_LAYER_COUNT, "LAYER_COUNT and DYNAMIC_KEYMAP_LAYER_COUNT are out of sync — update config.h");
#endif

// Force master/slave role at compile time. Needed when both halves have
// their own USB connection so they do not both detect USB and fight over
// who is master.
#if defined(FORCE_MASTER)
bool is_keyboard_master_impl(void) {
    return true;
}
#elif defined(FORCE_SLAVE)
#    include "usb_util.h" // QMK
bool is_keyboard_master_impl(void) {
    usb_disconnect();
    return false;
}
#endif
