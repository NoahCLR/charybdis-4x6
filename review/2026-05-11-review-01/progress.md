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
- Changed VIA import layer loading to pad short `layers[]` exports with
  transparent layers and preserve extra exported layers with numeric layer ids.
- Added Profile Studio clone, rename, and delete profile actions with matching
  `qmk.json` build-target updates.
- Fixed Profile Studio cloning to backfill a generated `rules.mk` when the
  source profile does not have a keymap-local rules file, and repaired the
  generated `test` profile with `USER_NAME := noah`.
- Added a Profile Studio active-profile generated-docs action.
- Reorganized the Profile Studio header into profile and source/docs action
  groups and removed the separate docs-check button.
- Refined the Profile Studio header into separate profile, Profile overview,
  and Source rows, with `Create overview doc` and `Reload source` labels.
- Reordered the Profile Studio header rows to show Profile, Source, Profile
  overview, then Firmware.
- Added a Profile Studio Firmware row with a `Compile left + right` action that
  builds explicit `FORCE_MASTER` and `FORCE_SLAVE` UF2 targets and warns about
  unsaved Studio edits before compiling.
- Changed the Profile Studio firmware compile path to open the Studio output
  pane immediately and stream QMK stdout/stderr while each side builds.
- Changed Profile Studio overview-doc generation to warn about unapplied Studio
  edits and avoid a model reload for docs-only generation so local drafts are
  not silently reset.
- Added all-profile validation and firmware compile runners that loop over
  Charybdis 4x6 `qmk.json` build targets.
- Changed the full host suite to use all-profile authored validation.
- Expanded tooling checks to exercise alternate profile introspection output and
  alternate VIA target preview/write simulation against a temp profile.
- Added tooling coverage for short VIA `macros[]` export padding.
- Added tooling coverage for short VIA `layers[]` padding, extra layer numeric
  fallback, and Profile Studio lifecycle helpers.
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
- `node --check tools/charybdis-profile-studio/extension.js`
- `sh tests/host/run_all_profile_validation_tests.sh`
- `npm run screenshots` from `tools/charybdis-profile-studio/`
- `sh tests/host/run_all_profile_compile_tests.sh`
- `npm run check` from `tools/charybdis-profile-studio/`
- `npm run screenshots` from `tools/charybdis-profile-studio/`
- `sh tests/host/run_tooling_checks.sh`
- `git diff --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah -e FORCE_MASTER=yes -e TARGET=bastardkb_charybdis_4x6_noah_left`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah -e FORCE_SLAVE=yes -e TARGET=bastardkb_charybdis_4x6_noah_right`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `npm run check` from `tools/charybdis-profile-studio/`
- `sh tests/host/run_real_profile_validation_tests.sh keyboards/bastardkb/charybdis/4x6/keymaps/test`
- `git diff --check`
- `qmk compile -kb bastardkb/charybdis/4x6 -km test`
- `qmk compile -kb bastardkb/charybdis/4x6 -km test -e FORCE_MASTER=yes -e TARGET=bastardkb_charybdis_4x6_test_left`
- `qmk compile -kb bastardkb/charybdis/4x6 -km test -e FORCE_SLAVE=yes -e TARGET=bastardkb_charybdis_4x6_test_right`
- `npm run check` from `tools/charybdis-profile-studio/` after header reorder
- `npm run screenshots` from `tools/charybdis-profile-studio/` after header
  reorder
- `sh tests/host/run_all_host_tests.sh` after header reorder
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` after header reorder
- `npm run check` from `tools/charybdis-profile-studio/` after overview-doc
  unsaved-change handling
- `npm run screenshots` from `tools/charybdis-profile-studio/` after
  overview-doc unsaved-change handling
- `sh tests/host/run_all_host_tests.sh` after overview-doc unsaved-change
  handling
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` after overview-doc
  unsaved-change handling

Notes:

- `npm run screenshots` needed an escalated rerun because the sandbox blocked
  the local screenshot server from listening on `127.0.0.1`.
- The first `sh tests/host/run_all_profile_compile_tests.sh` run hit a sandbox
  denial writing QMK `.build` artifacts in the sibling firmware checkout; the
  escalated rerun passed.

### Next Steps

1. Consider CI coverage that runs host tooling checks before firmware
   userspace builds.
