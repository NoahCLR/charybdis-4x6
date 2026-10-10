#pragma once
#include <stdint.h>

#include "qmk_host_setting.h"

// Detection cannot confirm the computer's input configuration. Includers
// provide QMK's OS detection when OS_DETECTION_ENABLE is set.

static inline uint8_t noah_host_effective(uint32_t setting, uint8_t detected) {
    uint8_t selected = setting & 3u;
    return selected ? selected : detected <= NOAH_HOST_LINUX ? detected : NOAH_HOST_AUTO;
}

static inline uint8_t noah_host_detected(void) {
#ifdef OS_DETECTION_ENABLE
    switch (detected_host_os()) {
        case OS_MACOS: return NOAH_HOST_MACOS;
        case OS_WINDOWS: return NOAH_HOST_WINDOWS;
        case OS_LINUX: return NOAH_HOST_LINUX;
        default: return NOAH_HOST_AUTO;
    }
#else
    return NOAH_HOST_AUTO;
#endif
}
