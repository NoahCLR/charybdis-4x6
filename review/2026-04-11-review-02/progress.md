# Implementation Progress

This file tracks the review pass captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-11

Completed in this pass:

- Audited the current `noah` userspace architecture after the follow-up work
  already captured in [review-01](../2026-04-11-review-01/userspace-architecture-review.md).
- Re-read the current authoring/runtime boundary, hook surface, key runtime,
  pd-mode runtime, RGB runtime, ownership modules, docs, and host verification
  scripts.
- Wrote a new review focused on:
  - separation of concerns
  - extensibility cost
  - abstraction quality
  - state management
  - long-term scalability
  - testing/debuggability
- Identified the main remaining architectural pressure points as:
  - the handled-key runtime still being an implicit FSM
  - the hard two-slot handled-key ceiling
  - pd-mode cross-cutting policy still requiring central consumers for novel behavior
  - duplicated build-source inventories between firmware build wiring and host compile gates

Verification run in this pass:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- host suite passed
- firmware build passed

Workspace scope:

- changed only this repo
- no sibling workspace folders were modified

Recommended next implementation work:

1. Convert the handled-key runtime toward an explicit slot reducer / FSM.
2. Separate key-runtime slot-capacity policy from lifecycle policy.
3. Unify build wiring and compile-gate source manifests.
4. Add one structured scenario harness for key-runtime event traces.

## 2026-04-11 Follow-Up Implementation

Completed in this pass:

- Implemented a canonical userspace source manifest in
  [`users/noah/source_manifest.mk`](../../users/noah/source_manifest.mk).
- Rewired the firmware build to consume that manifest from
  [`users/noah/rules.mk`](../../users/noah/rules.mk).
- Rewired the host compile gate to consume the same manifest through
  [`tests/host/noah_source_manifest.sh`](../../tests/host/noah_source_manifest.sh)
  instead of maintaining a second manual userspace source inventory in
  [`tests/host/run_feature_gate_compile_tests.sh`](../../tests/host/run_feature_gate_compile_tests.sh).
- Added the first structured key-runtime scenario harness:
  - [`tests/host/key_runtime_scenario_harness.h`](../../tests/host/key_runtime_scenario_harness.h)
  - [`tests/host/key_runtime_scenario_harness.c`](../../tests/host/key_runtime_scenario_harness.c)
  - [`tests/host/key_runtime_scenario_test.c`](../../tests/host/key_runtime_scenario_test.c)
  - [`tests/host/run_key_runtime_scenario_tests.sh`](../../tests/host/run_key_runtime_scenario_tests.sh)
- Wired the new scenario suite into the default host regression pass in
  [`tests/host/run_all_host_tests.sh`](../../tests/host/run_all_host_tests.sh).
- Extended the host compile stubs to cover the broader syntax surface now
  exercised by the unified compile gate:
  - [`tests/host/include/qmk_stub.h`](../../tests/host/include/qmk_stub.h)
  - [`tests/host/include/quantum_keycodes.h`](../../tests/host/include/quantum_keycodes.h)
  - [`tests/host/include/send_string.h`](../../tests/host/include/send_string.h)

Why this pass landed first:

- it removes recurring build/compile-gate drift without changing runtime behavior
- it adds a higher-level event-trace regression surface before any handled-key
  reducer/FSM refactor

Verification run in this pass:

- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted key-runtime scenario tests passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed

## 2026-04-11 Admission Boundary Extraction

Completed in this pass:

- Split handled-key slot lookup and press-admission policy into
  [`users/noah/lib/key/key_runtime_admission.c`](../../users/noah/lib/key/key_runtime_admission.c)
  and
  [`users/noah/lib/key/key_runtime_admission.h`](../../users/noah/lib/key/key_runtime_admission.h).
- Removed the admission/reclaim helper declarations from
  [`users/noah/lib/key/key_runtime_state.h`](../../users/noah/lib/key/key_runtime_state.h)
  so the state/lifecycle header no longer also owns slot-capacity policy.
- Kept slot lifecycle mutations in
  [`users/noah/lib/key/key_runtime_slot.c`](../../users/noah/lib/key/key_runtime_slot.c)
  and updated the press/preflight/transition modules to depend on the new
  admission boundary explicitly.
