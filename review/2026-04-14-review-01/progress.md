# Implementation Progress

This file tracks the architecture review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Architecture review pass

Completed in this pass:

- Started with `git status --short` and confirmed the worktree was clean.
- Confirmed the current review lineage under `review/` and opened the next
  sortable review folder:
  `review/2026-04-14-review-01/`.
- Re-read the newest prior review in
  `review/2026-04-13-review-07/` before auditing the live code.
- Reviewed the current userspace structure across:
  - hook entry points and runtime init
  - action classification and lifecycle dispatch
  - handled-key resolution, transparent-source lookup, and slot
    materialization
  - key-runtime preflight, transition planning, scans, feedback, and slot
    storage
  - pd-mode manifest, registry, state, sync, and policy helpers
  - macro dispatch, payload IR, VIA defaults, and live VIA playback
  - runtime debug, runtime trace, and the host test surface
- Wrote a new architecture review focused on long-term extensibility for the
  fixed hardware and the current software boundaries.

Key findings recorded in this review:

- high priority: key-behavior semantics are still spread across handled-key
  policy derivation, release-contract materialization, and feedback logic, so
  new hold styles still require cross-module edits
- high priority: handled-key contextual resolution still hides global
  dependencies on `layer_state` and keymap introspection behind lookup helpers
- high priority: key-runtime cross-key coordination still relies on repeated
  whole-table sweeps and broadcast interrupts rather than explicit registries
- high priority: pd-mode split sync still transports flag masks and reconstructs
  effective display identity from registry order
- medium priority: the macro subsystem shares one IR, but hardcoded, VIA
  default, and live VIA-backed sources still do not share one source/cache
  abstraction
- medium priority: a few core files are becoming mixed-responsibility
  navigation bottlenecks even though the directory layout remains strong

Areas assessed as strong in this pass:

- the authored keymap/runtime split remains real and well documented
- the slot/effect-plan runtime architecture is still the right core model
- pd mode has a legitimate command/state layer rather than scattered hook
  branching
- build wiring through `source_manifest.mk` and `rules.mk` remains explicit
- host tests plus `runtime_debug` and `runtime_trace` provide strong refactor
  support

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- full host suite passed
- firmware build passed
- final `git status --short` showed only the new review folder as an untracked
  change

Checks intentionally skipped in this pass:

- none

Workspace scope:

- no sibling workspace folders were edited
- all changes in this pass are confined to
  `charybdis-4x6/review/2026-04-14-review-01/`

Next steps:

- move hold semantics behind one materialized behavior contract so new
  interaction kinds do not require edits in handled-key, release, and feedback
  code at the same time
- make handled-key contextual resolution explicit instead of reading layer
  stack and physical lookup state implicitly inside lookup helpers
- add explicit active/pending registries to the key runtime so cross-key policy
  does not keep growing through whole-table sweeps
- move pd-mode split sync toward explicit effective mode identity instead of
  reconstructing display state from flag masks and registry order
- add a shared macro source/provider abstraction over hardcoded, VIA-default,
  and live VIA-backed macro slots
