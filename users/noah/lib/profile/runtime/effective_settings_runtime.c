#include "effective_settings_runtime.h"
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
#    include <string.h>
#    include "../../macro/macro_dispatch.h"
static uint8_t  settings[NOAH_SETTINGS_MAX_SIZE];
static uint16_t settings_length;
static bool     booting;
void            noah_effective_settings_boot(bool value) {
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
bool noah_effective_settings_copy(uint8_t *output, uint16_t *length) {
    if (!settings_length || !output || !length) return false;
    memcpy(output, settings, settings_length);
    *length = settings_length;
    return true;
}
bool noah_effective_settings_macro(uint8_t slot, macro_payload_ir_t *ir) {
    if (!settings_length || slot >= 16 || !ir) return false;
    uint16_t offset = NOAH_SETTINGS_FIXED_SIZE;
    for (uint8_t i = 0; i <= slot; i++) {
        uint16_t length = settings[offset] | (uint16_t)settings[offset + 1] << 8;
        offset += 2;
        if (i == slot) {
            ir->length = length;
            memcpy(ir->bytes, settings + offset, length);
            return true;
        }
        offset += length;
    }
    return false;
}
void noah_effective_settings_invalidate(void *context, uint32_t publication, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *view) {
    (void)context;
    (void)publication;
    (void)previous;
    (void)active;
    settings_length = 0;
    if (view && (view->profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_SETTINGS)) {
        uint16_t length = view->profile.settings.length;
        if (length <= sizeof(settings)) {
            bool ok = true;
            for (uint16_t offset = 0; offset < length; offset += 20) {
                uint16_t count = length - offset < 20 ? length - offset : 20;
                if (!noah_profile_reader_read(&view->reader, view->base_offset + view->profile.settings.offset + offset, settings + offset, count)) {
                    ok = false;
                    break;
                }
            }
            if (ok) settings_length = length;
        }
    }
    macro_dispatch_invalidate();
    if (settings_length) noah_qmk_portable_apply();
}
#endif
