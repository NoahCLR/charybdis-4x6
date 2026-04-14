# Implementation Progress

This file tracks the architecture review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-13

### Architecture review pass

Completed in this pass:

- Started with `git status --short` and confirmed the worktree was clean.
- Confirmed the current review lineage under `review/` and opened the next
  sortable same-day review folder:
  `review/2026-04-13-review-07/`.
- Re-read the newest prior architecture review in
  `review/2026-04-13-review-06/` before auditing the live code.
- Reviewed the current userspace structure across:
  - hook entry points and runtime init
  - handled-key resolution, slot lifecycle, transition planning, and shared
    state ownership
  - action classification, lifecycle dispatch, and authored validation
  - pd-mode manifest, registry, state, lifecycle, sync, and policy helpers
  - macro dispatch, payload IR, VIA defaults, and QMK playback compatibility
  - RGB stage orchestration, runtime-debug surfaces, and host-test fixtures
- Wrote a new architecture review focused on long-term extensibility and
  software boundaries for the current fixed hardware.

Key findings recorded in this review:

- high priority: action semantics are still hard-coded in one classifier and
  then reinterpreted by lifecycle dispatch, authored validation, and preflight
  routing, so new action kinds still require cross-module edits
- high priority: pd-mode identity and remote-display precedence still depend
  partly on manifest order and shared policy helpers instead of a fully
  explicit mode-owned descriptor/sync contract
- medium priority: key-runtime cross-key coordination still works by repeated
  whole-table sweeps, which keeps active/pending ownership implicit
- medium priority: the macro subsystem shares one IR, but hardcoded macros,
  VIA defaults, and live VIA playback still do not share one source/cache
  abstraction

Areas assessed as strong in this pass:

- the authored keymap/runtime split remains real and understandable
- the handled-key runtime keeps clear slot/effect boundaries
- QMK compatibility work is mostly centralized under `users/noah/lib/compat/`
- host tests, `runtime_debug`, and `runtime_trace` provide strong architecture
  feedback loops

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- full host suite passed
- firmware build passed

Checks intentionally skipped in this pass:

- none

Workspace scope:

- no sibling workspace folders were edited
- all changes are confined to
  `charybdis-4x6/review/2026-04-13-review-07/`

Next steps:

- move action semantics behind one action-ops surface so validation,
  preflight, and lifecycle dispatch share one contract
- make pd-mode display precedence and split-sync identity explicit instead of
  deriving them from manifest order
- add explicit active/pending registries to the key runtime so new cross-key
  rules do not require more whole-table sweeps
- add a shared macro source/cache abstraction over hardcoded, VIA-default, and
  live VIA-backed macro payloads

### Action contract first pass

Completed in this pass:

- Started with `git status --short` and continued from the in-flight review
  folder plus the new action/runtime changes for this follow-up.
- Extended `users/noah/lib/action/action_dispatch.h` so
  `noah_action_desc_t` now caches:
  - authored key-behavior keycode support
  - authored tap support
  - authored press-and-hold support
  - authored non-press-and-hold hold support
  - direct-press consumption for preflight-owned actions
- Switched `users/noah/lib/key/interaction/key_behavior_lookup.c` to use the
  shared descriptor-owned authored-surface helpers instead of re-deriving raw
  layer-action exceptions locally.
- Switched `users/noah/lib/key/interaction/keymap_validation.c` to use the same
  descriptor-owned behavior-keycode support contract for keymap layer-action
  validation.
- Switched `users/noah/lib/key/runtime/key_runtime_preflight.c` to use the new
  `noah_action_desc_consumes_direct_press(...)` capability instead of keeping a
  separate layer-lock / pd-mode-lock special case.
- Expanded `tests/host/action_dispatch_test.c` so the new descriptor contract
  is asserted directly for:
  - layer locks
  - owned momentary layers
  - layer taps
  - unsupported raw layer actions
  - qmk behavior keycodes
  - pd-mode hold and lock actions
  - macros, keymap custom actions, and literals

Contracts touched in this pass:

- shared action descriptor now owns authored-surface support for validation
  callers
