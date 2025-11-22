#pragma once

#if defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)

#    include "pointing_device_auto_mouse.h"
#    include "rgb_helpers.h"

// Utility to paint every LED regardless of half (master computes the whole frame).
static inline void automouse_rgb_set_all(rgb_t color) {
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        rgb_matrix_set_color(i, color.r, color.g, color.b);
    }
}

// Tunables for the countdown gradient and minimum visibility.
#    ifndef AUTOMOUSE_RGB_HUE_FULL
#        define AUTOMOUSE_RGB_HUE_FULL 85 // green-ish when plenty of time remains
#    endif
#    ifndef AUTOMOUSE_RGB_HUE_EMPTY
#        define AUTOMOUSE_RGB_HUE_EMPTY 0 // red when about to time out
#    endif
#    ifndef AUTOMOUSE_RGB_HUE_LOCKED
#        define AUTOMOUSE_RGB_HUE_LOCKED 180 // cyan when auto-mouse is locked on
#    endif
#    ifndef AUTOMOUSE_RGB_MIN_VALUE
#        define AUTOMOUSE_RGB_MIN_VALUE 12 // keep LEDs visible even near timeout
#    endif

// Local tracker for the last time the auto-mouse timer was reset.
static uint16_t automouse_rgb_last_activity = 0;
static bool     automouse_rgb_armed         = false;

static inline bool automouse_rgb_is_enabled(void) {
    return get_auto_mouse_enable();
}

static inline uint16_t automouse_rgb_timeout(void) {
    uint16_t timeout = get_auto_mouse_timeout();
    return timeout ? timeout : AUTO_MOUSE_TIME;
}

static inline void automouse_rgb_arm_timer(void) {
    automouse_rgb_last_activity = timer_read();
    automouse_rgb_armed         = true;
}

static inline void automouse_rgb_disarm(void) {
    automouse_rgb_armed = false;
}

static inline void automouse_rgb_note_activity(void) {
    if (!automouse_rgb_is_enabled()) {
        automouse_rgb_disarm();
        return;
    }
    automouse_rgb_arm_timer();
}

// Track pointer layer on/off so we can arm/disarm the countdown cleanly.
static inline void automouse_rgb_track_layer_state(layer_state_t state) {
    static layer_state_t previous_state = 0;
    bool                  now_on        = layer_state_cmp(state, get_auto_mouse_layer());
    bool                  was_on        = layer_state_cmp(previous_state, get_auto_mouse_layer());

    if (now_on && !was_on) {
        automouse_rgb_arm_timer();
    } else if (!now_on && was_on) {
        automouse_rgb_disarm();
    }

    previous_state = state;
}

// Called from pointing_device_task_user so we mirror the auto-mouse timer resets.
static inline void automouse_rgb_track_pointing(report_mouse_t mouse_report) {
    if (this_is_left_half()) {
        // No sensor on the non-pointer side.
        return;
    }

    if (!automouse_rgb_is_enabled()) {
        return;
    }

    // Core auto-mouse resets its timer whenever it is considered active.
    if (is_auto_mouse_active()) {
        automouse_rgb_arm_timer();
        return;
    }

    // Fallback: treat any movement or button as activity.
    if (mouse_report.x || mouse_report.y || mouse_report.v || mouse_report.h || mouse_report.buttons) {
        automouse_rgb_arm_timer();
    }
}

// Call on mouse-key presses (KC_MS_BTN*, DRG_TOG, etc.) to mirror key-driven resets.
static inline void automouse_rgb_track_mousekey(bool pressed) {
    if (pressed) {
        automouse_rgb_note_activity();
    }
}

static inline uint16_t automouse_rgb_time_remaining(void) {
    if (!automouse_rgb_armed) {
        return 0;
    }

    uint16_t timeout = automouse_rgb_timeout();
    uint16_t elapsed = timer_elapsed(automouse_rgb_last_activity);

    if (elapsed >= timeout) {
        return 0;
    }
    return timeout - elapsed;
}

// Render a simple gradient countdown on the entire board. Returns true when it handled the layer.
static inline bool automouse_rgb_render(uint8_t top_layer) {
    if (top_layer != get_auto_mouse_layer() || !automouse_rgb_is_enabled()) {
        return false;
    }

    uint16_t timeout    = automouse_rgb_timeout();
    uint16_t remaining  = automouse_rgb_time_remaining();
    uint8_t  base_value = rgb_matrix_get_val();

    // Avoid divide-by-zero and keep a minimal pulse even if we never saw activity.
    if (!timeout) {
        timeout = 1;
    }
    if (!automouse_rgb_armed) {
        automouse_rgb_arm_timer();
        remaining = timeout;
    }

    // When auto-mouse is locked (e.g. dragscroll toggle), pin to the lock color.
    if (get_auto_mouse_toggle()) {
        hsv_t hsv = {.h = AUTOMOUSE_RGB_HUE_LOCKED, .s = 255, .v = base_value};
        automouse_rgb_set_all(hsv_to_rgb(hsv));
        return true;
    }

    // Map remaining time to hue/value for a quick visual countdown.
    uint8_t  value    = (uint8_t)(((uint32_t)remaining * base_value) / timeout);
    int16_t  hue_span = (int16_t)AUTOMOUSE_RGB_HUE_FULL - (int16_t)AUTOMOUSE_RGB_HUE_EMPTY;
    uint8_t  hue      = (uint8_t)((int16_t)AUTOMOUSE_RGB_HUE_EMPTY + (hue_span * (int16_t)remaining) / (int16_t)timeout);
    hsv_t    hsv      = {.h = hue, .s = 255, .v = value < AUTOMOUSE_RGB_MIN_VALUE ? AUTOMOUSE_RGB_MIN_VALUE : value};

    automouse_rgb_set_all(hsv_to_rgb(hsv));
    return true;
}

#else // defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)

// No-op shims so callers can remain clean.
static inline void automouse_rgb_track_layer_state(layer_state_t state) { (void)state; }
static inline void automouse_rgb_track_pointing(report_mouse_t mouse_report) { (void)mouse_report; }
static inline void automouse_rgb_track_mousekey(bool pressed) { (void)pressed; }
static inline bool automouse_rgb_render(uint8_t top_layer) {
    (void)top_layer;
    return false;
}

#endif
