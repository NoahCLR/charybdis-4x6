// ────────────────────────────────────────────────────────────────────────────
// Split Runtime Sync
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#if defined(SPLIT_TRANSACTION_IDS_USER)

#    include <string.h>

#    ifdef CONSOLE_ENABLE
#        include "print.h"
#    endif
#    ifdef POINTING_DEVICE_ENABLE
#        include "../pointing/defs/pd_modes.h"
#    endif
#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
#        include "../rgb/automouse/rgb_automouse.h"
#    endif
#    include "../compat/qmk_auto_mouse_contract.h"
#    include "../key/runtime/feedback.h"
#    include "../state/diagnostics/runtime_trace.h"
#    include "runtime_sync.h"
#    include "runtime_sync_dirty.h"
#    include "transactions.h" // QMK

split_runtime_sync_remote_t                         split_runtime_sync_remote                     = SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT;
static split_runtime_base_sync_packet_t             split_runtime_base_last_sent                  = {0};
static split_runtime_combo_feedback_packet_t        split_runtime_combo_last_sent                 = {0};
static split_runtime_key_feedback_semantic_packet_t split_runtime_key_feedback_semantic_last_sent = {0};
static split_runtime_key_feedback_branch_packet_t   split_runtime_key_feedback_branch_last_sent   = {0};
static bool                                         split_runtime_base_sent_once                  = false;
static bool                                         split_runtime_combo_sent_once                 = false;
static bool                                         split_runtime_key_feedback_semantic_sent_once = false;
static bool                                         split_runtime_key_feedback_branch_sent_once   = false;
static bool                                         split_runtime_sync_initialized                = false;
static uint32_t                                     split_runtime_base_last_send                  = 0;
static uint32_t                                     split_runtime_combo_last_send                 = 0;
static uint32_t                                     split_runtime_key_feedback_semantic_last_send = 0;
static uint32_t                                     split_runtime_key_feedback_branch_last_send   = 0;

#    ifndef SPLIT_RUNTIME_SYNC_ACTIVE_HEARTBEAT_MS
#        define SPLIT_RUNTIME_SYNC_ACTIVE_HEARTBEAT_MS 250
#    endif

#    ifndef SPLIT_RUNTIME_SYNC_IDLE_HEARTBEAT_MS
#        define SPLIT_RUNTIME_SYNC_IDLE_HEARTBEAT_MS 1000
#    endif

static void split_runtime_sync_log_packet_size_mismatch(const char *packet_name, uint8_t size, uint8_t expected) {
#    ifdef CONSOLE_ENABLE
    uprintf("Split runtime %s packet size mismatch: received %u bytes, expected %u bytes\n", packet_name, (unsigned int)size, (unsigned int)expected);
#    else
    (void)packet_name;
    (void)size;
    (void)expected;
#    endif
}

static void split_runtime_sync_elapsed_internal(uint16_t raw_elapsed, bool force);

static split_runtime_base_sync_packet_t split_runtime_sync_build_base_packet(uint16_t raw_elapsed) {
    split_runtime_base_sync_packet_t packet = {
#    if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
        .automouse_progress = pd_any_local_mode_locked() ? 0 : automouse_rgb_quantize_progress(raw_elapsed),
#    else
        .automouse_progress = 0,
#    endif
#    ifdef POINTING_DEVICE_ENABLE
        .active_mode_id = pd_mode_id_from_mask(pd_mode_local_active_snapshot()),
        .locked_mode_id = pd_mode_id_from_mask(pd_mode_local_locked_snapshot()),
#        ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
        .pd_mode_owner_sides = pd_mode_local_owner_sides_snapshot(),
#        endif
#    else
        .active_mode_id = PD_MODE_ID_NONE,
        .locked_mode_id = PD_MODE_ID_NONE,
#        ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
        .pd_mode_owner_sides = SPLIT_SIDE_MASK_NONE,
#        endif
#    endif
        .key_preview_layer = key_feedback_preview_layer(),
    };

#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_ACTIVE_HALF_ENABLE)
    (void)pd_mode_local_owner_bitmap_snapshot(packet.pd_mode_owner_bitmap);
#    endif

    return packet;
}

