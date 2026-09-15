#include QMK_KEYBOARD_H
#include "qmk_portable_profile.h"
#include "qmk_portable_editor.h"
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
#    include <string.h>
#    include "eeconfig.h"
#    include "via.h"
#    include "qmk_via_sync_metadata.h"
#    include "pointing_device_auto_mouse.h"
#    include "noah_keymap_ids.h"
#    include "qmk_via_split_sync.h"
#    include "../profile/runtime/effective_settings_runtime.h"
#    include "../profile/storage/profile_checksum.h"
#    include "../profile/storage/profile_store_runtime.h"
#    include "../macro/macro_payload.h"

// Cold readback workspace, never used by key events or RGB rendering.
void noah_qmk_portable_storage_init(void) {
    uint32_t word = eeconfig_read_user();
#    ifdef NOAH_LEGACY_SNAPSHOT_BRIDGE
    noah_qmk_via_sync_metadata_t metadata;
    // Keep recognized five-layer data across a build-date change. Dirty
    // storage remains marked for recovery; never erase the recovery source.
    if (noah_qmk_via_sync_metadata_decode(word, &metadata)) via_eeprom_set_valid(true);
#    else
    // Geometry changed from five to eight. Never interpret the old macro
    // prefix as keycodes even when both builds happen on the same date.
    if ((word >> 28) != NOAH_QMK_VIA_SYNC_METADATA_SCHEMA) via_eeprom_set_valid(false);
#    endif
}
static uint8_t  snapshot[NOAH_SETTINGS_MAX_SIZE];
static uint16_t snapshot_length;
static void     u16(uint8_t *p, uint16_t v) {
    p[0] = v;
    p[1] = v >> 8;
}
static void u32(uint8_t *p, uint32_t v) {
    for (uint8_t i = 0; i < 4; i++)
        p[i] = v >> (8 * i);
}
static uint32_t setting_default(uint8_t id) {
    switch (id) {
        case NOAH_SETTING_TAPPING_TERM:
            return TAPPING_TERM;
        case NOAH_SETTING_TAP_HOLD_TERM:
            return CUSTOM_TAP_HOLD_TERM;
        case NOAH_SETTING_LONG_HOLD_TERM:
            return CUSTOM_LONGER_HOLD_TERM;
        case NOAH_SETTING_MULTI_TAP_TERM:
            return CUSTOM_MULTI_TAP_TERM;
        case NOAH_SETTING_AUTO_MOUSE_ENABLED:
            return get_auto_mouse_enable();
        case NOAH_SETTING_AUTO_MOUSE_LAYER:
            return get_auto_mouse_layer();
        case NOAH_SETTING_AUTO_MOUSE_TIMEOUT:
            return get_auto_mouse_timeout();
        case NOAH_SETTING_AUTO_MOUSE_DEBOUNCE:
            return get_auto_mouse_debounce();
        case NOAH_SETTING_AUTO_SNIPING_ENABLED:
            return 1;
        case NOAH_SETTING_AUTO_SNIPING_LAYER:
            return CHARYBDIS_AUTO_SNIPING_LAYER;
        case NOAH_SETTING_DRAGSCROLL_DPI:
            return CHARYBDIS_DRAGSCROLL_DPI;
        case NOAH_SETTING_VOLUME_DPI:
            return PD_MODE_VOLUME_DPI;
        case NOAH_SETTING_BRIGHTNESS_DPI:
            return PD_MODE_BRIGHTNESS_DPI;
        case NOAH_SETTING_ZOOM_DPI:
            return PD_MODE_ZOOM_DPI;
        case NOAH_SETTING_ARROW_DPI:
            return PD_MODE_ARROW_DPI;
        case NOAH_SETTING_FEEDBACK_PERIOD:
            return RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS;
        case NOAH_SETTING_AUTO_MOUSE_DEAD_TIME:
            return AUTOMOUSE_RGB_DEAD_TIME;
        case NOAH_SETTING_RGB_TIMEOUT:
            return 900000;
        case NOAH_SETTING_DEFAULT_DPI:
            return charybdis_get_pointer_default_dpi();
        case NOAH_SETTING_SNIPING_DPI:
            return charybdis_get_pointer_sniping_dpi();
        case NOAH_SETTING_COMBOS_ENABLED:
            return is_combo_enabled();
        case NOAH_SETTING_RGB_MODE:
            return (uint32_t)rgb_matrix_is_enabled() | (uint32_t)rgb_matrix_get_mode() << 8 | (uint32_t)rgb_matrix_get_speed() << 16 | (uint32_t)rgb_matrix_get_flags() << 24;
        case NOAH_SETTING_RGB_COLOR:
            return (uint32_t)rgb_matrix_get_hue() | (uint32_t)rgb_matrix_get_sat() << 8 | (uint32_t)rgb_matrix_get_val() << 16;
        case NOAH_SETTING_DEFAULT_LAYERS:
            return default_layer_state;
        case NOAH_SETTING_KEYMAP_OPTIONS:
            return keymap_config.raw;
        case NOAH_SETTING_AUTO_MOUSE_DELAY:
            return 200;
        case NOAH_SETTING_AUTO_MOUSE_THRESHOLD:
            return 10;
        case NOAH_SETTING_COMBO_REFERENCES:
            return 0x76543210u;
        default:
            return 0;
    }
}
static bool external_setting(uint8_t id) {
    return (id >= NOAH_SETTING_AUTO_MOUSE_ENABLED && id <= NOAH_SETTING_AUTO_MOUSE_DEBOUNCE) || (id >= NOAH_SETTING_DEFAULT_DPI && id <= NOAH_SETTING_KEYMAP_OPTIONS);
}
static bool capture(void) {
    bool live = noah_effective_settings_copy(snapshot, &snapshot_length);
    if (!live) {
        memset(snapshot, 0, sizeof(snapshot));
        const uint8_t header[8] = {1, 8, 28, 16, 0, 0, 0, 0};
        memcpy(snapshot, header, 8);
        snapshot_length = NOAH_SETTINGS_FIXED_SIZE;
        for (uint8_t slot = 0; slot < 16; slot++) {
            macro_payload_ir_t ir;
            const char        *text = hardcoded_macro_payloads[slot];
            if (!macro_payload_compile(text ? text : "", &ir) || snapshot_length + 2u + ir.length > sizeof(snapshot)) return false;
            u16(snapshot + snapshot_length, ir.length);
            snapshot_length += 2;
            memcpy(snapshot + snapshot_length, ir.bytes, ir.length);
            snapshot_length += ir.length;
        }
    }
    for (uint8_t id = 0; id < NOAH_SETTINGS_COUNT; id++) {
        uint32_t value = setting_default(id);
        if (!external_setting(id)) value = noah_setting(id, value);
        u32(&snapshot[8 + id * 4], value);
    }
    return true;
}
void noah_qmk_portable_apply(void) {
    set_auto_mouse_enable(noah_setting(NOAH_SETTING_AUTO_MOUSE_ENABLED, get_auto_mouse_enable()));
    set_auto_mouse_layer(noah_setting(NOAH_SETTING_AUTO_MOUSE_LAYER, get_auto_mouse_layer()));
    set_auto_mouse_timeout(noah_setting(NOAH_SETTING_AUTO_MOUSE_TIMEOUT, get_auto_mouse_timeout()));
    set_auto_mouse_debounce(noah_setting(NOAH_SETTING_AUTO_MOUSE_DEBOUNCE, get_auto_mouse_debounce()));
    if (noah_setting(NOAH_SETTING_COMBOS_ENABLED, is_combo_enabled()))
        combo_enable();
    else
        combo_disable();
    // These values already have durable QMK owners. At boot retain their
    // newer EEPROM state (for example a DPI key pressed after an import).
    if (noah_effective_settings_is_booting()) return;
    uint16_t dpi = noah_setting(NOAH_SETTING_DEFAULT_DPI, charybdis_get_pointer_default_dpi());
    for (uint8_t i = 0; i < 16 && charybdis_get_pointer_default_dpi() != dpi; i++)
        charybdis_cycle_pointer_default_dpi(true);
    dpi = noah_setting(NOAH_SETTING_SNIPING_DPI, charybdis_get_pointer_sniping_dpi());
    for (uint8_t i = 0; i < 4 && charybdis_get_pointer_sniping_dpi() != dpi; i++)
        charybdis_cycle_pointer_sniping_dpi(true);
    noah_qmk_portable_apply_lighting(noah_setting(NOAH_SETTING_RGB_MODE, setting_default(NOAH_SETTING_RGB_MODE)), noah_setting(NOAH_SETTING_RGB_COLOR, setting_default(NOAH_SETTING_RGB_COLOR)));
    uint8_t layers = noah_setting(NOAH_SETTING_DEFAULT_LAYERS, default_layer_state);
    default_layer_set(layers);
    eeconfig_update_default_layer(layers);
    keymap_config.raw = noah_setting(NOAH_SETTING_KEYMAP_OPTIONS, keymap_config.raw);
    eeconfig_update_keymap(&keymap_config);
}
bool noah_qmk_portable_profile_get(uint8_t *frame, uint8_t length) {
    if (!frame || length != 32 || frame[0] != 8 || frame[1] || (frame[2] != 7 && frame[2] != 8)) return false;
    bool malformed = !frame[3];
    for (uint8_t i = 5; i < 32; i++)
        malformed |= frame[i] != 0;
    memset(frame + 5, 0, 27);
    if (malformed) {
        frame[5] = 1;
        return true;
    }
    uint8_t *p = frame + 7;
    if (frame[2] == 8) {
        if (frame[4]) {
            frame[6] = noah_qmk_portable_editor_page(frame[4], p);
            if (!frame[6]) frame[5] = 2;
            return true;
        }
        noah_qmk_via_split_sync_debug_snapshot_t s = noah_qmk_via_split_sync_debug_snapshot();
        p[0]                                       = 1;
        p[1]                                       = s.local_dirty | s.recovery_required << 1 | s.digest_valid << 2 | s.replication_pending << 3 | s.receiver_active << 4;
        u32(p + 2, s.local_generation);
        u32(p + 6, s.local_digest);
        u32(p + 10, s.peer_generation);
        u32(p + 14, s.peer_digest);
        u32(p + 18, s.last_peer_ack_generation);
        p[22] = s.last_error;
        u16(p + 23, s.conflict_count);
        frame[6] = 25;
    } else if (!frame[4]) {
        if (!capture()) {
            snapshot_length = 0;
            frame[5]        = 3;
            return true;
        }
        p[0] = 1;
        p[1] = 25;
        u16(p + 2, snapshot_length);
        u32(p + 4, noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, snapshot, snapshot_length)));
        u32(p + 8, noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, snapshot, snapshot_length));
        frame[6] = 12;
    } else {
        uint16_t offset = (frame[4] - 1) * 25u;
        if (!snapshot_length || offset >= snapshot_length) {
            frame[5] = 2;
            return true;
        }
        uint8_t count = snapshot_length - offset < 25 ? snapshot_length - offset : 25;
        memcpy(p, snapshot + offset, count);
        frame[6] = count;
    }
    return true;
}
#endif

#if defined(NOAH_PORTABLE_PROFILE_ENABLE) && !defined(COMBO_ONLY_FROM_LAYER)
uint8_t combo_ref_from_layer(uint8_t layer) {
    return layer < 8 ? (noah_setting(NOAH_SETTING_COMBO_REFERENCES, 0x76543210u) >> (layer * 4)) & 15 : layer;
}
#endif
