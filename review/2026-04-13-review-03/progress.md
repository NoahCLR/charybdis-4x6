# Implementation Progress

This file tracks the architecture review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-13

Completed in this pass:

- Started with `git status --short` and confirmed the worktree was clean
  before opening a new review.
- Confirmed the active review lineage under `review/` and created the next
  sortable same-day review folder:
  `review/2026-04-13-review-03/`.
- Re-read the newest existing architecture review in
  `review/2026-04-13-review-02/` to avoid repeating already-landed contract
  work.
- Reviewed the live userspace structure and key runtime entrypoints,
  including:
  - `users/noah/noah_runtime.h`, `runtime_init.c`, and `hooks.c`
  - key interaction, key runtime, slot, transition, and feedback modules
  - action dispatch/lifecycle and owned-keycode surfaces
  - pd-mode defs, snapshot, state, lifecycle, policy, and runtime files
  - macro dispatch, payload, and VIA-default seeding modules
  - runtime shared-state, debug, trace, and split-sync surfaces
  - keymap authoring/materialization surfaces
  - host test runners, debug tests, pd-mode tests, and key-runtime scenario
    harnesses
- Wrote a new review focused on the remaining software-architecture bottlenecks
  after the earlier contract cleanup work.

Key findings recorded in this review:

- `noah_action_desc_t` is still a boolean-heavy descriptor rather than a
  durable action-family contract
- pd-mode read-side state is centralized through `pd_mode_snapshot()`, but the
  pd-mode write path and split-sync ownership are still fragmented
- key interaction is resolved centrally, but hold/release/feedback policy is
  still interpreted in several runtime reducers
- hardcoded macros are still runtime-string-interpreted instead of compiled as
  static authored data
- the host suite is strong, but too much integration coverage still depends on
  raw storage layout

Verification run in this pass:

- `git status --short`

Checks intentionally skipped in this pass:

- host test runners
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Why they were skipped:

- this pass only adds internal review documents under `review/`
- no runtime code, source manifests, authored keymap data, tests, or build
  wiring changed

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to `charybdis-4x6/review/2026-04-13-review-03/`

Next steps:

- if implementation work starts from this review, prioritize the action-family
  contract first because it affects key interaction, validation, lifecycle,
  and future feature growth
- next, centralize pd-mode writes behind a single command/apply seam so mode
  lifecycle, sync, and state mutation stop being split across modules
- after that, compute resolved hold policy once per slot interaction and move
  more integration tests onto semantic builders instead of raw storage setup
