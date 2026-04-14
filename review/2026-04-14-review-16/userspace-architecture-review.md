# Refactor Follow-Up Review

Date: 2026-04-14

Status: follow-up quality audit of the landed compile-gate scope and
manifest-derived runner cleanup after `review/2026-04-14-review-15/`.

Scope:

- whether the broadened compile gate now seals the full repo-owned production
  runtime boundary consistently
- whether the manifest-derived runner helpers actually achieved a cleaner,
  more stable test-build seam
- whether the review chain now matches the code that exists

## Findings

### No must-fix correctness regressions found in the current tree

The current tree still passes the full host suite and firmware compile, and I
did not find a concrete runtime behavior regression in the reviewed key-runtime,
pd-mode, or RGB paths.

### No remaining should-fix issues in the current tree

The strongest open items in this follow-up are now optional maintainability and
review-history cleanup, not a production boundary hole or behavioral regression.

### Landed follow-up: runtime sealing compile gate now uses one production-scope definition

References:

- `tests/host/run_feature_gate_compile_tests.sh:15`
- `tests/host/run_feature_gate_compile_tests.sh:16`
- `tests/host/run_feature_gate_compile_tests.sh:17`
- `tests/host/run_feature_gate_compile_tests.sh:19`
- `tests/host/run_feature_gate_compile_tests.sh:43`
- `tests/host/run_feature_gate_compile_tests.sh:69`
- `tests/host/run_feature_gate_compile_tests.sh:88`
- `tests/host/run_feature_gate_compile_tests.sh:111`
- `tests/host/run_feature_gate_compile_tests.sh:139`

Reasoning:

- The compile gate now defines one repo-owned production scope and one derived
  repo-owned code scope up front, then routes the full runtime-sealing pass
  through shared helper functions.
- The earlier inconsistency is gone: removed runtime aggregate headers, removed
  pd runtime headers, runtime internal storage headers, pd runtime internal
  storage headers, and the newer key-runtime checks now all hang off the same
  production-scope mechanism.
- That closes the remaining gap where keymap-owned production code could bypass
  the older runtime-sealing checks even though key-runtime checks already
  treated that same path as production-owned.

Why this matters:

- the boundary contract is now mechanically consistent across the full
  runtime-sealing section
- future header moves only need to update the shared helper layer and allowlist
  rules, not one special-case scan path per subsystem
- the compile gate now matches the intended claim of repo-owned production
  sealing rather than a key-runtime-only variant

### Optional cleanup: the runner abstraction is cleaner, but the source graphs are still implicit negative lists rather than pinned runner manifests

References:

- `tests/host/noah_source_manifest.sh:34`
- `tests/host/noah_source_manifest.sh:61`
- `tests/host/noah_source_manifest.sh:99`
- `tests/host/noah_source_manifest.sh:114`
- `tests/host/noah_source_manifest.sh:124`
- `tests/host/run_runtime_debug_tests.sh:9`
- `tests/host/run_key_runtime_modifier_hold_integration_tests.sh:9`
- `tests/host/run_key_runtime_scenario_tests.sh:9`

Reasoning:

- The new helper layer does remove the old hand-maintained mini-manifest, which
  is an improvement.
- But each runner now means “`NOAH_COMMON_SOURCES` minus this exclusion set”,
  not an explicit positive source manifest for that runner shape.
- That means adding a new file to `NOAH_COMMON_SOURCES` silently changes one or
  more runner binaries unless every wrapper exclusion list is reviewed in the
  same pass.
- The current implementation is therefore cleaner than the previous duplicated
  lists, but it still makes the intended runner source graphs implicit.

Why this matters:

- the cleanup reduced duplication, but it did not fully pin the reviewed test
  seams to explicit build surfaces
- maintainers still have to reason about exclusion drift rather than reading a
  small, positive runner manifest
- this is a maintainability tradeoff, not a correctness regression today

### Optional cleanup: review history still contains one contradictory same-day folder even though newer reviews are now coherent

References:

- `review/2026-04-14-review-13/userspace-architecture-review.md:15`
- `review/2026-04-14-review-13/userspace-architecture-review.md:46`
- `review/2026-04-14-review-13/userspace-architecture-review.md:104`
- `review/2026-04-14-review-13/progress.md:48`
- `review/2026-04-14-review-15/userspace-architecture-review.md:23`

Reasoning:

- `review-15` is a coherent source of truth for the latest landing and is more
  accurate than the older notes.
- But `review-13` still contains a landed-structure summary immediately above
  stale findings that describe the pre-remediation tree.
- That means the review chain is usable, but the historical trail is still
  noisier than it needs to be.

Why this matters:

- follow-up archaeology still requires readers to mentally reconcile one
  contradictory review folder in the middle of the same-day sequence
- the latest notes are accurate, but the history is not yet self-consistent

## Solid Areas

- The key-runtime-specific boundary cleanup did materially land.
  `key_runtime_index.h` is gone, the remaining index readers are internal-only,
  and the key-runtime compile-gate checks now cover keymap-owned production
  code as well as `users/noah`.
- The higher-level harness cleanup still looks solid. The scenario and
  integration harnesses continue to stay on public seams rather than private
  process headers.
- The runner cleanup improved the tree even with the exclusion-list caveat.
  The reviewed runners are easier to maintain now than when each carried its
  own long hand-written support graph.
- I still did not find a concrete semantic regression in the runtime,
  orchestration, or RGB ordering behavior from the landed refactor chain.

## Verification

Commands run in this audit pass:

- `git status --short`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Current conclusion:

- no must-fix or should-fix correctness regressions found in the reviewed
  refactor
- the remaining issues are optional maintainability and review-history cleanup
- the runner cleanup is directionally good, but still not a fully explicit test
  manifest model
