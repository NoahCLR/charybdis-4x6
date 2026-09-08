#pragma once
#include "../schema/profile_settings_v1.h"
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
#    include "effective_profile_provider.h"
#    include "../../macro/macro_payload.h"
uint32_t noah_setting(uint8_t id, uint32_t fallback);
void     noah_effective_settings_invalidate(void *context, uint32_t publication, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *view);
bool     noah_effective_settings_macro(uint8_t slot, macro_payload_ir_t *ir);
bool     noah_effective_settings_copy(uint8_t *output, uint16_t *length);
void     noah_qmk_portable_apply(void);
void     noah_effective_settings_boot(bool booting);
bool     noah_effective_settings_is_booting(void);
#else
static inline uint32_t noah_setting(uint8_t id, uint32_t fallback) {
    (void)id;
    return fallback;
}
#endif
