# Refactor Follow-Up Review

Date: 2026-04-14

Status: follow-up quality audit of the landed key-runtime boundary and
orchestration cleanup after `review/2026-04-14-review-13/`.

Scope:

- the sealed-boundary claim after the `key_runtime_api.h` / internal-header pass
- API and compile-gate cleanliness around the remaining low-level key-runtime
  read surfaces
- host-test seam quality after the public-harness migration
- review/doc integrity after the remediation notes were added to
  `review/2026-04-14-review-13/`

## Findings

### No must-fix correctness regressions found in the current tree

The current tree still passes the full host suite and firmware compile, and I
did not find a concrete behavioral regression in the landed runtime, RGB, or
pd-mode flows.

### Should-fix: the newest review folder is now internally contradictory, so it is no longer a reliable source of truth for follow-up work

References:

- `review/2026-04-14-review-13/userspace-architecture-review.md:15`
- `review/2026-04-14-review-13/userspace-architecture-review.md:46`
- `review/2026-04-14-review-13/userspace-architecture-review.md:104`
- `review/2026-04-14-review-13/progress.md:48`
- `review/2026-04-14-review-13/progress.md:74`

Reasoning:

- The top of `review-13/userspace-architecture-review.md` now says the
  remediation landed and describes the cleaned-up structure as current fact.
- The same file then keeps the earlier findings that say those same boundary
  and host-seam problems still exist.
- `progress.md` doubles down on the "landed" story and says the active review
  note was updated to match the final seams.
- AGENTS.md says the newest review folder is the primary source of truth for
  architecture work. Right now the newest prior review folder tells two
  incompatible stories at once.

Why this matters:

- a future refactor pass can easily chase already-closed issues or miss the
  remaining real ones
- the review history now weakens, rather than improves, architecture
  traceability
- this is precisely the kind of integrity drift the review process is supposed
  to prevent

## Landed Cleanup

### Sealed index boundary

The storage-shaped bypass called out at the start of this review is now closed.

References:

- `users/noah/lib/key/runtime/key_runtime_index_internal.h`
- `tests/host/run_feature_gate_compile_tests.sh`
- `docs/KEY_RUNTIME.md:45`

What landed:

- the neutral/public-looking `key_runtime_index.h` header is gone
- the read accessors now live in `key_runtime_index_internal.h` beside the
  existing index mutation hooks
- the compile gate now rejects any future include of the removed
  `key_runtime_index.h` name in both production code and host tests
- `key_runtime_index_internal.h` is now explicitly treated as an internal-only
  seam: production includes are limited to `users/noah/lib/key/runtime/**`, and
  host-test includes are limited to the existing low-level white-box suites

Why this matters:

- the public cross-module seam is back to the narrow API documented in
  `key_runtime_api.h`, `runtime_reset.h`, and `runtime_debug.h`
- future production modules can no longer bypass the owner boundary through a
  storage-shaped header that happens to compile cleanly

### Shared host runner wiring

The reviewed public-seam runners no longer each carry a full hand-maintained
copy of the same support graph.

References:

- `tests/host/noah_source_manifest.sh`
- `tests/host/run_runtime_debug_tests.sh`
- `tests/host/run_key_runtime_modifier_hold_integration_tests.sh`

What landed:

- `tests/host/noah_source_manifest.sh` now owns one host-only shared source
  helper for the common userspace support graph used by the reviewed runners
- `run_runtime_debug_tests.sh` and
  `run_key_runtime_modifier_hold_integration_tests.sh` now consume that helper
  plus only their local deltas
- the test translation units remain explicit in each runner, so the helper
  removes duplicated support wiring without hiding which test binary is being
  built

Why this matters:

- the public-seam test cleanup now holds at the runner layer as well as at the
  harness/header layer
- future key-runtime source moves in this area have fewer hand-maintained
  runner lists to keep in sync

## Solid Areas

- I did not find a concrete semantic regression in the landed boundary cleanup.
  `users/noah/lib/action/action_dispatch.c`, `users/noah/runtime_init.c`, and
  the higher-level host harnesses are now off the old broad private headers.
- The harness cleanup did materially land at the source level.
  `tests/host/key_runtime_integration_harness.h` and
  `tests/host/key_runtime_scenario_harness.c` now stay on public seams instead
  of including `key_runtime_process_internal.h` or `key_runtime_internal.h`.
- The orchestration story is clearer than it was before this pass.
  `users/noah/runtime_init.c`, `users/noah/lib/key/runtime/key_runtime_process.c`,
  and `users/noah/lib/rgb/core/rgb_runtime.c` now make order visible in one
  local stage table each, and the host suite includes explicit order coverage in
  `tests/host/runtime_init_order_test.c` plus render-precedence coverage in
  `tests/host/rgb_layer_render_test.c`.
- The stale helper-name issue in `docs/KEY_RUNTIME.md` appears fixed in the
  current tree.

## Verification

Commands run across the audit and cleanup work recorded in this folder:

- `git status --short`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_key_runtime_index_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Current conclusion:

- no must-fix correctness regressions found in the reviewed refactor
- the index-boundary and noisy-runner follow-ups from this review are now
  landed cleanup, not open findings
- the strongest remaining non-code issue is review integrity: the newest prior
  review folder is still internally inconsistent
