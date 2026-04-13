# Implementation Progress

This file tracks the architecture review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-13

Completed in this pass so far:

- Started from a clean worktree and confirmed the active review history under
  `review/`.
- Re-read the newest existing review before opening a distinct same-day review.
- Reviewed the current userspace runtime surfaces under `users/noah/`,
  including:
  - hook entry points and runtime init
  - source/build manifests
  - key interaction and key runtime modules
  - pointing runtime, registry, policy, and mode surfaces
  - shared state, ownership, macro, and action modules
  - maintainer docs and host-test surfaces
- Opened `review/2026-04-13-review-02/` for a deeper architecture pass focused
  on software extensibility rather than folder layout.
- Wrote a concrete userspace architecture review covering:
  - architecture and separation of concerns
  - modularity and extensibility
  - abstractions and interfaces
  - code organization
  - state management and flow
  - scalability risks
  - testing/debuggability
  - actionable refactoring recommendations

Key findings recorded in this review:

- the strongest remaining risk is semantic duplication, not directory layout
- multi-tap behavior bypasses the main handled-key abstraction
- action meaning is repeatedly inferred from raw keycodes instead of a typed
  descriptor
- `state/runtime/` still owns key-runtime internals that belong to the key
  domain
- pd-mode policy is manifest-driven at the edge but fragmented in the core
- the macro DSL implementation is structurally monolithic

Verification run so far:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git status --short`

Next steps:

- decide whether to turn the contract recommendations in
  `userspace-architecture-review.md` into an implementation review/pass
- if that work is started, prioritize the interaction-contract and
  action-descriptor seams before any new feature additions

## 2026-04-13: Recommendation 1 started

Completed in this pass:

- Landed the first implementation step for recommendation 1
  ("centralize interaction resolution").
- Extended handled-key resolution with a tap-count-aware entry point:
  `handled_key_lookup_tap_count(keycode, tap_count)`.
- Expanded `handled_key_view_t` so the resolved contract now carries:
  - tap repeat count
  - authored-step presence
  - whether higher tap counts still exist
  - whether a later tap resolves immediately on press
- Updated `multi_tap_t` to cache the resolved tap outcome for the current tap
  count instead of re-deriving it from raw behavior lookups during flush and
  hold resolution.
- Updated pending multi-tap runtime paths so later tap counts are resolved
  through the handled-key contract, while the first tap still seeds from the
  already-bound active slot state.
- Removed the remaining pending multi-tap release/scan dependency on raw
  authored behavior lookups by using cached slot binding and multi-tap
  semantics instead.
- Updated the affected host fixtures so their manual pending multi-tap state
  matches the new cached interaction contract.

Contracts touched:

- `users/noah/lib/key/interaction/handled_key.[ch]`
- `users/noah/lib/key/interaction/multi_tap_engine.[ch]`
- `users/noah/lib/key/runtime/key_runtime_state.h`
- `users/noah/lib/key/runtime/slot/key_runtime_slot*.c`

Verification run in this pass:

- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Next steps:

- continue recommendation 1 by moving more first-tap-only slot metadata behind
  the same resolved interaction surface instead of mixing slot binding and
  handled-key state
- start recommendation 2 so action meaning stops being rediscovered from raw
  keycodes across runtime, feedback, and policy modules
