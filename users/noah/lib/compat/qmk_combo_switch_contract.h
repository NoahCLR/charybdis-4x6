// ────────────────────────────────────────────────────────────────────────────
// QMK Combo Switch Contract
// ────────────────────────────────────────────────────────────────────────────
//
// QMK's combo flag, which CM_ON, CM_OFF and CM_TOGG switch. With
// COMBO_KEYCODES_DOWNSTREAM (BK) QMK leaves those keys to the firmware, which
// switches the flag here (D-F66). The flag is QMK's RAM state, never saved.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>

#ifdef COMBO_ENABLE
#    ifndef COMBO_KEYCODES_DOWNSTREAM
#        error "the firmware switches combos itself: QMK must leave the combo keys to it (COMBO_KEYCODES_DOWNSTREAM)"
#    endif

// QMK's process_combo.h, declared here so host builds need no QMK include path.
void combo_enable(void);
void combo_disable(void);
bool is_combo_enabled(void);

static inline bool noah_qmk_contract_combos_switched_on(void) {
    return is_combo_enabled();
}

static inline void noah_qmk_contract_switch_combos(bool on) {
    if (on) {
        combo_enable();
    } else {
        combo_disable();
    }
}
#else
static inline bool noah_qmk_contract_combos_switched_on(void) {
    return false;
}

static inline void noah_qmk_contract_switch_combos(bool on) {
    (void)on;
}
#endif
