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
#    include "../profile/storage/profile_checksum.h"
#    include "../profile/storage/profile_store_runtime.h"
#    include "../profile/schema/profile_pd_v1.h"

// Cold readback workspace, never used by key events or RGB rendering.
void noah_qmk_portable_storage_init(void) {
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
static uint32_t setting_default(uint8_t id) {
#ifdef NOAH_PD_PROFILE_ENABLE
    if (id >= NOAH_SETTING_DRAGSCROLL_DPI && id <= NOAH_SETTING_ARROW_DPI) return 0;
#endif
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
// The keymap's names fill a settings domain that no profile has stored.
_Static_assert((int)LAYER_COUNT == (int)NOAH_SETTINGS_LAYERS, "Settings name every one of the eight layers");
_Static_assert(NOAH_LAYER_NAME_SIZE == NOAH_SETTINGS_NAME_BYTES, "A layer name fills one settings name field");
_Static_assert(VIA_MACRO_SLOT_COUNT == NOAH_SETTINGS_MACRO_NAMES, "Settings name every VIA macro");
_Static_assert(NOAH_MACRO_NAME_SIZE == NOAH_SETTINGS_MACRO_NAME_ASCII_MAX + 1u, "A macro name is at most 20 characters");
_Static_assert(CUSTOM_KEY_SLOT_COUNT == NOAH_SETTINGS_CUSTOM_KEY_NAMES, "Settings name every custom key");
enum { LAYER_NAMES_OFFSET = 8u + NOAH_SETTINGS_COUNT * 4u };
// Name records after the fixed part: the macros' (v3), then the custom keys' (v5).
#if NOAH_PROFILE_SETTINGS_VERSION >= 5u
enum { NAME_RECORDS = NOAH_SETTINGS_MACRO_NAMES + NOAH_SETTINGS_CUSTOM_KEY_NAMES, CUSTOM_KEY_NAME_HEADER = NOAH_SETTINGS_CUSTOM_KEY_NAMES };
#else
enum { NAME_RECORDS = NOAH_SETTINGS_MACRO_NAMES, CUSTOM_KEY_NAME_HEADER = 0 };
#endif
static uint8_t name_length(const char *name, uint8_t max) {
    uint8_t length = 0;
    while (length < max && name[length])
        length++;
    return length;
}
static const char *record_name(uint8_t record) {
#if NOAH_PROFILE_SETTINGS_VERSION >= 5u
    if (record >= NOAH_SETTINGS_MACRO_NAMES) return custom_key_names[record - NOAH_SETTINGS_MACRO_NAMES];
#endif
    return via_macro_names[record];
}
static uint8_t record_name_length(uint8_t record) {
    return name_length(record_name(record), NOAH_SETTINGS_MACRO_NAME_ASCII_MAX);
}
// Settings readback streams from the effective settings rather than keeping
// a second copy. Page 0 describes the bytes as they are now; a change between
// pages shows up as the digest mismatch the reader already checks for.
static uint16_t settings_length(void) {
    uint16_t length = noah_effective_settings_length();
    if (length) return length;
#if NOAH_PROFILE_SETTINGS_VERSION >= 3u
    length = NOAH_SETTINGS_FIXED_SIZE + NAME_RECORDS;
    for (uint8_t record = 0; record < NAME_RECORDS; record++)
        length += record_name_length(record);
    return length;
#else
    return NOAH_SETTINGS_FIXED_SIZE + NOAH_SETTINGS_MACROS * 2u;
#endif
}
static uint8_t settings_byte(uint16_t offset) {
    if (offset >= 8u && offset < LAYER_NAMES_OFFSET) {
        uint8_t  id    = (offset - 8u) / 4u;
        uint32_t value = setting_default(id);
        if (!external_setting(id)) value = noah_setting(id, value);
        return value >> (8u * ((offset - 8u) % 4u));
    }
    if (noah_effective_settings_length()) return noah_effective_settings_byte(offset);
    // No profile settings are live: the current version with the keymap's
    // layer, macro (v3) and custom-key (v5) names, or its layer names and
    // every user macro empty (v1).
#if NOAH_PROFILE_SETTINGS_VERSION >= 3u
    const uint8_t header[8] = {NOAH_SETTINGS_VERSION, 8, NOAH_SETTINGS_COUNT, NOAH_SETTINGS_MACRO_NAMES, CUSTOM_KEY_NAME_HEADER, 0, 0, 0};
#else
    const uint8_t header[8] = {NOAH_SETTINGS_VERSION, 8, NOAH_SETTINGS_COUNT, NOAH_SETTINGS_MACROS, 0, 0, 0, 0};
#endif
    if (offset < 8u) return header[offset];
    if (offset < NOAH_SETTINGS_FIXED_SIZE) {
        offset -= LAYER_NAMES_OFFSET;
        const char *name = layer_names[offset / NOAH_SETTINGS_NAME_BYTES];
        uint8_t     byte = offset % NOAH_SETTINGS_NAME_BYTES;
        // Zero padded after the name, whatever the array holds there.
        return byte < name_length(name, NOAH_SETTINGS_NAME_BYTES - 1u) ? (uint8_t)name[byte] : 0u;
    }
#if NOAH_PROFILE_SETTINGS_VERSION >= 3u
    offset -= NOAH_SETTINGS_FIXED_SIZE;
    // Reads run forward, so resume from the record the last byte was in; the
    // authored names never change, so the cursor stays valid across reads.
    static uint8_t  cursor_record;
    static uint16_t cursor_start;
    if (offset < cursor_start) cursor_record = 0u, cursor_start = 0u;
    for (uint8_t record = cursor_record; record < NAME_RECORDS; record++) {
        uint8_t length = record_name_length(record);
        if (offset - cursor_start <= length) {
            cursor_record = record;
            return offset == cursor_start ? length : (uint8_t)record_name(record)[offset - cursor_start - 1u];
        }
        cursor_start += length + 1u;
    }
    cursor_record = NAME_RECORDS;
#endif
    return 0u;
}
#ifndef NOAH_PD_PROFILE_ENABLE
// The bridge's legacy pointing source is encoded once, at page 0, in the
// retired fixed eight-slot version 1 that schema-1 clients read.
static uint8_t  snapshot[NOAH_PROFILE_PD_V1_LEGACY_SIZE];
static uint16_t snapshot_length;
static bool     capture_legacy_pd(void) {
    const uint8_t header[8] = {NOAH_PROFILE_PD_V1_LEGACY_VERSION, NOAH_PROFILE_PD_V1_LEGACY_SLOT_COUNT, NOAH_PROFILE_PD_V1_RECORD_SIZE, 0, 0, 0, 0, 0};
    memcpy(snapshot, header, 8);
    snapshot_length = NOAH_PROFILE_PD_V1_LEGACY_SIZE;
    for (uint8_t id = 0; id < NOAH_PROFILE_PD_V1_LEGACY_SLOT_COUNT; id++) {
        uint8_t *record = snapshot + 8 + (size_t)id * 96;
        noah_profile_pd_v1_encode_record(&noah_pd_defaults[id], record);

    }
    return noah_profile_pd_v1_validate_legacy(snapshot, snapshot_length, NULL) == NOAH_PROFILE_PD_V1_OK;
}
#endif
// Up to 25 readback bytes of `kind` from `offset`; 0 past the end.
static uint8_t readback_fill(uint8_t kind, uint16_t offset, uint8_t *target) {
#ifndef NOAH_PD_PROFILE_ENABLE
    uint16_t length = kind == 7 ? settings_length() : snapshot_length;
#else
    uint16_t length = settings_length();
    (void)kind;
#endif
    if (offset >= length) return 0;
    uint16_t remaining = (uint16_t)(length - offset);
    uint8_t  count     = remaining < 25u ? (uint8_t)remaining : 25u;
    for (uint8_t i = 0; i < count; i++)
#ifndef NOAH_PD_PROFILE_ENABLE
        target[i] = kind == 7 ? settings_byte(offset + i) : snapshot[offset + i];
#else
        target[i] = settings_byte(offset + i);
#endif
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
    noah_qmk_portable_apply_lighting(noah_setting(NOAH_SETTING_RGB_MODE, setting_default(NOAH_SETTING_RGB_MODE)), noah_setting(NOAH_SETTING_RGB_COLOR, setting_default(NOAH_SETTING_RGB_COLOR)));
    uint8_t layers = noah_setting(NOAH_SETTING_DEFAULT_LAYERS, default_layer_state);
    default_layer_set(layers);
    eeconfig_update_default_layer(layers);
    keymap_config.raw = noah_setting(NOAH_SETTING_KEYMAP_OPTIONS, keymap_config.raw);
    eeconfig_update_keymap(&keymap_config);
}
bool noah_qmk_portable_profile_get(uint8_t *frame, uint8_t length) {
    if (!frame || length != 32 || frame[0] != 8 || frame[1] || (frame[2] != 7 && frame[2] != 8
#ifndef NOAH_PD_PROFILE_ENABLE
        && frame[2] != 9
#endif
        )) return false;
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
#ifndef NOAH_PD_PROFILE_ENABLE
        if (frame[2] == 9 && !capture_legacy_pd()) {
            snapshot_length = 0;
            frame[5]        = 3;
            return true;
        }
#endif
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
        frame[6] = readback_fill(frame[2], (frame[4] - 1) * 25u, p);
        if (!frame[6]) frame[5] = 2;
    }
    return true;
}
#endif

#if defined(NOAH_PORTABLE_PROFILE_ENABLE) && !defined(COMBO_ONLY_FROM_LAYER)
uint8_t combo_ref_from_layer(uint8_t layer) {
    return layer < 8 ? (noah_setting(NOAH_SETTING_COMBO_REFERENCES, 0x76543210u) >> (layer * 4)) & 15 : layer;
}
#endif
