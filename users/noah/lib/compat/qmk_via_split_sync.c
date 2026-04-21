// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Split-Sync Compatibility
// ────────────────────────────────────────────────────────────────────────────

#include "qmk_via_split_sync.h"

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)

#    include <string.h>

#    include "dynamic_keymap.h"
#    include "via.h"
#    include "../rgb/core/rgb_runtime.h"
#    include "qmk_via_storage_contract.h"
#    include "transactions.h" // QMK

static void noah_qmk_via_split_sync_apply_effects(uint8_t command_id) {
    if (noah_qmk_via_command_effects(command_id) & NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_RGB) {
        noah_rgb_runtime_invalidate_layer_maps();
    }
}

static void noah_qmk_via_split_sync_apply_command(const uint8_t *data, uint8_t length) {
    if (!data || length == 0) {
        return;
    }

    switch (data[0]) {
        case id_dynamic_keymap_set_keycode:
            if (length < 6u) {
                return;
            }

            dynamic_keymap_set_keycode(data[1], data[2], data[3], ((uint16_t)data[4] << 8) | data[5]);
            break;
        case id_dynamic_keymap_set_buffer: {
            uint8_t size = 0;

            if (length < 4u) {
                return;
            }

            size = data[3];
            if (length < (uint8_t)(4u + size)) {
                return;
            }

            dynamic_keymap_set_buffer(((uint16_t)data[1] << 8) | data[2], size, (uint8_t *)&data[4]);
            break;
        }
        case id_dynamic_keymap_reset:
            dynamic_keymap_reset();
            break;
#    ifdef VIA_EEPROM_ALLOW_RESET
        case id_eeprom_reset:
            via_eeprom_set_valid(false);
            eeconfig_init_via();
            break;
#    endif
#    ifdef ENCODER_MAP_ENABLE
        case id_dynamic_keymap_set_encoder:
            if (length < 6u) {
                return;
            }

            dynamic_keymap_set_encoder(data[1], data[2], data[3] != 0, ((uint16_t)data[4] << 8) | data[5]);
            break;
#    endif
        default:
            return;
    }

    noah_qmk_via_split_sync_apply_effects(data[0]);
}

static void noah_qmk_via_split_sync_slave_rpc(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    (void)target2initiator_buffer_size;
    (void)target2initiator_buffer;

    if (!initiator2target_buffer || initiator2target_buffer_size == 0) {
        return;
    }

    noah_qmk_via_split_sync_apply_command((const uint8_t *)initiator2target_buffer, initiator2target_buffer_size);
}

void noah_qmk_via_split_sync_init(void) {
    transaction_register_rpc(PUT_VIA_KEYMAP_SYNC, noah_qmk_via_split_sync_slave_rpc);
}

void noah_qmk_via_split_sync_command(const uint8_t *data, uint8_t length) {
    if (!data || length == 0 || length > RPC_M2S_BUFFER_SIZE || !is_keyboard_master()) {
        return;
    }

    transaction_rpc_send(PUT_VIA_KEYMAP_SYNC, length, data);
}

#endif
