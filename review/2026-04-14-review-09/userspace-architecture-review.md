# Userspace Architecture Review

Date: 2026-04-14

Status: follow-up quality audit of the refactor work landed through
`review/2026-04-14-review-08/`.

Scope:

- correctness and regression risk after the optional `runtime_debug` aggregate
  redesign
- abstraction quality and interface cleanliness in the current landed tree
- test quality, code organization, and review integrity for the refactored
  userspace runtime

Out of scope:

- fresh architecture brainstorming
- hardware changes
- upstream QMK redesign outside this repo

## Findings

### No Must-Fix Issues Found

I did not find a new correctness regression or an incomplete migration that
looks unsafe to ship. The semantic `runtime_debug` redesign is coherent with
the rest of the runtime, the full host suite is green, and the firmware build
is green.

### No Should-Fix Issues Found

I did not find another maintainability or interface problem at the same level
as the earlier review chain. The remaining issues are in optional-cleanup
territory, not “should-fix before we trust this refactor” territory.

### Optional Cleanup

#### 1. The semantic `runtime_debug` surface is cleaner now, but the main snapshot type is still monolithic and matrix-sized

References:

- `users/noah/lib/state/runtime/runtime_debug.h:22-62`
- `tests/host/key_runtime_integration_harness.h:64-70`
- `tests/host/key_runtime_scenario_harness.h:81-85`

Why this matters:

- The refactor did remove the raw `runtime_shared_state_t` leak from the public
  debug API, which was the important cleanup.
- But `noah_runtime_debug_snapshot_t` still embeds:
  - a full per-position slot snapshot table
  - ordered active-slot positions
  - ordered pending-multi-tap positions
  - every other subsystem snapshot as one large aggregate type
- That means the high-level integration and scenario harnesses still traffic in
  one broad cross-subsystem debug object, even when a given test only needs a
  small semantic subset.

Why this is only optional:

- The current shape is coherent and no longer leaks raw runtime storage types.
- The hardware is fixed, and the semantic helpers already cover the
  higher-value queries.
- The remaining bulk is mostly an API-weight / long-term cleanliness issue, not
  a correctness or regression-risk issue.

Recommended direction:

- Keep the current semantic snapshot for shipping.
- If a future cleanup is worth doing, split the “query-style semantic debug
  API” from any large aggregate snapshot type, instead of growing
  `noah_runtime_debug_snapshot_t` further.
- If low-level aggregate inspection is needed again later, add it as an
  explicit raw/low-level debug surface instead of widening the main semantic
  snapshot.

## Areas Assessed As Solid

- The optional `runtime_debug` cleanup achieved its intended design goal:
  the public debug API no longer exposes `runtime_shared_state_t` as
  `snapshot.core`.
- The new slot-level semantic queries are coherent and match how the higher
  harnesses already consume runtime state.
- The earlier action-kind invalid-kind cleanup still looks consistent between
  metadata and dispatch.
- The mutation-maintained key-runtime index still looks solid.
- The handled-key authored lookup -> materialize -> runtime interaction seam
  remains clean.

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
