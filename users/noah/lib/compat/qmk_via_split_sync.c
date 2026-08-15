// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Split-Sync Compatibility
// ────────────────────────────────────────────────────────────────────────────

#include "qmk_via_split_sync.h"

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)

#    include <stddef.h>

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

typedef struct {
    uint8_t        command_id;
    uint8_t        layer;
    uint8_t        row_or_encoder;
    uint8_t        column_or_direction;
    uint16_t       keycode_or_offset;
    uint16_t       size;
    const uint8_t *payload;
} noah_qmk_via_split_sync_command_t;

_Static_assert(RPC_M2S_BUFFER_SIZE >= 4u, "VIA split RPC buffer must fit the set-buffer header");

static bool noah_qmk_via_split_sync_decode(const uint8_t *data, uint8_t length, noah_qmk_via_split_sync_command_t *out) {
    size_t transport_length = length;

    if (!(data && out) || transport_length == 0u || transport_length > RPC_M2S_BUFFER_SIZE) {
        return false;
    }

    *out = (noah_qmk_via_split_sync_command_t){.command_id = data[0]};

    switch (data[0]) {
        case id_dynamic_keymap_set_keycode:
            if (transport_length < 6u || data[1] >= DYNAMIC_KEYMAP_LAYER_COUNT || data[2] >= MATRIX_ROWS || data[3] >= MATRIX_COLS) {
                return false;
            }

            out->layer               = data[1];
            out->row_or_encoder      = data[2];
            out->column_or_direction = data[3];
            out->keycode_or_offset   = ((uint16_t)data[4] << 8) | data[5];
            return true;
        case id_dynamic_keymap_set_buffer: {
            size_t   payload_available;
            size_t   payload_size;
            size_t   offset;
            size_t   capacity;

            if (transport_length < 4u) {
                return false;
            }

            payload_available = transport_length - 4u;
            payload_size      = data[3];
            if (payload_size > payload_available || payload_size > (size_t)RPC_M2S_BUFFER_SIZE - 4u) {
                return false;
            }

            offset   = ((uint16_t)data[1] << 8) | data[2];
            capacity = noah_qmk_via_keymap_buffer_capacity();
            if (offset > capacity || payload_size > capacity - offset) {
                return false;
            }

            out->keycode_or_offset = (uint16_t)offset;
            out->size              = (uint16_t)payload_size;
            out->payload           = payload_size != 0u ? &data[4] : NULL;
            return true;
        }
        case id_dynamic_keymap_reset:
            return true;
#    ifdef VIA_EEPROM_ALLOW_RESET
        case id_eeprom_reset:
            return true;
#    endif
#    ifdef ENCODER_MAP_ENABLE
        case id_dynamic_keymap_set_encoder:
            if (transport_length < 6u || data[1] >= DYNAMIC_KEYMAP_LAYER_COUNT || data[2] >= NUM_ENCODERS) {
                return false;
            }

            out->layer               = data[1];
            out->row_or_encoder      = data[2];
            out->column_or_direction = data[3] != 0u;
            out->keycode_or_offset   = ((uint16_t)data[4] << 8) | data[5];
            return true;
#    endif
        default:
            return false;
    }
}

static void noah_qmk_via_split_sync_apply_command(const noah_qmk_via_split_sync_command_t *command) {
    if (!command) {
        return;
    }

    switch (command->command_id) {
        case id_dynamic_keymap_set_keycode:
            dynamic_keymap_set_keycode(command->layer, command->row_or_encoder, command->column_or_direction, command->keycode_or_offset);
            break;
        case id_dynamic_keymap_set_buffer:
            if (command->size != 0u) {
                dynamic_keymap_set_buffer(command->keycode_or_offset, command->size, (uint8_t *)command->payload);
            }
            break;
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
            dynamic_keymap_set_encoder(command->layer, command->row_or_encoder, command->column_or_direction != 0u, command->keycode_or_offset);
            break;
#    endif
        default:
            return;
    }

    noah_qmk_via_split_sync_apply_effects(command->command_id);
}

static void noah_qmk_via_split_sync_slave_rpc(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    noah_qmk_via_split_sync_command_t command;

    (void)target2initiator_buffer_size;
    (void)target2initiator_buffer;

    if (!noah_qmk_via_split_sync_decode((const uint8_t *)initiator2target_buffer, initiator2target_buffer_size, &command)) {
        return;
    }

    noah_qmk_via_split_sync_apply_command(&command);
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
