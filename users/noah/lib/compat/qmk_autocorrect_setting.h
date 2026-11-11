// ────────────────────────────────────────────────────────────────────────────
// QMK Autocorrect Setting
// ────────────────────────────────────────────────────────────────────────────
//
// The stored autocorrect switch is bit 12 of QMK's key options (settings
// scalar 24), which the settings push writes into QMK's keymap_config. QMK's
// own autocorrect is not built (AUTOCORRECT_ENABLE stays off, D-F69): the
// firmware's engine reads the bit here.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>

#ifdef AUTOCORRECT_ENABLE
#    error "QMK's autocorrect must stay off: the firmware corrects from the profile's list (D-F69)"
#endif

static inline bool noah_qmk_autocorrect_stored(void) {
    return keymap_config.autocorrect_enable;
}
