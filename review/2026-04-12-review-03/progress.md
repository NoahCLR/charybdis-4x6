# Implementation Progress

This file tracks the review pass captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-12

Completed in this pass:

- Audited the current `noah` userspace after the follow-up work already
  recorded in
  [2026-04-12-review-02](../2026-04-12-review-02/userspace-architecture-review.md).
- Re-read the authored/runtime split, handled-key runtime flow, ownership
  modules, pd-mode manifest/registry/state/handlers, RGB staging, build
  manifest wiring, compile gates, and maintainer docs.
- Wrote a new review focused on long-term software structure for a fixed board
  rather than generic firmware advice.
- Confirmed that the current architecture should stay board-specific and
  data-driven rather than moving toward a plugin framework or hardware-generic
  abstraction layer.
- Identified the main current architecture risks as:
  - handled-key public contracts still spanning several adjacent reduction and
    effect surfaces
  - pd-mode state conflating authoritative local state with mirrored split-sync
    display state
  - `pd_mode_handlers.c` continuing to accumulate per-mode state in one file
  - missing saturation/debug coverage around effect-plan overflow and
    cross-subsystem live tracing

Verification run in this pass:

- `git status --short`

Verification intentionally not run in this pass:

- no host tests
- no firmware compile

Reason:

- this pass only added review documentation under `review/`
- no runtime, keymap, compat, or build-surface source files changed

Workspace scope:

- changed only this repo
- no sibling workspace folders were modified

Follow-up implementation completed in this pass:

- Split pd-mode shared state into explicit local-authority and mirrored
  display fields in `runtime_shared_state`.
- Replaced ambiguous pd-mode state queries with explicit local/display query
  names and updated pd-mode policy, split sync, pointer-layer policy, key
  runtime, and RGB consumers to use the correct surface.
- Kept split-runtime packets authoritative by syncing local pd-mode state from
  the master half while applying remote snapshots only to mirrored display
  state on the slave half.
- Extended host coverage so pd-mode tests assert local vs display semantics,
  RGB rendering uses mirrored slave-half pd-mode state, automouse rendering
  clamps on mirrored locked display state, and runtime debug snapshots cover
  both local and mirrored pd-mode storage.

Verification run for the follow-up implementation:

- `git status --short`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Recommended next implementation work:

1. Harden the handled-key public contract by separating handled-key
   resolution into its own implementation file, making the public handled-key
   view fully resolved, and reducing effect-pipeline naming overlap.
2. When the next bespoke pd mode lands, stop growing `pd_mode_handlers.c` and
   split mode-local implementations into per-mode translation units.
3. Add explicit overflow/saturation tests for the handled-key effect pipeline
   and a small shared runtime trace sink for cross-subsystem debugging.
