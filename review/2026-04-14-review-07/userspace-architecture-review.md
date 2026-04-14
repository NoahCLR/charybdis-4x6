# Userspace Architecture Review

Date: 2026-04-14

Status: follow-up quality audit of the refactor work landed through
`review/2026-04-14-review-06/`.

Scope:

- correctness and regression risk after the review-06 cleanup
- abstraction quality and interface cleanliness in the current landed tree
- test quality, code organization, and review integrity for the refactored
  userspace runtime

Out of scope:

- fresh architecture brainstorming
- hardware changes
- upstream QMK redesign outside this repo

## Findings

### No Must-Fix Issues Found

I did not find a new production correctness failure that is clearly shipping
broken firmware behavior today. The full host suite and firmware build are
green, and the review-06 cleanup did materially improve the action-kind split,
the handled-key test seam, and the key-runtime index read surface.

### Should-Fix

#### 1. Several host suites are still defining `noah_dispatch_synthetic_record(...)` with the wrong signature, so the compiler is not actually protecting the refactored dispatch seam

References:

- `users/noah/lib/action/synthetic_record.h:12-15`
- `tests/host/action_lifecycle_test.c:178-186`
- `tests/host/via_macro_action_lifecycle_test.c:166-176`
- `tests/host/key_behavior_validation_test.c:82-91`
- `tests/host/keymap_validation_test.c:107-116`
- `tests/host/key_behavior_lookup_test.c:214-220`

Why this matters:

- The real declaration of `noah_dispatch_synthetic_record(...)` now returns
  `bool`, but several host tests still provide local `void` definitions.
- That means those suites are compiling against stale assumptions instead of
  being forced to match the production dispatch contract.
- This is exactly the kind of interface drift the review-06 action-kind cleanup
  was supposed to reduce.

Why this is still a quality risk:

- The problem is test-only today, not a shipping firmware regression.
- But it weakens the refactor’s main promise: changing the dispatch seam should
  force consumers to update. These suites are currently able to drift silently.

Recommended direction:

- Make the affected host suites include the real dispatch dependency headers
  they are stubbing, and update the local definitions to the real signatures.
- Prefer compile-time failure over stale local prototypes for any action-kind
  dispatch dependency.

#### 2. The runtime-debug layer still exports raw core storage, and higher-level harnesses still bypass the new semantic helpers by reading `snapshot.core.key` directly

References:

- `users/noah/lib/state/runtime/runtime_debug.h:22-38`
- `tests/host/key_runtime_integration_harness.c:57-69`
- `tests/host/key_runtime_integration_harness.c:160-165`
- `tests/host/key_runtime_scenario_harness.c:239-247`
- `tests/host/runtime_debug_test.c:403-412`
- `tests/host/runtime_debug_test.c:479-484`

Why this matters:

- `noah_runtime_debug_snapshot_t` still exposes the full
  `runtime_shared_state_t core`, including raw slot-table storage and raw index
  representation.
- The new semantic debug helpers are real, but higher-level harnesses still
  read `snapshot.core.key.slots_by_position[...]` directly and still key some
  assertions off raw core/index fields.
- The integration harness also carries a weak local implementation of
  `noah_runtime_debug_snapshot(...)`, so some suites are still duplicating the
  production snapshot assembly logic instead of linking the real module.

Why this is still a maintainability problem:

- The key-runtime slot layout and index encoding are still de facto public
  contracts for host utilities.
- That makes future cleanup harder: changing slot storage, hiding index
  internals, or making the debug snapshot more semantic would still require
  touching broad test infrastructure.

Recommended direction:

- Keep the semantic debug helpers and keep extending them until the main
- harnesses no longer need `snapshot.core.key.*` for routine assertions.
- Link the real `runtime_debug.c` seam into the integration harnesses that use
  debug snapshots, instead of carrying a parallel weak implementation.

#### 3. Action-kind behavior is still spread across parallel per-kind surfaces with no completeness guard, so adding a new kind remains a drift-prone cross-cutting change

References:

- `users/noah/lib/action/action_dispatch.h:59-70`
- `users/noah/lib/action/action_dispatch.h:100-129`
- `users/noah/lib/action/action_kind.c:16-112`
- `users/noah/lib/action/action_kind_dispatch.c:156-239`

Why this matters:

- The refactor succeeded in making `action_kind.c` metadata-only, which is an
  improvement.
- But action-kind knowledge still lives in at least three separate surfaces:
  the public `NOAH_ACTION_KIND_*` enum and kind predicates in
  `action_dispatch.h`, the metadata table in `action_kind.c`, and the dispatch
  ops table in `action_kind_dispatch.c`.
- There is no compile-time or test-time completeness guard that forces those
  surfaces to stay in sync.

Why this is still a maintainability problem:

- Adding a new action kind is still a multi-file, multi-table edit.
- Because both per-kind tables are plain arrays keyed by
  `NOAH_ACTION_KIND_COUNT`, a partial update can compile cleanly and fail only
  later when behavior or metadata is queried for the missing row.

Recommended direction:

- Add a completeness guard for the per-kind tables, or move toward one internal
  per-kind definition that owns both metadata and dispatch ops while keeping the
  public/action-lifecycle split intact.
- At minimum, add a focused host assertion that every `NOAH_ACTION_KIND_*` row
  has both metadata and dispatch coverage.

### Optional Cleanup

#### 4. The review-06 progress note now overstates finality

References:

- `review/2026-04-14-review-06/progress.md:156-161`

Why this matters:

- The progress note says no immediate follow-up is required.
- The current tree is in much better shape than review-05, but the remaining
  dispatch-interface drift, raw runtime-debug exposure, and parallel action-kind
  tables mean that statement is stronger than the code supports today.

Recommended direction:

- Update the note so it reflects that the review-06 cleanup closed its original
  findings, but did not fully finish the long-term test and action-kind
  contract cleanup.

## Areas Assessed As Solid

- `users/noah/lib/action/action_kind.c` is now meaningfully metadata-only, and
  `users/noah/lib/action/action_lifecycle.c` owns preflight/intercept ordering
  cleanly through the dispatch seam.
- The key-runtime index is still genuinely mutation-maintained in production,
  and the dedicated index suite remains a worthwhile regression guard.
- The shared handled-key host fixture is much cleaner now: higher-level runtime
  suites are back on the authored lookup -> `handled_key_materialize(...)` ->
  runtime interaction seam.

## Verification

Commands run for this audit:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- full host suite passed
- firmware build passed and produced `.build/bastardkb_charybdis_4x6_noah.uf2`

Workspace scope:

- no sibling workspace folders were edited
