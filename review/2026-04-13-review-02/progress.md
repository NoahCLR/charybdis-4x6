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
