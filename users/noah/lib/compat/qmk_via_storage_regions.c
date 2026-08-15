// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Canonical Storage Regions
// ────────────────────────────────────────────────────────────────────────────

#include "qmk_via_storage_regions.h"

#ifdef VIA_ENABLE

#    include <stddef.h>
#    include <string.h>

#    include "dynamic_keymap.h"
#    include "via.h"
#    include "qmk_via_storage_contract.h"

#    define NOAH_QMK_VIA_CONFIG_REGION_SIZE (1u + VIA_EEPROM_LAYOUT_OPTIONS_SIZE)
#    define NOAH_QMK_VIA_DIGEST_OFFSET UINT32_C(2166136261)
#    define NOAH_QMK_VIA_DIGEST_PRIME UINT32_C(16777619)

_Static_assert(VIA_EEPROM_LAYOUT_OPTIONS_SIZE >= 1u && VIA_EEPROM_LAYOUT_OPTIONS_SIZE <= 4u, "VIA layout options must fit the canonical config region");
_Static_assert(NOAH_QMK_VIA_CONFIG_REGION_SIZE <= NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX, "VIA config region must fit one snapshot fragment");

static bool noah_qmk_via_storage_range_valid(uint16_t capacity, uint16_t offset, uint8_t length) {
    return offset <= capacity && length <= capacity - offset;
}

static uint32_t noah_qmk_via_storage_hash_byte(uint32_t hash, uint8_t byte) {
    return (hash ^ byte) * NOAH_QMK_VIA_DIGEST_PRIME;
}

static void noah_qmk_via_storage_read_config(uint8_t out[NOAH_QMK_VIA_CONFIG_REGION_SIZE]) {
    uint32_t layout_options = via_get_layout_options();

    out[0] = via_eeprom_is_valid() ? 1u : 0u;
    for (uint8_t index = 0u; index < VIA_EEPROM_LAYOUT_OPTIONS_SIZE; index++) {
        out[1u + index] = (uint8_t)(layout_options >> (index * 8u));
    }
}

static bool noah_qmk_via_storage_write_config(uint16_t offset, const uint8_t *data, uint8_t length) {
    uint32_t layout_options = 0u;

    if (offset != 0u || length != NOAH_QMK_VIA_CONFIG_REGION_SIZE || !data || data[0] > 1u) {
        return false;
    }
    for (uint8_t index = 0u; index < VIA_EEPROM_LAYOUT_OPTIONS_SIZE; index++) {
        layout_options |= (uint32_t)data[1u + index] << (index * 8u);
    }

    via_eeprom_set_valid(false);
    via_set_layout_options(layout_options);
    via_eeprom_set_valid(data[0] != 0u);
    return true;
}

#    ifdef ENCODER_MAP_ENABLE
static uint16_t noah_qmk_via_storage_encoder_size(void) {
    return (uint16_t)((uint32_t)DYNAMIC_KEYMAP_LAYER_COUNT * NUM_ENCODERS * 2u * 2u);
}

static uint16_t noah_qmk_via_storage_encoder_keycode(uint16_t byte_offset) {
    uint16_t slot            = byte_offset / 2u;
    uint16_t slots_per_layer = NUM_ENCODERS * 2u;
    uint8_t  layer           = (uint8_t)(slot / slots_per_layer);
    uint16_t layer_slot      = slot % slots_per_layer;
    uint8_t  encoder         = (uint8_t)(layer_slot / 2u);
    bool     clockwise       = (layer_slot % 2u) == 0u;

    return dynamic_keymap_get_encoder(layer, encoder, clockwise);
}

static void noah_qmk_via_storage_set_encoder_keycode(uint16_t byte_offset, uint16_t keycode) {
    uint16_t slot            = byte_offset / 2u;
    uint16_t slots_per_layer = NUM_ENCODERS * 2u;
    uint8_t  layer           = (uint8_t)(slot / slots_per_layer);
    uint16_t layer_slot      = slot % slots_per_layer;
    uint8_t  encoder         = (uint8_t)(layer_slot / 2u);
    bool     clockwise       = (layer_slot % 2u) == 0u;

    dynamic_keymap_set_encoder(layer, encoder, clockwise, keycode);
}

static bool noah_qmk_via_storage_encoder_read(uint16_t offset, uint8_t *data, uint8_t length) {
    if (!noah_qmk_via_storage_range_valid(noah_qmk_via_storage_encoder_size(), offset, length) || (length != 0u && !data)) {
        return false;
    }

    for (uint8_t index = 0u; index < length; index++) {
        uint16_t byte_offset = offset + index;
        uint16_t keycode     = noah_qmk_via_storage_encoder_keycode(byte_offset);
        data[index]          = (byte_offset & 1u) == 0u ? (uint8_t)(keycode >> 8u) : (uint8_t)keycode;
    }
    return true;
}

static bool noah_qmk_via_storage_encoder_write(uint16_t offset, const uint8_t *data, uint8_t length) {
    if (!noah_qmk_via_storage_range_valid(noah_qmk_via_storage_encoder_size(), offset, length) || (length != 0u && !data)) {
        return false;
    }

    for (uint8_t index = 0u; index < length; index++) {
        uint16_t byte_offset = offset + index;
        uint16_t keycode     = noah_qmk_via_storage_encoder_keycode(byte_offset);
        keycode              = (byte_offset & 1u) == 0u ? (uint16_t)((keycode & 0x00FFu) | ((uint16_t)data[index] << 8u)) : (uint16_t)((keycode & 0xFF00u) | data[index]);
        noah_qmk_via_storage_set_encoder_keycode(byte_offset, keycode);
    }
    return true;
}
#    endif

