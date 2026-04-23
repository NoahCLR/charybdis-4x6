# Progress

## 2026-04-23

### Review Opened

- Opened `review/2026-04-23-review-03/` for the userspace file-map review.
- Did not continue `review/2026-04-23-review-02/` because that folder tracks a
  legacy multi-tap and dead-code cleanup thread. This pass is a materially
  different whole-userspace C/H sanity map with no production code changes.
- Added the new prompt template `prompts/userspace-file-map-review.md`.

### Scope

- Exhaustive map is limited to `users/noah/**/*.c` and `users/noah/**/*.h`.
- Frozen corpus command:
  `find users/noah -type f \( -name '*.c' -o -name '*.h' \) | sort`
- Frozen corpus size: 160 files, 76 `.c`, 84 `.h`, 19,677 LOC.
- Supporting tests, docs, manifests, and keymap files were read only for
  ownership, dependency, and coverage context.
- No runtime, keymap, host-test, or normal documentation fixes were applied in
  this review pass.

### Completed Passes

- Read the previous newest review folder,
  `review/2026-04-23-review-02/`, and treated it as a separate legacy-cleanup
  history thread.
- Read every scoped userspace C/H file in subsystem order: root, compat, state,
  action, key, macro, pointing, and RGB.
- Checked header-boundary usage for `noah_keymap.h`, `noah_runtime.h`, and
  internal headers.
- Checked userspace source-manifest membership against all scoped `.c` files.
- Checked low-reference public declarations for obvious stale surfaces.
- Checked runtime state ownership and lifecycle flow across key runtime, PD
  mode, split sync, RGB, macro, and QMK/VIA compatibility surfaces.
- Checked supporting host tests and review/docs context where it clarified a
  subsystem contract.

### Findings

- No `must-fix` findings found.
- No `should-fix` findings found.
- One `optional cleanup` recommendation was recorded: future key runtime work
  should consider splitting `users/noah/lib/key/runtime/core/runtime.c` after
  behavior is stable, because it is a coherent but very large reducer/planner
  module.

### Verification

Passed:

- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

### Next Steps

1. Treat this userspace file-map review as closed.
2. If future implementation work is approved, start with the optional key
   runtime core decomposition only after adding targeted safety coverage for
   the reducer boundaries being moved.
3. Open a new sortable review folder for any post-closure architecture work.

### Closure Verification

- Closure checked with `prompts/closure-verification-review.md` on 2026-04-23.
- Closure verdict: close thread.
- No `must-fix` or `should-fix` findings remain.
- The optional `key_runtime_core` split is deferred as future work, not an open
  blocker.
- Closure verification passed:
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
  - `git diff --check`