static split_runtime_combo_feedback_packet_t split_runtime_sync_build_combo_packet(void) {
    split_runtime_combo_feedback_packet_t packet = {0};

    combo_feedback_underlay_bitmap(packet.combo_underlay_bitmap);
    combo_feedback_overlay_bitmap(packet.combo_overlay_bitmap);

    return packet;
}

static split_runtime_key_feedback_semantic_packet_t split_runtime_sync_build_key_feedback_semantic_packet(void) {
    split_runtime_key_feedback_semantic_packet_t packet = {0};

    key_feedback_semantic_map(packet.key_feedback_semantic_map);
    key_feedback_flash_visibility_bitmap_for_semantic_map(packet.key_feedback_semantic_map, packet.key_feedback_flash_visibility_bitmap);

    return packet;
}

static split_runtime_key_feedback_branch_packet_t split_runtime_sync_build_key_feedback_branch_packet(void) {
    split_runtime_key_feedback_branch_packet_t packet = {0};

    key_feedback_broad_owner_map(packet.key_feedback_broad_owner_map);
    key_feedback_tap_branch_map(packet.key_feedback_tap_branch_map);

    return packet;
}

static bool split_runtime_base_packet_is_active(const split_runtime_base_sync_packet_t *pkt) {
    if (!pkt) {
        return false;
    }

    return pkt->automouse_progress != 0u || pkt->active_mode_id != PD_MODE_ID_NONE || pkt->locked_mode_id != PD_MODE_ID_NONE
#    ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
           || pkt->pd_mode_owner_sides != SPLIT_SIDE_MASK_NONE || key_origin_bitmap_has_any(pkt->pd_mode_owner_bitmap)
#    endif
           || pkt->key_preview_layer != UINT8_MAX;
}

static bool split_runtime_combo_packet_is_active(const split_runtime_combo_feedback_packet_t *pkt) {
    return pkt && (key_origin_bitmap_has_any(pkt->combo_underlay_bitmap) || key_origin_bitmap_has_any(pkt->combo_overlay_bitmap));
}

static bool split_runtime_key_feedback_semantic_packet_is_active(const split_runtime_key_feedback_semantic_packet_t *pkt) {
    return pkt && key_feedback_semantic_map_has_any(pkt->key_feedback_semantic_map);
}

static bool split_runtime_key_feedback_branch_packet_is_active(const split_runtime_key_feedback_branch_packet_t *pkt) {
    if (!pkt) {
        return false;
    }

    if (key_feedback_broad_owner_map_has_any(pkt->key_feedback_broad_owner_map)) {
        return true;
    }

    for (uint8_t index = 0; index < KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE; index++) {
        if (pkt->key_feedback_tap_branch_map[index] != 0u) {
            return true;
        }
    }

    return false;
}

static bool split_runtime_sync_heartbeat_due(bool sent_once, uint32_t last_send, bool active) {
    uint32_t heartbeat_ms = active ? SPLIT_RUNTIME_SYNC_ACTIVE_HEARTBEAT_MS : SPLIT_RUNTIME_SYNC_IDLE_HEARTBEAT_MS;

    return !sent_once || timer_elapsed32(last_send) >= heartbeat_ms;
}

static bool split_runtime_sync_should_build_packet(bool force, bool dirty, bool sent_once, uint32_t last_send, bool last_sent_active) {
    if (force || dirty || last_sent_active) {
        return true;
    }

    return split_runtime_sync_heartbeat_due(sent_once, last_send, false);
}

static bool split_runtime_sync_broadcast_base(const split_runtime_base_sync_packet_t *pkt, bool force) {
    bool unchanged = split_runtime_base_sent_once && memcmp(&split_runtime_base_last_sent, pkt, sizeof(*pkt)) == 0;
    bool active    = split_runtime_base_packet_is_active(pkt);

    if (!force && unchanged && !split_runtime_sync_heartbeat_due(split_runtime_base_sent_once, split_runtime_base_last_send, active)) {
        return true;
    }

    if (transaction_rpc_send(PUT_SPLIT_RUNTIME_BASE_SYNC, sizeof(*pkt), pkt)) {
        split_runtime_base_last_sent = *pkt;
        split_runtime_base_sent_once = true;
        split_runtime_base_last_send = timer_read32();
        noah_runtime_trace_emit(NOAH_TRACE_SPLIT_SYNC, NOAH_TRACE_SPLIT_SYNC_EVENT_SEND, pd_mode_mask_from_id(pkt->active_mode_id), pd_mode_mask_from_id(pkt->locked_mode_id));
        return true;
    }

    return false;
}

