// ────────────────────────────────────────────────────────────────────────────
// Split State Sync (Master → Slave)
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#if defined(SPLIT_TRANSACTION_IDS_USER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)

#    include "pointing_device_auto_mouse.h" // QMK (firmware fork)
#    include "pointing_device_modes.h"
#    include "../rgb/rgb_automouse.h"
#    include "split_sync.h"
#    include "transactions.h" // QMK

pd_sync_packet_t        pd_sync_remote    = {0};
static pd_sync_packet_t pd_sync_last_sent = {0};

static void pd_sync_broadcast(const pd_sync_packet_t *pkt) {
    if (memcmp(&pd_sync_last_sent, pkt, sizeof(pd_sync_packet_t)) == 0) {
        return;
    }
    transaction_rpc_send(PUT_PD_SYNC, sizeof(*pkt), pkt);
    pd_sync_last_sent = *pkt;
}

static void pd_sync_slave_rpc(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    (void)target2initiator_buffer_size;
    (void)target2initiator_buffer;

    if (initiator2target_buffer_size < sizeof(pd_sync_packet_t)) {
        return;
    }

    memcpy(&pd_sync_remote, initiator2target_buffer, sizeof(pd_sync_packet_t));
    pd_mode_flags        = pd_sync_remote.pd_mode_flags;
    pd_mode_locked_flags = pd_sync_remote.pd_mode_locked_flags;
}

void split_sync_init(void) {
    transaction_register_rpc(PUT_PD_SYNC, pd_sync_slave_rpc);
    pd_sync_remote    = (pd_sync_packet_t){0};
    pd_sync_last_sent = (pd_sync_packet_t){0};

    if (is_keyboard_master()) {
        pd_sync_broadcast(&pd_sync_remote);
    }
}

void pd_state_sync_elapsed(uint16_t raw_elapsed) {
    if (!is_keyboard_master()) return;

    pd_sync_packet_t pkt = {
        .automouse_progress   = pd_any_mode_locked() ? 0 : automouse_rgb_quantize_progress(raw_elapsed),
        .pd_mode_flags        = pd_mode_flags,
        .pd_mode_locked_flags = pd_mode_locked_flags,
    };

    pd_sync_broadcast(&pkt);
}

void pd_state_sync(void) {
    pd_state_sync_elapsed(auto_mouse_get_time_elapsed());
}

#endif // defined(SPLIT_TRANSACTION_IDS_USER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
