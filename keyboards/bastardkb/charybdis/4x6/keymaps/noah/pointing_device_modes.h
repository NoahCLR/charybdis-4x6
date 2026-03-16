// ────────────────────────────────────────────────────────────────────────────
// Pointing Device Modes
// ────────────────────────────────────────────────────────────────────────────
//
// This module lets the trackball do different things depending on which
// "mode" is active.  Mode keys are dual-purpose: tap sends the base-layer
// key at that position, hold activates the trackball mode.
//
// Available modes:
//
//   VOLUME mode  — Trackball Y-axis controls system volume.
//                  Roll up = volume up, roll down = volume down.
//
//   ARROW mode   — Trackball motion sends arrow key presses.
//                  Dominant axis wins: horizontal motion sends Left/Right,
//                  vertical motion sends Up/Down.  Respects Shift for
//                  text selection.
//
//   BRIGHTNESS mode — Trackball Y-axis controls screen brightness.
//                    Roll up = brighter, roll down = dimmer.
//
//   ZOOM mode    — Trackball Y-axis controls zoom level.
//                  Roll up = zoom in (GUI+Plus), roll down = zoom out (GUI+Minus).
//
//   DRAGSCROLL   — Not handled here; this flag just tracks whether the
//                  Charybdis firmware's native drag-scroll is active,
//                  so RGB can reflect the state.
//
// How to add a new mode:
//   1. Add a PD_MODE_xxx define here (next free bit)
//   2. Add it to pd_mode_priority[] at the desired position, update PD_MODE_COUNT
//   3. Add a custom keycode in keymap.c's enum
//   4. Add the keycode to the tap/hold mode key handler in process_record_user
//   5. Add a handler case in pointing_device_task_user's switch
//   6. Add an RGB color and pd_mode_rgb[] entry in rgb_matrix_indicators (optional)
//   7. Add the keycode to is_mouse_record_user (keeps auto-mouse alive)
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

// ─── Mode flags (bitfield) ────────────────────────────────────────────────
// Each mode is a single bit.  Multiple modes could theoretically be active
// at once; pd_mode_priority[] defines which one wins (checked first = highest).

#define PD_MODE_VOLUME (1 << 0)     // Trackball Y → volume up/down
#define PD_MODE_ARROW (1 << 1)      // Trackball → arrow keys
#define PD_MODE_DRAGSCROLL (1 << 2) // Mirrors charybdis_get_pointer_dragscroll_enabled()
#define PD_MODE_BRIGHTNESS (1 << 3) // Trackball Y → screen brightness up/down
#define PD_MODE_ZOOM (1 << 4)       // Trackball Y → GUI+Plus / GUI+Minus
// #define PD_MODE_xxx       (1 << 5)  // next free slot
// ... up to (1 << 7) for 8 modes in a uint8_t

// Priority order for mode resolution — first active mode wins.
// Both pointing_device_task_user (handler dispatch) and
// rgb_matrix_indicators_advanced_user (right-half color overlay) iterate this
// array, so reordering here changes behavior in both places.
#define PD_MODE_COUNT 5
static const uint8_t pd_mode_priority[PD_MODE_COUNT] = {
    PD_MODE_DRAGSCROLL,
    PD_MODE_VOLUME,
    PD_MODE_BRIGHTNESS,
    PD_MODE_ZOOM,
    PD_MODE_ARROW,
};

// ─── Global mode state ─────────────────────────────────────────────────────
// Single translation unit (keymap.c includes this header) — static is safe.
// If this were included from multiple .c files, each would get its own copy.
static uint8_t pd_mode_flags = 0;

static inline void pd_mode_set(uint8_t mode) {
    pd_mode_flags |= mode;
}
static inline void pd_mode_clear(uint8_t mode) {
    pd_mode_flags &= ~mode;
}
static inline bool pd_mode_active(uint8_t mode) {
    return (pd_mode_flags & mode) != 0;
}

// Convenience: set or clear a mode based on a boolean.
// Typical use: pd_mode_update(PD_MODE_VOLUME, record->event.pressed)
//   → activates on key-down, deactivates on key-up.
static inline void pd_mode_update(uint8_t mode, bool active) {
    if (active)
        pd_mode_set(mode);
    else
        pd_mode_clear(mode);
}

// ─── Mode handler implementations ──────────────────────────────────────────
#if defined(POINTING_DEVICE_ENABLE)

// ─── Helpers ────────────────────────────────────────────────────────────────

