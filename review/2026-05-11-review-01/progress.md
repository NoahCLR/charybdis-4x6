# Multi-Profile Tooling Progress

This review was opened because `review/2026-05-05-review-01/` is closed and
cannot receive post-closure follow-up work. The newest open review,
`review/2026-05-08-review-01/`, tracks runtime loop performance and is not the
right thread for profile-tooling changes.

## 2026-05-11

### Completed

- Added `--keymap` and `--keymap-path` target selection to
  `tools/profile_introspect.py`.
- Kept default `noah` generated docs at `docs/KEYMAP-OVERVIEW.md` with assets
  under `docs/media/profile-introspection/`.
- Added non-`noah` default output paths under `docs/profiles/<name>/` and
  `docs/media/profiles/<name>/profile-introspection/`.
- Added `--output-dir` and `--asset-dir` overrides for profile introspection.
- Added all-profile keymap path discovery through
  `charybdis_profile_keymap_paths()` in `tests/host/noah_source_manifest.sh`.
- Updated feature-gate header-boundary and legacy drag-scroll config checks to
  scan every discovered Charybdis 4x6 profile directory.
- Added `--keymap` and `--keymap-path` target selection to
  `tools/via_to_qmk_layout.py`.
- Changed VIA import rendering to read the selected profile's layer enum and
  keymap-local custom keycodes.
- Changed VIA import macro loading to accept short older exports by padding
  missing `macros[]` slots as empty defaults while still rejecting oversized
  exports.
- Expanded tooling checks to exercise alternate profile introspection output and
  alternate VIA target preview/write simulation against a temp profile.
- Added tooling coverage for short VIA `macros[]` export padding.
- Updated user-facing tooling docs and architecture docs for selected-profile
  introspection and VIA import.

### In Flight

- No implementation work is currently in flight.

### Verification

Passed:

- `python3 -m py_compile tools/profile_introspect.py tools/via_to_qmk_layout.py`
- `python3 tools/profile_introspect.py --check`
- `python3 tools/profile_introspect.py --keymap noah --check`
- `python3 tools/via_to_qmk_layout.py --print --via-json tools/charybdis.layout.json`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_tooling_checks.sh`
- `git diff --check`
- `sh tests/host/run_profile_introspection_checks.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_tooling_checks.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

### Next Steps

1. Consider adding Profile Studio UI actions for generating the active profile
   overview.
2. Consider profile lifecycle operations in Studio: clone, rename, and delete
   with matching `qmk.json` updates.
3. Consider CI coverage that runs host tooling checks before firmware
   userspace builds.
