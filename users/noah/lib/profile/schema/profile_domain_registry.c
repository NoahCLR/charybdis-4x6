#include "profile_domain_registry.h"

static const noah_profile_domain_shape_t domains[] = {
#define NOAH_DOMAIN_ROW(NAME, module, id, version) {id, NOAH_PROFILE_DOMAIN_INDEX_##NAME, NOAH_PROFILE_DOMAIN_MASK_##NAME, version},
    NOAH_PROFILE_DOMAIN_ROWS(NOAH_DOMAIN_ROW)
#undef NOAH_DOMAIN_ROW
};

const noah_profile_domain_shape_t *noah_profile_domain_at(size_t index) {
    return index < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT ? &domains[index] : NULL;
}

const noah_profile_domain_shape_t *noah_profile_domain_find(uint8_t id) {
    for (size_t index = 0; index < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT; index++) {
        if (domains[index].id == id) return &domains[index];
    }
    return NULL;
}

uint8_t noah_profile_domain_count(uint8_t mask) {
    uint8_t count = 0;
    for (size_t index = 0; index < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT; index++) {
        if (mask & domains[index].mask) count++;
    }
    return count;
}

_Static_assert(sizeof(domains) / sizeof(domains[0]) == NOAH_PROFILE_DOMAIN_REGISTRY_COUNT, "domain registry drifted");
