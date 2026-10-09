#pragma once
#include "../schema/profile_settings_v1.h"
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
#    include "effective_profile_provider.h"
uint32_t noah_setting(uint8_t id, uint32_t fallback);
uint8_t  noah_setting_layer_record(uint8_t layer, uint8_t field);
void     noah_effective_settings_invalidate(void *context, uint32_t publication, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *view);
uint16_t noah_effective_settings_length(void); // 0 when no profile settings are live
uint8_t  noah_effective_settings_byte(uint16_t offset);
typedef void (*noah_effective_settings_apply_fn)(void);
void     noah_effective_settings_set_apply(noah_effective_settings_apply_fn apply);
void     noah_effective_settings_boot(bool booting);
bool     noah_effective_settings_is_booting(void);
#else
static inline uint32_t noah_setting(uint8_t id, uint32_t fallback) {
    (void)id;
    return fallback;
}
// Without portable settings each layer refers combos to itself and bypasses
// or excludes nothing.
static inline uint8_t noah_setting_layer_record(uint8_t layer, uint8_t field) {
    return field == NOAH_SETTINGS_LAYER_REFERENCE ? layer : 0u;
}
#endif
