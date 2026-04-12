# Implementation Progress

This file tracks the review pass captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-12

Completed in this pass:

- Audited the current `noah` userspace after the follow-up work already landed
  in
  [2026-04-12-review-01](../2026-04-12-review-01/userspace-architecture-review.md).
- Re-read the authored/runtime split, handled-key runtime flow, pd-mode
  registry and handlers, ownership/state modules, RGB runtime, host scenario
  harness, and current maintainer docs.
- Wrote a new review focused on current-state software architecture rather than
  repeating already-completed recommendations.
- Identified the main remaining architecture risks as:
  - action emission still carrying hidden key-runtime mutation policy
  - pd-mode lifecycle extensibility still depending on registry-owned special
    cases for unusual modes
  - handled-key effect interfaces still exposing overlapping request/result/
    transition layers
  - scenario testing still mirroring runtime contracts instead of consuming the
    shared debug/effect surfaces directly

Verification run in this pass:

- `git status --short`

Verification intentionally not run in this pass:

- no host tests
- no firmware compile

Reason:

- this pass only added review documentation under `review/`
- no runtime, keymap, compat, or build-surface source files changed

Workspace scope:

- changed only this repo
- no sibling workspace folders were modified

Recommended next implementation work:

1. Make action-emission policy explicit and remove ad hoc fallback-hold
   activation from pd-mode output helpers.
2. Move pd-mode lifecycle hooks into the mode definition surface so unusual
   modes stop requiring registry switch edits.
3. Rebuild the scenario harness on `key_runtime_effect_t` and
   `noah_runtime_reset_for_test()` before adding broader multi-subsystem
   scripted scenarios.

## 2026-04-12 follow-up: explicit action emission

Completed in this follow-up:

- Added explicit emission-policy helpers in
  `users/noah/lib/action/action_dispatch.h` /
  `users/noah/lib/action/action_dispatch.c`:
  - `noah_emit_action_tap()`
  - `noah_emit_synthetic_qmk_tap()`
  - `noah_emit_literal_tap()`
- Made fallback-hold settlement and temporary modifier suspension caller-owned
  policy instead of leaving those semantics implicit in scattered output paths.
- Routed the main runtime-owned tap emitters through that explicit surface:
  - handled-key transition execution
  - direct action taps in preflight
  - held-repeat dispatch
  - delayed-action replay
  - pd-mode synthetic taps and arrow shortcuts
- Kept `action_dispatch()` as a compatibility wrapper with the historical
  runtime-default policy so the pass stayed behavior-preserving.
- Added a new dedicated host runner,
  `sh tests/host/run_action_dispatch_tests.sh`, for the explicit emit-policy
  seam.
- Updated the affected host harnesses and the layer-lock integration runner for
  the new output surface.
- Updated `docs/KEY_RUNTIME.md` so the maintainer map points at the new output
  seam instead of the older hidden-coupling model.

Verification run in this follow-up:

- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted emission, pd-mode, and runtime integration tests passed
- feature-gate compile tests passed
- full host suite passed
- firmware build passed

Workspace scope:

- changed only this repo
- no sibling workspace folders were modified

Recommended next implementation work:

1. Move pd-mode lifecycle hooks into the mode definition surface so unusual
   modes stop requiring registry switch edits.
2. Rebuild the scenario harness on `key_runtime_effect_t` and
   `noah_runtime_reset_for_test()` so higher-level scenarios stop mirroring
   production effect/reset contracts.
3. Collapse the remaining handled-key request/result/transition naming overlap
   now that the executable effect path is no longer the main coupling risk.

## 2026-04-12 follow-up: pd-mode lifecycle ownership

Completed in this follow-up:

- Added an optional lifecycle pointer to `pd_mode_def_t` in
  `users/noah/lib/pointing/pd_modes.h`.
- Moved pd-mode lifecycle ownership onto the mode definition row by extending
  `NOAH_PD_MODE_LIST(...)` with a lifecycle argument in
  `users/noah/lib/pointing/pd_mode_manifest.h`.
- Removed the registry-owned per-mode lifecycle switch in
  `users/noah/lib/pointing/pd_mode_registry.c` and replaced it with row-owned
  lifecycle lookup through each mode definition.