- Added focused admission coverage in
  [`tests/host/key_runtime_admission_test.c`](../../tests/host/key_runtime_admission_test.c)
  and
  [`tests/host/run_key_runtime_admission_tests.sh`](../../tests/host/run_key_runtime_admission_tests.sh),
  then wired that suite into
  [`tests/host/run_all_host_tests.sh`](../../tests/host/run_all_host_tests.sh).
- Updated the canonical userspace source manifest and the affected host
  integration runners so the new module participates in both firmware builds
  and host-only link steps.
- Updated
  [`userspace-architecture-review.md`](./userspace-architecture-review.md) so
  the active review now reflects the already-landed source manifest, scenario
  harness, and admission-boundary work instead of listing them as still pending.

Why this pass landed now:

- it makes the handled-key runtime boundary more explicit without changing
  press/release behavior
- it gives future reducer/FSM work a smaller protocol surface to cut across

Verification run in this pass:

- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted admission, slot, preflight, transition, scenario, and integration
  tests passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed

## 2026-04-11 Release And Scan Boundary Extraction

Completed in this pass:

- Split release-specific slot contracts and helpers into
  [`users/noah/lib/key/key_runtime_slot_release.h`](../../users/noah/lib/key/key_runtime_slot_release.h)
  and
  [`users/noah/lib/key/key_runtime_slot_release.c`](../../users/noah/lib/key/key_runtime_slot_release.c).
- Split scan-specific slot contracts into
  [`users/noah/lib/key/key_runtime_slot_scan.h`](../../users/noah/lib/key/key_runtime_slot_scan.h)
  with the corresponding reducer/helpers now consolidated in
  [`users/noah/lib/key/key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c).
- Removed the release/scan resolution, apply, and pending-multi-tap plan types
  from
  [`users/noah/lib/key/key_runtime_state.h`](../../users/noah/lib/key/key_runtime_state.h)
  so the shared state header is narrower and less event-specific.
- Kept shared slot storage, press preparation, and slot-owned mutation helpers
  in
  [`users/noah/lib/key/key_runtime_slot.c`](../../users/noah/lib/key/key_runtime_slot.c),
  while updating transition/test runners to depend on the new event-specific
  module boundaries explicitly.
- Updated
  [`userspace-architecture-review.md`](./userspace-architecture-review.md) so
  the active review reflects the now-landed release/scan boundary work.

Why this pass landed now:

- it shrinks the oversized internal key-runtime header without changing
  behavior
- it moves the runtime closer to an event-specific reducer shape by giving
  release and scan semantics their own named contracts

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted slot, feedback, transition, scenario, and integration tests passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed

## 2026-04-12 Press And Release Reducer Collapse

Completed in this pass:

- Moved the reducer's remaining press-begin and release-resolution logic fully
  into
  [`users/noah/lib/key/key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c).
- Updated
  [`tests/host/key_runtime_slot_test.c`](../../tests/host/key_runtime_slot_test.c)
  so the focused press/release assertions now exercise the reducer seam
  directly instead of the deleted helper contracts.
- Deleted the no-longer-needed helper modules:
  - `users/noah/lib/key/key_runtime_slot_press.c`
  - `users/noah/lib/key/key_runtime_slot_press.h`
  - `users/noah/lib/key/key_runtime_slot_release.c`
  - `users/noah/lib/key/key_runtime_slot_release.h`
- Removed those files from the canonical userspace source manifest and the
  affected host runners so the build surface matches the current reducer
  architecture.
- Updated
  [`userspace-architecture-review.md`](./userspace-architecture-review.md) so
  the active review reflects that press/release helper modules are no longer
  part of the current runtime structure.

Why this pass landed now:

- it removes the last separate press/release helper boundary that the slot
  reducer still depended on
- it makes `key_runtime_slot_step.c` the single runtime-owned reducer surface
  for handled-key events, leaving the remaining architecture work focused on
  simplifying the reducer internals rather than deleting more modules

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted slot, feedback, transition, scenario, and integration tests passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed

## 2026-04-12 Phase-Local Active Release Reducer

Completed in this pass:

