# Implementation Progress

This file tracks the architecture review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-13

### Architecture review pass

Completed in this pass:

- Started with `git status --short` and confirmed the worktree was clean.
- Confirmed the current review lineage under `review/` and opened the next
  sortable same-day review folder:
  `review/2026-04-13-review-07/`.
- Re-read the newest prior architecture review in
  `review/2026-04-13-review-06/` before auditing the live code.
- Reviewed the current userspace structure across:
  - hook entry points and runtime init
  - handled-key resolution, slot lifecycle, transition planning, and shared
    state ownership
  - action classification, lifecycle dispatch, and authored validation
  - pd-mode manifest, registry, state, lifecycle, sync, and policy helpers
  - macro dispatch, payload IR, VIA defaults, and QMK playback compatibility
  - RGB stage orchestration, runtime-debug surfaces, and host-test fixtures
- Wrote a new architecture review focused on long-term extensibility and
  software boundaries for the current fixed hardware.

Key findings recorded in this review:

- high priority: action semantics are still hard-coded in one classifier and
  then reinterpreted by lifecycle dispatch, authored validation, and preflight
  routing, so new action kinds still require cross-module edits
- high priority: pd-mode identity and remote-display precedence still depend
  partly on manifest order and shared policy helpers instead of a fully
  explicit mode-owned descriptor/sync contract
- medium priority: key-runtime cross-key coordination still works by repeated
  whole-table sweeps, which keeps active/pending ownership implicit
- medium priority: the macro subsystem shares one IR, but hardcoded macros,
  VIA defaults, and live VIA playback still do not share one source/cache
  abstraction

Areas assessed as strong in this pass:

- the authored keymap/runtime split remains real and understandable
- the handled-key runtime keeps clear slot/effect boundaries
- QMK compatibility work is mostly centralized under `users/noah/lib/compat/`
- host tests, `runtime_debug`, and `runtime_trace` provide strong architecture
  feedback loops

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
  `charybdis-4x6/review/2026-04-13-review-07/`

Next steps:

- move action semantics behind one action-ops surface so validation,
  preflight, and lifecycle dispatch share one contract
- make pd-mode display precedence and split-sync identity explicit instead of
  deriving them from manifest order
- add explicit active/pending registries to the key runtime so new cross-key
  rules do not require more whole-table sweeps
- add a shared macro source/cache abstraction over hardcoded, VIA-default, and
  live VIA-backed macro payloads