// Return a zeroed mouse report — effectively "eats" the trackball motion
// so the OS doesn't also see mouse movement while a mode is active.
static inline report_mouse_t freeze_mouse(void) {
    return (report_mouse_t){0};
}

// ─── Volume mode ────────────────────────────────────────────────────────────
// Accumulates trackball Y-axis motion and sends volume up/down keypresses
// when the threshold is crossed.  X-axis motion is ignored.

#    ifndef VOLUME_THRESHOLD
#        define VOLUME_THRESHOLD 60 // sensor counts of Y motion per volume step
#    endif

static int32_t vol_acc_y    = 0; // accumulated Y motion
static int8_t  vol_last_dir = 0; // last direction

__attribute__((noinline)) static report_mouse_t handle_volume_mode(report_mouse_t mouse_report) {
    int16_t dy = mouse_report.y;

    if (dy != 0) {
        int8_t dir = (dy > 0) ? 1 : -1;
        // Reset accumulator on direction change to prevent "volume bounce"
        if (vol_last_dir != 0 && dir != vol_last_dir) vol_acc_y = 0;
        vol_last_dir = dir;

        vol_acc_y += dy;
        while (vol_acc_y >= VOLUME_THRESHOLD) {
            tap_code(KC_AUDIO_VOL_DOWN);
            vol_acc_y -= VOLUME_THRESHOLD;
        }
        while (vol_acc_y <= -VOLUME_THRESHOLD) {
            tap_code(KC_AUDIO_VOL_UP);
            vol_acc_y += VOLUME_THRESHOLD;
        }
    }

    return freeze_mouse();
}

// ─── Brightness mode ─────────────────────────────────────────────────────────
// Accumulates trackball Y-axis motion and sends brightness up/down keypresses
// when the threshold is crossed.  X-axis motion is ignored.

#    ifndef BRIGHTNESS_THRESHOLD
#        define BRIGHTNESS_THRESHOLD 60 // sensor counts of Y motion per brightness step
#    endif

static int32_t bright_acc_y    = 0; // accumulated Y motion
static int8_t  bright_last_dir = 0; // last direction

__attribute__((noinline)) static report_mouse_t handle_brightness_mode(report_mouse_t mouse_report) {
    int16_t dy = mouse_report.y;

    if (dy != 0) {
        int8_t dir = (dy > 0) ? 1 : -1;
        // Reset accumulator on direction change to prevent "brightness bounce"
        if (bright_last_dir != 0 && dir != bright_last_dir) bright_acc_y = 0;
        bright_last_dir = dir;

        bright_acc_y += dy;
        while (bright_acc_y >= BRIGHTNESS_THRESHOLD) {
            tap_code(KC_BRID);
            bright_acc_y -= BRIGHTNESS_THRESHOLD;
        }
        while (bright_acc_y <= -BRIGHTNESS_THRESHOLD) {
            tap_code(KC_BRIU);
            bright_acc_y += BRIGHTNESS_THRESHOLD;
        }
    }

    return freeze_mouse();
}

// ─── Zoom mode ──────────────────────────────────────────────────────────────
// Accumulates trackball Y-axis motion and sends GUI+Plus / GUI+Minus
// when the threshold is crossed.  Works in browsers, editors, Figma, etc.

#    ifndef ZOOM_THRESHOLD
#        define ZOOM_THRESHOLD 80 // sensor counts of Y motion per zoom step
#    endif

static int32_t zoom_acc_y    = 0; // accumulated Y motion
static int8_t  zoom_last_dir = 0; // last direction

__attribute__((noinline)) static report_mouse_t handle_zoom_mode(report_mouse_t mouse_report) {
    int16_t dy = mouse_report.y;

    if (dy != 0) {
        int8_t dir = (dy > 0) ? 1 : -1;
        // Reset accumulator on direction change to prevent "zoom bounce"
        if (zoom_last_dir != 0 && dir != zoom_last_dir) zoom_acc_y = 0;
        zoom_last_dir = dir;

        zoom_acc_y += dy;
        while (zoom_acc_y >= ZOOM_THRESHOLD) {
            tap_code16(G(KC_MINS));
            zoom_acc_y -= ZOOM_THRESHOLD;
        }
        while (zoom_acc_y <= -ZOOM_THRESHOLD) {
            tap_code16(G(KC_EQL));
            zoom_acc_y += ZOOM_THRESHOLD;
        }
    }

    return freeze_mouse();
}

