// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Core Projection Helpers
// ────────────────────────────────────────────────────────────────────────────
//
// Reducer-owned projector surface for applying authored runtime effects to
// QMK-facing state. Transition/release orchestration may still transport
// effect queues, but effect execution should flow through this one seam.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../effects/effect.h"
#include "runtime.h"

void key_runtime_core_project_effect(const key_runtime_effect_t *effect);
void key_runtime_core_project_pending_release_dispatch(const pending_release_t *pending);
projection_snapshot_t key_runtime_core_projection_snapshot_capture(void);
bool                  key_runtime_core_projection_snapshot_equal(const projection_snapshot_t *lhs, const projection_snapshot_t *rhs);
