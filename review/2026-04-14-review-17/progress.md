# Implementation Progress

This file tracks the retrospective audit recorded in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Retrospective audit of the review chain

Completed in this pass:

- started with `git status --short`
- read the newest existing review folder in `review/2026-04-14-review-16/`
- reviewed the same-day review chain from `review-10` through `review-16`
- inspected the major landed runtime, key-runtime, action, RGB, compile-gate,
  and host-runner changes in the current tree
- compared the review claims against the code that now exists
- recorded a retrospective assessment in `review/2026-04-14-review-17/`
- reconciled `review/2026-04-14-review-13/` so it now clearly separates its
  audit-time findings from the later landed structure update
- replaced the exclusion-driven host-runner helper layer in
  `tests/host/noah_source_manifest.sh` with explicit positive manifests:
  one shared base list plus runner-specific add-on lists for
  `runtime_debug`, `key_runtime_modifier_hold`, and `key_runtime_scenario`

Key conclusions recorded in this review:

- no must-fix correctness regressions were found in the landed refactor work
- the code changes were mostly good and materially improved the userspace design
- the weakest part of the sequence was review integrity: the review chain
  repeatedly declared closure before the boundary work was fully sealed and
  re-audited
- the remaining technical debt from this sequence is optional maintainability
  work, not an open runtime correctness issue

Verification run in this pass:

- `git status --short`
- `git log --since='3 days ago' --oneline --decorate --stat -- review tests/host users/noah keyboards/bastardkb/charybdis/4x6/keymaps/noah`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- `run_feature_gate_compile_tests.sh` passed
- the reviewed runner checks passed
- the full host suite passed
- the firmware compile passed
- no sibling workspace folders were edited

Next steps:

- keep future runner-helper changes on explicit positive manifests rather than
  reintroducing subtraction-based support graphs
- keep future review closures strict: do not mark architecture work resolved
  until compile gates, tests, docs, and the active review note all agree
