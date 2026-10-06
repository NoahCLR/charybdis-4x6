#pragma once
#include "../schema/profile_pd_v1.h"
#ifdef NOAH_PD_PROFILE_ENABLE
#include "effective_profile_provider.h"
// Immutable warm records, one per slot: no storage reads on movement or
// key-event paths. Publication materializes every slot; a slot the sparse
// domain leaves out reads as disabled with an empty name.
const uint8_t *noah_effective_pd_record(uint8_t slot);
// mode is a pd_mode_mask_t (one bit per slot); the profile layer does not
// include the pointing headers. NULL unless exactly one bit names a
// configured slot.
const uint8_t *noah_effective_pd_for_mask(uint32_t mode);
bool noah_effective_pd_ready(void);
// Factory-only images have no profile owner to publish the compiled generation.
void noah_effective_pd_load_compiled_defaults(const noah_pd_config_t defaults[NOAH_PROFILE_PD_V1_SLOT_COUNT]);
void noah_effective_pd_invalidate(void *context, uint32_t publication, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *view);
#endif