uint16_t noah_qmk_via_storage_region_size(noah_qmk_via_sync_region_t region) {
    switch (region) {
        case NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG:
            return NOAH_QMK_VIA_CONFIG_REGION_SIZE;
        case NOAH_QMK_VIA_SYNC_REGION_KEYMAP:
            return noah_qmk_via_keymap_buffer_capacity();
        case NOAH_QMK_VIA_SYNC_REGION_ENCODER:
#    ifdef ENCODER_MAP_ENABLE
            return noah_qmk_via_storage_encoder_size();
#    else
            return 0u;
#    endif
        case NOAH_QMK_VIA_SYNC_REGION_MACRO:
            return noah_qmk_via_macro_seed_capacity();
        default:
            return 0u;
    }
}

bool noah_qmk_via_storage_region_read(noah_qmk_via_sync_region_t region, uint16_t offset, uint8_t *data, uint8_t length) {
    uint16_t capacity = noah_qmk_via_storage_region_size(region);

    if (!noah_qmk_via_storage_range_valid(capacity, offset, length) || (length != 0u && !data)) {
        return false;
    }

    switch (region) {
        case NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG: {
            uint8_t config[NOAH_QMK_VIA_CONFIG_REGION_SIZE];
            noah_qmk_via_storage_read_config(config);
            memcpy(data, &config[offset], length);
            return true;
        }
        case NOAH_QMK_VIA_SYNC_REGION_KEYMAP:
            dynamic_keymap_get_buffer(offset, length, data);
            return true;
        case NOAH_QMK_VIA_SYNC_REGION_ENCODER:
#    ifdef ENCODER_MAP_ENABLE
            return noah_qmk_via_storage_encoder_read(offset, data, length);
#    else
            return length == 0u;
#    endif
        case NOAH_QMK_VIA_SYNC_REGION_MACRO:
            dynamic_keymap_macro_get_buffer(offset, length, data);
            return true;
        default:
            return false;
    }
}

bool noah_qmk_via_storage_region_write(noah_qmk_via_sync_region_t region, uint16_t offset, const uint8_t *data, uint8_t length) {
    uint16_t capacity = noah_qmk_via_storage_region_size(region);

    if (!noah_qmk_via_storage_range_valid(capacity, offset, length) || (length != 0u && !data)) {
        return false;
    }

    switch (region) {
        case NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG:
            return noah_qmk_via_storage_write_config(offset, data, length);
        case NOAH_QMK_VIA_SYNC_REGION_KEYMAP:
            dynamic_keymap_set_buffer(offset, length, (uint8_t *)data);
            return true;
        case NOAH_QMK_VIA_SYNC_REGION_ENCODER:
#    ifdef ENCODER_MAP_ENABLE
            return noah_qmk_via_storage_encoder_write(offset, data, length);
#    else
            return length == 0u;
#    endif
        case NOAH_QMK_VIA_SYNC_REGION_MACRO:
            noah_qmk_via_macro_set_buffer(offset, length, (uint8_t *)data);
            return true;
        default:
            return false;
    }
}

void noah_qmk_via_storage_digest_init(noah_qmk_via_storage_digest_cursor_t *cursor) {
    if (!cursor) {
        return;
    }
    *cursor = (noah_qmk_via_storage_digest_cursor_t){
        .hash   = NOAH_QMK_VIA_DIGEST_OFFSET,
        .region = NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG,
    };
}

bool noah_qmk_via_storage_digest_step(noah_qmk_via_storage_digest_cursor_t *cursor, uint8_t byte_budget, uint32_t *out_digest) {
    uint8_t bytes[NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX];

    if (!cursor || !out_digest || byte_budget == 0u || byte_budget > sizeof(bytes) || cursor->complete) {
        return false;
    }

    while (cursor->region <= NOAH_QMK_VIA_SYNC_REGION_MACRO) {
        uint16_t capacity = noah_qmk_via_storage_region_size(cursor->region);

        if (cursor->offset == 0u) {
            cursor->hash = noah_qmk_via_storage_hash_byte(cursor->hash, (uint8_t)cursor->region);
            cursor->hash = noah_qmk_via_storage_hash_byte(cursor->hash, (uint8_t)capacity);
            cursor->hash = noah_qmk_via_storage_hash_byte(cursor->hash, (uint8_t)(capacity >> 8u));
        }
        if (cursor->offset < capacity) {
            uint16_t remaining = capacity - cursor->offset;
            uint8_t  length    = remaining < byte_budget ? (uint8_t)remaining : byte_budget;

            if (!noah_qmk_via_storage_region_read(cursor->region, cursor->offset, bytes, length)) {
                return false;
            }
            for (uint8_t index = 0u; index < length; index++) {
                cursor->hash = noah_qmk_via_storage_hash_byte(cursor->hash, bytes[index]);
            }
            cursor->offset += length;
            *out_digest = cursor->hash;
            return true;
        }

        cursor->region = (noah_qmk_via_sync_region_t)(cursor->region + 1u);
        cursor->offset = 0u;
    }

    cursor->complete = true;
    *out_digest      = cursor->hash;
    return true;
}

#endif
