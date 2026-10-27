#pragma once

// A combo has up to sixteen inputs (D-F14): QMK's long-combo representation
// gives a 16-bit state mask, sixteen members and a sixteen-key press buffer.
// The queue of completed combos (COMBO_BUFFER_LENGTH) is a separate limit.
#define EXTRA_LONG_COMBOS
// BK asks combo_key_record_allowed() before any member state update; the
// participation policy answers it (participation-policy.md). It is the firmware's own,
// portable policy, so combo readback does not report it as a user hook.
#define COMBO_KEY_RECORD_FILTER
#define NOAH_COMBO_PARTICIPATION_HOOK

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
