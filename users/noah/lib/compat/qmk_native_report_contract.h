// QMK native report actions whose basic edge is owned by another press.
#pragma once

#include "qmk_mod_contract.h"

#ifndef NOAH_HOST_QMK_STUB
#    include "action_util.h"
#endif

// Suppress only the aggregate-owned usage edge. Modified native actions still
// change weak mods, and plain keyboard actions still publish the weak-mod clear
// action_exec performed before the press. Sending the report also preserves
// QMK's one-shot modifier consumption without registering the usage again.
static inline void noah_qmk_native_overlap_report_event(uint16_t keycode, bool pressed) {
    if (IS_QK_MODS(keycode)) {
        uint8_t basic = QK_MODS_GET_BASIC_KEYCODE(keycode);
        if (basic == KC_NO || IS_MODIFIER_KEYCODE(basic)) return;
        uint8_t mods = noah_qmk_mods_to_report_mask(QK_MODS_GET_MODS(keycode));
        set_weak_mods(pressed ? get_weak_mods() | mods : get_weak_mods() & (uint8_t)~mods);
    } else if (!IS_BASIC_KEYCODE(keycode)) {
        return;
    }
    send_keyboard_report();
}
