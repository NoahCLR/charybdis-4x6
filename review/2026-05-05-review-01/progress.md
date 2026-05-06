# Profile Studio New-Keymap Progress

This review was opened because the previous tree had no active review folder,
and the topic is a new architecture thread: making Profile Studio create and
edit new Charybdis 4x6 keymaps instead of only editing
`keyboards/bastardkb/charybdis/4x6/keymaps/noah/`.

Prompt template used for the initial pass: `prompts/initial-architecture-review.md`.

## 2026-05-05

### Completed

- Inspected the current Profile Studio path model.
- Confirmed Studio is hardwired to `keymaps/noah`.
- Confirmed QMK defaults `USER_NAME := $(KEYMAP)`, so generated keymaps must
  set `USER_NAME := noah` to reuse the shared runtime.
- Confirmed generic real-profile validation currently compiles the noah files
  directly.
- Confirmed real-profile validation rejects blank profiles through non-empty
  behavior/combo assertions.
- Confirmed `profile_introspect.py` generates only the noah-oriented
  `docs/KEYMAP-OVERVIEW.md`.
- Created this review folder and documented the required architecture plan in
  `userspace-architecture-review.md`.
- Clarified that the v1 priority is a fresh `keymap.c` authoring surface.
  `config.h` and `rgb_config.c` should be compile-ready support files, not the
  main design focus for the first create-profile workflow.
- Implemented the Profile Studio `profileTarget` model and routed reads,
  writes, source opening, stale-write checks, model building, and screenshot
  harness behavior through the selected profile.
- Added profile discovery from `qmk.json` and the Charybdis 4x6 keymap
  folders.
- Added the Studio profile selector and new-profile creation flow.
- Added starter templates under
  `tools/charybdis-profile-studio/templates/charybdis-4x6/`.
  The starter `keymap.c` begins with empty macro payloads, no active combos,
  no active key behaviors, a plain base layer, transparent support layers, and
  compile-ready empty-table materialization flags.
- Added generated-profile `rules.mk` support with `USER_NAME := noah` so QMK
  loads the shared userspace runtime for arbitrary generated keymap names.
- Parameterized `tests/host/run_real_profile_validation_tests.sh` so it can
  validate a selected keymap directory while still defaulting to noah.
- Updated `users/noah/rules.mk` so the firmware build gate validates
  `$(KEYMAP_PATH)`.
- Made empty combo and key-behavior starter data valid in
  `users/noah/keymap_materialize.h`, including a QMK introspection-safe empty
  combo-count override.
- Relaxed generic real-profile validation so zero combos and zero key
  behaviors are valid.
- Added Profile Studio regression coverage for creating a fresh profile, then
  adding the first combo and first behavior row.
- Updated Profile Studio screenshots after the profile selector/header changes.

### In Flight

- No implementation work is currently in flight.
- No required work remains for the v1 create-profile workflow.
- Profile-specific generated overview docs, all-profile feature-gate scanning,
  and VIA import parameterization remain optional follow-up work outside this
  pass.

### Verification

- Passed:
  - `npm run check` from `tools/charybdis-profile-studio/`
  - `npm run screenshots` from `tools/charybdis-profile-studio/`
  - `sh tests/host/run_real_profile_validation_tests.sh`
  - `sh tests/host/run_real_profile_validation_tests.sh /private/tmp/profile-studio-fresh-profile.iyTJnQ/fresh_profile`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km codex_tmp_profile`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Notes:

- `npm run screenshots` initially failed in the sandbox because the harness
  could not bind a local `127.0.0.1` Chrome DevTools port. It passed after
  rerunning with the approved local-port permission.
- `codex_tmp_profile` was a temporary in-repo profile generated from the new
  templates to verify the actual QMK `USER_NAME := noah` path. The temporary
  source files were removed after the compile passed.

### Next Steps

1. Optional: decide whether generated profile overviews should be emitted as
   profile-local docs or a multi-profile index.
2. Optional: expand feature-gate checks to discover every authored Charybdis
   4x6 keymap directory if multiple long-lived profiles become part of the
   daily workflow.
3. Optional: parameterize VIA import tooling if generated profiles should
   import VIA JSON directly.

### Closure Verdict

Closed for the requested v1 scope. Profile Studio can create, select, edit,
validate, and build a fresh Charybdis 4x6 profile with a blank `keymap.c`
authoring surface while reusing the shared `users/noah` runtime.
