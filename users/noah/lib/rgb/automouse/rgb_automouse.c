// ────────────────────────────────────────────────────────────────────────────
// Auto-Mouse RGB Progress
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)

#    include "../../compat/qmk_auto_mouse_contract.h"
#    include "../../pointing/defs/pd_mode_flags.h"
#    include "../../split/runtime_sync.h"
#    include "rgb_automouse.h"

bool automouse_rgb_should_render(void) {
    return automouse_rgb_current_progress() != 0;
}

uint16_t automouse_rgb_current_progress(void) {
    // Master derives progress from the real QMK timeout clock; the slave only
    // mirrors synced progress. Both halves then apply the same clamp/lock
    // rules before rendering.
    uint16_t progress;
    if (is_keyboard_master()) {
        progress = automouse_rgb_progress(noah_qmk_contract_auto_mouse_elapsed());
    } else {
        progress = split_runtime_sync_remote.automouse_progress;
    }

    if (pd_any_display_mode_locked()) {
        progress = 0;
    }

    if (progress > AUTOMOUSE_RGB_ACTIVE_SPAN) {
        progress = AUTOMOUSE_RGB_ACTIVE_SPAN;
    }

    return progress;
}

#endif // defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)