- Reworked active release resolution inside
  [`users/noah/lib/key/key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
  around explicit lifecycle phases instead of one larger release-condition
  pile.
- Added a reducer-local release context plus named phase handlers for:
  - `TAP_WINDOW`
  - `PRESS_HELD_WINDOW`
  - `RELEASE_HOLD_PENDING`
  - `HOLD_TIER_ACTIVE`
  - `HOLD_COMPLETE`
- Kept the shared release result contract intact while making the remaining
  release-owned-state, tap, action, and pd-mode lock-tap decisions phase-local
  inside the reducer.
- Added direct reducer-seam coverage in
  [`tests/host/key_runtime_slot_test.c`](../../tests/host/key_runtime_slot_test.c)
  for:
  - release-hold-pending release actions
  - hold-tier-active release long-hold actions
- Updated
  [`userspace-architecture-review.md`](./userspace-architecture-review.md) so
  the active review reflects that release, like scan, now reduces through
  explicit phase-local logic inside `key_runtime_slot_step.c`.

Why this pass landed now:

- it removes another implicit lifecycle branch from the handled-key reducer
  without changing the public slot-step contract
- it narrows the remaining reducer work to press setup, pending multi-tap
  paths, and the effect-helper boundary instead of both scan and release
  resolution shape

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

Verification result:

- targeted slot, feedback, transition, scenario, and integration tests passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed
- diff cleanliness checks passed

## 2026-04-12 Scenario Matrix Expansion

Completed in this pass:

- Expanded
  [`tests/host/key_runtime_scenario_test.c`](../../tests/host/key_runtime_scenario_test.c)
  from basic harness smoke coverage into a broader reducer regression matrix.
- Added scenario traces for:
  - reclaim after pending multi-tap ownership when another slot is already busy
  - interrupt-driven fallback hold activation on another key press
  - long-hold promotion after immediate-hold registration
  - two-slot contention where the primary slot is reused as overflow
- Kept the harness surface stable; the new coverage landed without changing the
  scenario harness API.
- Updated
  [`userspace-architecture-review.md`](./userspace-architecture-review.md) so
  the active review reflects that the first high-risk reducer traces now live
  in the scenario harness.

Why this pass landed now:

- it reduces the risk of the remaining pending multi-tap and effect-helper
  refactors without changing runtime behavior
- it covers the exact transition families the review called out as the best
  next traces for reducer/FSM work

Verification run in this pass:

- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted scenario tests passed
- full host suite passed
- firmware build passed

## 2026-04-12 Pending Multi-Tap Reducer Cleanup

Completed in this pass:

- Reworked the pending multi-tap release path inside
  [`users/noah/lib/key/key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
  around an explicit reducer-local context and resolution model instead of the
  old resolve-then-patch flow.
- Reworked pending multi-tap scan-hold handling in the same file around a
  named resolution step so threshold-hold and long-hold promotion are now
  explicit reducer outcomes.
- Added direct reducer-seam coverage in
  [`tests/host/key_runtime_slot_test.c`](../../tests/host/key_runtime_slot_test.c)
  for:
  - pending multi-tap threshold-hold scan dispatch
  - pending multi-tap release choosing the release long-hold action
- Kept the public slot-step and slot-result contracts unchanged while making
  the remaining pending multi-tap lifecycle logic easier to reason about
  inside the reducer.
- Updated
  [`userspace-architecture-review.md`](./userspace-architecture-review.md) so
  the active review reflects that pending multi-tap release/scan now use
  explicit local resolutions inside `key_runtime_slot_step.c`.

Why this pass landed now:

- it attacks the last large pending multi-tap condition pile after the active
  release and scan paths were already made more explicit
- it narrows the remaining handled-key reducer work further toward press setup
  and the effect-helper boundary instead of pending multi-tap release/scan
  behavior

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

Verification result:

- targeted slot tests passed
- targeted scenario tests passed
- full host suite passed
- firmware build passed
- diff cleanliness checks passed

## 2026-04-11 Slot Step Reducer Seam

Completed in this pass:

- Added a reducer-style slot-event contract in
  [`users/noah/lib/key/key_runtime_slot_step.h`](../../users/noah/lib/key/key_runtime_slot_step.h).
- Moved the public handled-key slot entry point to
  `key_runtime_slot_step(...)` in
  [`users/noah/lib/key/key_runtime_slot_result.c`](../../users/noah/lib/key/key_runtime_slot_result.c),
  so press, release, active scan, pending multi-tap scan, interrupt, and
  pending multi-tap flush now all cross one slot-event seam.
- Moved the old per-event slot-result producer declarations behind
  [`users/noah/lib/key/key_runtime_slot_result_internal.h`](../../users/noah/lib/key/key_runtime_slot_result_internal.h),
  leaving [`key_runtime_slot_result.h`](../../users/noah/lib/key/key_runtime_slot_result.h)
  focused on the shared result shape.
- Updated
  [`users/noah/lib/key/key_runtime_transition.c`](../../users/noah/lib/key/key_runtime_transition.c)
  to drive all slot orchestration through `key_runtime_slot_step(...)` instead
  of calling the fragmented per-event producers directly.
- Reworked slot and feedback host coverage to assert the reducer seam in
  [`tests/host/key_runtime_slot_test.c`](../../tests/host/key_runtime_slot_test.c)
  and
  [`tests/host/key_runtime_feedback_test.c`](../../tests/host/key_runtime_feedback_test.c).
- Updated
  [`userspace-architecture-review.md`](./userspace-architecture-review.md) so
  the active review reflects the new slot-step boundary.

Why this pass landed now:

- it gives the handled-key runtime one explicit transition-facing slot-event
  contract instead of several event-specific entry points
- it narrows the remaining reducer/FSM work to the duplicated internal
  press/release/scan state logic instead of both API and implementation work

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted slot, feedback, transition, scenario, and integration tests passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed

## 2026-04-11 Slot Event Wrapper Extraction

Completed in this pass:

- Added a handled-press slot-event wrapper in
  [`users/noah/lib/key/key_runtime_slot_press.h`](../../users/noah/lib/key/key_runtime_slot_press.h)
  and
  [`users/noah/lib/key/key_runtime_slot_press.c`](../../users/noah/lib/key/key_runtime_slot_press.c)
  so transition orchestration no longer needs to build raw press-plan inputs by
  hand.
- Added a handled-release slot-event wrapper in
  [`users/noah/lib/key/key_runtime_slot_release.h`](../../users/noah/lib/key/key_runtime_slot_release.h)
  and
  [`users/noah/lib/key/key_runtime_slot_release.c`](../../users/noah/lib/key/key_runtime_slot_release.c)
  so pending multi-tap release, active release, and unmatched cleanup now come
  back through one release-event contract.
- Updated
  [`users/noah/lib/key/key_runtime_transition.c`](../../users/noah/lib/key/key_runtime_transition.c)
  to consume the new handled press/release slot-event wrappers instead of
  coordinating the lower-level helper calls directly.
- Extended slot-level host coverage in
  [`tests/host/key_runtime_slot_test.c`](../../tests/host/key_runtime_slot_test.c)
  to exercise the new event-oriented wrappers.
- Updated
  [`userspace-architecture-review.md`](./userspace-architecture-review.md) so
  the active review reflects the new slot-event wrapper layer.

Why this pass landed now:

- it is the first step from separated helper modules toward an explicit
  slot-event API
- it reduces event orchestration inside `key_runtime_transition.c` without
  changing runtime behavior

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted slot, feedback, transition, scenario, and integration tests passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed

## 2026-04-11 Scan Event Contract Extraction

Completed in this pass:

- Replaced the public scan resolution/apply structs in
  [`users/noah/lib/key/key_runtime_slot_scan.h`](../../users/noah/lib/key/key_runtime_slot_scan.h)
  with named slot-event contracts for active scan effects and pending
  multi-tap scan effects/flushes.
- Moved the old scan resolution/apply details private to
  [`users/noah/lib/key/key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
  so the scan module now exposes an event-shaped surface instead of a leaky
  internal protocol.
- Updated
  [`users/noah/lib/key/key_runtime_transition.c`](../../users/noah/lib/key/key_runtime_transition.c)
  to consume the new scan events directly rather than coordinating scan
  resolution/apply steps itself.
- Reworked slot-level host coverage in
  [`tests/host/key_runtime_slot_test.c`](../../tests/host/key_runtime_slot_test.c)
  so it validates the new scan event contracts and the same state mutations
  through the public API.
- Updated
  [`userspace-architecture-review.md`](./userspace-architecture-review.md) so
  the active review reflects the now-landed scan event boundary.

Why this pass landed now:

- it removes another leaky internal protocol from the handled-key public
  surface
- it leaves `key_runtime_transition.c` closer to an event consumer than a
  transition coordinator

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

Verification result:

- targeted slot, transition, scenario, and integration tests passed
- feature-gate compile checks passed

## 2026-04-11 Direct Slot Result Producers

Completed in this pass:

- Reworked press, release, and scan to build
  [`key_runtime_slot_result_t`](../../users/noah/lib/key/key_runtime_slot_result.h)
  directly in their owning modules instead of translating through separate
  internal press/release/scan event-plan structs:
  - [`key_runtime_slot_press.c`](../../users/noah/lib/key/key_runtime_slot_press.c)
  - [`key_runtime_slot_release.c`](../../users/noah/lib/key/key_runtime_slot_release.c)
  - scan reducer logic that now lives in [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
- Added a single shared internal result-builder helper header in
  [`key_runtime_slot_result_internal.h`](../../users/noah/lib/key/key_runtime_slot_result_internal.h)
  and reduced
  [`key_runtime_slot_result.c`](../../users/noah/lib/key/key_runtime_slot_result.c)
  to shared result-builder helpers plus the remaining direct result producers
  that are not owned by press/release/scan.
- Deleted the no-longer-needed internal cross-module protocol headers:
  - `key_runtime_slot_press_internal.h`
  - `key_runtime_slot_release_internal.h`
  - `key_runtime_slot_scan_internal.h`
- Kept the public slot surfaces narrow while removing another layer of internal
  indirection:
  - [`key_runtime_slot_press.h`](../../users/noah/lib/key/key_runtime_slot_press.h)
  - [`key_runtime_slot_release.h`](../../users/noah/lib/key/key_runtime_slot_release.h)
  - [`key_runtime_slot_scan.h`](../../users/noah/lib/key/key_runtime_slot_scan.h)
- Updated host coverage and runner wiring that depended on the shared result
  surface and its direct producers:
  - [`tests/host/key_runtime_slot_test.c`](../../tests/host/key_runtime_slot_test.c)
  - [`tests/host/key_runtime_feedback_test.c`](../../tests/host/key_runtime_feedback_test.c)
  - [`tests/host/run_key_runtime_feedback_tests.sh`](../../tests/host/run_key_runtime_feedback_tests.sh)
- Updated
  [`userspace-architecture-review.md`](./userspace-architecture-review.md) so
  the active review reflects the removal of the internal event-plan headers.

Why this pass landed now:

- it removes the last cross-module press/release/scan result protocols instead
  of merely hiding them
- it leaves the next handled-key reducer step focused on consolidating
  implementation logic and state transitions, not on another protocol rewrite

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted slot, feedback, transition, scenario, and integration tests passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed

## 2026-04-11 Internal Slot Protocol Header Narrowing

Completed in this pass:

- Moved the old press/release/scan event and apply structs behind new internal
  headers:
  - [`key_runtime_slot_press_internal.h`](../../users/noah/lib/key/key_runtime_slot_press_internal.h)
  - [`key_runtime_slot_release_internal.h`](../../users/noah/lib/key/key_runtime_slot_release_internal.h)
  - [`key_runtime_slot_scan_internal.h`](../../users/noah/lib/key/key_runtime_slot_scan_internal.h)
- Narrowed the public headers so they expose only the remaining externally
  useful helpers:
  - [`key_runtime_slot_press.h`](../../users/noah/lib/key/key_runtime_slot_press.h)
  - [`key_runtime_slot_release.h`](../../users/noah/lib/key/key_runtime_slot_release.h)
  - [`key_runtime_slot_scan.h`](../../users/noah/lib/key/key_runtime_slot_scan.h)
- Rewired the internal module includes so
  [`key_runtime_slot_result.c`](../../users/noah/lib/key/key_runtime_slot_result.c),
  [`key_runtime_slot_press.c`](../../users/noah/lib/key/key_runtime_slot_press.c),
  [`key_runtime_slot_release.c`](../../users/noah/lib/key/key_runtime_slot_release.c),
  and
  [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
  consume those internal headers directly.
- Reworked host coverage so the slot and feedback tests assert the shared
  slot-result surface instead of the deprecated local event structs:
  - [`tests/host/key_runtime_slot_test.c`](../../tests/host/key_runtime_slot_test.c)
  - [`tests/host/key_runtime_feedback_test.c`](../../tests/host/key_runtime_feedback_test.c)
- Updated the feedback runner wiring in
  [`run_key_runtime_feedback_tests.sh`](../../tests/host/run_key_runtime_feedback_tests.sh)
  so the result-layer dependencies are linked explicitly.
- Updated
  [`userspace-architecture-review.md`](./userspace-architecture-review.md) so
  the active review reflects the narrower public boundary.

Why this pass landed now:

- it makes the shared slot-result layer the real public seam for slot events
  instead of just one adapter sitting next to the old public protocols
- it reduces the next reducer pass to internal implementation work rather than
  another public-header reshuffle

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted slot, feedback, transition, and scenario tests passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed

## 2026-04-11 Shared Slot Result Surface

Completed in this pass:

- Added a shared handled-key slot-result module in
  [`users/noah/lib/key/key_runtime_slot_result.h`](../../users/noah/lib/key/key_runtime_slot_result.h)
  and
  [`users/noah/lib/key/key_runtime_slot_result.c`](../../users/noah/lib/key/key_runtime_slot_result.c)
  so press, release, scan, interrupt, and pending multi-tap flush paths all
  adapt into one common result shape before transition planning.
- Rewired
  [`users/noah/lib/key/key_runtime_transition.c`](../../users/noah/lib/key/key_runtime_transition.c)
  to consume that shared slot-result surface instead of separately translating
  `key_runtime_slot_press_plan_t`, `key_runtime_slot_release_event_t`, and
  `key_runtime_slot_scan_event_t`.
- Kept the existing press/release/scan local helper protocols intact under the
  new adapter layer so this pass changes the architectural boundary without
  trying to collapse the reducer internals in the same step.
- Wired the new module into the canonical userspace build manifest in
  [`users/noah/source_manifest.mk`](../../users/noah/source_manifest.mk) and the
  affected host runners:
  - [`run_key_runtime_slot_tests.sh`](../../tests/host/run_key_runtime_slot_tests.sh)
  - [`run_key_runtime_transition_tests.sh`](../../tests/host/run_key_runtime_transition_tests.sh)
  - [`run_key_runtime_scenario_tests.sh`](../../tests/host/run_key_runtime_scenario_tests.sh)
  - [`run_key_runtime_modifier_hold_integration_tests.sh`](../../tests/host/run_key_runtime_modifier_hold_integration_tests.sh)
  - [`run_pd_mode_key_runtime_integration_tests.sh`](../../tests/host/run_pd_mode_key_runtime_integration_tests.sh)
- Added direct host coverage in
  [`tests/host/key_runtime_slot_test.c`](../../tests/host/key_runtime_slot_test.c)
  for the new shared slot-result adapter paths so the new boundary is tested
  independently of the transition planner.
- Updated
  [`userspace-architecture-review.md`](./userspace-architecture-review.md) so
  the active review reflects the new slot-result layer.

Why this pass landed now:

- it removes the last major place where `key_runtime_transition.c` had to know
  several different handled-key result structs
- it makes the remaining handled-key reducer work narrower: the next pass can
  collapse the local press/release/scan protocols behind one shared slot-result
  surface instead of also changing the transition layer again

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted slot, transition, scenario, and integration tests passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed

## 2026-04-11 Slot Lifecycle Phase Extraction

Completed in this pass:

- Replaced the handled-key slot's old hold/strategy booleans in
  [`users/noah/lib/state/runtime_shared_state.h`](../../users/noah/lib/state/runtime_shared_state.h)
  with explicit `phase` and `hold_strategy` enums.
- Added phase/strategy helper predicates and transitions in
  [`users/noah/lib/key/key_runtime_state.h`](../../users/noah/lib/key/key_runtime_state.h)
  and
  [`users/noah/lib/key/key_runtime_slot.c`](../../users/noah/lib/key/key_runtime_slot.c)
  so the runtime uses semantic slot-state helpers instead of direct flag
  combinations.
- Updated press, scan, release, effect, and feedback paths to consume the new
  lifecycle model:
  - [`key_runtime_slot_press.c`](../../users/noah/lib/key/key_runtime_slot_press.c)
  - [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
  - [`key_runtime_slot_release.c`](../../users/noah/lib/key/key_runtime_slot_release.c)
  - [`key_runtime_slot_effect.c`](../../users/noah/lib/key/key_runtime_slot_effect.c)
  - [`key_runtime_feedback.c`](../../users/noah/lib/key/key_runtime_feedback.c)

## 2026-04-11 Slot Reducer Implementation Consolidation

Completed in this pass:

- Added the consolidated slot reducer implementation in
  [`users/noah/lib/key/key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c).
- Moved handled press, handled release, active scan, pending multi-tap scan,
  interrupt, and pending multi-tap flush reduction into that one file so
  `key_runtime_slot_step(...)` now has a real implementation boundary instead
  of dispatching back out to fragmented per-event producers.
- Reduced
  [`users/noah/lib/key/key_runtime_slot_result.c`](../../users/noah/lib/key/key_runtime_slot_result.c)
  to shared result-builder helpers only.
- Reduced
  [`users/noah/lib/key/key_runtime_slot_press.c`](../../users/noah/lib/key/key_runtime_slot_press.c)
  to the focused press-begin helper and
  [`users/noah/lib/key/key_runtime_slot_release.c`](../../users/noah/lib/key/key_runtime_slot_release.c)
  to release resolution helpers.
- Deleted the no-longer-needed reducer implementation file
  `users/noah/lib/key/key_runtime_slot_scan.c`.
- Updated the canonical userspace source manifest and the affected host
  runners to compile `key_runtime_slot_step.c` instead of the removed scan
  reducer file.
- Updated
  [`userspace-architecture-review.md`](./userspace-architecture-review.md) so
  the active review reflects the consolidated reducer implementation boundary.

Why this pass landed now:

- it turns the slot-step API into a real reducer implementation boundary, not
  just a wrapper contract
- it narrows the remaining handled-key architecture work to the internal
  phase/event logic inside that reducer instead of both reducer API and
  reducer implementation shape

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted slot, feedback, transition, scenario, and integration tests passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed

## 2026-04-11 Phase-Local Scan Reducer

Completed in this pass:

- Replaced the active-scan `resolution/apply` mini-protocol inside
  [`users/noah/lib/key/key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
  with explicit phase-local reducer branches for:
  - `TAP_WINDOW`
  - `PRESS_HELD_WINDOW`
  - `RELEASE_HOLD_PENDING`
  - `HOLD_TIER_ACTIVE`
- Replaced the pending-multi-tap scan `resolution/apply` mini-protocol in the
  same file with direct reducer logic that mutates slot state and emits the
  shared slot-result surface in one step.
- Added direct slot coverage for the previously implicit scan phases in
  [`tests/host/key_runtime_slot_test.c`](../../tests/host/key_runtime_slot_test.c).
- Updated
  [`userspace-architecture-review.md`](./userspace-architecture-review.md) so
  the active review reflects that scan now has explicit phase-local reducer
  logic inside `key_runtime_slot_step.c`.

Why this pass landed now:

- it turns the scan portion of the handled-key reducer into explicit
  phase-local logic instead of another resolution/apply helper protocol
- it narrows the remaining reducer work to the press/release side and any
  future tighter phase/event core extraction

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted slot, feedback, transition, scenario, and integration tests passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed
- Kept backward-compatible behavior for manually constructed active slot
  fixtures by normalizing `phase == IDLE` plus `keycode != KC_NO` to the
  effective tap window in the shared slot helpers.
- Reworked the affected host coverage to assert the new lifecycle/strategy
  state instead of the removed booleans.
- Updated
  [`userspace-architecture-review.md`](./userspace-architecture-review.md) so
  the active review reflects the new lifecycle-state model.

Why this pass landed now:

- it is the first pass that changes the handled-key slot storage shape rather
  than only reshaping module boundaries
- it moves the runtime closer to an actual reducer by making lifecycle state a
  named enum instead of an implicit boolean protocol

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted slot, transition, feedback, preflight, scenario, and integration
  tests passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed

## 2026-04-11 Press And Effect Boundary Extraction

Completed in this pass:

- Split slot effect-request contracts and mutation helpers into
  [`users/noah/lib/key/key_runtime_slot_effect.h`](../../users/noah/lib/key/key_runtime_slot_effect.h)
  and
  [`users/noah/lib/key/key_runtime_slot_effect.c`](../../users/noah/lib/key/key_runtime_slot_effect.c).
- Split press-specific slot planning helpers into
  [`users/noah/lib/key/key_runtime_slot_press.h`](../../users/noah/lib/key/key_runtime_slot_press.h)
  and
  [`users/noah/lib/key/key_runtime_slot_press.c`](../../users/noah/lib/key/key_runtime_slot_press.c).
- Removed the effect-request and press-plan types from
  [`users/noah/lib/key/key_runtime_state.h`](../../users/noah/lib/key/key_runtime_state.h)
  so the shared state header is now closer to pure storage plus shared slot
  helpers.
- Updated transition/runtime wiring and host runners to consume the new press
  and effect boundaries explicitly.

Why this pass landed now:

- it removes the last large event/protocol chunk from the shared state header
- it leaves the remaining handled-key core closer to an explicit slot-event
  surface instead of one mixed internal protocol

Verification run in this pass:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted slot, feedback, transition, scenario, and integration tests passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed
