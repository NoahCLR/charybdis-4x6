// One row per current profile domain, in canonical wire order. A row names
// its domain module: each consumer binds that module's operations by name,
// so a domain added here without them does not compile.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// X(NAME, module, id, version). A row's index is its position, numbered by
// the enum below, and its mask bit is 1 << index.
#define NOAH_PROFILE_DOMAIN_ROWS(X) \
    X(RGB,           rgb,           0x10u, 4u) \
    X(KEY_BEHAVIORS, key_behaviors, 0x20u, 2u) \
    X(COMBOS,        combos,        0x30u, 3u) \
    X(SETTINGS,      settings,      0x40u, 6u) \
    X(PD,            pd,            0x50u, 3u)

enum {
#define NOAH_DOMAIN_INDEX(NAME, module, id, version) NOAH_PROFILE_DOMAIN_INDEX_##NAME,
    NOAH_PROFILE_DOMAIN_ROWS(NOAH_DOMAIN_INDEX)
#undef NOAH_DOMAIN_INDEX
    NOAH_PROFILE_DOMAIN_REGISTRY_COUNT,
};

enum {
#define NOAH_DOMAIN_CONSTANTS(NAME, module, id, version) \
    NOAH_PROFILE_DOMAIN_V1_##NAME = id, \
    NOAH_PROFILE_DOMAIN_MASK_##NAME = 1u << NOAH_PROFILE_DOMAIN_INDEX_##NAME, \
    NOAH_PROFILE_DOMAIN_VERSION_##NAME = version,
    NOAH_PROFILE_DOMAIN_ROWS(NOAH_DOMAIN_CONSTANTS)
#undef NOAH_DOMAIN_CONSTANTS
#define NOAH_DOMAIN_MASK(NAME, module, id, version) | NOAH_PROFILE_DOMAIN_MASK_##NAME
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
    uint8_t id, index, mask, version;
} noah_profile_domain_shape_t;

// A domain's payload inside one blob: offset from the blob start, zero length
// when the blob omits the domain. Recorded once by the envelope walk.
typedef struct {
    uint16_t offset, length;
} noah_profile_domain_range_t;

// The registry describes the wire vocabulary even in feature-gate builds.
// A build's allowed mask remains a separate admission policy.
const noah_profile_domain_shape_t *noah_profile_domain_at(size_t index);
const noah_profile_domain_shape_t *noah_profile_domain_find(uint8_t id);
uint8_t noah_profile_domain_count(uint8_t mask);
