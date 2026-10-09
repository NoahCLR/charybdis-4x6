#include QMK_KEYBOARD_H
#include "qmk_portable_profile.h"
#include "qmk_portable_editor.h"
#include "qmk_live_tapping_config.h"
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
#    include <string.h>
#    include "eeconfig.h"
#    include "via.h"
#    include "qmk_via_sync_metadata.h"
#    include "pointing_device_auto_mouse.h"
#    include "noah_keymap_ids.h"
#    include "qmk_via_split_sync.h"
#    include "../profile/runtime/effective_settings_runtime.h"
#    include "../profile/schema/profile_settings_defaults.h"
#    include "../profile/storage/profile_checksum.h"
#    include "../profile/storage/profile_store_runtime.h"

// Cold readback workspace, never used by key events or RGB rendering.
void noah_qmk_portable_storage_init(void) {
    noah_effective_settings_set_apply(noah_qmk_portable_apply);
    uint32_t word = eeconfig_read_user();
    // An older bank's layout uses another keycode numbering or geometry. Never
    // interpret it, even when both builds happen on the same date.
    if ((word >> 28) != NOAH_QMK_VIA_SYNC_METADATA_SCHEMA) via_eeprom_set_valid(false);
}
// The dual-role setting is QMK's tapping term. Quick tap follows it, as it
// does by default, so a second press within the term still auto-repeats.
uint16_t get_tapping_term(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return noah_setting(NOAH_SETTING_TAPPING_TERM, TAPPING_TERM);
}
uint16_t get_quick_tap_term(uint16_t keycode, keyrecord_t *record) {
    return get_tapping_term(keycode, record);
}
static void u16(uint8_t *p, uint16_t v) {
    p[0] = v;
    p[1] = v >> 8;
}
static void u32(uint8_t *p, uint32_t v) {
    for (uint8_t i = 0; i < 4; i++)
        p[i] = v >> (8 * i);
}
static uint32_t current_setting(uint8_t id) {
    switch (id) {
        case NOAH_SETTING_AUTO_MOUSE_ENABLED:
            return get_auto_mouse_enable();
        case NOAH_SETTING_AUTO_MOUSE_LAYER:
            return get_auto_mouse_layer();
        case NOAH_SETTING_AUTO_MOUSE_TIMEOUT:
            return get_auto_mouse_timeout();
        case NOAH_SETTING_AUTO_MOUSE_DEBOUNCE:
            return get_auto_mouse_debounce();
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
        default:
            return noah_profile_settings_default(id);
    }
}
static bool external_setting(uint8_t id) {
    return (id >= NOAH_SETTING_AUTO_MOUSE_ENABLED && id <= NOAH_SETTING_AUTO_MOUSE_DEBOUNCE) || (id >= NOAH_SETTING_DEFAULT_DPI && id <= NOAH_SETTING_KEYMAP_OPTIONS);
}

static uint16_t settings_length(void) {
    uint16_t length = noah_effective_settings_length();
    return length ? length : noah_profile_settings_defaults_length();
}
static uint8_t settings_byte(uint16_t offset) {
    if (offset >= NOAH_SETTINGS_SCALARS_OFFSET && offset < NOAH_SETTINGS_LAYER_RECORDS_OFFSET) {
        uint8_t  id    = (offset - NOAH_SETTINGS_SCALARS_OFFSET) / 4u;
        uint32_t value = current_setting(id);
        if (!external_setting(id)) value = noah_setting(id, value);
        return value >> (8u * ((offset - NOAH_SETTINGS_SCALARS_OFFSET) % 4u));
    }
    return noah_effective_settings_length() ? noah_effective_settings_byte(offset) : noah_profile_settings_defaults_byte(offset);
}
// Up to 25 readback bytes of `kind` from `offset`; 0 past the end.
static uint8_t readback_fill(uint8_t kind, uint16_t offset, uint8_t *target) {
    uint16_t length = settings_length();
    (void)kind;
    if (offset >= length) return 0;
    uint16_t remaining = (uint16_t)(length - offset);
    uint8_t  count     = remaining < 25u ? (uint8_t)remaining : 25u;
    for (uint8_t i = 0; i < count; i++)
        target[i] = settings_byte(offset + i);
    return count;
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
    noah_qmk_portable_apply_lighting(noah_setting(NOAH_SETTING_RGB_MODE, current_setting(NOAH_SETTING_RGB_MODE)), noah_setting(NOAH_SETTING_RGB_COLOR, current_setting(NOAH_SETTING_RGB_COLOR)));
    layer_state_t layers = noah_setting(NOAH_SETTING_DEFAULT_LAYERS, default_layer_state);
    default_layer_set(layers);
    eeconfig_update_default_layer(layers);
    keymap_config.raw = noah_setting(NOAH_SETTING_KEYMAP_OPTIONS, keymap_config.raw);
    eeconfig_update_keymap(&keymap_config);
}
bool noah_qmk_portable_profile_get(uint8_t *frame, uint8_t length) {
    if (!frame || length != 32 || frame[0] != 8 || frame[1] || (frame[2] != 7 && frame[2] != 8)) return false;
    // Settings readback (GET 7) takes a wide page: byte 5 is its high byte.
    uint16_t page      = frame[2] == 7 ? (uint16_t)(frame[4] | frame[5] << 8) : frame[4];
    bool     malformed = !frame[3];
    for (uint8_t i = frame[2] == 7 ? 6 : 5; i < 32; i++)
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
    } else if (!page) {
        uint16_t length = 0;
        uint32_t crc = NOAH_PROFILE_CRC32_INITIAL, fnv = NOAH_PROFILE_FNV1A_INITIAL;
        uint8_t  chunk[25];
        for (uint8_t count; (count = readback_fill(frame[2], length, chunk)); length += count) {
            crc = noah_profile_crc32_update(crc, chunk, count);
            fnv = noah_profile_fnv1a_update(fnv, chunk, count);
        }
        p[0] = 1;
        p[1] = 25;
        u16(p + 2, length);
        u32(p + 4, noah_profile_crc32_finish(crc));
        u32(p + 8, fnv);
        frame[6] = 12;
    } else {
        uint32_t offset = (uint32_t)(page - 1u) * 25u;
        frame[6]        = offset <= UINT16_MAX ? readback_fill(frame[2], (uint16_t)offset, p) : 0u;
        if (!frame[6]) frame[5] = 2;
    }
    return true;
}
#endif

#if defined(NOAH_PORTABLE_PROFILE_ENABLE) && !defined(COMBO_ONLY_FROM_LAYER)
// Settings v6 keeps each layer's combo reference layer in its layer record.
uint8_t combo_ref_from_layer(uint8_t layer) {
    return layer < NOAH_SETTINGS_LAYERS ? noah_setting_layer_record(layer, NOAH_SETTINGS_LAYER_REFERENCE) : layer;
}
#endif
