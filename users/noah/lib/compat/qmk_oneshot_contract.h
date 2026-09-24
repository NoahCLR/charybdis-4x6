// ────────────────────────────────────────────────────────────────────────────
// QMK One-Shot Contract
// ────────────────────────────────────────────────────────────────────────────
//
// OSL() is owned by userspace (layer_ownership_oneshot_*); QMK's own one-shot
// layer state is never set. QMK's keymap_config.oneshot_enable (Magic / VIA
// "one-shot keys") still decides whether a tap arms it: with one-shots off,
// OSL(layer) is only a hold, as QMK's is. ONESHOT_TIMEOUT and
// ONESHOT_TAP_TOGGLE apply to QMK's OSM() only.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

static inline bool noah_qmk_contract_oneshot_enabled(void) {
#ifdef NO_ACTION_ONESHOT
    return false;
#else
    return is_oneshot_enabled();
#endif
}
