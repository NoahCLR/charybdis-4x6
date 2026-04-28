# Progress

## 2026-04-28 - Review Opened

Used `prompts/initial-architecture-review.md`.

This folder was opened as a new review thread because
`review/2026-04-27-review-01/` recorded a final `close thread` verdict and is
now immutable history. This review is materially different: it focuses on
folder/package layout after the earlier runtime-authority cleanup, not on
duplicated release/modifier/PD/combo behavior.

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Read the closed `review/2026-04-27-review-01/` closure section before opening
  this new thread.
- Inspected current userspace package directories under `users/noah/lib`.
- Checked `users/noah/source_manifest.mk`, `tests/host/noah_source_manifest.sh`,
  and layout references in `docs/KEY_RUNTIME.md` / `docs/ADDING_PD_MODE.md`.
- Created `userspace-architecture-review.md` with layout findings, intended
  package direction, and a recommended refactor sequence.

## Finding Status

- No must-fix behavior issue was found from folder layout alone.
- `key/runtime/core/` is the main layout mismatch; it now contains several
  distinct internal roles.
- `state/runtime/` is a naming mismatch because it contains shared services, not
  a third runtime owner.
- `key/runtime/slot/slot_interaction.h` should be renamed to avoid confusion with
  `key/behavior/`.
- `key/runtime/effects/` is optional cleanup tied to the key-runtime internal
  layout pass.

## Verification

- `git diff --check`

`git diff --check` passed. Host tests and firmware compile are intentionally
skipped for this review-note only pass because no runtime source, authored
profile data, build wiring, generated firmware input, or behavior changed.

## Next Steps

1. Start with the low-risk `key/runtime/slot/slot_interaction.h` rename if we choose to
   act on the review.

## 2026-04-28 - Layout Refactor Pass

Starting worktree status:

- `git status --short` showed the active review folder as untracked and then
  source/doc changes from the mechanical path move as the pass progressed.

Reconciliation note:

- The earlier `2026-04-28 - Review Opened` finding status is the audit-time
  snapshot before remediation. The statuses in this layout-refactor section are
  the current state after the package move.

## Completed

- Moved authored handled-key and key-behavior code from `key/interaction/` to
  `key/behavior/`.
- Moved key-runtime internals out of the old broad `key/runtime/core/` bucket:
  - reducer state/query/ownership into `key/runtime/reducer/`
  - release/scan/tap-series/effect planning into `key/runtime/planning/`
  - QMK-facing projection helpers into `key/runtime/projection/`
  - pending-release queue mechanics into `key/runtime/queue/`
  - reducer trace helpers into `key/runtime/trace/`
  - slot interaction, key-position codec, and origin registry into
    `key/runtime/slot/`
- Folded the header-only `key/runtime/effects/` package into
  `key/runtime/planning/`.
- Split shared state services out of `state/runtime/` into:
  - `state/shared/`
  - `state/diagnostics/`
  - `state/modifiers/`
  - `split/`
- Updated production includes, host includes, `users/noah/source_manifest.mk`,
  `tests/host/noah_source_manifest.sh`, and compile-gate allowlists for the new
  layout.
- Updated `docs/KEY_RUNTIME.md` and `userspace-architecture-review.md` to match
  the landed layout.

## Finding Status

- `key/runtime/core/` layout mismatch: resolved by role-based reducer,
  planning, projection, queue, and trace packages.
- `state/runtime/` naming mismatch: resolved by shared, diagnostics, modifiers,
  and split packages.
- Runtime slot interaction naming collision: resolved by
  `key/runtime/slot/slot_interaction.h` and `key/behavior/`.
- Thin `key/runtime/effects/` package: resolved by moving effect vocabulary and
  queue helpers into `key/runtime/planning/`.

## Verification So Far

- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_runtime_diag_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `git diff --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

All commands above passed.

## Next Steps

1. Review the large move diff for path-only intent before staging.
2. Decide whether to close this folder-layout review or keep it open for one
   follow-up audit pass.
