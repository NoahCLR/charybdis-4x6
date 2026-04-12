# Implementation Progress

This file tracks the review pass captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-12

Completed in this pass:

- Audited the current `noah` userspace after the follow-up work already landed
  in
  [2026-04-12-review-01](../2026-04-12-review-01/userspace-architecture-review.md).
- Re-read the authored/runtime split, handled-key runtime flow, pd-mode
  registry and handlers, ownership/state modules, RGB runtime, host scenario
  harness, and current maintainer docs.
- Wrote a new review focused on current-state software architecture rather than
  repeating already-completed recommendations.
- Identified the main remaining architecture risks as:
  - action emission still carrying hidden key-runtime mutation policy
  - pd-mode lifecycle extensibility still depending on registry-owned special
    cases for unusual modes
  - handled-key effect interfaces still exposing overlapping request/result/
    transition layers
  - scenario testing still mirroring runtime contracts instead of consuming the
    shared debug/effect surfaces directly

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

Recommended next implementation work:

1. Make action-emission policy explicit and remove ad hoc fallback-hold
   activation from pd-mode output helpers.
2. Move pd-mode lifecycle hooks into the mode definition surface so unusual
   modes stop requiring registry switch edits.
3. Rebuild the scenario harness on `key_runtime_effect_t` and
   `noah_runtime_reset_for_test()` before adding broader multi-subsystem
   scripted scenarios.