- Kept existing activation, deactivation, lock, and unlock ordering intact
  while moving:
  - dragscroll's auto-mouse lock ownership
  - pinch mode's activate/deactivate GUI ownership
  - pinch mode's lock/unlock auto-mouse ownership
  onto mode-owned lifecycle data.
- Simplified `pd_mode_deactivate()` to use direct mode lookup for reset hooks
  instead of re-scanning the registry table.
- Updated `docs/ADDING_PD_MODE.md` and `docs/KEY_RUNTIME.md` so they no longer
  direct contributors to add registry switch cases for unusual mode side
  effects.
- Updated the pd-mode metadata tests and manifest-expansion host fixtures for
  the new row shape.

Verification run in this follow-up:

- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- targeted pd-mode and profile/integration tests passed
- feature-gate compile tests passed
- full host suite passed
- firmware build passed

Workspace scope:

- changed only this repo
- no sibling workspace folders were modified

Recommended next implementation work:

1. Rebuild the scenario harness on `key_runtime_effect_t` and
   `noah_runtime_reset_for_test()` so higher-level scenarios stop mirroring
   production effect/reset contracts.
2. Collapse the remaining handled-key request/result/transition naming overlap
   so there is one obvious executable effect vocabulary.

## 2026-04-12 follow-up: scenario harness shared runtime contracts

Completed in this follow-up:

- Rebuilt `tests/host/key_runtime_scenario_harness.h` around the shared
  `key_runtime_effect_t` contract instead of maintaining a parallel
  scenario-only effect enum and payload layout.
- Reworked `tests/host/key_runtime_scenario_harness.c` to log real runtime
  effect payloads for:
  - action dispatch
  - delayed action replay
  - held action register/unregister
  - repeat start
  - owned-state release
  - layer press/release
  - feedback pulse
  - pd-mode lock taps
- Switched `key_runtime_scenario_reset()` to the production
  `noah_runtime_reset_for_test()` seam and linked
  `users/noah/lib/state/runtime_debug.c` into
  `sh tests/host/run_key_runtime_scenario_tests.sh`.
- Updated `tests/host/key_runtime_scenario_test.c` so its assertions now read
  the real shared effect payload union instead of flattened harness-only
  fields.
- Updated `docs/KEY_RUNTIME.md` so the maintainer guidance for scripted
  scenarios points at the shared effect/reset surfaces.

Verification run in this follow-up:

- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- scenario harness tests passed on the shared effect/reset surface
- runtime debug tests passed
- full host suite passed
- firmware build passed

Workspace scope:

- changed only this repo
- no sibling workspace folders were modified

Recommended next implementation work:

1. Collapse the remaining handled-key request/result/transition naming overlap
   so there is one obvious executable effect vocabulary.

## 2026-04-12 follow-up: handled-key effect vocabulary cleanup

Completed in this follow-up:

- Renamed the reducer-local slot-policy surface in
  `users/noah/lib/key/key_runtime_slot_effect.h` to
  `key_runtime_effect_builder_t` so it no longer reads like another
  executable effect protocol.
- Removed the duplicated executable-effect aliases from:
  - `users/noah/lib/key/key_runtime_slot_result.h`
  - `users/noah/lib/key/key_runtime_transition.h`
- Updated the slot-result builders, transition execution, and trace helpers so
  slot results and transition plans now carry raw `key_runtime_effect_t`
  payloads directly.
- Updated the slot and transition host tests to assert `KEY_RUNTIME_EFFECT_*`
  directly instead of slot-result or transition-specific re-export macros.
- Updated `docs/KEY_RUNTIME.md` and the active review so the maintainer-facing
  architecture notes now match the simplified effect surface.

Verification run in this follow-up:

- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification result:

- slot and transition tests passed on the shared effect vocabulary
- feature-gate compile tests passed
- full host suite passed
- firmware build passed

Workspace scope:

- changed only this repo
- no sibling workspace folders were modified

Recommended next implementation work:

1. No remaining required follow-up items remain from review-02.
2. If another bespoke pd mode lands, consider splitting
   `users/noah/lib/pointing/pd_mode_handlers.c` into per-mode files.
