# Userspace File-Map Review Prompt

You are performing an exhaustive file-map review of the local userspace C/H
surface for a custom keyboard firmware.

This is not an implementation pass. Do not change runtime, keymap, test, or
documentation behavior while performing the review. Supporting files may be
read for context, but only the scoped userspace C/H files belong in the
exhaustive ledger.

Before reviewing:
- Follow `AGENTS.md`.
- Start with `git status --short`.
- Read the newest review folder under `review/`.
- If the newest review folder is contradictory, reconcile it before using it
  as source-of-truth context.
- Open a new sortable review folder when this review type is materially
  different from the current active thread. State why the previous folder was
  not continued.
- Freeze the review corpus with:
  `find users/noah -type f \( -name '*.c' -o -name '*.h' \) | sort`

Review scope:
- Review every `.c` and `.h` file under `users/noah/`.
- Exclude keymap files, host tests, docs, generated media, ignored artifacts,
  and sibling workspace folders from the exhaustive file ledger.
- Read supporting tests, docs, manifests, and QMK compatibility code only as
  needed to classify ownership, dependencies, and coverage.

For each scoped file, record:
- purpose
- owner subsystem
- public/internal role
- build or feature-gate relationship
- important dependencies
- test or compile-gate coverage references
- verdict

Cross-cutting passes:
1. Header-boundary rules for `noah_keymap.h`, `noah_runtime.h`, and
   `*_internal.h`.
2. Source-manifest and feature-gated build coverage.
3. Low-reference exported functions and types.
4. Runtime state ownership and lifecycle flow.
5. QMK, VIA, pointing, and RGB compatibility contracts.
6. Documentation and review-note alignment when supporting context is needed.

Output requirements:
- Create or update the active review folder with:
  - `progress.md`
  - `userspace-architecture-review.md`
  - `userspace-file-map.md`
- Put findings first in `userspace-architecture-review.md`, ordered by
  severity.
- Classify findings as `must-fix`, `should-fix`, or `optional cleanup`.
- Include file references for every finding.
- If an area is solid, say so explicitly instead of inventing issues.
- End with a recommended next refactor sequence.

Verification:
- Run `sh tests/host/run_feature_gate_compile_tests.sh`.
- Run `sh tests/host/run_all_host_tests.sh`.
- Run `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.
- Run `git diff --check`.
- If any command fails, diagnose the likely root cause and record it as a
  review finding; do not mutate production code unless a separate
  implementation request authorizes it.
