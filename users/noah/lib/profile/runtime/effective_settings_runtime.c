#include "effective_settings_runtime.h"
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
#    include "../schema/profile_settings_defaults.h"
static uint8_t                          settings[NOAH_SETTINGS_MAX_SIZE];
static uint16_t                         settings_length;
static bool                             booting;
static uint32_t                         publication_count;
static noah_effective_settings_apply_fn apply_settings;
void                                    noah_effective_settings_set_apply(noah_effective_settings_apply_fn apply) {
    apply_settings = apply;
}
void noah_effective_settings_boot(bool value) {
    booting = value;
}
bool noah_effective_settings_is_booting(void) {
    return booting;
}
uint32_t noah_setting(uint8_t id, uint32_t fallback) {
    if (!settings_length || id >= NOAH_SETTINGS_COUNT) return fallback;
    const uint8_t *p = &settings[8 + id * 4];
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
// One byte of a layer's fixed record (its combo reference layer or one of its
// placement bitmaps). Before any settings are live, the compiled defaults.
uint8_t noah_setting_layer_record(uint8_t layer, uint8_t field) {
    if (layer >= NOAH_SETTINGS_LAYERS || field >= NOAH_SETTINGS_LAYER_RECORD_SIZE) return 0u;
    uint16_t offset = (uint16_t)(NOAH_SETTINGS_LAYER_RECORDS_OFFSET + layer * NOAH_SETTINGS_LAYER_RECORD_SIZE + field);
    return settings_length ? settings[offset] : noah_profile_settings_defaults_byte(offset);
}
// A custom key's shortcut record (D-F26). Before any settings are live, the
// keymap's own.
noah_custom_key_shortcut_t noah_custom_key_shortcut(uint8_t slot) {
    if (slot >= NOAH_SETTINGS_CUSTOM_KEY_SHORTCUTS) return (noah_custom_key_shortcut_t){0};
    uint16_t offset = (uint16_t)(NOAH_SETTINGS_SHORTCUTS_OFFSET + slot * NOAH_SETTINGS_SHORTCUT_SIZE);
    if (settings_length) return (noah_custom_key_shortcut_t){.mods = settings[offset + NOAH_CUSTOM_KEY_SHORTCUT_MODS], .key = settings[offset + NOAH_CUSTOM_KEY_SHORTCUT_KEY]};
    return (noah_custom_key_shortcut_t){.mods = noah_profile_settings_defaults_byte(offset + NOAH_CUSTOM_KEY_SHORTCUT_MODS), .key = noah_profile_settings_defaults_byte(offset + NOAH_CUSTOM_KEY_SHORTCUT_KEY)};
}
// The whole current settings domain stays cached for name readback.
uint16_t noah_effective_settings_length(void) {
    return settings_length;
}
uint8_t noah_effective_settings_byte(uint16_t offset) {
    return offset < settings_length ? settings[offset] : 0u;
}
void noah_effective_settings_keyboard_name(noah_profile_keyboard_name_t *name) {
    noah_profile_reader_t reader = noah_profile_reader_from_memory(settings, settings_length);
    (void)noah_profile_settings_v1_keyboard_name(&reader, 0, settings_length, name);
}
uint32_t noah_effective_settings_publication(void) {
    return publication_count;
}
bool noah_setting_combo_output_behaviors(void) {
    return !(noah_effective_settings_byte(NOAH_SETTINGS_FLAGS_OFFSET) & NOAH_SETTINGS_FLAG_COMBO_OUTPUT_BYPASS);
}
void noah_effective_settings_invalidate(void *context, uint32_t publication, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *view) {
    // An edit to other domains (D-F32) leaves the cached settings, and what
    // they projected into QMK, as they are.
    if (settings_length && view && active.kind == NOAH_EFFECTIVE_PROFILE_KIND_WORKING_COPY && !(view->changed_domain_mask & NOAH_PROFILE_DOMAIN_MASK_SETTINGS)) return;
    // The compiled defaults are projected too: QMK's EEPROM owns none of
    // these values any more (D-F32). Only the boot's first look at them waits:
    // a saved profile may be about to activate, and the owner projects the
    // compiled defaults once it knows none will.
    bool apply = view != NULL && !(booting && active.kind == NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS);
    if (view && !(view->profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_SETTINGS) && context) view = context;
    (void)publication;
    (void)previous;
    (void)active;
    publication_count++;
    settings_length = 0;
    noah_profile_domain_range_t payload;
    if (view && (view->profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_SETTINGS) && noah_profile_blob_v1_find_domain(&view->reader, view->base_offset, view->profile.byte_length, NOAH_PROFILE_DOMAIN_V1_SETTINGS, &payload)) {
        uint16_t length = payload.length;
        if (length <= sizeof(settings)) {
            bool ok = true;
            for (uint16_t offset = 0; offset < length; offset += 20) {
                uint16_t count = length - offset < 20 ? length - offset : 20;
                if (!noah_profile_reader_read(&view->reader, view->base_offset + payload.offset + offset, settings + offset, count)) {
                    ok = false;
                    break;
                }
            }
            if (ok) settings_length = length;
        }
    }
    if (apply && settings_length && apply_settings) apply_settings();
}
#else
#    include "noah_keymap_ids.h"
// Without portable settings a custom key keeps the keymap's shortcut.
noah_custom_key_shortcut_t noah_custom_key_shortcut(uint8_t slot) {
    if (slot >= CUSTOM_KEY_SLOT_COUNT) return (noah_custom_key_shortcut_t){0};
    return (noah_custom_key_shortcut_t){.mods = custom_key_shortcuts[slot][NOAH_CUSTOM_KEY_SHORTCUT_MODS], .key = custom_key_shortcuts[slot][NOAH_CUSTOM_KEY_SHORTCUT_KEY]};
}
#endif
