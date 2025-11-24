#pragma once

#if defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)

#    include <string.h>
#    include "pointing_device_auto_mouse.h"
#    include "rgb_helpers.h"
#    ifdef SPLIT_TRANSACTION_IDS_USER
#        include "transactions.h"
#    endif
#    define AUTOMOUSE_RGB_FLAG_LOCKED 0x01
#    define AUTOMOUSE_RGB_FLAG_ARMED 0x02

typedef struct __attribute__((packed)) {
    uint16_t remaining;
    uint16_t timeout;
    uint8_t  flags;
} automouse_rgb_packet_t;

// Utility to paint every LED regardless of half (master computes the whole frame).
static inline void automouse_rgb_set_all(rgb_t color, uint8_t led_min, uint8_t led_max) {
    set_both_sides(color, led_min, led_max);
}

// Tunables for the countdown gradient and minimum visibility.
#    ifndef AUTOMOUSE_RGB_MIN_VALUE
#        define AUTOMOUSE_RGB_MIN_VALUE 12 // keep LEDs visible even near timeout
#    endif
#    ifndef AUTOMOUSE_RGB_DEAD_TIME_NUM
#        define AUTOMOUSE_RGB_DEAD_TIME_NUM 1 // numerator of dead-time fraction (e.g. 1/3)
#    endif
#    ifndef AUTOMOUSE_RGB_DEAD_TIME_DEN
#        define AUTOMOUSE_RGB_DEAD_TIME_DEN 3 // denominator of dead-time fraction
#    endif

// Local tracker for the last time the auto-mouse timer was reset.
static uint16_t automouse_rgb_last_activity = 0;
static bool     automouse_rgb_armed         = false;
#    ifdef SPLIT_TRANSACTION_IDS_USER
static automouse_rgb_packet_t automouse_rgb_remote    = {0};
static automouse_rgb_packet_t automouse_rgb_last_sent = {0};
#    endif

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
    bool                 now_on         = layer_state_cmp(state, get_auto_mouse_layer());
    bool                 was_on         = layer_state_cmp(previous_state, get_auto_mouse_layer());

    if (now_on && !was_on) {
        automouse_rgb_arm_timer();
    } else if (!now_on && was_on) {
        automouse_rgb_disarm();
    }

    previous_state = state;
}

