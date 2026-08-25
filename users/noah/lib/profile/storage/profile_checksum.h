// ───────────────────────────────────────────────────────────────────────────
// Live Profile Storage Checksums
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stddef.h>
#include <stdint.h>

#define NOAH_PROFILE_CRC32_INITIAL UINT32_C(0xFFFFFFFF)
#define NOAH_PROFILE_FNV1A_INITIAL UINT32_C(0x811C9DC5)
#define NOAH_PROFILE_CRC16_INITIAL UINT16_C(0xFFFF)

uint32_t noah_profile_crc32_update(uint32_t crc, const uint8_t *bytes, size_t length);
uint32_t noah_profile_crc32_finish(uint32_t crc);
uint32_t noah_profile_fnv1a_update(uint32_t digest, const uint8_t *bytes, size_t length);
uint16_t noah_profile_crc16_ccitt_update(uint16_t crc, const uint8_t *bytes, size_t length);
