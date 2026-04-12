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
