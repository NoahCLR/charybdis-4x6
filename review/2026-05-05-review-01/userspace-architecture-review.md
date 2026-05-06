# Profile Studio New-Keymap Architecture Plan

This review documents what needs to change before Charybdis Profile Studio can
create, open, edit, validate, document, and build a new keymap from scratch
instead of being hardwired to the current `noah` profile.

Prompt template used for this pass: `prompts/initial-architecture-review.md`.

## Review Scope

The goal is not to rename the shared userspace runtime. The goal is to let one
repo-local userspace support more than one authored Charybdis 4x6 profile.

The v1 product priority is fresh `keymap.c` authoring. A generated profile
should give the user a clean place to build layers, macros, combos, custom
keycodes, and `key_behaviors[]` without cloning the current `noah` keymap
decisions. `config.h` and `rgb_config.c` still need to exist because the
runtime, validation, and Studio parser expect them, but they can be seeded as
compile-ready support files and refined later.

Target workflow:

1. A user opens the repo in VS Code.
2. Profile Studio can open the existing `noah` profile or create a new profile
   under `keyboards/bastardkb/charybdis/4x6/keymaps/<name>/`.
3. A new profile gets starter `rules.mk`, `config.h`, `keymap.c`, and
   `rgb_config.c` files that compile against the shared `users/noah` runtime,
   with `keymap.c` as the intentionally fresh authoring surface.
4. Profile Studio edits the selected profile, not a globally hardcoded path.
5. Generic profile validation and generated profile documentation can target
   the selected profile.
6. Noah-specific regression tests remain attached to the current `noah` profile.
7. `qmk compile -kb bastardkb/charybdis/4x6 -km <name>` works for generated
   profiles when `QMK_USERSPACE` points at this repo.

Non-goals for the first implementation:

- Renaming `users/noah/` to a neutral userspace name.
- Supporting other keyboards or other Charybdis form factors.
- Creating a general QMK keymap generator.
- Supporting arbitrary source layouts outside the current authored file model.
- Making Studio parse every possible C style; starter profiles should use the
  repo's known parseable style.
- Designing a new RGB authoring model for generated profiles.
- Making profile-specific generated docs required for the first successful
  create/edit/compile flow.

## Implementation Status - 2026-05-05

The first implementation pass has landed the minimum create/edit/validate/build
workflow:

- Profile Studio now discovers Charybdis 4x6 profiles from `qmk.json` and the
  keymap directory, exposes a profile selector, and routes model reads plus
  writes through an explicit selected `profileTarget`.
- Profile Studio can create
  `keyboards/bastardkb/charybdis/4x6/keymaps/<name>/` from templates and adds
  the new `[keyboard, keymap]` pair to `qmk.json`.
- Generated profiles include `rules.mk`, `config.h`, `keymap.c`, and
  `rgb_config.c`.
- Generated `rules.mk` sets `USER_NAME := noah`, which was verified with an
  actual temporary QMK build for `codex_tmp_profile`.
- Generated `keymap.c` starts fresh: empty VIA payloads, empty hardcoded macro
  payloads, no active combos, no active key behavior rows, a plain base layer,
  and transparent support layers.
- Empty combo/key-behavior materialization is supported by explicit starter
  flags that must appear before `noah_keymap.h`, because
  `users/noah/keymap_materialize.h` selects its output macros at include time.
- The empty combo path emits a strong `combo_count()` override only when
  `keymap.c` is compiled as its own translation unit. This avoids duplicate
  symbol definitions when QMK's `quantum/keymap_introspection.c` includes
  `keymap.c`.
- Generic real-profile validation accepts a selected keymap path and allows
  zero combos and zero key behaviors.
- `users/noah/rules.mk` passes `$(KEYMAP_PATH)` into generic profile
  validation, so a generated firmware build validates the active profile's
  authored files.

Optional follow-up outside the requested v1 scope:

- Profile-specific generated overview docs.
- Feature gates that scan every authored profile directory if multiple
  long-lived profiles become part of regular development.
- VIA import tooling that targets a selected profile.

Closure state: closed for the requested v1 scope. The implemented workflow
creates a fresh profile, routes Studio edits through the selected profile,
validates the selected profile, and compiles both generated-profile and noah
firmware targets.

## Findings

### must-fix: Profile Studio is hardwired to `keymaps/noah`

References:

