# Progress

## 2026-04-14 Initial Review Start

- Started a fresh active review thread in `review/2026-04-14-review-18/`.
- This folder was created instead of continuing an older review because the prior review history was intentionally removed and the user explicitly requested a fresh start.
- Review prompt used: `prompts/initial-architecture-review.md`.

## Completed Work

- Audited the current userspace/runtime structure across `users/noah/`, the keymap-owned authoring surface under `keyboards/bastardkb/charybdis/4x6/keymaps/noah/`, and the build/test enforcement scripts under `tests/host/`.
- Wrote the initial architecture review in `userspace-architecture-review.md`.
- Captured a verified baseline for the current tree before opening any new refactor thread work.

## Findings Snapshot

- `must-fix`: none in the current tree.
- `should-fix`: the positional registry DSLs are now the main maintainability risk; `key_runtime_internal.h` is still too broad for an internal seam.
- `optional cleanup`: the keymap materialization macros and mixed-responsibility pointing bridge are acceptable now but are the next likely growth hotspots.

## Verification

- Passed: `sh tests/host/run_all_host_tests.sh`
- Passed: `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Sibling workspace folders touched: none

## Next Steps

1. Choose which `should-fix` item to land first in this thread.
2. If the action-kind registry is chosen first, preserve one source of truth and existing test coverage while making the row shape more explicit.
3. After remediation starts, keep updating this folder with finding status and verification instead of opening a same-topic duplicate review folder.
