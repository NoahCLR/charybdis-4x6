// ───────────────────────────────────────────────────────────────────────────
// Live Profile Storage Checksums
// ───────────────────────────────────────────────────────────────────────────

#include "profile_checksum.h"

// The reflected CRC-32 of each half byte. Taking four bits at a time gives
// the bit-at-a-time result in a quarter of the steps from a table of a few
// cache lines; the bitwise loop was most of a save's checking time on the
// keyboard (D-F29, "Pacing").
static const uint32_t crc32_nibbles[16] = {
    UINT32_C(0x00000000), UINT32_C(0x1DB71064), UINT32_C(0x3B6E20C8), UINT32_C(0x26D930AC), UINT32_C(0x76DC4190), UINT32_C(0x6B6B51F4), UINT32_C(0x4DB26158), UINT32_C(0x5005713C),
    UINT32_C(0xEDB88320), UINT32_C(0xF00F9344), UINT32_C(0xD6D6A3E8), UINT32_C(0xCB61B38C), UINT32_C(0x9B64C2B0), UINT32_C(0x86D3D2D4), UINT32_C(0xA00AE278), UINT32_C(0xBDBDF21C),
};

uint32_t noah_profile_crc32_update(uint32_t crc, const uint8_t *bytes, size_t length) {
    size_t index;

    if (!bytes && length != 0u) {
        return crc;
    }

    for (index = 0u; index < length; index++) {
        crc ^= bytes[index];
        crc = (crc >> 4u) ^ crc32_nibbles[crc & 15u];
        crc = (crc >> 4u) ^ crc32_nibbles[crc & 15u];
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
