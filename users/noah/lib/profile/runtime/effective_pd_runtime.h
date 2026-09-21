#pragma once
#include "../schema/profile_pd_v1.h"
#ifdef NOAH_PD_PROFILE_ENABLE
#include "effective_profile_provider.h"
// Immutable warm records: no storage reads on movement or key-event paths.
const uint8_t *noah_effective_pd_record(uint8_t slot);
const uint8_t *noah_effective_pd_for_mask(uint8_t mode);
bool noah_effective_pd_ready(void);
void noah_effective_pd_invalidate(void *context, uint32_t publication, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *view);
#endif
