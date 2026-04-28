# Progress

## 2026-04-27 - Review Opened

Used `prompts/initial-architecture-review.md`.

This folder was opened as a new review thread because the newest active review folder, `review/2026-04-23-review-04/`, is scoped to RGB runtime architecture. This audit is materially different: it reviews every `.c` and `.h` file under `users/noah` for ad-hoc patches, duplicated responsibilities, and overlapping runtime authority.

Starting worktree status:

- `git status --short` returned no entries at the start of the pass.

## Completed

- Read the newest review folder before starting this review.
- Enumerated all `users/noah` `.c` and `.h` files.
- Inspected 161 files by subsystem:
  - root userspace headers and hooks
  - action dispatch and lifecycle
  - QMK/VIA/pointing compatibility
  - handled-key interaction and validation
  - held ownership registries
  - key runtime core, process, release, trace, and API wrappers
  - macro payload, dispatch, and VIA providers
  - pointing-device modes and PD runtime
  - RGB runtime, validation, automouse, and stages
  - runtime shared state, ownership, diagnostics, trace, and split sync
- Created `userspace-architecture-review.md` with findings, current architecture assessment, recommended refactor sequence, and a per-file inventory.

## Findings Summary

- No immediate must-fix runtime bug was proven by the static pass.
- The highest-risk clutter is concentrated in key runtime authority:
  - `users/noah/lib/key/runtime/core/runtime.c`
  - `users/noah/lib/key/runtime/core/runtime.h`
  - release planning and slot release helpers
  - owner ledgers for held actions, repeats, layers, keyboard modifiers, and PD modes
- The biggest compatibility patch is `users/noah/lib/compat/qmk_combo_origin.c`.
- Modifier mask/replay behavior is split across action dispatch, key process, delayed action, and PD mode modules.
- RGB and macro code are comparatively cohesive, with mostly low-risk duplicate helper shapes.

## In Flight

- None.

## Verification

- `git diff --check` passed for the tracked diff.
- `git diff --check --no-index /dev/null review/2026-04-27-review-01/userspace-architecture-review.md` produced no whitespace diagnostics for the new review file.
- `git diff --check --no-index /dev/null review/2026-04-27-review-01/progress.md` produced no whitespace diagnostics for the new progress file.
- Host tests and `qmk compile` are intentionally skipped for this review-note-only pass because no runtime source, authored profile data, build wiring, generated firmware input, or behavior changed.

## Next Steps

1. Freeze current behavior with targeted host tests before changing runtime code.
2. Write an ownership authority map for core leases and subsystem ledgers.
3. Consolidate release behavior behind one release planner API.
4. Split `runtime.c` into smaller internal modules after tests prove parity.
5. Tighten the `qmk_combo_origin` compatibility contract and coverage.
6. Centralize keyboard modifier mask/replay policy.
7. Clean up low-risk duplicate helpers after the authority and release work is stable.
