// ───────────────────────────────────────────────────────────────────────────
// Live Profile Storage Checksums
// ───────────────────────────────────────────────────────────────────────────

#include "profile_checksum.h"

uint32_t noah_profile_crc32_update(uint32_t crc, const uint8_t *bytes, size_t length) {
    size_t index;

    if (!bytes && length != 0u) {
        return crc;
    }

    for (index = 0u; index < length; index++) {
        uint8_t bit;

        crc ^= bytes[index];
        for (bit = 0u; bit < 8u; bit++) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc           = (crc >> 1u) ^ (UINT32_C(0xEDB88320) & mask);
        }
    }
    return crc;
}
uint32_t noah_profile_crc32_finish(uint32_t crc) {
    return crc ^ UINT32_C(0xFFFFFFFF);
}

uint32_t noah_profile_fnv1a_update(uint32_t digest, const uint8_t *bytes, size_t length) {
    size_t index;

    if (!bytes && length != 0u) {
        return digest;
    }

    for (index = 0u; index < length; index++) {
        digest ^= bytes[index];
        digest *= UINT32_C(16777619);
    }
    return digest;
}

uint16_t noah_profile_crc16_ccitt_update(uint16_t crc, const uint8_t *bytes, size_t length) {
    size_t index;

    if (!bytes && length != 0u) {
        return crc;
    }

    for (index = 0u; index < length; index++) {
        uint8_t bit;

        crc ^= (uint16_t)bytes[index] << 8u;
        for (bit = 0u; bit < 8u; bit++) {
            crc = (crc & UINT16_C(0x8000)) != 0u ? (uint16_t)((crc << 1u) ^ UINT16_C(0x1021)) : (uint16_t)(crc << 1u);
        }
    }
    return crc;
}
