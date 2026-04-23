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

### Findings

- No `must-fix` findings found.
- One `should-fix` finding recorded: PD-mode RGB locality is internally
  inconsistent because comments/config imply trigger-half feedback while the
  authored rows all render on the right half.
- Two `optional cleanup` recommendations recorded:
  - decide whether to add or document normal RGB Matrix control keys
  - make key-feedback semantic paint priority explicit if new semantics are
    added later

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

### Next Steps

1. Choose and apply the PD overlay locality policy.
2. If editing `rgb_config.c`, `users/noah/config.h`, or `keymap.c`, regenerate
   and verify profile introspection with `python3 tools/profile_introspect.py
   --write` and `python3 tools/profile_introspect.py --check`.
3. If changing authored RGB behavior, run the targeted RGB tests, real profile
   validation, full host suite, and firmware compile before closure.
4. If no behavior changes are desired, update only the stale profile comment
   and document whether RGB Matrix controls are intentionally VIA-only.
