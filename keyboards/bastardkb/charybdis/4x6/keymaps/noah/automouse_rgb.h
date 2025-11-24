#pragma once

#if defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)

#    include <string.h>
#    include "pointing_device_auto_mouse.h"
#    include "rgb_helpers.h"
#    ifdef SPLIT_TRANSACTION_IDS_USER
#        include "transactions.h"
#    endif

#    define AUTOMOUSE_RGB_FLAG_LOCKED 0x01

typedef struct __attribute__((packed)) {
    uint16_t remaining;
    uint16_t timeout;
    uint8_t  flags;
} automouse_rgb_packet_t;

// Utility to paint a range of LEDs on both halves.
static inline void automouse_rgb_set_all(rgb_t color, uint8_t led_min, uint8_t led_max) {
    set_both_sides(color, led_min, led_max);
}

// Tunables for the countdown gradient and minimum visibility.
#    ifndef AUTOMOUSE_RGB_START_H
#        define AUTOMOUSE_RGB_START_H 0
#    endif
#    ifndef AUTOMOUSE_RGB_START_S
#        define AUTOMOUSE_RGB_START_S 0
#    endif
#    ifndef AUTOMOUSE_RGB_START_V
#        define AUTOMOUSE_RGB_START_V 75
#    endif
#    ifndef AUTOMOUSE_RGB_END_H
#        define AUTOMOUSE_RGB_END_H 0
#    endif
#    ifndef AUTOMOUSE_RGB_END_S
#        define AUTOMOUSE_RGB_END_S 255
#    endif
#    ifndef AUTOMOUSE_RGB_END_V
#        define AUTOMOUSE_RGB_END_V 255
#    endif
#    ifndef AUTOMOUSE_RGB_LOCKED_H
#        define AUTOMOUSE_RGB_LOCKED_H 21
#    endif
#    ifndef AUTOMOUSE_RGB_LOCKED_S
#        define AUTOMOUSE_RGB_LOCKED_S 255
#    endif
#    ifndef AUTOMOUSE_RGB_LOCKED_V
#        define AUTOMOUSE_RGB_LOCKED_V 255
#    endif
#    ifndef AUTOMOUSE_RGB_MIN_VALUE
#        define AUTOMOUSE_RGB_MIN_VALUE 12 // keep LEDs visible even near timeout
#    endif
#    ifndef AUTOMOUSE_RGB_DEAD_TIME_NUM
#        define AUTOMOUSE_RGB_DEAD_TIME_NUM 1 // numerator of dead-time fraction (e.g. 1/3)
#    endif
#    ifndef AUTOMOUSE_RGB_DEAD_TIME_DEN
#        define AUTOMOUSE_RGB_DEAD_TIME_DEN 3 // denominator of dead-time fraction
#    endif

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

// Time remaining comes directly from auto-mouse core now.
static inline uint16_t automouse_rgb_time_remaining(void) {
    return auto_mouse_get_time_remaining();
}

static inline automouse_rgb_packet_t automouse_rgb_local_packet(void) {
    uint16_t timeout   = automouse_rgb_timeout();
    uint16_t remaining = automouse_rgb_time_remaining();

    automouse_rgb_packet_t p = {
        .remaining = remaining,
        .timeout   = timeout,
        .flags     = 0,
    };
    if (get_auto_mouse_toggle()) p.flags |= AUTOMOUSE_RGB_FLAG_LOCKED;
    return p;
}

#    ifdef SPLIT_TRANSACTION_IDS_USER
static inline automouse_rgb_packet_t automouse_rgb_seed_packet(void) {
    uint16_t timeout = automouse_rgb_timeout();
    return (automouse_rgb_packet_t){
        .remaining = timeout,
        .timeout   = timeout,
        .flags     = 0,
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
static inline bool automouse_rgb_render(uint8_t top_layer, uint8_t led_min, uint8_t led_max) {
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

    uint16_t timeout   = pkt.timeout ? pkt.timeout : automouse_rgb_timeout();
    uint16_t remaining = pkt.remaining;

    // Avoid divide-by-zero and keep a minimal pulse even if we never saw activity.
    if (!timeout) {
        timeout = 1;
    }

    // Define start, end, and locked colors in HSV space.
    hsv_t start  = {.h = AUTOMOUSE_RGB_START_H, .s = AUTOMOUSE_RGB_START_S, .v = AUTOMOUSE_RGB_START_V};
    hsv_t end    = {.h = AUTOMOUSE_RGB_END_H, .s = AUTOMOUSE_RGB_END_S, .v = AUTOMOUSE_RGB_END_V};
    hsv_t locked = {.h = AUTOMOUSE_RGB_LOCKED_H, .s = AUTOMOUSE_RGB_LOCKED_S, .v = AUTOMOUSE_RGB_LOCKED_V};

    // Clamp to current brightness setting.
    uint8_t base_value = rgb_matrix_get_val();
    if (start.v > base_value) start.v = base_value;
    if (end.v > base_value) end.v = base_value;
    if (locked.v > base_value) locked.v = base_value;

    // When auto-mouse is locked (e.g. dragscroll toggle), pin to the lock color.
    if (pkt.flags & AUTOMOUSE_RGB_FLAG_LOCKED) {
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

    if (value < AUTOMOUSE_RGB_MIN_VALUE) {
        value = AUTOMOUSE_RGB_MIN_VALUE;
    }

    hsv_t hsv = {.h = hue, .s = sat, .v = value};

    automouse_rgb_set_all(hsv_to_rgb(hsv), led_min, led_max);

    if (is_master) {
        automouse_rgb_broadcast(&pkt);
    }

    return true;
}

#else // defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)

static inline void automouse_rgb_post_init(void) {}
static inline bool automouse_rgb_render(uint8_t top_layer, uint8_t led_min, uint8_t led_max) {
    (void)top_layer;
    (void)led_min;
    (void)led_max;
    return false;
}

#endif