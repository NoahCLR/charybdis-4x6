#pragma once

#include QMK_KEYBOARD_H
#include <stdbool.h>
#include <stdint.h>

#if defined(NOAH_PD_PROFILE_ENABLE) && defined(POINTING_DEVICE_ENABLE)
// Records must pass profile_pd_v1 validation and stay immutable until exit.
// Late button releases remain owned after exit, until their physical release.
void noah_pd_engine_enter(const uint8_t *record);
void noah_pd_engine_exit(void);
report_mouse_t noah_pd_engine_motion(report_mouse_t report);
bool noah_pd_engine_key(uint16_t keycode, keyrecord_t *record);
bool noah_pd_engine_pending_release(void);
uint8_t noah_pd_engine_masked_mods(const uint8_t *record);
report_mouse_t noah_pd_configured_scroll(const uint8_t *record, report_mouse_t report);
#endif
