# Progress

## 2026-04-23

### Review Opened

- Opened `review/2026-04-23-review-04/` for an RGB runtime and authored RGB
  profile architecture review.
- Did not continue `review/2026-04-23-review-03/` because that folder records a
  closure verdict and is immutable history. This pass is a distinct
  post-closure RGB review.
- Used `prompts/initial-architecture-review.md`.

### Scope

- Reviewed `users/noah/lib/rgb/**`, RGB-related split sync, key-runtime
  feedback surfaces, the authored `rgb_config.c`, the keymap's visible layer
  data, RGB docs, and relevant upstream Charybdis RGB defaults.
- No runtime behavior, keymap data, generated introspection output, or normal
  documentation was changed.

### Completed Passes

- Checked the newest review folder and confirmed it was closed.
- Read RGB runtime orchestration, layer, auto-mouse, preview, PD-mode, combo,
  key-feedback, defaults, helper, and validation modules.
- Checked how the RGB renderer consumes key-runtime semantic feedback.
- Checked split-sync transport for auto-mouse, PD display state, preview,
  combo locality, and key-feedback semantic maps.
- Compared authored PD-mode color rows with the profile comments and
  `RGB_PD_MODE_ACTIVE_HALF_ENABLE` config.
- Checked current keymap layers and upstream default keymap RGB Matrix control
  placement.

### Initial Findings

- No `must-fix` findings found.
- One `should-fix` finding was recorded: PD-mode RGB locality was internally
  inconsistent because comments/config imply trigger-half feedback while the
  authored rows all render on the right half.
- Two `optional cleanup` recommendations recorded:
  - decide whether to add or document normal RGB Matrix control keys
  - make key-feedback semantic paint priority explicit if new semantics are
    added later

Reconciliation note: the PD-mode comment/config mismatch was resolved in the
implementation follow-up below. The current architecture review now records no
open `should-fix` findings.

### Verification

Passed:

- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `git diff --check`

Skipped under the review-note-only exception:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Runtime/build behavior was not changed in this pass.

### Implementation Follow-Up

- Updated `rgb_config.c` so the PD-mode color comment reflects the current
  fixed pointer-half policy and does not mention backend debug defines.
- Added `KEY_FEEDBACK_MODE_LEFT_HALF` and `KEY_FEEDBACK_MODE_RIGHT_HALF`.
- Updated the key-feedback renderer to paint fixed left/right halves with the
  same global semantic priority used by both-halves mode.
- Updated RGB validation, profile introspection, `rgb_config.c` comments,
  README, `docs/RGB_CONFIG.md`, and the generated keymap overview.
- Extended `tests/host/run_rgb_layer_render_tests.sh` and
  `tests/host/rgb_layer_render_test.c` with fixed left/right key-feedback
  variants.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped for the implementation follow-up.

### Next Steps

1. Choose whether ordinary RGB Matrix controls should be reachable from an
   authored physical key path or intentionally left to VIA remapping.
2. If new key-feedback semantic states are added later, make feedback paint
   priority explicit instead of relying on enum ordering.
3. Keep the staged RGB runtime; this pass did not identify a reason to rewrite
   the pipeline.

## 2026-04-24

### PD Trigger-Key RGB Follow-Up

- Added `PD_COLOR_MODE_TRIGGER_KEYS` so pd-mode overlays can paint the exact
  key footprint that triggered the current effective PD mode.
- Extended the pd-mode owner contract from side-only display state to include
  an owner bitmap for RGB display consumers.
- Mirrored the pd-mode owner bitmap through the base split-runtime sync packet
  so the slave half can render exact trigger-key placement.
- Updated RGB validation, the pd-mode renderer, authored RGB comments, README,
  RGB docs, profile introspection, and the generated keymap overview.
- Extended pd-mode, split-sync, and RGB render host coverage for direct,
  combo-footprint, fallback, and remote exact-key behavior.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_rgb_validation_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

### PD Held-Action Projection Follow-Up

- Reproduced the crash path as a key-runtime/PD projection mismatch, not split
  sync: the physical `PINCH_MODE` key could project Pinch immediately while its
  authored double-hold branch registered `ZOOM_MODE`.
