// ────────────────────────────────────────────────────────────────────────────
// QMK Tapping Contract
// ────────────────────────────────────────────────────────────────────────────
//
// QMK's TT(layer) toggles its layer on the TAPPING_TOGGLE-th tap. QMK
// defaults TAPPING_TOGGLE to 5 in quantum/action_tapping.h, which is not in
// every translation unit's include path; run_qmk_contract_checks.sh pins the
// default here to the fork's. 0 means TT() never toggles.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#ifdef TAPPING_TOGGLE
#    define NOAH_QMK_TAPPING_TOGGLE TAPPING_TOGGLE
#else
#    define NOAH_QMK_TAPPING_TOGGLE 5
#endif
