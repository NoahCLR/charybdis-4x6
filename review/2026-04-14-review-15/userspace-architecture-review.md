# Refactor Follow-Up Review

Date: 2026-04-14

Status: follow-up quality audit of the landed index-boundary and host-runner
cleanup after `review/2026-04-14-review-14/`.

Scope:

- whether the post-`key_runtime_index_internal.h` cleanup actually sealed the
  production boundary
- whether the host-runner cleanup reduced maintenance risk or only relocated it
- whether the review notes now accurately describe the code that exists

## Findings

### No must-fix correctness regressions found in the current tree

The current tree still passes the full host suite and firmware compile, and I
did not find a concrete runtime behavior regression in the reviewed key-runtime,
pd-mode, or RGB paths.

### Optional cleanup: review history is better than before, but `review-13` is still contradictory and now sits behind a corrected follow-up note

References:

- `review/2026-04-14-review-13/userspace-architecture-review.md:15`
- `review/2026-04-14-review-13/userspace-architecture-review.md:46`
- `review/2026-04-14-review-13/userspace-architecture-review.md:104`
- `review/2026-04-14-review-13/progress.md:48`
- `review/2026-04-14-review-14/userspace-architecture-review.md:18`

Reasoning:

- `review-14` is now the newest review folder and is materially more accurate,
  so the immediate source-of-truth problem is improved.
- But `review-13` still mixes a "landed" structure summary with stale open
  findings that describe the pre-cleanup tree.
- The result is better than the prior state because the newest review is now
  coherent, but the historical trail is still noisier than it needs to be.

Why this matters:

- architecture archaeology still requires readers to notice that `review-13`
  is partially superseded inside the same day’s review chain
- the current follow-up note is correct, but the earlier contradictory note was
  not actually reconciled

## Landed Cleanup

### Keymap-owned production boundary coverage

The compile gate now treats the active keymap as repo-owned production code for
key-runtime boundary enforcement, not just `users/noah`.

References:

- `tests/host/run_feature_gate_compile_tests.sh`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c:18`

What landed:

- the key-runtime internal-header production scans now cover both `users/noah`
  and `keyboards/bastardkb/charybdis/4x6/keymaps/noah`
- the removed-header rejection for `key_runtime_state.h`,
  `key_runtime_process.h`, and removed `key_runtime_index.h` now uses that same
  repo-owned production scope
- the error text now matches the contract it is enforcing: repo-owned
  production code is the boundary, not only `users/noah`

Why this matters:

- the compile gate now matches the architectural claim that the public seam is
  for production code, not just for userspace-owner files
- keymap-owned translation units can no longer bypass the internal-header
  boundary without tripping the gate

### Derived public-seam runner helpers

The reviewed public-seam runners now derive their userspace support graphs from
`NOAH_COMMON_SOURCES` instead of a hand-maintained mini-manifest.

References:

- `tests/host/noah_source_manifest.sh`
- `tests/host/run_runtime_debug_tests.sh`
- `tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `tests/host/run_key_runtime_scenario_tests.sh`

What landed:

- `noah_host_public_key_runtime_support_paths()` is gone
- `tests/host/noah_source_manifest.sh` now exposes one low-level helper that
  expands `NOAH_COMMON_SOURCES`, removes an explicit exact-match exclusion
  list, and returns final source paths for `cc`
- the runtime-debug, modifier-hold, and scenario runners now use derived
  runner-specific wrappers on top of that helper
- `run_key_runtime_scenario_tests.sh` now shares the same helper model as the
  other reviewed public-seam runners

Why this matters:

- the runner cleanup is now mechanically tied to the canonical userspace
  manifest instead of shadowing it with a second hard-coded source inventory
- future key-runtime support-graph changes in `NOAH_COMMON_SOURCES` only need
  one exclusion update per runner shape instead of parallel source-list edits

## Solid Areas

- `users/noah/lib/key/runtime/key_runtime_index.h` is gone, and the remaining
  index readers are clearly marked internal in
  `users/noah/lib/key/runtime/key_runtime_index_internal.h`.
- Higher-level harnesses still look meaningfully cleaner than the pre-refactor
  tree. `tests/host/key_runtime_integration_harness.h` and
  `tests/host/key_runtime_scenario_harness.c` continue to operate through public
  seams instead of private key-runtime process headers.
- The orchestration cleanup from the previous pass still looks solid.
  `users/noah/runtime_init.c`, `users/noah/lib/key/runtime/key_runtime_process.c`,
  and `users/noah/lib/rgb/core/rgb_runtime.c` remain locally readable, and the
  explicit order tests are still in place and passing.

## Verification

Commands run in this audit pass:

- `git status --short`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Current conclusion:

- no must-fix correctness regressions found in the reviewed refactor
- the keymap-owned production boundary gap and runner mini-manifest issue are
  now landed cleanup, not open findings
- the strongest remaining issue in this review chain is review-history
  contradiction in `review-13`, not runtime behavior