- Updated the key-runtime core so direct and implicit PD holds still attach PD
  leases on press, while explicit held-action PD branches attach and release
  their PD projection when the held action actually registers/unregisters.
- Added focused host regressions for `PINCH_MODE` double-tap hold into
  `ZOOM_MODE` and for the user-visible Volume -> Pinch/Zoom -> Volume
  alternation.
- Updated the runtime debug fixture so handled-key PD tests resolve
  `pd_mode_for_keycode()` instead of treating PD keycodes as non-PD actions.
- Regenerated `docs/KEYMAP-OVERVIEW.md` after the full host suite found stale
  generated PD color rows from the previous trigger-key RGB follow-up.

Verification passed:

- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

### Next Steps

1. Flash and try the Volume/Pinch/Zoom alternation on hardware.
2. Keep explicit held-action PD branches covered as more `.mode` key behavior
   options are added.

### Pinch Double-Tap Salvo Follow-Up

- After hardware testing narrowed the repro to repeated `PINCH_MODE`
  double-tap salvos, tried moving the second-tap action off `VIA_MACRO_6` and
  onto the equivalent direct `LAG(KC_8)` chord.
- Hardware still froze with the direct chord, ruling out the VIA macro playback
  path as the root cause. Restored `VIA_MACRO_6` on the double-tap path.
- The same keymap shape was confirmed present at
  `cd8050f3b5970dba0711d8503b5bccf1c1054d6e`, so the remaining hardware
  failure is treated as a long-lived same-key PD lifecycle issue rather than a
  regression introduced by the RGB/key-feedback work.
- Changed the authored `PINCH_MODE` single-tap hold from implicit immediate
  Pinch ownership to explicit `PRESS_AND_HOLD_UNTIL_RELEASE(PINCH_MODE)`.
  Quick Pinch tap/double-tap prefixes no longer enter Pinch or take its
  mode-owned GUI lifecycle; Pinch activates only after the first hold threshold
  is crossed.
- Added focused host coverage for repeated Pinch double-tap salvos in both the
  PD/key-runtime fixture and the real authored-profile integration test, plus
  coverage that quick Pinch taps defer the mode-owned GUI path while real Pinch
  holds still mask mode-owned GUI during concurrent keyboard processing.
- Updated `docs/KEYMAP.md`, `docs/ADDING_PD_MODE.md`, and regenerated
  `docs/KEYMAP-OVERVIEW.md`.

Verification passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_macro_payload_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

### Next Steps

1. Flash and hammer repeated `PINCH_MODE` double taps on hardware.
2. If the freeze remains reproducible, inspect same-key PD ownership/lifecycle
   transitions after the explicit Pinch-hold change.

### Same-Key PD Lifecycle Root-Cause Follow-Up

- Kept the authored `PINCH_MODE` keymap on the explicit-hold containment path
  and added a separate host-test legacy Pinch variant that restores the old
  implicit first-hold shape only inside
  `run_pd_mode_key_runtime_integration_tests.sh`.
- Reproduced the deeper invariant failure in host: when a held PD mode was
  preempted by another PD mode, the PD engine moved to the new active mode but
  the key runtime could retain a stale held-action owner for the old mode until
  that old physical key was released.
- Updated key-runtime effect projection so registering a held PD action, or
  activating a PD lock/tap, first unregisters held PD actions for other modes.
  This makes the PD engine, key-runtime shadow projection, held-action owner,
  PD owner key, pointer anchor, and Pinch-owned GUI lifecycle move together at
  the preemption boundary.
- Added legacy-Pinch assertions for Pinch -> Volume, Volume -> Pinch, repeated
  Pinch double-tap salvos, and legacy Pinch double-tap hold into Zoom. The
  tests assert active PD mode, key-runtime shadow PD mode, held action owner,
  PD owner key, pointer anchor, and Pinch-managed GUI state at each boundary.

Verification passed:

- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No required checks were skipped.

### Next Steps

1. Flash a build with the legacy-Pinch fixture behavior if we want hardware
   proof that the root invariant is fixed independent of the safe authored
   keymap containment.
2. Decide separately whether the authored `PINCH_MODE` single hold should stay
   explicit or return to implicit after hardware validation.