static bool split_runtime_sync_broadcast_combo(const split_runtime_combo_feedback_packet_t *pkt, bool force) {
    bool unchanged = split_runtime_combo_sent_once && memcmp(&split_runtime_combo_last_sent, pkt, sizeof(*pkt)) == 0;
    bool active    = split_runtime_combo_packet_is_active(pkt);

    if (!force && unchanged && !split_runtime_sync_heartbeat_due(split_runtime_combo_sent_once, split_runtime_combo_last_send, active)) {
        return true;
    }

    if (transaction_rpc_send(PUT_SPLIT_COMBO_FEEDBACK_SYNC, sizeof(*pkt), pkt)) {
        split_runtime_combo_last_sent = *pkt;
        split_runtime_combo_sent_once = true;
        split_runtime_combo_last_send = timer_read32();
        return true;
    }

    return false;
}

static bool split_runtime_sync_broadcast_key_feedback_semantic(const split_runtime_key_feedback_semantic_packet_t *pkt, bool force) {
    bool unchanged = split_runtime_key_feedback_semantic_sent_once && memcmp(&split_runtime_key_feedback_semantic_last_sent, pkt, sizeof(*pkt)) == 0;
    bool active    = split_runtime_key_feedback_semantic_packet_is_active(pkt);

    if (!force && unchanged && !split_runtime_sync_heartbeat_due(split_runtime_key_feedback_semantic_sent_once, split_runtime_key_feedback_semantic_last_send, active)) {
        return true;
    }

    if (transaction_rpc_send(PUT_SPLIT_KEY_FEEDBACK_SEMANTIC_SYNC, sizeof(*pkt), pkt)) {
        split_runtime_key_feedback_semantic_last_sent = *pkt;
        split_runtime_key_feedback_semantic_sent_once = true;
        split_runtime_key_feedback_semantic_last_send = timer_read32();
        return true;
    }

    return false;
}

static bool split_runtime_sync_broadcast_key_feedback_branch(const split_runtime_key_feedback_branch_packet_t *pkt, bool force) {
    bool unchanged = split_runtime_key_feedback_branch_sent_once && memcmp(&split_runtime_key_feedback_branch_last_sent, pkt, sizeof(*pkt)) == 0;
    bool active    = split_runtime_key_feedback_branch_packet_is_active(pkt);

    if (!force && unchanged && !split_runtime_sync_heartbeat_due(split_runtime_key_feedback_branch_sent_once, split_runtime_key_feedback_branch_last_send, active)) {
        return true;
    }

    if (transaction_rpc_send(PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC, sizeof(*pkt), pkt)) {
        split_runtime_key_feedback_branch_last_sent = *pkt;
        split_runtime_key_feedback_branch_sent_once = true;
        split_runtime_key_feedback_branch_last_send = timer_read32();
        return true;
    }

    return false;
}

