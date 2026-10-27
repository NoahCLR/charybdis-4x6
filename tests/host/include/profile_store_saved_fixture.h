#pragma once

// What a half's store says it has saved, asked through the store's own
// answer rather than read from its fields.

#include <assert.h>
#include <stdint.h>

#include "users/noah/lib/profile/storage/profile_store.h"

// The saved record; the caller expects the store to hold one.
static inline const noah_profile_store_record_t *saved_of(const noah_profile_store_t *store) {
    const noah_profile_store_record_t *saved = noah_profile_store_saved(store);

    assert(saved);
    return saved;
}

// The saved generation, or 0 while nothing is saved.
static inline uint32_t saved_generation(const noah_profile_store_t *store) {
    const noah_profile_store_record_t *saved = noah_profile_store_saved(store);

    return saved ? saved->generation : 0u;
}
