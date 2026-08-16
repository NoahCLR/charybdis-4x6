// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Split Write-Through Mirror
// ────────────────────────────────────────────────────────────────────────────

#include "qmk_via_split_mirror.h"

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)

#    include "dynamic_keymap.h"
#    include "via.h"
#    include "../macro/via_macro_provider.h"
#    include "../rgb/core/rgb_runtime.h"
#    include "qmk_via_split_sync.h"
#    include "qmk_via_storage_contract.h"
#    include "transactions.h" // QMK

static void noah_qmk_via_split_mirror_apply_effects(uint8_t command_id) {
    uint8_t effects = noah_qmk_via_command_effects(command_id);

    if (effects & NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_RGB) {
        noah_rgb_runtime_invalidate_layer_maps();
    }
    if (effects & NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_MACROS) {
        via_macro_provider_invalidate_all();
    }
}

static void noah_qmk_via_split_mirror_apply_command(const uint8_t *data, uint8_t length) {
    if (!data || length == 0u) {
        return;
    }

    switch (data[0]) {
        case id_dynamic_keymap_set_keycode:
            if (length < 6u) {
                return;
            }

            dynamic_keymap_set_keycode(data[1], data[2], data[3], (uint16_t)(((uint16_t)data[4] << 8) | data[5]));
            break;
        case id_dynamic_keymap_set_buffer: {
            uint8_t size;

            if (length < 4u) {
                return;
            }

            size = data[3];
            if (length < (uint8_t)(4u + size)) {
                return;
            }

            dynamic_keymap_set_buffer((uint16_t)(((uint16_t)data[1] << 8) | data[2]), size, (uint8_t *)&data[4]);
            break;
        }
        case id_dynamic_keymap_macro_set_buffer: {
            uint8_t size;

            if (length < 4u) {
                return;
            }

            size = data[3];
            if (length < (uint8_t)(4u + size)) {
                return;
            }

            noah_qmk_via_macro_set_buffer((uint16_t)(((uint16_t)data[1] << 8) | data[2]), size, (uint8_t *)&data[4]);
            break;
        }
        case id_dynamic_keymap_reset:
            dynamic_keymap_reset();
            break;
        case id_dynamic_keymap_macro_reset:
            dynamic_keymap_macro_reset();
            break;
#    ifdef ENCODER_MAP_ENABLE
        case id_dynamic_keymap_set_encoder:
            if (length < 6u) {
                return;
            }

            dynamic_keymap_set_encoder(data[1], data[2], data[3] != 0u, (uint16_t)(((uint16_t)data[4] << 8) | data[5]));
            break;
#    endif
        default:
            return;
    }

    noah_qmk_via_split_mirror_apply_effects(data[0]);
    // The receiver's storage just changed underneath the reconciliation layer,
    // so let it recompute its digest rather than advertise a stale one.
    noah_qmk_via_split_sync_note_local_storage_changed();
}

static void noah_qmk_via_split_mirror_rpc(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    (void)target2initiator_buffer_size;
    (void)target2initiator_buffer;

    if (!initiator2target_buffer || initiator2target_buffer_size == 0u) {
        return;
    }

    noah_qmk_via_split_mirror_apply_command((const uint8_t *)initiator2target_buffer, initiator2target_buffer_size);
}

void noah_qmk_via_split_mirror_init(void) {
    transaction_register_rpc(PUT_VIA_KEYMAP_MIRROR, noah_qmk_via_split_mirror_rpc);
}

void noah_qmk_via_split_mirror_command(const uint8_t *data, uint8_t length) {
    if (!data || length == 0u || length > RPC_M2S_BUFFER_SIZE || !is_keyboard_master()) {
        return;
    }

    // Best effort by design. A dropped mirror leaves the halves briefly out of
    // step and the reconciliation layer repairs it; blocking or retrying here
    // would put storage writes on the scan path.
    (void)transaction_rpc_send(PUT_VIA_KEYMAP_MIRROR, length, data);
}

#endif // defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)
