# Progress

## 2026-05-03

Opened this review because the active `review/2026-04-29-review-01/` thread is
about PD-runtime semantics, while this work changes RGB LED group inheritance
and all-target authoring.

Completed:

- Added shared RGB group inheritance helpers and all-target selectors.
- Updated layer, PD-mode, combo-feedback, and key-behavior feedback render
  stages so `HSV(0, 0, 0)` inherits the active stage color instead of painting
  black.
- Added `RGB_LAYER_GROUP_ALL` and `RGB_PD_MODE_GROUP_ALL` handling to render and
  validation paths.
- Updated authored RGB comments and changed the existing combo-feedback thumb
  group to inherit the combo-feedback color.
- Updated Profile Studio owner choices and inherited-color rendering.
- Updated profile introspection so inherited LED groups do not generate black
  swatches.
- Updated RGB/user-facing documentation for all-target groups and zero-HSV
  inheritance.

Verification so far:

- `sh tests/host/run_rgb_layer_render_tests.sh` passed.
- `sh tests/host/run_rgb_validation_tests.sh` passed.
- `node --check tools/charybdis-profile-studio/extension.js` passed.
- `/Users/noah/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3 tools/profile_introspect.py --check` passed after regenerating outputs.
- `npm run check` passed from `tools/charybdis-profile-studio`.
- `PYTHON=/Users/noah/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3 sh tests/host/run_profile_introspection_checks.sh` passed.
- `sh tests/host/run_real_profile_validation_tests.sh` passed.
- `sh tests/host/run_feature_gate_compile_tests.sh` passed.
- `git diff --check` passed.
- `PYTHON=/Users/noah/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3 sh tests/host/run_all_host_tests.sh` passed.
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passed.

Next steps:

- Flash the generated UF2 when ready.
- If additional LED groups are authored later, prefer `HSV(0, 0, 0)` for
  inherited stage color and nonzero HSV only for explicit overrides.
