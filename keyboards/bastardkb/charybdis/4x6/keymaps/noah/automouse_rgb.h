#pragma once

#if defined(SPLIT_TRANSACTION_IDS_USER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)

#    include <string.h>
#    include "pointing_device_auto_mouse.h"
#    include "rgb_helpers.h"
#    include "transactions.h"

#    ifndef AUTOMOUSE_RGB_DEAD_TIME
#        define AUTOMOUSE_RGB_DEAD_TIME (AUTO_MOUSE_TIME / 3) // time at start where color is static
#    endif

#    define AUTOMOUSE_RGB_ACTIVE_SPAN (AUTO_MOUSE_TIME - AUTOMOUSE_RGB_DEAD_TIME)
#    define AUTOMOUSE_RGB_FLAG_LOCKED 0x01

typedef struct __attribute__((packed)) {
    uint16_t elapsed;
    uint8_t  flags;
} automouse_rgb_packet_t;

// Split packet tracking.
static automouse_rgb_packet_t automouse_rgb_remote    = {0};
static automouse_rgb_packet_t automouse_rgb_last_sent = {0};

// Create a local Auto Mouse RGB packet with current elapsed time and lock status.
static inline automouse_rgb_packet_t automouse_rgb_local_packet(void) {
    uint16_t               elapsed = auto_mouse_get_time_elapsed();
    automouse_rgb_packet_t p       = {
              .elapsed = elapsed,
              .flags   = 0,
    };
    if (get_auto_mouse_toggle()) p.flags |= AUTOMOUSE_RGB_FLAG_LOCKED;
    return p;
}

// Create a default seed packet with zeroed fields.
static inline automouse_rgb_packet_t automouse_rgb_seed_packet(void) {
    return (automouse_rgb_packet_t){
        .elapsed = 0,
        .flags   = 0,
    };
}

// Broadcast the given Auto Mouse RGB packet to the slave side if it has changed.
static inline void automouse_rgb_broadcast(const automouse_rgb_packet_t *pkt) {
    if (memcmp(&automouse_rgb_last_sent, pkt, sizeof(automouse_rgb_packet_t)) == 0) {
        return;
    }
    transaction_rpc_send(PUT_AUTOMOUSE_RGB, sizeof(*pkt), pkt);
    automouse_rgb_last_sent = *pkt;
}

// RPC handler for slave side to receive Auto Mouse RGB packets.
static inline void automouse_rgb_slave_rpc(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    (void)target2initiator_buffer_size;
    (void)target2initiator_buffer;
    if (initiator2target_buffer_size < sizeof(automouse_rgb_packet_t)) {
        return;
    }
    memcpy(&automouse_rgb_remote, initiator2target_buffer, sizeof(automouse_rgb_packet_t));
}

// Initialize Auto Mouse RGB tracking and broadcast initial state.
static inline void automouse_rgb_post_init(void) {
    transaction_register_rpc(PUT_AUTOMOUSE_RGB, automouse_rgb_slave_rpc);

    automouse_rgb_remote    = automouse_rgb_seed_packet();
    automouse_rgb_last_sent = automouse_rgb_seed_packet();

    if (is_keyboard_master()) {
        automouse_rgb_broadcast(&automouse_rgb_remote);
    }
}

// Render a simple gradient countdown on the entire board. Returns true when it handled the layer.
static inline bool automouse_rgb_render(uint8_t top_layer, uint8_t led_min, uint8_t led_max, hsv_t start, hsv_t end, hsv_t locked) {
    bool                   is_master = is_keyboard_master();
    automouse_rgb_packet_t pkt       = is_master ? automouse_rgb_local_packet() : automouse_rgb_remote;

    uint16_t elapsed = pkt.elapsed;

    // Broadcast ASAP once we've validated we're on the right layer and auto-mouse is enabled.
    if (is_master) {
        automouse_rgb_broadcast(&pkt);
    }

    // When auto-mouse is locked (e.g. dragscroll toggle), pin to the lock color.
    if (pkt.flags & AUTOMOUSE_RGB_FLAG_LOCKED) {
        rgb_set_both_halves(hsv_to_rgb(locked), led_min, led_max);
        return true;
    }

    // Clamp elapsed so it does not exceed timeout.
    if (elapsed > AUTO_MOUSE_TIME) {
        elapsed = AUTO_MOUSE_TIME;
    }

    uint16_t prog = (elapsed <= AUTOMOUSE_RGB_DEAD_TIME) ? 0 : (elapsed - AUTOMOUSE_RGB_DEAD_TIME);

    uint8_t value = end.v;
    uint8_t hue   = end.h;
    uint8_t sat   = end.s;

    if (prog == 0) {
        value = start.v;
        hue   = start.h;
        sat   = start.s;
    } else {
        uint32_t t = (uint32_t)prog * 255 / AUTOMOUSE_RGB_ACTIVE_SPAN; // 0-255 lerp factor
        // linear interpolation per channel
        hue   = start.h + (uint8_t)(((int16_t)end.h - (int16_t)start.h) * t / 255);
        sat   = start.s + (uint8_t)(((int16_t)end.s - (int16_t)start.s) * t / 255);
        value = start.v + (uint8_t)(((int16_t)end.v - (int16_t)start.v) * t / 255);
    }

    hsv_t hsv = (hsv_t){.h = hue, .s = sat, .v = value};

    rgb_set_both_halves(hsv_to_rgb(hsv), led_min, led_max);

    return true;
}

#else // SPLIT_TRANSACTION_IDS_USER && POINTING_DEVICE_AUTO_MOUSE_ENABLE && RGB_MATRIX_ENABLE not all defined: define empty stubs to avoid compiler errors.

static inline void automouse_rgb_post_init(void) {}
static inline bool automouse_rgb_render(uint8_t top_layer, uint8_t led_min, uint8_t led_max, hsv_t start, hsv_t end, hsv_t locked) {
    (void)top_layer;
    (void)led_min;
    (void)led_max;
    (void)start;
    (void)end;
    (void)locked;
    return false;
}

#endif // defined(SPLIT_TRANSACTION_IDS_USER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_MATRIX_ENABLE)
