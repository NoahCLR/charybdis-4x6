// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Transitions
// ────────────────────────────────────────────────────────────────────────────
//
// The executor of the reducer's effect plan. A transition is one reducer call
// planning into one stack-held plan (reducer/runtime.h), which the caller then
// executes here: traced, announced to split consumers and projected through
// projection/projection.c. A streaming plan is also projected here while it is
// planned (planning/effect_plan.h).
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "reducer/runtime.h"

// The feedback sequence a key event or scan starts from, for
// key_runtime_transition_execute_event_plan().
uint32_t key_runtime_transition_feedback_mark(void);

// Traces the plan under `stage` (none for NULL), announces it to split
// consumers if it holds anything, projects it and empties it.
void key_runtime_transition_execute_plan(const char *stage, key_runtime_core_effect_plan_t *plan);

// The same for a key event or a scan, which can change what the feedback maps
// render without leaving anything to project: opening a multi-tap window makes
// a pending tap series visible while the tap itself is still deferred. An empty
// plan still announces a feedback change since `feedback_mark`, or the split
// worker never learns and the slave renders nothing while the master lights up.
void key_runtime_transition_execute_event_plan(const char *stage, key_runtime_core_effect_plan_t *plan, uint32_t feedback_mark);

// Projects and empties a full streaming plan without announcing it; the
// transition's own execute announces it, since it still holds what was pushed
// after the drain.
void key_runtime_transition_drain_plan(key_runtime_core_effect_plan_t *plan);
