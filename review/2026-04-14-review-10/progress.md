# Implementation Progress

This file tracks the architecture review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Deep userspace architecture review

Completed in this pass:

- started with `git status --short`
- read the newest existing review folder before beginning a new review pass
- mapped the current userspace structure under `users/noah/`
- reviewed the authored keymap boundary under
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/`
- inspected the main runtime seams for:
  - hook entry points
  - handled-key runtime state and transitions
  - action classification and dispatch
  - pd-mode registry/state/policy
  - layer ownership and held-action ownership
  - RGB stage orchestration
  - test and compile-gate coverage
- wrote a new review folder:
  `review/2026-04-14-review-10/`

Key findings recorded in this review:

- should-fix: runtime state ownership is split between the global
  `runtime_shared_state` aggregate and several module-private static state
  stores, which keeps reset/debug/integration work non-local
- should-fix: action extensibility is still a manually synchronized closed set
  across multiple core files, unlike the cleaner manifest-driven pd-mode design
- should-fix: top-level userspace orchestration is still a hard-coded ordered
  pipeline in `process_record`, init/scan wiring, and RGB stage composition
- optional: the authored keymap surface is data-driven but too consolidated in
  one large `keymap.c`

Areas assessed as solid in this pass:

- the `noah_runtime.h` vs `noah_keymap.h` boundary is clear and enforced
- the handled-key engine is a real reducer/effect-plan state machine rather
  than ad hoc QMK hook logic
- the pd-mode subsystem is the strongest extensibility model in the tree
- test and compile-gate discipline is strong enough to support incremental
  architecture work safely

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- full host suite passed
- firmware build passed and produced
  `.build/bastardkb_charybdis_4x6_noah.uf2`

Checks intentionally skipped in this pass:

- none

Workspace scope:

- no sibling workspace folders were edited
- all changes in this pass are confined to
  `charybdis-4x6/review/2026-04-14-review-10/`

### Runtime context Milestone 1

Completed in this pass:

- added `users/noah/lib/state/runtime/runtime_context.h` as the canonical
  singleton-owned runtime storage surface
- rehomed mutable state for:
  - `runtime_shared_state`
  - layer ownership
  - held-action ownership
  - held-repeat ownership
  - keyboard modifier ownership
  - runtime trace
- preserved the existing module APIs and the
  `runtime_shared_state.h` surface as compatibility wrappers into the runtime
  context
- changed `noah_runtime_debug_snapshot()` into a true context-backed aggregate
  snapshot instead of a manual cross-module callback collector
- changed `noah_runtime_reset_for_test()` to reset the singleton context, while
  preserving the QMK layer/mod/report cleanup behavior
- moved repo-owned host tests off
  `runtime_shared_state_reset(&noah_runtime_shared_state)`
- removed local host-test `*_debug_snapshot()` / `*_reset_for_test()` fallback
  stubs that were only compensating for the old split ownership model
- updated standalone host runners that now need the context-backed shared-state
  object linked in

Architecture result after this pass:

- runtime-owned mutable state now has one canonical owner
- public behavior and authored keymap semantics are unchanged
- the remaining state debt is no longer “private statics everywhere”; it is
  boundary cleanup around the compatibility alias and a few direct shared-state
  reads that still need to be narrowed in Milestone 2

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_layer_ownership_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_index_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- all targeted Milestone 1 runners passed
- full host suite passed
- firmware build passed and produced
  `.build/bastardkb_charybdis_4x6_noah.uf2`

Workspace scope for this pass:

- no sibling workspace folders were edited
- changes are confined to `charybdis-4x6/`
- runtime files, host tests, host runners, and the active review folder were
  updated together

Next steps:

- Milestone 2: remove remaining repo-owned direct reads of the compatibility
  alias, especially in pd-mode/runtime helpers, and narrow
  `runtime_shared_state.h` to an explicit compatibility shim
- after that, convert action kinds to a single-source definition list, reusing
  the pd-mode manifest pattern where possible
- only then tackle declarative hook registration and authored keymap file
  decomposition
