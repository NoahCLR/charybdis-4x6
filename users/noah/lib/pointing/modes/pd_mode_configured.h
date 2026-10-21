#pragma once

#include QMK_KEYBOARD_H
#include <stdbool.h>
#include <stdint.h>

#if defined(NOAH_PD_PROFILE_ENABLE) && defined(POINTING_DEVICE_ENABLE)
#    include "../../profile/schema/profile_pd_v1.h"
// slot: from noah_effective_pd_for_mask (in tests, any noah_pd_config_t). The
// engine trusts its kind, axis, policies and taps as the validator admits
// them, and the slot must stay unchanged until exit. NULL or a disabled slot
// leaves the engine idle. Late button releases remain owned after exit, until
// their physical release. Main loop only.
void noah_pd_engine_enter(const noah_pd_config_t *slot);
void noah_pd_engine_exit(void);
report_mouse_t noah_pd_engine_motion(report_mouse_t report);
bool noah_pd_engine_key(uint16_t keycode, keyrecord_t *record);
bool noah_pd_engine_pending_release(void);
// The modifiers a scrolling slot holds that only it manages; 0 otherwise.
uint8_t noah_pd_engine_masked_mods(const noah_pd_config_t *slot);
report_mouse_t noah_pd_configured_scroll(const noah_pd_config_t *slot, report_mouse_t report);
#endif