- shared action descriptor now owns direct-press consumption for preflight
  callers
- lifecycle dispatch behavior remains unchanged; this pass narrows the
  remaining action refactor to tap/press/release execution ownership

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- targeted action, key-behavior, keymap-validation, and preflight suites passed
- full host suite passed
- firmware build passed

Workspace scope:

- no sibling workspace folders were edited
- changes in this pass touched:
  - `users/noah/lib/action/action_dispatch.h`
  - `users/noah/lib/key/interaction/key_behavior_lookup.c`
  - `users/noah/lib/key/interaction/keymap_validation.c`
  - `users/noah/lib/key/runtime/key_runtime_preflight.c`
  - `tests/host/action_dispatch_test.c`
  - this review folder

Next steps:

- move action tap/press/release branching out of `action_lifecycle.c` and onto
  an action-ops surface behind `noah_action_desc_t`
- after that lands, re-check whether handled-key fallback and implicit-hold
  logic should consume action-owned helpers instead of inspecting action kinds
  directly

### Action ops execution pass

Completed in this pass:

- Started with `git status --short` and continued from the in-flight review
  folder plus the action-contract changes already present in the worktree.
- Added `NOAH_ACTION_KIND_COUNT` in
  `users/noah/lib/action/action_dispatch.h` so action-kind-owned tables can be
  indexed explicitly.
- Reworked `users/noah/lib/action/action_lifecycle.c` so tap/press/release
  execution now routes through a `noah_action_ops_t` table keyed by
  `noah_action_kind_t` instead of one large branch tree in each top-level
  action function.
- Kept the existing one-shot press path intact as the compatibility shim for:
  - layer locks
  - pd-mode lock taps
  - macro / VIA playback taps
- Preserved the existing behavior contracts for:
  - unsupported raw layer actions logging and early no-op behavior
  - owned momentary layer press/release ownership
  - QMK behavior synthetic dispatch
  - keymap custom synthetic dispatch
  - literal / pd-mode fallback press and release behavior

Contracts touched in this pass:

- action lifecycle dispatch is now owned by a kind-indexed ops table rather
  than repeated tap/press/release branching
- descriptor-owned capability queries from the previous pass remain the shared
  contract for validation and preflight callers
- handled-key fallback and implicit-hold policy remain unchanged in this pass

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- targeted action, preflight, and key-behavior lookup suites passed
- full host suite passed
- firmware build passed

Workspace scope:

- no sibling workspace folders were edited
- changes in this pass touched:
  - `users/noah/lib/action/action_dispatch.h`
  - `users/noah/lib/action/action_lifecycle.c`
  - this review folder

Next steps:

- decide whether handled-key fallback and implicit-hold logic should consume
  narrower action-owned policy helpers instead of direct kind checks

## 2026-04-14

### Momentary-layer RGB feedback contract pass

Completed in this pass:

- Started with `git status --short` and continued from the clean worktree.
- Narrowed the remaining RGB feedback inconsistency for owned momentary-layer
  actions:
  - the normal hold threshold-window overlay bug had already been removed in
    the previous pass
  - pending multi-tap hold and long-hold promotion still opted momentary-layer
    actions into trigger pulses
- Updated
  `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c` so
  pending multi-tap hold and long-hold promotion no longer request a feedback
  pulse for `PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer))`.
- Expanded `tests/host/key_runtime_transition_test.c` with explicit transition
  coverage for:
  - pending multi-tap hold -> momentary-layer activation without feedback pulse
  - pending multi-tap long-hold -> momentary-layer activation without feedback
    pulse
- Updated the main docs contract in:
  - `README.md`
  - `docs/RGB_CONFIG.md`
  - `docs/INTERACTION_MODEL.md`

Contracts touched in this pass:

- owned momentary-layer actions now use only preview-layer color before
  activation and real layer color after activation
- the no-pulse rule now applies consistently across:
  - ordinary hold activation
  - pending multi-tap hold activation
  - pending multi-tap long-hold promotion
- non-layer hold and long-hold actions keep the existing pulse / active-overlay
  semantics

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- targeted transition, feedback, and RGB render suites passed
- full host suite passed
- firmware build passed

