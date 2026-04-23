# Userspace Architecture Review

## Scope

This review records an exhaustive file-map sanity pass over
`users/noah/**/*.c` and `users/noah/**/*.h` as of 2026-04-23. The scoped corpus
contains 160 files: 76 `.c`, 84 `.h`, and 19,677 LOC.

This is review-only. No firmware APIs, runtime behavior, keymap data, host
tests, or normal documentation were changed during the review.

The review did not continue `review/2026-04-23-review-02/` because that folder
tracks the legacy multi-tap and dead-code cleanup thread. This folder is a new
review type: a whole-userspace C/H file ledger and architecture sanity map.

## Findings

### Optional Cleanup

- `optional cleanup`: `users/noah/lib/key/runtime/core/runtime.c` is coherent
  but very large. It owns event reduction, press-token lifecycle, tap-series
  state, lease/projection state, pending-release queues, release planning,
  scan planning, debug queries, and projection snapshots in one 3,665-line
  source file. The current boundaries make sense and are covered by the key
  runtime host matrix, so this is not a correctness issue. If future runtime
  work needs to change this area, split it behind the existing
  `key_runtime_core_*` API into reducer, lease/projection, release-planning,
  scan-planning, and debug-snapshot files with targeted tests before and after
  each move.

## Non-Findings

- No `must-fix` findings were found.
- No `should-fix` findings were found.
- All 76 scoped userspace `.c` files are represented in the userspace source
  manifest. The only manifest path outside the scoped corpus is the keymap RGB
  config file, which is expected keymap-owned authored data.
- `noah_keymap.h` is only used by keymap-owned translation units in the checked
  tree. Runtime-owned userspace files do not include it.
- `noah_runtime.h` is used by `users/noah/hooks.c` and
  `users/noah/runtime_init.c`; keymap-owned files do not include it.
- Internal headers are used inside their owning subsystems or by narrow
  adjacent seams. The notable cross-subsystem internal dependency is intentional:
  `handled_key_policy.h` uses action-kind internals to keep action policy
  classification centralized.

## Current Structure Assessment

Root files provide the keymap authoring umbrella, runtime hook umbrella, global
config, QMK weak hook chaining, keymap materialization glue, and runtime init
ordering. Ownership is clear: `noah_keymap.h` faces authored keymap data, while
`noah_runtime.h` faces QMK hook wrappers.

Compatibility files isolate QMK, VIA, split-role, auto-mouse, pointing, and
combo-origin contracts under `users/noah/lib/compat/`. The layout matches the
repo guardrail that fork-specific QMK/VIA assumptions should stay centralized.

State files own shared runtime storage, modifier ownership, layer ownership,
runtime diagnostics, trace buffers, and split runtime synchronization. Runtime
state is reached through explicit accessors rather than scattered global
storage.

Action files centralize keycode description, dispatch, action-kind metadata,
lifecycle preflight, synthetic records, and owned keycode registration. Runtime
modules consume this surface instead of open-coding QMK action handling.

Key interaction files own authored behavior lookup, materialization,
transparent resolution, and profile validation. Key ownership files own held
actions and repeats. Key runtime files then consume these surfaces through
staged process, preflight, transition, press, release, scan, feedback, and core
state APIs.

Macro files are cleanly separated into hardcoded dispatch, reusable slot
provider, payload parse/IR/encode/run/decode, and VIA default/provider support.
The previous direct-run visitor path is gone, and the compiled IR path is the
single payload execution model.

Pointing files use a manifest-driven PD-mode registry, mode-owned handlers,
policy helpers, runtime state transitions, lifecycle hooks, and a small
pointing task/layer hook surface. Local/display state split is explicit and
feeds split sync and RGB.

RGB files use a stage pipeline: layer base, auto-mouse fade, preview, PD-mode
overlay, combo feedback, key feedback, and validation. Authored RGB config is
validated at post-init, while rendering code stays runtime-owned.

## Coverage Evidence

Passed verification:

- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

- Header boundaries are covered by `sh tests/host/run_feature_gate_compile_tests.sh`.
- Authored behavior/profile/RGB data contracts are covered by the validation
  runners and `sh tests/host/run_real_profile_validation_tests.sh` inside the
  full suite.
- Key runtime behavior is covered by the release matrix, modifier-hold, PD-mode
  integration, layer-lock, scenario, and harness runners inside the full suite.
- Pointing and PD mode behavior is covered by PD mode, PD mode handlers, PD
  runtime, pointer layer policy, split runtime sync, and PD-mode key-runtime
  integration runners inside the full suite.
- Macro and VIA behavior is covered by macro dispatch, macro payload, VIA macro
  defaults, VIA macro action lifecycle, action lifecycle, and QMK contract
  runners inside the full suite.
- RGB behavior is covered by RGB validation, RGB layer render, and RGB base
  underlay runners inside the full suite.

## Recommended Next Refactor Sequence

1. Keep normal feature work local to the current subsystem boundaries.
2. If key-runtime churn continues, first add focused coverage around the exact
   core reducer behavior being moved.
3. Split `key_runtime_core` only in small mechanical moves: projection/leases,
   release planning, scan planning, then debug snapshots.
4. After each move, run the targeted key-runtime runners, the feature gate
   compile tests, the full host suite, and firmware compile before marking any
   boundary finding resolved.