static void split_runtime_sync_slave_base_rpc(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    const split_runtime_base_sync_packet_t *packet = initiator2target_buffer;

    (void)target2initiator_buffer_size;
    (void)target2initiator_buffer;

    if (initiator2target_buffer_size < sizeof(split_runtime_base_sync_packet_t)) {
        split_runtime_sync_log_packet_size_mismatch("base", initiator2target_buffer_size, sizeof(split_runtime_base_sync_packet_t));
        return;
    }

    if (initiator2target_buffer_size != sizeof(split_runtime_base_sync_packet_t)) {
        split_runtime_sync_log_packet_size_mismatch("base", initiator2target_buffer_size, sizeof(split_runtime_base_sync_packet_t));
    }

    split_runtime_sync_remote.automouse_progress = packet->automouse_progress;
    split_runtime_sync_remote.active_mode_id     = packet->active_mode_id;
    split_runtime_sync_remote.locked_mode_id     = packet->locked_mode_id;
#    ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    split_runtime_sync_remote.pd_mode_owner_sides = packet->pd_mode_owner_sides;
    key_origin_bitmap_copy(split_runtime_sync_remote.pd_mode_owner_bitmap, packet->pd_mode_owner_bitmap);
#    endif
    split_runtime_sync_remote.key_preview_layer = packet->key_preview_layer;

    noah_runtime_trace_emit(NOAH_TRACE_SPLIT_SYNC, NOAH_TRACE_SPLIT_SYNC_EVENT_RECEIVE, pd_mode_mask_from_id(packet->active_mode_id), pd_mode_mask_from_id(packet->locked_mode_id));
#    ifdef POINTING_DEVICE_ENABLE
    pd_mode_apply_remote_mode_ids_with_owner_bitmap(packet->active_mode_id, packet->locked_mode_id,
#        ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
                                                    packet->pd_mode_owner_sides, packet->pd_mode_owner_bitmap
#        else
                                                    SPLIT_SIDE_MASK_NONE, NULL
#        endif
    );
#    endif
}

static void split_runtime_sync_slave_combo_rpc(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    const split_runtime_combo_feedback_packet_t *packet = initiator2target_buffer;

    (void)target2initiator_buffer_size;
    (void)target2initiator_buffer;

    if (initiator2target_buffer_size < sizeof(split_runtime_combo_feedback_packet_t)) {
        split_runtime_sync_log_packet_size_mismatch("combo", initiator2target_buffer_size, sizeof(split_runtime_combo_feedback_packet_t));
        return;
    }

    if (initiator2target_buffer_size != sizeof(split_runtime_combo_feedback_packet_t)) {
        split_runtime_sync_log_packet_size_mismatch("combo", initiator2target_buffer_size, sizeof(split_runtime_combo_feedback_packet_t));
    }

    key_origin_bitmap_copy(split_runtime_sync_remote.combo_underlay_bitmap, packet->combo_underlay_bitmap);
    key_origin_bitmap_copy(split_runtime_sync_remote.combo_overlay_bitmap, packet->combo_overlay_bitmap);
}

static void split_runtime_sync_slave_key_feedback_semantic_rpc(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    const split_runtime_key_feedback_semantic_packet_t *packet = initiator2target_buffer;

    (void)target2initiator_buffer_size;
    (void)target2initiator_buffer;

    if (initiator2target_buffer_size < sizeof(split_runtime_key_feedback_semantic_packet_t)) {
        split_runtime_sync_log_packet_size_mismatch("key-feedback semantic", initiator2target_buffer_size, sizeof(split_runtime_key_feedback_semantic_packet_t));
        return;
    }

    if (initiator2target_buffer_size != sizeof(split_runtime_key_feedback_semantic_packet_t)) {
        split_runtime_sync_log_packet_size_mismatch("key-feedback semantic", initiator2target_buffer_size, sizeof(split_runtime_key_feedback_semantic_packet_t));
    }

    key_origin_bitmap_copy(split_runtime_sync_remote.key_feedback_flash_visibility_bitmap, packet->key_feedback_flash_visibility_bitmap);
    memcpy(split_runtime_sync_remote.key_feedback_semantic_map, packet->key_feedback_semantic_map, KEY_FEEDBACK_SEMANTIC_MAP_SIZE);
}

static void split_runtime_sync_slave_key_feedback_branch_rpc(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    const split_runtime_key_feedback_branch_packet_t *packet = initiator2target_buffer;

    (void)target2initiator_buffer_size;
    (void)target2initiator_buffer;

    if (initiator2target_buffer_size < sizeof(split_runtime_key_feedback_branch_packet_t)) {
        split_runtime_sync_log_packet_size_mismatch("key-feedback branch", initiator2target_buffer_size, sizeof(split_runtime_key_feedback_branch_packet_t));
        return;
    }

    if (initiator2target_buffer_size != sizeof(split_runtime_key_feedback_branch_packet_t)) {
        split_runtime_sync_log_packet_size_mismatch("key-feedback branch", initiator2target_buffer_size, sizeof(split_runtime_key_feedback_branch_packet_t));
    }

    memcpy(split_runtime_sync_remote.key_feedback_broad_owner_map, packet->key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE);
    memcpy(split_runtime_sync_remote.key_feedback_tap_branch_map, packet->key_feedback_tap_branch_map, KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE);
}

