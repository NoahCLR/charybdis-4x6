#pragma once
#include "active_profile_provider.h"
#include "../schema/profile_autocorrect_v1.h"

// The active profile's autocorrect list (domain 0x80, D-F68), read in place
// from the generation the provider keeps immutable: a saved slot in flash or
// the working copy. The compiled defaults, and a profile without the domain,
// read the authored list. Nothing is copied or indexed in RAM; the handle
// holds the list's range and its four counts. Publication is scan-owned.
void noah_active_profile_autocorrect_invalidate(void *context, uint32_t publication, noah_active_profile_identity_t previous, noah_active_profile_identity_t active, const noah_active_profile_snapshot_t *view);

// Never NULL. Empty before the first publication, and when a profile's list
// cannot be opened. Look typos up with noah_profile_autocorrect_v1_find,
// _part_at and _correction.
const noah_profile_autocorrect_v1_list_t *noah_active_profile_autocorrect_list(void);

// Changes whenever the list may have: a row or an index read before a
// publication says nothing about the list after it.
uint32_t noah_active_profile_autocorrect_publication(void);
