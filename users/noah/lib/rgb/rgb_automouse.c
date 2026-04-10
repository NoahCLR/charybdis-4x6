// ────────────────────────────────────────────────────────────────────────────
// Auto-Mouse RGB Progress
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)

#    include "pointing_device_auto_mouse.h" // QMK (firmware fork)
#    include "../pointing/pd_mode_flags.h"
#    include "../state/split_runtime_sync.h"
#    include "rgb_automouse.h"

uint16_t automouse_rgb_current_progress(void) {
    uint16_t progress;
    if (is_keyboard_master()) {
        progress = automouse_rgb_progress(auto_mouse_get_time_elapsed());
    } else {
        progress = split_runtime_sync_remote.automouse_progress;
    }

    if (pd_any_mode_locked()) {
        progress = 0;
    }

    if (progress > AUTOMOUSE_RGB_ACTIVE_SPAN) {
        progress = AUTOMOUSE_RGB_ACTIVE_SPAN;
    }

    return progress;
}

#endif // defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)
