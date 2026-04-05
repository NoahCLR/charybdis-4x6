// ────────────────────────────────────────────────────────────────────────────
// Runtime Shared State
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#if defined(SPLIT_TRANSACTION_IDS_USER)

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
#        include "pointing_device_auto_mouse.h" // QMK (firmware fork)
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
#        include "../key/key_runtime_feedback.h"
#    endif
#    ifdef POINTING_DEVICE_ENABLE
#        include "../pointing/pointing_device_modes.h"
#    endif
#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
#        include "../rgb/rgb_automouse.h"
#    endif
#    include "runtime_shared_state.h"
#    include "transactions.h" // QMK

runtime_shared_state_packet_t        runtime_shared_state_remote      = {0};
static runtime_shared_state_packet_t runtime_shared_state_last_sent   = {0};
static bool                          runtime_shared_state_sent_once   = false;
static bool                          runtime_shared_state_initialized = false;
static uint32_t                      runtime_shared_state_last_send   = 0;

#    ifndef RUNTIME_SHARED_STATE_HEARTBEAT_MS
#        define RUNTIME_SHARED_STATE_HEARTBEAT_MS 250
#    endif

static runtime_shared_state_packet_t runtime_shared_state_build_packet(uint16_t raw_elapsed) {
    return (runtime_shared_state_packet_t){
#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
        .automouse_progress = pd_any_mode_locked() ? 0 : automouse_rgb_quantize_progress(raw_elapsed),
#    else
        .automouse_progress = 0,
#    endif
#    ifdef POINTING_DEVICE_ENABLE
        .pd_mode_flags        = pd_mode_active_snapshot(),
        .pd_mode_locked_flags = pd_mode_locked_snapshot(),
#    else
        .pd_mode_flags        = 0,
        .pd_mode_locked_flags = 0,
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
        .key_feedback_flags = key_feedback_pack(),
#    else
        .key_feedback_flags = 0,
#    endif
    };
}

static bool runtime_shared_state_heartbeat_due(void) {
    return !runtime_shared_state_sent_once || timer_elapsed32(runtime_shared_state_last_send) >= RUNTIME_SHARED_STATE_HEARTBEAT_MS;
}

static void runtime_shared_state_broadcast(const runtime_shared_state_packet_t *pkt, bool force) {
    bool unchanged = runtime_shared_state_sent_once && memcmp(&runtime_shared_state_last_sent, pkt, sizeof(runtime_shared_state_packet_t)) == 0;

    if (!force && unchanged && !runtime_shared_state_heartbeat_due()) {
        return;
    }

    if (transaction_rpc_send(PUT_RUNTIME_SHARED_SYNC, sizeof(*pkt), pkt)) {
        runtime_shared_state_last_sent = *pkt;
        runtime_shared_state_sent_once = true;
        runtime_shared_state_last_send = timer_read32();
    }
}

static void runtime_shared_state_slave_rpc(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    (void)target2initiator_buffer_size;
    (void)target2initiator_buffer;

    if (initiator2target_buffer_size < sizeof(runtime_shared_state_packet_t)) {
        return;
    }

    memcpy(&runtime_shared_state_remote, initiator2target_buffer, sizeof(runtime_shared_state_packet_t));
#    ifdef POINTING_DEVICE_ENABLE
    pd_mode_apply_remote_snapshot(runtime_shared_state_remote.pd_mode_flags, runtime_shared_state_remote.pd_mode_locked_flags);
#    endif
}

void runtime_shared_state_init(void) {
    transaction_register_rpc(PUT_RUNTIME_SHARED_SYNC, runtime_shared_state_slave_rpc);
    runtime_shared_state_remote      = (runtime_shared_state_packet_t){0};
    runtime_shared_state_last_sent   = (runtime_shared_state_packet_t){0};
    runtime_shared_state_sent_once   = false;
    runtime_shared_state_initialized = true;
    runtime_shared_state_last_send   = timer_read32();

    if (is_keyboard_master()) {
        runtime_shared_state_sync();
    }
}

void runtime_shared_state_sync_tick(void) {
    uint16_t raw_elapsed = 0;

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    raw_elapsed = auto_mouse_get_time_elapsed();
#    endif

    runtime_shared_state_sync_elapsed(raw_elapsed);
}

static void runtime_shared_state_sync_elapsed_internal(uint16_t raw_elapsed, bool force) {
    if (!runtime_shared_state_initialized || !is_keyboard_master()) return;

    runtime_shared_state_packet_t pkt = runtime_shared_state_build_packet(raw_elapsed);

    runtime_shared_state_broadcast(&pkt, force);
}

void runtime_shared_state_sync_elapsed(uint16_t raw_elapsed) {
    runtime_shared_state_sync_elapsed_internal(raw_elapsed, false);
}

void runtime_shared_state_sync(void) {
    uint16_t raw_elapsed = 0;

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    raw_elapsed = auto_mouse_get_time_elapsed();
#    endif

    runtime_shared_state_sync_elapsed_internal(raw_elapsed, true);
}

#endif // defined(SPLIT_TRANSACTION_IDS_USER)