void split_runtime_sync_init(void) {
    transaction_register_rpc(PUT_SPLIT_RUNTIME_BASE_SYNC, split_runtime_sync_slave_base_rpc);
    transaction_register_rpc(PUT_SPLIT_COMBO_FEEDBACK_SYNC, split_runtime_sync_slave_combo_rpc);
    transaction_register_rpc(PUT_SPLIT_KEY_FEEDBACK_SEMANTIC_SYNC, split_runtime_sync_slave_key_feedback_semantic_rpc);
    transaction_register_rpc(PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC, split_runtime_sync_slave_key_feedback_branch_rpc);
    split_runtime_sync_remote                     = (split_runtime_sync_remote_t)SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT;
    split_runtime_base_last_sent                  = (split_runtime_base_sync_packet_t){0};
    split_runtime_combo_last_sent                 = (split_runtime_combo_feedback_packet_t){0};
    split_runtime_key_feedback_semantic_last_sent = (split_runtime_key_feedback_semantic_packet_t){0};
    split_runtime_key_feedback_branch_last_sent   = (split_runtime_key_feedback_branch_packet_t){0};
    split_runtime_base_sent_once                  = false;
    split_runtime_combo_sent_once                 = false;
    split_runtime_key_feedback_semantic_sent_once = false;
    split_runtime_key_feedback_branch_sent_once   = false;
    split_runtime_sync_initialized                = true;
    split_runtime_sync_dirty_reset();
    split_runtime_base_last_send                  = timer_read32();
    split_runtime_combo_last_send                 = split_runtime_base_last_send;
    split_runtime_key_feedback_semantic_last_send = split_runtime_base_last_send;
    split_runtime_key_feedback_branch_last_send   = split_runtime_base_last_send;
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

    split_runtime_sync_elapsed_internal(raw_elapsed, false);
}

static void split_runtime_sync_elapsed_internal(uint16_t raw_elapsed, bool force) {
    if (!split_runtime_sync_initialized || !is_keyboard_master()) {
        return;
    }

    split_runtime_base_sync_packet_t base_packet = split_runtime_sync_build_base_packet(raw_elapsed);
    (void)split_runtime_sync_broadcast_base(&base_packet, force);

    split_runtime_combo_feedback_packet_t combo_packet = split_runtime_sync_build_combo_packet();
    if (split_runtime_sync_broadcast_combo(&combo_packet, force)) {
        split_runtime_sync_clear_combo_dirty();
    }

    if (split_runtime_sync_should_build_packet(force, split_runtime_sync_key_feedback_semantic_is_dirty(), split_runtime_key_feedback_semantic_sent_once, split_runtime_key_feedback_semantic_last_send, split_runtime_key_feedback_semantic_packet_is_active(&split_runtime_key_feedback_semantic_last_sent))) {
        split_runtime_key_feedback_semantic_packet_t key_feedback_semantic_packet = split_runtime_sync_build_key_feedback_semantic_packet();
        if (split_runtime_sync_broadcast_key_feedback_semantic(&key_feedback_semantic_packet, force)) {
            split_runtime_sync_clear_key_feedback_semantic_dirty();
        }
    }

    if (split_runtime_sync_should_build_packet(force, split_runtime_sync_key_feedback_branch_is_dirty(), split_runtime_key_feedback_branch_sent_once, split_runtime_key_feedback_branch_last_send, split_runtime_key_feedback_branch_packet_is_active(&split_runtime_key_feedback_branch_last_sent))) {
        split_runtime_key_feedback_branch_packet_t key_feedback_branch_packet = split_runtime_sync_build_key_feedback_branch_packet();
        if (split_runtime_sync_broadcast_key_feedback_branch(&key_feedback_branch_packet, force)) {
            split_runtime_sync_clear_key_feedback_branch_dirty();
        }
    }
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
