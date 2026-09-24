// ────────────────────────────────────────────────────────────────────────────
// Profile-Wire v1 Action Placement
// ────────────────────────────────────────────────────────────────────────────
//
// The keyboard's check of a profile it is asked to save: whether each action
// can run where the profile places it. The rules are the ones compile-time
// validation applies to the authored profile (noah_action_supported_at), so a
// saved profile is held to the same contract as keymap.c.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>

#include "../schema/profile_validator_v1.h"

bool noah_profile_action_placement_v1_supported(const noah_profile_action_v1_t *action, noah_profile_validator_v1_placement_t placement);
