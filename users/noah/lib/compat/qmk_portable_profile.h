#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
void noah_qmk_portable_storage_init(void);
void noah_qmk_portable_apply(void);
// What QMK runs now for setting `id` (the runtime readback beside the stored
// settings, D-F32).
uint32_t noah_qmk_portable_live_setting(uint8_t id);
bool noah_qmk_portable_profile_get(uint8_t *frame, uint8_t length);
#else
static inline void noah_qmk_portable_storage_init(void) {}
static inline bool noah_qmk_portable_profile_get(uint8_t *frame, uint8_t length) {
    (void)frame;
    (void)length;
    return false;
}
#endif
