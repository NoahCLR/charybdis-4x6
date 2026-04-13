// ────────────────────────────────────────────────────────────────────────────
// Split Runtime Sync
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#if defined(SPLIT_TRANSACTION_IDS_USER)

#    ifdef CONSOLE_ENABLE
#        include "print.h"
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
#        include "../../key/runtime/key_runtime_feedback.h"
#    endif
#    ifdef POINTING_DEVICE_ENABLE
#        include "../../pointing/defs/pd_modes.h"
#    endif
#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
#        include "../../rgb/automouse/rgb_automouse.h"
#    endif
#    include "../../compat/qmk_contract.h"
#    include "runtime_trace.h"
#    include "split_runtime_sync.h"
#    include "transactions.h" // QMK

split_runtime_sync_packet_t        split_runtime_sync_remote      = {0};
static split_runtime_sync_packet_t split_runtime_sync_last_sent   = {0};
static bool                        split_runtime_sync_sent_once   = false;
static bool                        split_runtime_sync_initialized = false;
static uint32_t                    split_runtime_sync_last_send   = 0;

#    ifndef SPLIT_RUNTIME_SYNC_HEARTBEAT_MS
#        define SPLIT_RUNTIME_SYNC_HEARTBEAT_MS 250
#    endif

static void split_runtime_sync_log_packet_size_mismatch(uint8_t size) {
#    ifdef CONSOLE_ENABLE
    uprintf("Split runtime sync packet size mismatch: received %u bytes, expected %u bytes\n", (unsigned int)size, (unsigned int)sizeof(split_runtime_sync_packet_t));
#    else
    (void)size;
#    endif
}

static split_runtime_sync_packet_t split_runtime_sync_build_packet(uint16_t raw_elapsed) {
    return (split_runtime_sync_packet_t){
#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
        .automouse_progress = pd_any_local_mode_locked() ? 0 : automouse_rgb_quantize_progress(raw_elapsed),
#    else
        .automouse_progress = 0,
#    endif
#    ifdef POINTING_DEVICE_ENABLE
        .pd_mode_flags        = pd_mode_local_active_snapshot(),
        .pd_mode_locked_flags = pd_mode_local_locked_snapshot(),
#    else
        .pd_mode_flags        = 0,
        .pd_mode_locked_flags = 0,
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
        .key_feedback_flags = key_feedback_pack(),
        .key_preview_layer  = key_feedback_preview_layer(),
#    else
        .key_feedback_flags = 0,
        .key_preview_layer  = UINT8_MAX,
#    endif
    };
}

static bool split_runtime_sync_heartbeat_due(void) {
    return !split_runtime_sync_sent_once || timer_elapsed32(split_runtime_sync_last_send) >= SPLIT_RUNTIME_SYNC_HEARTBEAT_MS;
}

static void split_runtime_sync_broadcast(const split_runtime_sync_packet_t *pkt, bool force) {
    bool unchanged = split_runtime_sync_sent_once && memcmp(&split_runtime_sync_last_sent, pkt, sizeof(split_runtime_sync_packet_t)) == 0;

    if (!force && unchanged && !split_runtime_sync_heartbeat_due()) {
        return;
    }

    if (transaction_rpc_send(PUT_SPLIT_RUNTIME_SYNC, sizeof(*pkt), pkt)) {
        split_runtime_sync_last_sent = *pkt;
        split_runtime_sync_sent_once = true;
        split_runtime_sync_last_send = timer_read32();
        noah_runtime_trace_emit(NOAH_TRACE_SPLIT_SYNC, NOAH_TRACE_SPLIT_SYNC_EVENT_SEND, pkt->pd_mode_flags, pkt->pd_mode_locked_flags);
    }
}

static void split_runtime_sync_slave_rpc(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    (void)target2initiator_buffer_size;
    (void)target2initiator_buffer;

    if (initiator2target_buffer_size < sizeof(split_runtime_sync_packet_t)) {
        split_runtime_sync_log_packet_size_mismatch(initiator2target_buffer_size);
        return;
    }

    if (initiator2target_buffer_size != sizeof(split_runtime_sync_packet_t)) {
        split_runtime_sync_log_packet_size_mismatch(initiator2target_buffer_size);
    }

    memcpy(&split_runtime_sync_remote, initiator2target_buffer, sizeof(split_runtime_sync_packet_t));
    noah_runtime_trace_emit(NOAH_TRACE_SPLIT_SYNC, NOAH_TRACE_SPLIT_SYNC_EVENT_RECEIVE, split_runtime_sync_remote.pd_mode_flags, split_runtime_sync_remote.pd_mode_locked_flags);
#    ifdef POINTING_DEVICE_ENABLE
    pd_mode_apply_remote_snapshot(split_runtime_sync_remote.pd_mode_flags, split_runtime_sync_remote.pd_mode_locked_flags);
#    endif
}

void split_runtime_sync_init(void) {
    transaction_register_rpc(PUT_SPLIT_RUNTIME_SYNC, split_runtime_sync_slave_rpc);
    split_runtime_sync_remote      = (split_runtime_sync_packet_t){.key_preview_layer = UINT8_MAX};
    split_runtime_sync_last_sent   = (split_runtime_sync_packet_t){.key_preview_layer = UINT8_MAX};
    split_runtime_sync_sent_once   = false;
    split_runtime_sync_initialized = true;
    split_runtime_sync_last_send   = timer_read32();
    noah_runtime_trace_emit(NOAH_TRACE_SPLIT_SYNC, NOAH_TRACE_SPLIT_SYNC_EVENT_INIT, is_keyboard_master() ? 1u : 0u, 0u);

    if (is_keyboard_master()) {
        split_runtime_sync();
    }
}

void split_runtime_sync_tick(void) {
    uint16_t raw_elapsed = 0;

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    raw_elapsed = noah_qmk_contract_auto_mouse_elapsed();
#    endif

    split_runtime_sync_elapsed(raw_elapsed);
}

static void split_runtime_sync_elapsed_internal(uint16_t raw_elapsed, bool force) {
    if (!split_runtime_sync_initialized || !is_keyboard_master()) return;

    split_runtime_sync_packet_t pkt = split_runtime_sync_build_packet(raw_elapsed);

    split_runtime_sync_broadcast(&pkt, force);
}

void split_runtime_sync_elapsed(uint16_t raw_elapsed) {
    split_runtime_sync_elapsed_internal(raw_elapsed, false);
}

void split_runtime_sync(void) {
    uint16_t raw_elapsed = 0;

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    raw_elapsed = noah_qmk_contract_auto_mouse_elapsed();
#    endif

    split_runtime_sync_elapsed_internal(raw_elapsed, true);
}

#endif // defined(SPLIT_TRANSACTION_IDS_USER)