Workspace scope:

- no sibling workspace folders were edited
- changes in this pass touched:
  - `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c`
  - `tests/host/key_runtime_transition_test.c`
  - `README.md`
  - `docs/RGB_CONFIG.md`
  - `docs/INTERACTION_MODEL.md`
  - this review folder

Next steps:

- if desired, make the preview-layer helper surface more explicit so RGB does
  not need to infer momentary-layer special cases from hold contracts
- otherwise return to the higher-priority pd-mode contract work
- if that coupling stays acceptable, move on to the next architecture item:
  pd-mode explicit identity / precedence in the sync and policy contract

### Transparent authored-action support pass

Completed in this pass:

- Started with `git status --short` and continued from the in-flight handled-key
  transparency work already in the tree.
- Extended `users/noah/lib/key/interaction/handled_key.c` so `KC_TRNS` is now
  resolved natively for all authored action fields at the handled-key layer:
  - tap fields still resolve the lower active layer's tap action at the same
    physical position
  - hold and long-hold fields now resolve the lower active layer's same-tier
    action target at the same physical position
  - explicit lower `KC_TRNS` fields keep chaining downward until a real action
    or `KC_NO` boundary is found
- Kept helper ownership explicit:
  - transparent tap keeps the current tap branch timing and repeat behavior
  - transparent hold and long-hold keep the current helper mode
    (`PRESS_AND_HOLD_UNTIL_RELEASE`, `REPEAT_WHILE_HELD`, and so on) while only
    the action target falls through
- Updated the runtime materialization sites in
  `users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.c` and
  `users/noah/lib/key/runtime/slot/key_runtime_slot.c` so cached slot bindings
  and pending multi-tap resolution both consume the new position-aware
  transparent hold/long-hold helpers.
- Expanded `tests/host/key_behavior_lookup_test.c` so the handled-key contract
  is asserted directly for:
  - lower plain-key tap and hold fallthrough
  - bare `LT()` tap and press-and-hold fallthrough
  - unsupported hold-other fallthrough over lower `LT()` hold targets
  - lower authored transparent chaining
  - lower multi-tap branch selection by current tap count
  - lower explicit long-hold fallthrough
- Updated the maintainer-facing authoring docs and comments in:
  - `users/noah/lib/key/interaction/key_behavior.h`
  - `docs/INTERACTION_MODEL.md`
  - `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`

Contracts touched in this pass:

- `KC_TRNS` is now a native field-level fallback across authored tap, hold, and
  long-hold actions
- tap fallthrough still resolves lower tap behavior
- hold and long-hold fallthrough now resolve lower same-tier action targets
  without changing the current helper mode
- pending multi-tap reuse now consumes the same transparent action resolution
  rules as the initial handled press

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- targeted handled-key, slot, transition, scenario, release-matrix, and
  integration suites passed
- full host suite passed
- firmware build passed

Workspace scope:

- no sibling workspace folders were edited
- changes in this pass touched:
  - `users/noah/lib/key/interaction/handled_key.c`
  - `users/noah/lib/key/interaction/handled_key.h`
  - `users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.c`
  - `users/noah/lib/key/runtime/slot/key_runtime_slot.c`
  - `tests/host/key_behavior_lookup_test.c`
  - `tests/host/key_runtime_slot_test.c`
  - `tests/host/key_runtime_preflight_test.c`
  - `tests/host/key_runtime_feedback_test.c`
  - `tests/host/key_runtime_admission_test.c`
  - `tests/host/runtime_debug_test.c`
  - `users/noah/lib/key/interaction/key_behavior.h`
  - `docs/INTERACTION_MODEL.md`
  - `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`
  - this review folder

Next steps:

- decide whether transparent authored actions should eventually inherit lower
  helper mode as well as lower action target, or whether the current
  helper-mode-preserving contract is the intended long-term surface
- if helper-mode preservation stays correct, consider moving the new
  transparent-action resolution behind a narrower handled-key materialization
  helper so slot callers stop assembling position-aware binding pieces by hand
