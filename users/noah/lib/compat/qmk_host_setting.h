#pragma once
#include <stdint.h>

// Settings scalar 27, the Host settings word, without QMK dependencies so the
// settings validator can include it. Host IDs are portable settings values,
// not QMK's os_variant_t numbering. Bits 0..1 select Auto/macOS/Windows/Linux;
// bit 8 enables Unicode entry; bits 16..23 name the host layout
// (docs/architecture/host-layouts-v1.md) and bit 24 says macOS classified the
// keyboard as ISO.
enum { NOAH_HOST_AUTO = 0, NOAH_HOST_MACOS = 1, NOAH_HOST_WINDOWS = 2, NOAH_HOST_LINUX = 3,
       NOAH_HOST_UNICODE_ENABLED = 0x100 };
#define NOAH_HOST_LAYOUT_SHIFT 16u
#define NOAH_HOST_LAYOUT_MASK UINT32_C(0x00FF0000)
#define NOAH_HOST_MACOS_ISO UINT32_C(0x01000000)
// Layout IDs below this exist; tests/host/host_layout_test.c ties it to the tables.
#define NOAH_HOST_LAYOUT_LIMIT 15u
#define NOAH_HOST_SETTING_BITS (UINT32_C(0x103) | NOAH_HOST_LAYOUT_MASK | NOAH_HOST_MACOS_ISO)

static inline uint8_t noah_host_layout_id(uint32_t setting) {
    return (uint8_t)((setting & NOAH_HOST_LAYOUT_MASK) >> NOAH_HOST_LAYOUT_SHIFT);
}
