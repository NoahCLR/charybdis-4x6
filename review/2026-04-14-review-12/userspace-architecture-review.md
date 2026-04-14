# Refactor Follow-Up Review

Date: 2026-04-14

Status: follow-up audit plus remediation of the `review-12` should-fix items.

Scope:

- public runtime debug surface after the sealing/remediation passes
- host-test fixture boundaries after the hard-cut cleanup
- review/doc integrity after the cleanup landed

## Findings

### No must-fix correctness regressions found in the current tree

The runtime-sealing and follow-up cleanup work now land cleanly. I did not find
a concrete firmware behavior regression in the reviewed runtime, pd, or host
test seams.

### Resolved: `runtime_debug.h` is now a live semantic query surface

References:

- `users/noah/lib/state/runtime/runtime_debug.h:1`
- `users/noah/lib/state/runtime/runtime_debug.c:1`
- `tests/host/key_runtime_scenario_harness.c:228`

What changed:

- the public snapshot struct and `noah_runtime_debug_snapshot(...)` entry point
  are gone
- `runtime_debug.h` now exposes only live key-runtime queries for slot state,
  active-slot ordering, pending multi-tap ordering, and preview/fallback owner
  lookup
- the runtime-debug implementation now answers directly from key-runtime slot
  and index helpers instead of copying the whole matrix into a transient public
  aggregate

Why this is materially better:

- matrix-sized storage layout is no longer part of the public debug contract
- higher-level tests can read the semantics they need without carrying a
  heavyweight snapshot object through helper layers
- the scenario/integration harnesses no longer allocate internal debug snapshots
  just to answer single-slot questions

Residual note:

- if a future integration test genuinely needs a bulk runtime dump, keep that
  heavier shape local to the harness instead of reintroducing it as the primary
  public runtime-debug API

### Resolved: the catch-all host runtime fixture has been split

References:

- `tests/host/include/host_runtime_reset_fixture.h:1`
- `tests/host/include/host_pd_fixture.h:1`
- `tests/host/run_feature_gate_compile_tests.sh:30`

What changed:

- the old `host_runtime_fixture.h` umbrella header is removed
- common reset/time/mod/layer stubs now live in
  `host_runtime_reset_fixture.h`
- pd-mode display/snapshot and split-remote helpers now live in
  `host_pd_fixture.h`
- the compile gate now fails on any include of the removed umbrella header

Why this is materially better:

- lightweight key-runtime suites no longer import pd/split helpers by default
- pd/rgb suites can depend on their own helper header explicitly
- the intended host-test boundaries are now enforced mechanically instead of by
  convention

### Resolved: review-11 no longer advertises stale verification steps

References:

- `review/2026-04-14-review-11/progress.md:63`
- `review/2026-04-14-review-12/progress.md:1`

What changed:

- the old review-11 “next steps” now record the completed verification state
  instead of claiming the remediation still needed full-suite/build checks
- review-12 carries the actual follow-up cleanup history for the debug/test seam
  hard cut

Why this matters:

- the review folders remain the architecture record for this repo
- stale “next steps” create confusion about what was truly landed and verified

## Solid Areas

- the strict runtime reset seam remains sound
- pd raw storage sealing remains intact
- the compile gate now enforces both runtime storage boundaries and the removed
  umbrella fixture boundary
- key-runtime integration tests now use live runtime-debug queries or
  module-owned seams instead of a public aggregate snapshot

## Remaining Architecture Debt

- the main remaining structural debt is still hook/stage orchestration in
  `key_runtime_process.c`, `runtime_init.c`, and `rgb_runtime.c`
- authored keymap structure in `keymap.c` remains a secondary maintainability
  issue, but it is lower priority than hook/stage registration

## Verification

Commands run for this cleanup pass:

- `git status --short`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_index_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`

Current conclusion:

- the `review-12` should-fix items are resolved in the current tree
- runtime sealing and observation/test seam cleanup should stop being treated as
  active debt
- the next architecture target is hook/stage orchestration