// Called from pointing_device_task_user so we mirror the auto-mouse timer resets.
static inline void automouse_rgb_track_pointing(report_mouse_t mouse_report) {
    if (!is_keyboard_master()) {
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

static inline automouse_rgb_packet_t automouse_rgb_local_packet(void) {
    automouse_rgb_packet_t p = {
        .remaining = automouse_rgb_time_remaining(),
        .timeout   = automouse_rgb_timeout(),
        .flags     = 0,
    };
    if (get_auto_mouse_toggle()) p.flags |= AUTOMOUSE_RGB_FLAG_LOCKED;
    if (automouse_rgb_armed) p.flags |= AUTOMOUSE_RGB_FLAG_ARMED;
    return p;
}

#    ifdef SPLIT_TRANSACTION_IDS_USER
static inline automouse_rgb_packet_t automouse_rgb_seed_packet(void) {
    uint16_t timeout = automouse_rgb_timeout();
    return (automouse_rgb_packet_t){
        .remaining = timeout,
        .timeout   = timeout,
        .flags     = AUTOMOUSE_RGB_FLAG_ARMED,
    };
}

static inline void automouse_rgb_broadcast(const automouse_rgb_packet_t *pkt) {
    if (memcmp(&automouse_rgb_last_sent, pkt, sizeof(automouse_rgb_packet_t)) == 0) {
        return;
    }
    transaction_rpc_send(PUT_AUTOMOUSE_RGB, sizeof(*pkt), pkt);
    automouse_rgb_last_sent = *pkt;
}

static inline void automouse_rgb_slave_rpc(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    (void)target2initiator_buffer_size;
    (void)target2initiator_buffer;
    if (initiator2target_buffer_size < sizeof(automouse_rgb_packet_t)) {
        return;
    }
    memcpy(&automouse_rgb_remote, initiator2target_buffer, sizeof(automouse_rgb_packet_t));
}

static inline void automouse_rgb_post_init(void) {
    automouse_rgb_remote    = automouse_rgb_seed_packet();
    automouse_rgb_last_sent = (automouse_rgb_packet_t){0};
    if (is_keyboard_master()) {
        automouse_rgb_broadcast(&automouse_rgb_remote);
    }
    transaction_register_rpc(PUT_AUTOMOUSE_RGB, automouse_rgb_slave_rpc);
}
#    else
static inline void automouse_rgb_broadcast(const automouse_rgb_packet_t *pkt) {
    (void)pkt;
}
static inline void automouse_rgb_post_init(void) {}
#    endif

// Render a simple gradient countdown on the entire board. Returns true when it handled the layer.
static inline bool automouse_rgb_render(uint8_t top_layer, uint8_t led_min, uint8_t led_max, hsv_t start, hsv_t end, hsv_t locked) {
    if (top_layer != get_auto_mouse_layer() || !automouse_rgb_is_enabled()) {
        return false;
    }

    bool                   is_master = is_keyboard_master();
    automouse_rgb_packet_t pkt =
#    ifdef SPLIT_TRANSACTION_IDS_USER
        is_master ? automouse_rgb_local_packet() : automouse_rgb_remote;
#    else
        automouse_rgb_local_packet();
#    endif

    uint16_t remaining = pkt.remaining;
    uint16_t timeout   = pkt.timeout ? pkt.timeout : automouse_rgb_timeout();

    // Avoid divide-by-zero and keep a minimal pulse even if we never saw activity.
    if (!timeout) {
        timeout = 1;
    }
    if (!(pkt.flags & AUTOMOUSE_RGB_FLAG_ARMED)) {
        automouse_rgb_arm_timer();
        remaining = timeout;
    }

    // When auto-mouse is locked (e.g. dragscroll toggle), pin to the lock color.
    if (pkt.flags & AUTOMOUSE_RGB_FLAG_LOCKED) {
        // Broadcast the locked state so the slave mirrors the solid color.
        if (is_master) {
            automouse_rgb_broadcast(&pkt);
        }
        automouse_rgb_set_all(hsv_to_rgb(locked), led_min, led_max);
        return true;
    }

    // Map remaining time to hue/value for a quick visual countdown with a dead-time window.
    uint16_t dead_time   = (timeout * AUTOMOUSE_RGB_DEAD_TIME_NUM) / AUTOMOUSE_RGB_DEAD_TIME_DEN;
    uint16_t active_span = (timeout > dead_time) ? (timeout - dead_time) : 1;

    uint16_t elapsed = timeout > remaining ? (timeout - remaining) : 0;
    uint16_t prog    = (elapsed <= dead_time) ? 0 : (elapsed - dead_time);

    uint8_t value = end.v;
    uint8_t hue   = end.h;
    uint8_t sat   = end.s;

    if (prog == 0) {
        value = start.v;
        hue   = start.h;
        sat   = start.s;
    } else {
        uint32_t t = (uint32_t)prog * 255 / active_span; // 0-255 lerp factor
        // linear interpolation per channel
        hue   = start.h + (uint8_t)(((int16_t)end.h - (int16_t)start.h) * t / 255);
        sat   = start.s + (uint8_t)(((int16_t)end.s - (int16_t)start.s) * t / 255);
        value = start.v + (uint8_t)(((int16_t)end.v - (int16_t)start.v) * t / 255);
    }

    hsv_t hsv = {.h = hue, .s = sat, .v = value < AUTOMOUSE_RGB_MIN_VALUE ? AUTOMOUSE_RGB_MIN_VALUE : value};

    automouse_rgb_set_all(hsv_to_rgb(hsv), led_min, led_max);

    if (is_master) {
        automouse_rgb_broadcast(&pkt);
    }

    return true;
}

#else // defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)

// No-op shims so callers can remain clean.
static inline void automouse_rgb_track_layer_state(layer_state_t state) {
    (void)state;
}
static inline void automouse_rgb_track_pointing(report_mouse_t mouse_report) {
    (void)mouse_report;
}
static inline void automouse_rgb_track_mousekey(bool pressed) {
    (void)pressed;
}
static inline void automouse_rgb_post_init(void) {}
static inline bool automouse_rgb_render(uint8_t top_layer, uint8_t led_min, uint8_t led_max) {
    (void)top_layer;
    (void)led_min;
    (void)led_max;
    return false;
}

#endif
