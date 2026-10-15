#pragma once
#include "lighting_history_bank.h"

// Observe ONE coherent state frame after advancing the bank's local calendar.
// The supplied monotonic observation token is shared by all full-match entries
// first seen in this frame; continuous refresh preserves their earlier token.
// Scratch is dead after return. Facts, the bank and scratch may not overlap;
// no input producer/profile publication/callback may reenter this main walk.
// A production owner must prove complete bank capacity before enabling it:
// false exposes reserve/invariant failure, never permission to drop a frame.
bool noah_lighting_history_observe(noah_lighting_history_bank_t *bank, const noah_lighting_facts_t *facts, uint64_t order, uint8_t scratch[NOAH_LIGHTING_BATCH_BYTES]);
