// ────────────────────────────────────────────────────────────────────────────
// Pointing Shared State
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#if defined(SPLIT_TRANSACTION_IDS_USER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)

#    include "pointing_device_auto_mouse.h" // QMK (firmware fork)
#    include "../pointing/pointing_device_modes.h"
#    include "../rgb/rgb_automouse.h"
#    include "pd_shared_state.h"
#    include "transactions.h" // QMK

pd_shared_state_packet_t        pd_shared_state_remote    = {0};
static pd_shared_state_packet_t pd_shared_state_last_sent = {0};
static bool                     pd_shared_state_sent_once = false;

static void pd_shared_state_broadcast(const pd_shared_state_packet_t *pkt) {
    if (pd_shared_state_sent_once && memcmp(&pd_shared_state_last_sent, pkt, sizeof(pd_shared_state_packet_t)) == 0) {
        return;
    }
    if (transaction_rpc_send(PUT_PD_SYNC, sizeof(*pkt), pkt)) {
        pd_shared_state_last_sent = *pkt;
        pd_shared_state_sent_once = true;
    }
}

static void pd_shared_state_slave_rpc(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    (void)target2initiator_buffer_size;
    (void)target2initiator_buffer;

    if (initiator2target_buffer_size < sizeof(pd_shared_state_packet_t)) {
        return;
    }

    memcpy(&pd_shared_state_remote, initiator2target_buffer, sizeof(pd_shared_state_packet_t));
    pd_mode_apply_remote_snapshot(pd_shared_state_remote.pd_mode_flags, pd_shared_state_remote.pd_mode_locked_flags);
}

void pd_shared_state_init(void) {
    transaction_register_rpc(PUT_PD_SYNC, pd_shared_state_slave_rpc);
    pd_shared_state_remote    = (pd_shared_state_packet_t){0};
    pd_shared_state_last_sent = (pd_shared_state_packet_t){0};
    pd_shared_state_sent_once = false;

    if (is_keyboard_master()) {
        pd_shared_state_broadcast(&pd_shared_state_remote);
    }
}

void pd_shared_state_sync_tick(void) {
    pd_shared_state_sync_elapsed(auto_mouse_get_time_elapsed());
}

void pd_shared_state_sync_elapsed(uint16_t raw_elapsed) {
    if (!is_keyboard_master()) return;

    pd_shared_state_packet_t pkt = {
        .automouse_progress   = pd_any_mode_locked() ? 0 : automouse_rgb_quantize_progress(raw_elapsed),
        .pd_mode_flags        = pd_mode_active_snapshot(),
        .pd_mode_locked_flags = pd_mode_locked_snapshot(),
    };

    pd_shared_state_broadcast(&pkt);
}

void pd_shared_state_sync(void) {
    pd_shared_state_sync_tick();
}

#endif // defined(SPLIT_TRANSACTION_IDS_USER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
