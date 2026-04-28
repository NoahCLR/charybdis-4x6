// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Deferred Release Transport
// ────────────────────────────────────────────────────────────────────────────
//
// Adapter between release transition plans and the core-owned pending-release
// queue.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "transition.h"

void key_runtime_deferred_release_defer_dispatch_actions_until_release(keypos_t key_pos, key_runtime_transition_plan_t *plan);
void key_runtime_deferred_release_drain_dispatches(void);
