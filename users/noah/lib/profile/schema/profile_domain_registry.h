// One row per current profile domain, in canonical wire order.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NOAH_PROFILE_DOMAIN_ROWS(X) \
    X(RGB,           0x10u, 0, 3u) \
    X(KEY_BEHAVIORS, 0x20u, 1, 1u) \
    X(COMBOS,        0x30u, 2, 2u) \
    X(SETTINGS,      0x40u, 3, 5u) \
    X(PD,            0x50u, 4, 2u)

enum {
#define NOAH_DOMAIN_CONSTANTS(name, id, bit, version) \
    NOAH_PROFILE_DOMAIN_V1_##name = id, \
    NOAH_PROFILE_DOMAIN_MASK_##name = 1u << bit, \
    NOAH_PROFILE_DOMAIN_VERSION_##name = version,
    NOAH_PROFILE_DOMAIN_ROWS(NOAH_DOMAIN_CONSTANTS)
#undef NOAH_DOMAIN_CONSTANTS
#define NOAH_DOMAIN_COUNT(name, id, bit, version) + 1u
    NOAH_PROFILE_DOMAIN_REGISTRY_COUNT = 0u NOAH_PROFILE_DOMAIN_ROWS(NOAH_DOMAIN_COUNT),
#undef NOAH_DOMAIN_COUNT
#define NOAH_DOMAIN_MASK(name, id, bit, version) | (1u << bit)
    NOAH_PROFILE_DOMAIN_MASK_ALL = 0u NOAH_PROFILE_DOMAIN_ROWS(NOAH_DOMAIN_MASK),
#undef NOAH_DOMAIN_MASK
};

#ifdef NOAH_PD_PROFILE_ENABLE
#    define NOAH_PROFILE_ENABLED_PD_MASK NOAH_PROFILE_DOMAIN_MASK_PD
#else
#    define NOAH_PROFILE_ENABLED_PD_MASK 0u
#endif

enum {
    NOAH_PROFILE_ENABLED_DOMAIN_MASK = (NOAH_PROFILE_DOMAIN_MASK_ALL & ~NOAH_PROFILE_DOMAIN_MASK_PD) | NOAH_PROFILE_ENABLED_PD_MASK,
};

typedef struct {
    uint8_t id, mask, version;
} noah_profile_domain_shape_t;

// The registry describes the wire vocabulary even in feature-gate builds.
// A build's allowed mask remains a separate admission policy.
const noah_profile_domain_shape_t *noah_profile_domain_at(size_t index);
const noah_profile_domain_shape_t *noah_profile_domain_find(uint8_t id);
uint8_t noah_profile_domain_count(uint8_t mask);
