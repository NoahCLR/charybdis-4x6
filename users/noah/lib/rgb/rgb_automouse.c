// ────────────────────────────────────────────────────────────────────────────
// Auto-Mouse RGB Progress
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)

#    include "pointing_device_auto_mouse.h" // QMK (firmware fork)
#    include "../pointing/pd_mode_flags.h"
#    include "../state/split_runtime_sync.h"
#    include "rgb_automouse.h"

bool automouse_rgb_should_render(void) {
    if (is_keyboard_master()) {
        uint16_t raw_elapsed = auto_mouse_get_time_elapsed();
        return automouse_rgb_should_render_from_state(is_auto_mouse_active(), raw_elapsed);
    }

    return (split_runtime_sync_remote.automouse_flags & SPLIT_RUNTIME_SYNC_AUTOMOUSE_FLAG_RENDER) != 0;
}

uint16_t automouse_rgb_current_progress(void) {
    uint16_t progress;
    if (is_keyboard_master()) {
        uint16_t raw_elapsed = auto_mouse_get_time_elapsed();
        progress             = automouse_rgb_should_render_from_state(is_auto_mouse_active(), raw_elapsed) ? automouse_rgb_progress(raw_elapsed) : 0;
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