// ─── Arrow mode ─────────────────────────────────────────────────────────────
// Converts trackball motion into arrow key presses.  Only the dominant axis
// (whichever has more motion) emits keys — the other axis is suppressed and
// its accumulator is reset.  This makes navigation feel intentional rather
// than jittery.

#    ifndef ARROW_THRESHOLD_X
#        define ARROW_THRESHOLD_X 40 // sensor counts of X motion per Left/Right arrow press
#    endif
#    ifndef ARROW_THRESHOLD_Y
#        define ARROW_THRESHOLD_Y 50 // sensor counts of Y motion per Up/Down arrow press
#    endif

static int32_t arrow_acc_x      = 0;    // accumulated X motion
static int32_t arrow_acc_y      = 0;    // accumulated Y motion
static int8_t  arrow_last_x_dir = 0;    // last X direction (+1 or -1)
static int8_t  arrow_last_y_dir = 0;    // last Y direction (+1 or -1)
static bool    arrow_axis_is_x  = true; // true = X dominant, false = Y dominant

// Send an arrow key, adding Shift if it's currently held.
// This enables text selection in arrow mode by holding Shift.
static inline void arrow_send_key(uint16_t keycode) {
    if (get_mods() & MOD_MASK_SHIFT)
        tap_code16(S(keycode));
    else
        tap_code(keycode);
}

// Emit as many arrow presses as the accumulated delta allows.
// Returns the leftover delta (less than one threshold).
static inline int32_t arrow_emit_many(int32_t delta, uint16_t positive, uint16_t negative, uint16_t threshold) {
    while (delta >= threshold) {
        arrow_send_key(positive);
        delta -= threshold;
    }
    while (delta <= -(int32_t)threshold) {
        arrow_send_key(negative);
        delta += threshold;
    }
    return delta;
}

// Determine which axis has more motion in this report.
// The dominant axis "wins" — only that axis emits arrow keys.
// This prevents diagonal trackball movement from producing both
// horizontal and vertical arrows simultaneously.
static inline void arrow_update_dominant_axis(int16_t dx, int16_t dy) {
    int16_t ax = dx >= 0 ? dx : -dx;
    int16_t ay = dy >= 0 ? dy : -dy;
    if (ax > ay && ax > 0)
        arrow_axis_is_x = true;
    else if (ay > ax && ay > 0)
        arrow_axis_is_x = false;
}

__attribute__((noinline)) static report_mouse_t handle_arrow_mode(report_mouse_t mouse_report) {
    int16_t dx = mouse_report.x;
    int16_t dy = mouse_report.y;

    arrow_update_dominant_axis(dx, dy);

    if (arrow_axis_is_x) {
        if (dx != 0) {
            int8_t dir = (dx > 0) ? 1 : -1;
            if (arrow_last_x_dir != 0 && dir != arrow_last_x_dir) arrow_acc_x = 0;
            arrow_last_x_dir = dir;
        }
        arrow_acc_x += dx;
        arrow_acc_x = arrow_emit_many(arrow_acc_x, KC_RIGHT, KC_LEFT, ARROW_THRESHOLD_X);

        // Suppress the off-axis to prevent accidental vertical movement.
        if (dy != 0) {
            arrow_acc_y      = 0;
            arrow_last_y_dir = 0;
        }
    } else {
        if (dy != 0) {
            int8_t dir = (dy > 0) ? 1 : -1;
            if (arrow_last_y_dir != 0 && dir != arrow_last_y_dir) arrow_acc_y = 0;
            arrow_last_y_dir = dir;
        }
        arrow_acc_y += dy;
        arrow_acc_y = arrow_emit_many(arrow_acc_y, KC_DOWN, KC_UP, ARROW_THRESHOLD_Y);

        // Suppress the off-axis to prevent accidental horizontal movement.
        if (dx != 0) {
            arrow_acc_x      = 0;
            arrow_last_x_dir = 0;
        }
    }

    return freeze_mouse();
}

#else  // POINTING_DEVICE_ENABLE not defined: provide empty stubs so keymap.c compiles.
static inline report_mouse_t handle_volume_mode(report_mouse_t mouse_report) {
    return mouse_report;
}
static inline report_mouse_t handle_brightness_mode(report_mouse_t mouse_report) {
    return mouse_report;
}
static inline report_mouse_t handle_zoom_mode(report_mouse_t mouse_report) {
    return mouse_report;
}
static inline report_mouse_t handle_arrow_mode(report_mouse_t mouse_report) {
    return mouse_report;
}
#endif // POINTING_DEVICE_ENABLE
