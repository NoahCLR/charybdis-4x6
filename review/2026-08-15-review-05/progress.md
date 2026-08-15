# Synthetic-Key Ownership Progress

## Why This Review Exists

The prior pending-release review is closed and immutable. Finding 08 begins a
new ownership boundary and therefore uses this next sortable review folder.

## 2026-08-15 — Baseline and Strict Macro Balance

### Baseline

- Confirmed ordinary synthetic 8-bit keys called QMK register/unregister
  directly without aggregate ownership.
- Confirmed QMK's basic-key path can force a press transition for an already
  present usage, while a later unregister removes it regardless of another
  producer.
- Confirmed modifiers already had separate physical/managed counts.
- Confirmed macro hold balance treated orphan key-up as valid and playback
  could unregister before disproving local ownership.

### Completed Checkpoint

- Made orphan macro key-up invalid in authored parsing, QMK/VIA decoding, and
  IR preflight.
- Required playback to prove and consume macro-local ownership before release.
- Replaced the unsafe orphan-release fixture and added zero-side-effect cases.
- Updated `docs/KEYMAP.md` with the strict local-balance contract.
- Committed this checkpoint as `5100f2a5` (`Reject orphan macro key releases`).

## 2026-08-15 — Aggregate Ownership and Lease Closure

### Implementation

- Added compact physical and managed usage counts for basic, system, consumer,
  and mouse report domains in `owned_keycode.c`.
- Added explicit `owned_keycode_lease_t` acquisition/release. Modded actions
  prevalidate modifier capacity, share the same basic count, and release the
  exact normalized components stored at acquisition.
- Recorded physical usages in the real pre-process hook and suppressed QMK
  defaults in preflight while managed ownership keeps the usage live.
- Corrected the preflight handled-release exception to require an actual
  handled press token; a generic physical token can no longer bypass ownership
  suppression.
- Migrated held literal actions, persistent macro holds, macro chords, taps,
  abort cleanup, and PD arrow-selection Shift to scoped leases.
- Added saturation, underflow, unsupported-action, and idempotent-release
  diagnostics. Release invariant failure changes no report component.
- Added a test-reset seam without adding a production path that blindly clears
  physical ownership.
- Added source guards that confine raw QMK and unscoped owned-key mutations to
  the established action boundary.

### Tests Added or Extended

- Aggregate transition tests cover physical-first and managed-first overlap,
  two managed owners, shared basics across modded actions, mouse, consumer,
  system, idempotence, saturation, underflow, and atomic modifier failure.
- The real key-runtime scenario harness now simulates QMK default report
  handling and proves physical `KC_C` survives synthetic taps in both orders.
- Held-action tests prove per-owner leases and fail-closed acquisition.
- Macro tests prove exact lease cleanup on abort and reverse chord cleanup.
- PD, runtime debug/trace, layer-lock, VIA lifecycle, profile validation, and
  feature variants were updated to compile and exercise the new seams.

### Target Measurements

- Exact baseline commit `5100f2a5`: text 142,364 B; BSS 245,840 B;
  `noah_runtime_singleton` 19,752 B; arrow hold flag 1 B.
- Final instrumented image: text 143,524 B; BSS 245,592 B;
  `owned_keycode_state` 456 B; `noah_runtime_singleton` 20,000 B; arrow lease
  4 B.
- Explicit ownership storage increase: 707 B. The linked total BSS difference
  is reported separately because LTO can eliminate or reshape unrelated data.
- Worst reviewed main-process path: 1,808 B of a 1,920 B budget.
- Worst reviewed split-thread path: 328 B of a 768 B budget.

### Verification

- Focused ownership, modifier, held-action, macro, VIA, PD-mode, hook, scenario,
  QMK-contract, and compile-gate runners passed.
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
  passed on the final source tree.
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passed on the final source
  tree.
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`
  passed from fresh target artifacts.
- `git diff --check` passed.

## Current Status

Finding 08 is **resolved**. Aggregate ownership, scoped persistent leases,
strict macro balance, failure diagnostics, hook ordering, source boundaries,
target resource evidence, and documentation satisfy the closure bar.

This review is closed and immutable. New ownership architecture work must open
the next sortable review folder.

## Next Steps

1. Commit this Finding 08 closure as one coherent change.
2. Begin Finding 06 (combo-origin cache lifecycle) in a new review folder.
3. Keep Finding 07 dependent on this landed lease contract rather than adding
   a second macro ownership mechanism.
