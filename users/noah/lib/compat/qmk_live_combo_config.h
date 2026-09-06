#pragma once

// QMK exposes per-combo hooks for timing and matching, but its hold/tap wait
// threshold is global. Keep that contract global in the live profile too.
#ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
#    define COMBO_TERM_PER_COMBO
#    define COMBO_MUST_HOLD_PER_COMBO
#    define COMBO_MUST_TAP_PER_COMBO
#    define COMBO_MUST_PRESS_IN_ORDER_PER_COMBO
#    ifndef __ASSEMBLER__
#        include <stdint.h>
uint16_t noah_qmk_combo_hold_term(void);
#    endif
#    define COMBO_HOLD_TERM noah_qmk_combo_hold_term()
#endif
