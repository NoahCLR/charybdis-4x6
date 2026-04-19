// ────────────────────────────────────────────────────────────────────────────
// Runtime V2 Projection Helpers
// ────────────────────────────────────────────────────────────────────────────
//
// Reducer-owned projector surface for applying authored runtime effects to
// QMK-facing state. Transition/release orchestration may still transport
// effect queues, but effect execution should flow through this one seam.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../key/runtime/effects/key_runtime_effect.h"
#include "runtime_v2.h"

void runtime_v2_project_effect(const key_runtime_effect_t *effect);
void runtime_v2_project_pending_release_dispatch(const pending_release_t *pending);