- `tools/charybdis-profile-studio/extension.js`
  - `KEYMAP_RELATIVE_PATH`
  - `RGB_RELATIVE_PATH`
  - `KEYMAP_CONFIG_RELATIVE_PATH`
  - `findProfileRoot()`
  - `buildModel(root)`
  - all patch helpers that join `root` with those constants
- `tools/charybdis-profile-studio/package.json`
  - `workspaceContains:keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`

Problem:

Profile Studio does not have a concept of "current profile". It only has repo
root plus three fixed source paths. That means it cannot discover multiple
profiles, cannot switch profiles, and cannot create a profile when the `noah`
path is missing.

Required design:

Introduce a `profile target` object and pass it through all model-building and
patching paths.

Proposed shape:

```js
{
    id: "bastardkb/charybdis/4x6:noah",
    keyboard: "bastardkb/charybdis/4x6",
    keymap: "noah",
    keymapDir: "keyboards/bastardkb/charybdis/4x6/keymaps/noah",
    keymapPath: "keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c",
    configPath: "keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h",
    rgbPath: "keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c",
    rulesPath: "keyboards/bastardkb/charybdis/4x6/keymaps/noah/rules.mk",
    userspaceName: "noah",
    userspaceDir: "users/noah",
    generated: false
}
```

Implementation notes:

- Keep `root` as the repo root.
- Replace global profile path constants with a default target factory and
  target-relative helpers.
- `buildModel(root)` becomes `buildModel(root, profileTarget)`.
- `openSource(root, file)` becomes `openSource(root, profileTarget, file)`.
- Every patch helper that writes `keymap.c`, `config.h`, or `rgb_config.c`
  takes a target or explicit paths.
- The model sent to the webview includes:
  - `activeProfile`
  - `profiles`
  - `files`
  - `root`
- The webview includes a profile picker before editing surfaces.
- If no profile exists, Studio should still open and show a create-profile
  screen instead of only showing an error.

### must-fix: QMK will not load `users/noah/rules.mk` for arbitrary keymap names unless generated profiles opt in

References:

- `../bastardkb-qmk/builddefs/build_keyboard.mk`
  - QMK defaults `USER_NAME := $(KEYMAP)`
  - QMK includes `$(USER_PATH)/rules.mk`
- `../bastardkb-qmk/docs/feature_userspace.md`
  - documents `USER_NAME := mylayout`
- `users/noah/rules.mk`
  - owns runtime source lists, feature toggles, and the real-profile validation
    firmware build gate

Problem:

The current userspace is loaded automatically for `-km noah` because QMK maps
the keymap name to `users/noah`. A generated profile named `alice` would
default to `users/alice`, so the shared runtime would not be included.

Required design:

Every Studio-created keymap must include a keymap-local `rules.mk` containing:

```make
USER_NAME := noah
```

That is the minimum wiring needed for QMK to include `users/noah/rules.mk`.

The starter `rules.mk` may later include profile-specific feature toggles, but
the first version should stay small:

```make
# Use the shared Charybdis Profile Studio runtime from users/noah.
USER_NAME := noah
```

Risks:

- `users/noah/rules.mk` currently runs `sh tests/host/run_real_profile_validation_tests.sh`
  without passing the active `KEYMAP_PATH`. That build gate will continue to
  validate the current `noah` profile, not the generated one, unless it is made
  profile-aware. This must be fixed before claiming generated firmware is
  guarded by profile validation.

### must-fix: real-profile validation is noah-only and rejects blank starter profiles

References:

- `tests/host/run_real_profile_validation_tests.sh`
  - compiles `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`
  - compiles `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`
  - includes `tests/host/include/noah_compile_config.h`
- `tests/host/include/noah_compile_config.h`
  - includes `keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h`
- `tests/host/real_profile_validation_test.c`
  - checks `key_behavior_count > 0`
  - checks `noah_combo_output_count > 0`

Problem:

Generic validation currently means "validate Noah's current profile". It also
assumes a profile has at least one behavior row and at least one combo output.
A scratch profile should be allowed to start with zero custom behaviors and
zero combos.

Required design:

Split validation into two modes:

- Generic profile validation:
  - accepts `--keymap-path` or `CHARYBDIS_PROFILE_KEYMAP_PATH`
  - includes the selected profile's `config.h`
  - compiles the selected profile's `keymap.c`
  - compiles the selected profile's `rgb_config.c`
  - allows zero key behaviors
  - allows zero combos
  - still validates layer action ownership, behavior row validity when present,
    combo member uniqueness when present, macro payload syntax, and RGB config
