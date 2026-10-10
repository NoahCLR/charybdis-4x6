#pragma once
#include <stdint.h>

// Host IDs are portable settings values, not QMK's os_variant_t numbering.
// Scalar 27: low two bits select Auto/macOS/Windows/Linux; bit 8 enables
// Unicode entry. Detection cannot confirm the computer's input configuration.
enum { NOAH_HOST_AUTO = 0, NOAH_HOST_MACOS = 1, NOAH_HOST_WINDOWS = 2, NOAH_HOST_LINUX = 3,
       NOAH_HOST_UNICODE_ENABLED = 0x100 };

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
