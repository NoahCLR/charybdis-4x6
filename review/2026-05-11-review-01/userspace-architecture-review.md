# Multi-Profile Tooling Review

This review was opened because `review/2026-05-05-review-01/` is closed and
must remain an immutable snapshot. The work here continues the multi-profile
tooling follow-up from that closed thread. It is not part of the active runtime
loop performance thread in `review/2026-05-08-review-01/`.

## Review Scope

The requested work makes existing profile tooling explicit about the selected
profile target:

- `tools/profile_introspect.py` can target a profile by `--keymap` or
  `--keymap-path`.
- Non-`noah` profile overviews default to
  `docs/profiles/<name>/KEYMAP-OVERVIEW.md`.
- Non-`noah` profile SVG assets default to
  `docs/media/profiles/<name>/profile-introspection/`.
- `tests/host/run_feature_gate_compile_tests.sh` discovers every
  `keyboards/bastardkb/charybdis/4x6/keymaps/*/keymap.c` profile for
  keymap-owned header and config-surface boundary checks.
- `tools/via_to_qmk_layout.py` can target a profile by `--keymap` or
  `--keymap-path`.

Out of scope:

- Moving the shared runtime out of `users/noah/`.
- Changing Profile Studio UI lifecycle actions such as rename, clone, or delete.
- Generating profile docs automatically from Profile Studio.
- Changing authored key behavior or runtime semantics.

## Current Design

The profile target remains a keymap directory under
`keyboards/bastardkb/charybdis/4x6/keymaps/`. The shared runtime remains
`users/noah/`, and generated profiles continue to select that runtime with
`USER_NAME := noah`.

`profile_introspect.py` keeps the default `noah` output stable. Running without a
profile argument still reads `keymaps/noah` and writes the established generated
overview at `docs/KEYMAP-OVERVIEW.md` with SVG assets under
`docs/media/profile-introspection/`. Supplying a non-default profile changes the
read target and default output target together. `--output-dir` and `--asset-dir`
allow callers and tests to redirect generated artifacts.

`via_to_qmk_layout.py` also keeps the `noah` default, but now configures its
target before rendering or writing. It reads the selected profile's layer enum
from `config.h`, and keymap-local custom keycode mappings from that profile's
`enum keymap_custom_keycodes`.

Feature-gate production scans now use a shared
`charybdis_profile_keymap_paths()` helper from `tests/host/noah_source_manifest.sh`.
The compile variants still mirror the shared userspace source manifest; only the
repo-owned production path scans expand to every discovered authored profile.

## Contracts

- Default `profile_introspect.py --check` remains a check of the current `noah`
  generated overview.
- `profile_introspect.py --keymap noah --check` must be equivalent to the
  default check.
- Non-`noah` introspection must not overwrite `docs/KEYMAP-OVERVIEW.md` unless
  an explicit output path asks for that.
- VIA import defaults to `keymaps/noah`, but `--keymap` and `--keymap-path`
  select the target keymap before preview or write behavior.
- Feature gates must scan all discovered Charybdis 4x6 profile directories for
  keymap-owned header-boundary violations and legacy drag-scroll config aliases.

## Verification Coverage

Expected coverage for this thread:

- `python3 -m py_compile tools/profile_introspect.py tools/via_to_qmk_layout.py`
  covers Python syntax.
- `python3 tools/profile_introspect.py --check` covers the default generated
  `noah` overview.
- `python3 tools/profile_introspect.py --keymap noah --check` covers explicit
  default-profile selection.
- `sh tests/host/run_tooling_checks.sh` covers alternate profile introspection
  with redirected output paths and alternate VIA target preview/write
  simulation.
- `sh tests/host/run_feature_gate_compile_tests.sh` covers all-profile boundary
  scanning and source manifest compile variants.
- Closure requires `sh tests/host/run_all_host_tests.sh` and the firmware
  compile gate because build/tooling and validation behavior changed.

## Next Steps

1. Consider adding Profile Studio UI actions for generating the active profile
   overview.
2. Consider profile lifecycle operations in Studio: clone, rename, and delete
   with matching `qmk.json` updates.
3. Consider CI coverage that runs host tooling checks before firmware
   userspace builds.
