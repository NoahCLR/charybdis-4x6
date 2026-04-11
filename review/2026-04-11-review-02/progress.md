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
- Split scan-specific slot contracts and helpers into
  [`users/noah/lib/key/key_runtime_slot_scan.h`](../../users/noah/lib/key/key_runtime_slot_scan.h)
  and
  [`users/noah/lib/key/key_runtime_slot_scan.c`](../../users/noah/lib/key/key_runtime_slot_scan.c).
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
  [`users/noah/lib/key/key_runtime_slot_scan.c`](../../users/noah/lib/key/key_runtime_slot_scan.c)
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
  - [`key_runtime_slot_scan.c`](../../users/noah/lib/key/key_runtime_slot_scan.c)
  - [`key_runtime_slot_release.c`](../../users/noah/lib/key/key_runtime_slot_release.c)
  - [`key_runtime_slot_effect.c`](../../users/noah/lib/key/key_runtime_slot_effect.c)
  - [`key_runtime_feedback.c`](../../users/noah/lib/key/key_runtime_feedback.c)
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
