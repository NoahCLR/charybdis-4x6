# Implementation Progress

This file tracks the architecture review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-13

### Architecture review pass

Completed in this pass:

- Started with `git status --short` and confirmed the worktree was clean.
- Confirmed the current review lineage under `review/` and opened the next
  sortable same-day review folder:
  `review/2026-04-13-review-06/`.
- Re-read the most recent architecture thread in
  `review/2026-04-13-review-05/` before auditing the live code.
- Reviewed the current userspace architecture across:
  - key runtime authored lookup, cached interaction, release reduction,
    multi-tap handling, effect planning, and slot state ownership
  - pd-mode manifest, registry, state transitions, lifecycle hooks, and
    pointer-layer policy
  - macro dispatch / IR / VIA-default seeding
  - RGB stage structure and split-sync state consumption
  - host-test harness structure, debug/reset surfaces, and compile/test wiring
- Wrote a new architecture review focused on long-term extensibility rather
  than only immediate correctness.

Key findings recorded in this review:

- high priority: release semantics still live in both the active-slot release
  reducer and the pending-multi-tap release reducer, with the release-matrix
  suite acting as the backstop that keeps those paths aligned
- high priority: `key_runtime_slot_interaction_t` is still not a completely
  stable boundary because the press reducer rebuilds and mutates interaction
  state after the canonical resolution-to-interaction translation
- high priority: pd-mode extension is still distributed policy; the manifest
  row mixes identity with behavior policy, and generic trait interpretation is
  spread across state, lifecycle, and pointer-layer policy modules
- medium priority: fixed effect queue capacities (`8` per slot result, `16` per
  transition plan) are currently silent scalability ceilings
- medium priority: test architecture is strongest in the key runtime, while
  pd-mode/RGB/integration suites still rebuild local host-runtime scaffolding

Areas assessed as strong in this pass:

- the keymap-owned data vs userspace-owned runtime boundary is now explicit and
  mostly well-kept
- the key runtime slot/effect model is much easier to reason about than direct
  side-effect branching
- pd-mode state is clearer because active/locked selection is explicit
- macro behavior is centered on one IR pipeline
- docs and host coverage are strong enough to support architecture work

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- full host suite passed
- firmware build passed

Checks intentionally skipped in this pass:

- none

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to
  `charybdis-4x6/review/2026-04-13-review-06/`

Next steps:

- extract one shared release-semantic resolver for active release and pending
  multi-tap release
- replace interaction post-construction patching with a dedicated slot
  materialization API
- reduce pd-mode trait scattering by separating identity/registration from
  behavior policy
- fail host tests on key-runtime effect queue overflow and add shared runtime
  fixtures for non-key-runtime host suites

### Key runtime release/materialization follow-up

Completed in this pass:

- Started with `git status --short` and confirmed the only pre-existing
  worktree delta was the active review folder itself.
- Reworked the cached interaction boundary in
  `users/noah/lib/key/runtime/key_runtime_interaction.h` so
  `key_runtime_slot_materialize(...)` is now the canonical way to produce a
  slot-owned interaction contract, and
  `key_runtime_slot_interaction_from_resolution(...)` delegates to it.
- Replaced the press reducer's post-construction interaction patching in
  `users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.c` with
  explicit binding construction plus slot materialization.
- Extracted shared release-decision logic into
  `users/noah/lib/key/runtime/slot/key_runtime_slot_release_resolver.h`.
- Updated
  `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c` and
  `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c` to
  use the shared release resolver as thin adapters while preserving their
  path-specific effect mapping.
- Added direct host coverage in `tests/host/key_runtime_slot_test.c` for:
  - materialization from an authored resolution matching the default cached
    interaction contract
  - materialization with binding/hold-strategy overrides recomputing flags,
    policy, and release contracts correctly

Contracts touched in this pass:

- shared active-release vs pending-multi-tap release semantics
- slot materialization as the stable authored-resolution to cached-slot
  boundary
- pending multi-tap held-lifecycle synthesis when the base pending resolution
  itself produces the held action

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- targeted key-runtime slot, release-matrix, and transition suites passed
- full host suite passed
- firmware build passed

Workspace scope:

- no sibling workspace folders were edited
- changes in this pass touched:
  - `users/noah/lib/key/runtime/`
  - `tests/host/key_runtime_slot_test.c`
  - this review folder

Next steps:

- fail host tests on key-runtime effect queue overflow instead of silently
  tolerating `overflowed`
- decide whether the next highest-value refactor is pd-mode policy ownership or
  a shared host runtime fixture for non-key-runtime suites

### Key runtime overflow follow-up

Completed in this pass:

- Started with `git status --short` and continued from the in-flight
  key-runtime/review changes already present in the worktree.
- Updated `tests/host/include/qmk_stub.h` to expose an explicit host-test-only
  runtime-overflow failure hook.
- Changed
  `users/noah/lib/key/runtime/slot/key_runtime_slot_result.c` and
  `users/noah/lib/key/runtime/key_runtime_transition.c` so slot-result and
  transition-plan overflow still set `overflowed`, but now immediately fail
  host tests instead of only logging and continuing.
- Replaced the old intentional-overflow coverage in
  `tests/host/key_runtime_slot_test.c` and
  `tests/host/key_runtime_transition_test.c` with exact-capacity boundary
  checks that prove the queues stay non-overflowing at the current limit.

Contracts touched in this pass:

- key-runtime slot-result overflow is now a host-test failure
- key-runtime transition-plan overflow is now a host-test failure
- firmware runtime behavior stays unchanged outside the host-test stub
  environment

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- targeted key-runtime slot, transition, and release-matrix suites passed
- full host suite passed
- firmware build passed

Workspace scope:

- no sibling workspace folders were edited
- changes in this pass touched:
  - `tests/host/include/qmk_stub.h`
  - `tests/host/key_runtime_slot_test.c`
  - `tests/host/key_runtime_transition_test.c`
  - `users/noah/lib/key/runtime/slot/key_runtime_slot_result.c`
  - `users/noah/lib/key/runtime/key_runtime_transition.c`
  - this review folder

Next steps:

- choose between the remaining review items:
  - pd-mode policy ownership split
  - shared host runtime fixtures for non-key-runtime suites
