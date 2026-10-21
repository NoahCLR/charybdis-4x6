#pragma once

// What a half's store says it has saved, asked through the store's own
// answer rather than read from its fields.

#include <assert.h>
#include <stdint.h>

#include "users/noah/lib/profile/storage/profile_store.h"

// The saved record; the caller expects the store to hold one.
static inline const noah_profile_store_record_t *committed_of(const noah_profile_store_t *store) {
    const noah_profile_store_record_t *committed = noah_profile_store_committed(store);

    assert(committed);
    return committed;
}

// The saved generation, or 0 while nothing is saved.
static inline uint32_t committed_generation(const noah_profile_store_t *store) {
    const noah_profile_store_record_t *committed = noah_profile_store_committed(store);

    return committed ? committed->generation : 0u;
}
