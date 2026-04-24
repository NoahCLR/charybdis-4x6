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