- Noah regression validation:
  - keeps the stricter profile-shape assumptions if useful
  - remains attached to `keymaps/noah`
  - runs in the full host suite as the daily-driver regression target

Concrete changes:

- Add a small generated include file in the build dir for generic validation:

  ```c
  #include "users/noah/config.h"
  #include "<selected-keymap-path>/config.h"
  ```

- Change `run_real_profile_validation_tests.sh` to accept:

  ```sh
  sh tests/host/run_real_profile_validation_tests.sh
  sh tests/host/run_real_profile_validation_tests.sh keyboards/bastardkb/charybdis/4x6/keymaps/alice
  ```

- Default remains `keyboards/bastardkb/charybdis/4x6/keymaps/noah`.
- Add an optional strict flag for the current noah assumptions, or move the
  noah-specific `> 0` checks into a separate noah-only runner.
- Ensure `users/noah/rules.mk` calls generic validation for `$(KEYMAP_PATH)`,
  not hardcoded noah validation.

### must-fix: starter profile materialization must support empty tables

References:

- `users/noah/keymap_materialize.h`
  - `MATERIALIZE_KEYMAP_DATA()`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`
  - `VIA_MACROS(MACRO)`
  - `HARDCODED_MACROS(MACRO)`
  - `COMBOS(COMBO)`
  - `key_behaviors[]`
  - `MATERIALIZE_KEYMAP_DATA()`

Problem:

The current authored profile has behavior rows and combo rows. A generated
scratch profile may not. The runtime should tolerate:

- no combo rows
- no key behavior rows
- no hardcoded macro payloads beyond empty starter slots

Required design:

Define the starter file pattern so empty data still expands cleanly.

Recommended starter behavior:

- Keep `VIA_MACROS(MACRO)` with all 64 slots, empty payloads.
- Keep `HARDCODED_MACROS(MACRO)` with all 16 slots, empty payloads.
- Represent no combos as:

  ```c
  #define COMBOS(COMBO)
  ```

- Represent no behavior rows with an explicit sentinel strategy if C requires
  it. Options:
  - Preferred: update materialization so an empty `key_behaviors[]` is valid in
    hosted and firmware builds.
  - Fallback: generate one harmless behavior row for a reachable key. This is
    worse because a "blank" profile is not truly blank.

Validation should prove the preferred path works:

- `key_behavior_count == 0` is valid.
- `noah_combo_count == 0` is valid.
- `noah_combo_output_count == 0` is valid.
- `noah_combo_output_keycodes` can be null or point at an empty/sentinel list,
  but all consumers must handle it consistently.

### must-fix: Profile Studio needs a create-profile workflow and starter templates

References:

- `tools/charybdis-profile-studio/extension.js`
  - currently opens only after existing profile files are found
- `tools/charybdis-profile-studio/scripts/check-key-behavior-coverage.js`
  - test harness assumes fixed profile paths
- `tools/charybdis-profile-studio/scripts/capture-screenshots.js`
  - builds model from the current repo root and fixed current profile

Required create flow:

1. Discover repo root.
2. Discover existing profiles.
3. If profiles exist, show profile picker and "Create Profile".
4. If no profiles exist, show only "Create Profile".
5. Ask for a keymap name.
6. Validate name:
   - non-empty
   - lower-case preferred
   - allowed characters: `[a-z0-9_-]`
   - no path separators
   - does not already exist
7. Generate:
   - `rules.mk`
   - `config.h`
   - `keymap.c`
   - `rgb_config.c`
8. Update `qmk.json` `build_targets`.
9. Build a model from the new profile and switch Studio to it.

Template requirements:

- `rules.mk`
  - sets `USER_NAME := noah`
- `config.h`
  - defines `enum charybdis_keymap_layers`
  - may keep the current layer/settings shape so existing runtime and Studio
    assumptions stay compile-ready
  - is not the main v1 scratch surface
- `keymap.c`
  - includes `"keycodes.h"`
  - includes `"noah_keymap.h"`
  - defines `enum keymap_custom_keycodes` with only the sentinel
  - defines full `VIA_MACROS(MACRO)`
  - defines full `HARDCODED_MACROS(MACRO)`
  - defines empty `COMBOS(COMBO)`
  - defines empty or supported `key_behaviors[]`
  - defines a base `keymaps[][]` with a valid `LAYOUT(...)` of 56 positions
  - calls `MATERIALIZE_KEYMAP_DATA()`
  - does not clone the current `noah` behavior rows, combos, macro payloads, or
    layout-specific custom keycodes
- `rgb_config.c`
  - includes `"noah_keymap.h"`
  - compiles when `RGB_MATRIX_ENABLE` is defined
  - provides a compile-ready RGB configuration for whatever layers `config.h`
    declares
  - should not be the focus of the first create-profile workflow

Starter layout choices:

- `keymap.c` starts as a clean profile, not a clone of `noah`.
- Conservative base layer:
  - standard QWERTY where obvious
  - transparent/no-op positions where the user should fill in controls
  - no custom tap/hold behavior by default
- If `config.h` keeps the current layer enum, generate the matching non-base
  `keymaps[][]` entries as transparent starter layers so the profile compiles
  while still feeling empty.
- A later "Duplicate current profile" action can clone `noah`, but that is not
  the first create-from-scratch path.

### should-fix: generated profile docs are single-output and noah-oriented

References:

- `tools/profile_introspect.py`
  - hardcoded `KEYMAP_FILE`
  - hardcoded `CONFIG_FILE`
  - hardcoded `RGB_CONFIG_FILE`
  - hardcoded `MARKDOWN_OUTPUT = docs/KEYMAP-OVERVIEW.md`
- `docs/KEYMAP-OVERVIEW.md`
  - generated snapshot for current noah profile
- `docs/KEYMAP.md`
  - prose documentation for current noah profile

Problem:

If there are multiple profiles, one `docs/KEYMAP-OVERVIEW.md` is ambiguous.
The generated report currently describes the current `noah` profile, not a
selected profile.

Required design:

Parameterize profile introspection:

```sh
python3 tools/profile_introspect.py --keymap noah --check
python3 tools/profile_introspect.py --keymap alice --write
python3 tools/profile_introspect.py --keymap-path keyboards/bastardkb/charybdis/4x6/keymaps/alice --print-markdown
```

Output policy options:

- Keep `docs/KEYMAP-OVERVIEW.md` as the canonical current noah profile output.
- Add profile-specific generated outputs:
  - `docs/profiles/noah/KEYMAP-OVERVIEW.md`
  - `docs/profiles/alice/KEYMAP-OVERVIEW.md`
  - `docs/media/profiles/noah/profile-introspection/`
  - `docs/media/profiles/alice/profile-introspection/`

Recommended first implementation:

- Keep existing noah output unchanged to avoid breaking README/docs links.
- Add optional `--output-dir docs/profiles/<name>` and
  `--asset-dir docs/media/profiles/<name>/profile-introspection`.
- Let Profile Studio show a "Generate overview" action for the active profile.

### should-fix: qmk.json build targets should become part of profile lifecycle

References:

- `qmk.json`
  - currently contains only `["bastardkb/charybdis/4x6", "noah"]`

Problem:

QMK userspace commands and repo-level build discovery only know about profiles
listed in `qmk.json`. Studio-created profiles should not be hidden from normal
QMK workflows.

Required design:

- When Studio creates a profile, append:

  ```json
  ["bastardkb/charybdis/4x6", "<new-keymap>"]
  ```

- Keep targets sorted or preserve existing order with `noah` first.
- Avoid duplicate entries.
- Add a small helper:
  - `readQmkUserspaceTargets(root)`
  - `writeQmkUserspaceTargets(root, targets)`
  - `ensureBuildTarget(root, keyboard, keymap)`

Validation:

- Studio check script should create a temporary qmk.json and assert the target
  is added once.

### should-fix: feature gates and boundary checks scan only noah profile paths

References:

- `tests/host/run_feature_gate_compile_tests.sh`
  - `REPO_OWNED_PRODUCTION_PATHS="users/noah keyboards/bastardkb/charybdis/4x6/keymaps/noah"`
  - header-boundary check scans only `keymaps/noah`
  - drag-scroll config surface check scans only `keymaps/noah`

Problem:

If there are multiple authored profiles, boundary checks must include them.
Otherwise new generated profiles can violate keymap/runtime header boundaries
without host tests noticing.

Required design:

- Add a helper that lists profile paths:

  ```sh
  charybdis_profile_keymap_paths "$ROOT"
  ```

- Use that helper in:
  - feature gate production path list
  - keymap-owned header-boundary scans
  - drag-scroll config scans

Initial helper behavior:

- Find directories matching:
  - `keyboards/bastardkb/charybdis/4x6/keymaps/*`
  - containing `keymap.c`
- Exclude non-directories and artifacts such as `.DS_Store`.

### should-fix: VIA import/export tooling remains noah-specific

References:

- `tools/via_to_qmk_layout.py`
  - hardcoded `KEYMAP_FILE`
- `tests/host/run_tooling_checks.sh`
  - exercises the hardcoded VIA tool

Problem:

If a generated profile uses VIA and Studio, users will expect the VIA import
tool to target that profile too. Keeping it noah-only is acceptable for the
first create-profile milestone, but it should be explicit.

Recommended staged approach:

- First milestone: document `via_to_qmk_layout.py` as noah/current-profile-only.
- Second milestone: add `--keymap <name>` or `--keymap-path <path>`.
- Update tooling checks to cover the default noah path and one temporary
  alternate keymap path.

### optional cleanup: runtime and file names still say `noah`

References:

- `users/noah/`
- `users/noah/noah_keymap.h`
- `users/noah/noah_keymap_ids.h`
- `users/noah/noah_runtime.h`
- symbols such as `noah_keymap_validate`

Problem:

Once multiple profiles exist, "noah" becomes both:

- the shared runtime/userspace name
- the current author's profile name

That naming is confusing, but not a blocker.

Recommendation:

Do not rename in the first implementation. Keep generated keymaps using
`USER_NAME := noah`. Rename later only if the multi-profile workflow proves
stable.

If renamed later, consider:

- directory: `users/charybdis_profile/`
- headers: `charybdis_profile_keymap.h`, `charybdis_profile_runtime.h`
- symbols: keep `noah_*` until a deliberate compatibility break, or migrate
  with aliases

Renaming now would expand the diff dramatically and distract from the actual
profile-target work.

## Target Architecture

### Source Ownership

Keep the current ownership model:

| Area | Owner |
| --- | --- |
| shared runtime and QMK integration | `users/noah/` |
| authored profile data | `keyboards/bastardkb/charybdis/4x6/keymaps/<name>/` |
| Profile Studio extension | `tools/charybdis-profile-studio/` |
| generated profile overview | `docs/KEYMAP-OVERVIEW.md` for `noah`, optional profile-specific docs for others |
| host validation | `tests/host/` |

The important change is that authored profile paths become data instead of
constants.

### Profile Target Contract

Every supported profile directory must contain:

- `rules.mk`
- `config.h`
- `keymap.c`
- `rgb_config.c`

Every supported profile must satisfy:

- `rules.mk` sets `USER_NAME := noah`
- `config.h` defines `enum charybdis_keymap_layers` with `LAYER_COUNT`
- `keymap.c` defines:
  - `VIA_MACROS(MACRO)`
  - `HARDCODED_MACROS(MACRO)`
  - `COMBOS(COMBO)`
  - `key_behaviors[]` or an approved empty-table equivalent
  - `keymaps[][]`
  - `MATERIALIZE_KEYMAP_DATA()`
- `rgb_config.c` defines the authored RGB config expected by runtime validation
- the profile compiles with:

  ```sh
  qmk compile -kb bastardkb/charybdis/4x6 -km <name>
  ```

### Studio Internal Flow

Current:

```text
findProfileRoot()
  -> fixed KEYMAP_RELATIVE_PATH / RGB_RELATIVE_PATH
  -> buildModel(root)
  -> patch fixed files
```

Target:

```text
findRepoRoot()
  -> discoverProfileTargets(root)
  -> choose active target or create target
  -> buildModel(root, target)
  -> patch target files
```

The active target must be included in every webview message that can write
source, or the extension host must maintain the active target and reject writes
for stale/unknown target ids.

Recommended write safety:

- Webview sends `profileId` with each write request.
- Extension compares `profileId` with the active target.
- Extension rejects stale writes if the active profile changed since the UI
  staged edits.

### Profile Discovery

Discovery should combine:

1. `qmk.json` build targets for `bastardkb/charybdis/4x6`
2. filesystem scan under `keyboards/bastardkb/charybdis/4x6/keymaps/*`

Reason:

- `qmk.json` records intended build targets.
- Filesystem scan catches unregistered but existing profiles.

Profile status fields:

- `complete`: all required files exist
- `registered`: present in `qmk.json`
- `editable`: has `keymap.c`, `config.h`, and `rgb_config.c`
- `buildable`: has `rules.mk` and expected userspace wiring

Studio can show incomplete profiles with repair actions rather than silently
ignoring them.

### Create-Profile Template Strategy

Use templates stored under:

```text
tools/charybdis-profile-studio/templates/charybdis-4x6/
```

Suggested files:

- `rules.mk`
- `config.h`
- `keymap.c`
- `rgb_config.c`

Template rendering should be deliberately small:

- Replace `{{KEYMAP_NAME}}`
- Optionally replace `{{PROFILE_TITLE}}`
- Avoid templating large sections unless necessary.

Why files instead of string literals:

- Easier to review.
- Easier to test with `git diff --check`.
- Easier to keep starter code formatted.
- Avoids hiding firmware source inside JavaScript string escapes.

### Validation Contract

Generic validation must answer:

- Does this profile compile in the host validation harness?
- Are authored layer actions supported by userspace ownership?
- Are behavior rows valid when present?
- Are behavior rows reachable when present?
- Are combo rows valid when present?
- Are macro payloads valid?
- Is RGB config internally valid?
- Does `LAYER_COUNT` line up with dynamic keymap layer count?

Noah-specific regression must answer:

- Does the current noah profile still expose its expected behavior-rich shape?
- Do thumb layer locks, nav interactions, dragscroll overlap, and profile
  history scenarios still behave as expected?

These are different contracts and should not be forced into one runner.

### Firmware Build Gate

Current issue:

`users/noah/rules.mk` runs hardcoded real-profile validation. For `-km alice`,
that would validate `noah` unless changed.

Target:

`users/noah/rules.mk` validates the active build keymap through QMK-provided
`KEYMAP_PATH`.

Possible approach:

```make
NOAH_PROFILE_VALIDATION_RESULT := $(shell \
  log="$$(mktemp "$${TMPDIR:-/tmp}/noah-profile-validation.XXXXXX")"; \
  cd "$(NOAH_USERSPACE_ROOT)" && \
  sh tests/host/run_real_profile_validation_tests.sh "$(KEYMAP_PATH)" >"$$log" 2>&1; \
  status=$$?; \
  if [ $$status -ne 0 ]; then cat "$$log" >&2; echo failed; fi; \
  rm -f "$$log")
```

Need to verify whether `KEYMAP_PATH` is absolute or relative in this context
and normalize accordingly in the shell runner.

## Implementation Plan

### Phase 0: Lock down terminology and review docs

Files:

- `review/2026-05-05-review-01/userspace-architecture-review.md`
- `review/2026-05-05-review-01/progress.md`

Tasks:

- Record this plan.
- Keep this review folder active until the multi-profile/Profile Studio thread
  is closed.
- Update `progress.md` after each implementation pass.

Verification:

```sh
git diff --check
```

### Phase 1: Make profile target discovery explicit in Studio without changing behavior

Files:

- `tools/charybdis-profile-studio/extension.js`
- `tools/charybdis-profile-studio/scripts/check-key-behavior-coverage.js`
- `tools/charybdis-profile-studio/scripts/capture-screenshots.js`
- `tools/charybdis-profile-studio/package.json`

Tasks:

- Introduce `defaultProfileTarget()`.
- Introduce `discoverProfileTargets(root)`.
- Replace fixed path joins in model-building and patch helpers with target
  paths.
- Keep `noah` as the selected default.
- Keep UI behavior effectively unchanged.
- Adjust activation event so the extension activates for the repo generally:
  - `workspaceContains:qmk.json`
  - or command activation only, then discovery after open
- Add unit/harness checks for discovery and target path resolution.

Verification:

```sh
cd tools/charybdis-profile-studio
npm run check
npm run screenshots
git diff --check
```

Expected result:

- No user-visible create flow yet.
- Existing noah Studio behavior remains unchanged.
- The code no longer depends on global noah path constants for normal model
  building and writes.

### Phase 2: Parameterize generic profile validation

Files:

- `tests/host/run_real_profile_validation_tests.sh`
- `tests/host/real_profile_validation_test.c`
- `tests/host/include/noah_compile_config.h`
- possibly new generated config-header helper inside the shell runner
- `users/noah/rules.mk`

Tasks:

- Let the runner accept a keymap path argument.
- Generate a temporary compile config header for the requested profile.
- Compile selected `keymap.c` and `rgb_config.c`.
- Remove or gate `key_behavior_count > 0`.
- Remove or gate `noah_combo_output_count > 0`.
- Update `users/noah/rules.mk` to validate `$(KEYMAP_PATH)`.
- Preserve default noah behavior when no argument is passed.
- Add negative-path validation for missing files.

Verification:

```sh
sh tests/host/run_real_profile_validation_tests.sh
sh tests/host/run_feature_gate_compile_tests.sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
```

Expected result:

- Existing noah profile remains green.
- Generic runner is ready for alternate profile paths.

### Phase 3: Support empty starter authored data

Files:

- `users/noah/keymap_materialize.h`
- `users/noah/lib/key/behavior/key_behavior_lookup.c`
- `users/noah/lib/key/behavior/keymap_validation.c`
- `tests/host/`
- starter templates once they exist

Tasks:

- Decide and implement the empty `key_behaviors[]` representation.
- Ensure empty `COMBOS(COMBO)` materializes valid combo symbols.
- Add host tests for:
  - zero behavior rows
  - zero combos
  - no combo output pointer misuse
  - validation still catches invalid rows when rows exist
- Keep noah profile behavior unchanged.

Verification:

```sh
sh tests/host/run_key_behavior_lookup_tests.sh
sh tests/host/run_key_behavior_validation_tests.sh
sh tests/host/run_keymap_validation_tests.sh
sh tests/host/run_real_profile_validation_tests.sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
```

Expected result:

- A truly blank generated profile can be represented without fake behavior
  rows or fake combos.

### Phase 4: Add create-profile templates and write path

Files:

- `tools/charybdis-profile-studio/templates/charybdis-4x6/rules.mk`
- `tools/charybdis-profile-studio/templates/charybdis-4x6/config.h`
- `tools/charybdis-profile-studio/templates/charybdis-4x6/keymap.c`
- `tools/charybdis-profile-studio/templates/charybdis-4x6/rgb_config.c`
- `tools/charybdis-profile-studio/extension.js`
- `tools/charybdis-profile-studio/scripts/check-key-behavior-coverage.js`
- `qmk.json` helper code

Tasks:

- Add a create-profile command/message path.
- Validate keymap names.
- Render templates into the new keymap directory.
- Add the profile to `qmk.json`.
- Switch the active Studio target to the new profile.
- Add tests that create a temp profile and parse/build a model from it.
- Add tests that reject invalid names and existing target directories.

Verification:

```sh
cd tools/charybdis-profile-studio
npm run check
git diff --check
```

Then after creating a temporary or sample generated profile:

```sh
sh tests/host/run_real_profile_validation_tests.sh keyboards/bastardkb/charybdis/4x6/keymaps/<generated>
qmk compile -kb bastardkb/charybdis/4x6 -km <generated>
```

Expected result:

- Studio can create a new profile and immediately load it.
- The generated `keymap.c` is fresh: no copied noah behavior rows, combos,
  non-empty macro payloads, or custom layout decisions beyond the starter
  physical matrix.
- The generated profile validates and compiles.

### Phase 5: Add profile picker and stale-write protection

Files:

- `tools/charybdis-profile-studio/extension.js`

Tasks:

- Add profile picker UI.
- Add active profile display.
- Include `profileId` in write messages.
- Reject writes if the profile changed since staging edits.
- Clear staged edits when switching profiles.
- Show profile file paths in the model.

Verification:

```sh
cd tools/charybdis-profile-studio
npm run check
npm run screenshots
```

Expected result:

- Multiple profiles can be opened and edited in one workspace without writes
  landing in the wrong profile.

### Phase 6: Parameterize profile introspection

Files:

- `tools/profile_introspect.py`
- `tests/host/run_profile_introspection_checks.sh`
- `tests/host/run_tooling_checks.sh`
- `docs/tooling/PROFILE_INTROSPECT.md`
- `docs/tooling/PROFILE_STUDIO.md`

Tasks:

- Add `--keymap`.
- Add `--keymap-path`.
- Add optional output path controls.
- Keep current default noah output unchanged.
- Update docs for generated profile overview.
- Add tests for default noah output and alternate output to a temp dir.

Verification:

```sh
python3 tools/profile_introspect.py --check
python3 tools/profile_introspect.py --keymap noah --check
python3 tools/profile_introspect.py --keymap <generated> --print-markdown
sh tests/host/run_profile_introspection_checks.sh
sh tests/host/run_tooling_checks.sh
git diff --check
```

Expected result:

- Current docs stay stable.
- Alternate profiles can produce their own read-only overview.

### Phase 7: Expand feature gates to all profile paths

Files:

- `tests/host/noah_source_manifest.sh` or a new profile helper script
- `tests/host/run_feature_gate_compile_tests.sh`

Tasks:

- Add a helper that lists profile directories.
- Scan all profile directories for keymap-owned header boundary violations.
- Scan all profile directories for legacy drag-scroll config violations.
- Keep source manifest compile gates unchanged unless build surfaces change.

Verification:

```sh
sh tests/host/run_feature_gate_compile_tests.sh
sh tests/host/run_all_host_tests.sh
```

Expected result:

- New generated profiles are covered by header-boundary and config-surface
  checks.

### Phase 8: Documentation updates

Files:

- `README.md`
- `docs/tooling/PROFILE_STUDIO.md`
- `docs/tooling/PROFILE_INTROSPECT.md`
- `docs/architecture/source-map.md`
- `docs/architecture/change-guide.md`
- possibly `docs/tooling/VIA_TO_QMK.md`

Tasks:

- Document creating a new profile in Studio.
- Document generated profile file ownership.
- Document validation commands for a selected profile.
- Document compile command:

  ```sh
  qmk compile -kb bastardkb/charybdis/4x6 -km <name>
  ```

- Explain that `users/noah` is the shared runtime currently selected by
  generated profile `rules.mk`.
- Explain noah-specific docs vs generated per-profile docs.

Verification:

```sh
git diff --check
```

Expected result:

- A new user can understand the scratch-profile path without reading review
  notes.

### Phase 9: Optional VIA tooling parameterization

Files:

- `tools/via_to_qmk_layout.py`
- `tests/host/run_tooling_checks.sh`
- `docs/tooling/VIA_TO_QMK.md`

Tasks:

- Add `--keymap`.
- Add `--keymap-path`.
- Keep current noah default.
- Add temp-profile tooling tests.

Verification:

```sh
sh tests/host/run_tooling_checks.sh
git diff --check
```

Expected result:

- VIA import can target profiles created by Studio.

## Acceptance Criteria

Minimum viable completion:

- Profile Studio can create `keyboards/bastardkb/charybdis/4x6/keymaps/<name>/`.
- Generated profile includes `rules.mk`, `config.h`, `keymap.c`, and
  `rgb_config.c`.
- Generated `keymap.c` starts fresh rather than cloning the current noah
  profile behavior table, combo table, macro payloads, or custom keycodes.
- Generated `config.h` and `rgb_config.c` are compile-ready support files; they
  do not need a special v1 customization flow beyond the existing Studio edit
  surfaces.
- Generated `rules.mk` uses `USER_NAME := noah`.
- Generated profile appears in `qmk.json`.
- Profile Studio can open and edit the generated profile.
- Generic profile validation passes for the generated profile.
- Firmware compile passes:

  ```sh
  qmk compile -kb bastardkb/charybdis/4x6 -km <name>
  ```

- Existing noah profile still passes:

  ```sh
  sh tests/host/run_all_host_tests.sh
  qmk compile -kb bastardkb/charybdis/4x6 -km noah
  ```

- No generated profile writes land in the noah profile unless noah is the active
  selected target.

Full completion:

- Profile-specific generated overview works.
- Feature gates scan all authored profile directories.
- Docs explain the full new-profile workflow.
- VIA import can target a selected profile or is explicitly documented as
  noah-only until later.

## Current Architecture Assessment

The runtime/profile separation is already strong enough to support multiple
profiles in principle. `users/noah/source_manifest.mk` already uses QMK's
`KEYMAP_PATH` for `rgb_config.c`, which is the right direction. The keymap data
is mostly isolated to `keyboards/.../keymaps/noah/`, and shared behavior is
mostly under `users/noah/`.

The weak point is not runtime ownership. The weak point is tooling identity:
Profile Studio, profile introspection, validation runners, feature gates, and
docs all use `noah` as an implicit global profile. That implicit global must
become an explicit selected profile target.

The biggest correctness risk is generating a profile that appears editable but
does not actually compile through QMK because `USER_NAME := noah` or active
profile validation was missed. The second biggest risk is allowing Studio to
stage edits for one profile and apply them to another after a profile switch.

## Recommended Next Refactor Sequence

1. Refactor Profile Studio internally from fixed paths to `profileTarget`, while
   keeping noah as the only visible target.
2. Parameterize generic profile validation and update the firmware build gate to
   validate `$(KEYMAP_PATH)`.
3. Make empty authored data valid and covered by host tests.
4. Add starter templates and create-profile write path.
5. Add profile picker and stale-write protection.
6. Parameterize profile introspection and generated overview outputs.
7. Expand feature gates to all profile directories.
8. Update user-facing docs.
9. Parameterize VIA import tooling if needed for the first public workflow.
