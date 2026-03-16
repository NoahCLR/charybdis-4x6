// ────────────────────────────────────────────────────────────────────────────
// Split State Sync (Master → Slave)
// ────────────────────────────────────────────────────────────────────────────
//
// The master half (left) knows the auto-mouse elapsed time and which
// pointing device modes are active.  The slave half (right) needs this
// info to render the correct LED colors.  We use QMK's split RPC
// transport to send a small packet (3 bytes) from master → slave
// whenever the state changes.
//
// To minimize RPC traffic, the elapsed time is quantized to 50ms steps
// and the packet is only sent when it differs from the last one sent.
// During continuous mouse movement, elapsed stays at 0, so no RPCs
// fire.  RPCs only happen during the countdown after the user stops
// moving (~24 RPCs total over the 1200ms timeout).
//
// Both the auto-mouse elapsed time and the pointing device mode flags
// are bundled into a single 3-byte packet to avoid the overhead of
// multiple RPC transactions.
//
// NOTE: The elapsed time comes from auto_mouse_get_time_elapsed(), a
// custom function added to QMK's auto-mouse subsystem in the firmware
// fork at https://github.com/NoahCLR/bastardkb-qmk (branch bkb-master).
// It is not available in stock QMK.
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#if defined(SPLIT_TRANSACTION_IDS_USER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)

#    include "pointing_device_auto_mouse.h"
#    include "pointing_device_modes.h"
#    include "transactions.h"

// ─── Sync packet ─────────────────────────────────────────────────────────────

// Quantization step for elapsed time.  Smaller = more RPCs but smoother
// gradient on the slave.  50ms gives ~24 updates over the countdown.
#    ifndef PD_SYNC_ELAPSED_STEP
#        define PD_SYNC_ELAPSED_STEP 50
#    endif

// 3-byte packet sent from master → slave.
typedef struct __attribute__((packed)) {
    uint16_t elapsed;       // auto-mouse time elapsed, quantized to PD_SYNC_ELAPSED_STEP
    uint8_t  pd_mode_flags; // bitfield of active pointing device modes
} pd_sync_packet_t;

// pd_sync_remote:    latest packet received on the slave side.
// pd_sync_last_sent: last packet sent by the master (for change detection).
static pd_sync_packet_t pd_sync_remote    = {0};
static pd_sync_packet_t pd_sync_last_sent = {0};

// ─── Internal helpers ────────────────────────────────────────────────────────

// Quantize a raw elapsed time to reduce unnecessary RPC sends.
// E.g. 137ms → 100ms, 1500ms → 1200ms (clamped to AUTO_MOUSE_TIME).
static inline uint16_t pd_sync_quantize(uint16_t raw) {
    if (raw > AUTO_MOUSE_TIME) raw = AUTO_MOUSE_TIME;
    return raw / PD_SYNC_ELAPSED_STEP * PD_SYNC_ELAPSED_STEP;
}

// Send the packet to the slave, but only if it changed since last send.
__attribute__((noinline)) static void pd_sync_broadcast(const pd_sync_packet_t *pkt) {
    if (memcmp(&pd_sync_last_sent, pkt, sizeof(pd_sync_packet_t)) == 0) {
        return; // identical to last send — skip
    }
    transaction_rpc_send(PUT_PD_SYNC, sizeof(*pkt), pkt);
    pd_sync_last_sent = *pkt;
}

// RPC handler that runs on the slave when it receives a packet.
// Updates pd_sync_remote (for RGB rendering) and pd_mode_flags (for mode
// overlay colors).
static inline void pd_sync_slave_rpc(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    (void)target2initiator_buffer_size;
    (void)target2initiator_buffer;
    if (initiator2target_buffer_size < sizeof(pd_sync_packet_t)) {
        return;
    }
    memcpy(&pd_sync_remote, initiator2target_buffer, sizeof(pd_sync_packet_t));
    pd_mode_flags = pd_sync_remote.pd_mode_flags;
}

// ─── Public API ──────────────────────────────────────────────────────────────

// Call once during pointing device initialization to register the RPC handler.
static inline void split_sync_init(void) {
    transaction_register_rpc(PUT_PD_SYNC, pd_sync_slave_rpc);
    pd_sync_remote    = (pd_sync_packet_t){0};
    pd_sync_last_sent = (pd_sync_packet_t){0};

    // Send an initial sync so the slave starts with a known state.
    if (is_keyboard_master()) {
        pd_sync_broadcast(&pd_sync_remote);
    }
}

// Broadcast current state to the slave using a pre-read elapsed time.
// Called by automouse_rgb_render() which already reads the timer for
// rendering, avoiding a redundant auto_mouse_get_time_elapsed() call.
__attribute__((noinline)) static void pd_state_sync_elapsed(uint16_t raw_elapsed) {
    if (!is_keyboard_master()) return;
    pd_sync_packet_t pkt = {
        .elapsed       = pd_sync_quantize(raw_elapsed),
        .pd_mode_flags = pd_mode_flags,
    };
    pd_sync_broadcast(&pkt);
}

// Convenience: read elapsed from the timer and broadcast.
// Used by process_record_user on mode flag changes (where no timer
// value is available yet).
__attribute__((noinline)) static void pd_state_sync(void) {
    pd_state_sync_elapsed(auto_mouse_get_time_elapsed());
}

#else  // Required features not all enabled — provide empty stubs.
static inline void split_sync_init(void) {}
static inline void pd_state_sync_elapsed(uint16_t raw_elapsed) {
    (void)raw_elapsed;
}
static inline void pd_state_sync(void) {}
#endif // defined(SPLIT_TRANSACTION_IDS_USER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
