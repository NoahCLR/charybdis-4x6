#pragma once
#include "../schema/profile_pd_v1.h"
#include "active_profile_provider.h"
// Every slot's effective configuration, warm in RAM: the 32 validated records,
// held as noah_pd_config_t so their fields read directly and aligned. A slot
// the sparse domain leaves out reads as disabled with an empty name (zero but
// its id).
// A returned slot stays valid and unchanged until the next publication.
// Publication (activation, or an in-place working-copy edit that changed the
// PD domain) runs only at the provider's safe boundary: no mode active or
// locked and no consumed button release pending, so an engine holding a slot
// never sees it change. Movement and key paths do no storage read, decode,
// copy or allocation.
// Read on the main loop (pointing task, key processing, scan). The split
// sync receive also reaches it, through pointing_mode_apply_command's snapshot
// of the active mode's traits, and discards what it read.
// NULL until ready, or for a slot past the last.
const noah_pd_config_t *noah_active_profile_pd_record(uint8_t slot);
// mode is a pointing_mode_mask_t (one bit per slot); the profile layer does not
// include the pointing headers. NULL unless ready and exactly one bit names a
// slot whose kind is not NOAH_PD_KIND_DISABLED.
const noah_pd_config_t *noah_active_profile_pd_for_mask(uint32_t mode);
bool noah_active_profile_pd_ready(void);
// Factory-only images have no profile owner to publish the compiled generation.
void noah_active_profile_pd_load_compiled_defaults(const noah_pd_config_t defaults[NOAH_PROFILE_PD_V1_SLOT_COUNT]);
void noah_active_profile_pd_invalidate(void *context, uint32_t publication, noah_active_profile_identity_t previous, noah_active_profile_identity_t active, const noah_active_profile_snapshot_t *view);
