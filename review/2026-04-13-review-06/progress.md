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
