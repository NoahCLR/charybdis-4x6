# Refactor Retrospective Review

Date: 2026-04-14

Status: retrospective audit of the review chain and landed refactor work from
`review/2026-04-14-review-10/` through `review/2026-04-14-review-16/`.

Scope:

- whether the code changes from the last review sequence actually improved the
  userspace architecture
- whether the resulting runtime, boundary, and test seams are now high quality
- whether the review process itself told an accurate and maintainable story

## Findings

### No must-fix correctness regressions found in the current tree

The current tree passes the full host suite and firmware compile, and I did not
find a concrete runtime, pd-mode, RGB, or action-dispatch behavior regression in
the landed refactor chain.

### Should-fix: the review process repeatedly declared closure before the code and compile gates actually matched the claim

References:

- `AGENTS.md:16`
- `AGENTS.md:18`
- `review/2026-04-14-review-10/userspace-architecture-review.md:186`
- `review/2026-04-14-review-13/userspace-architecture-review.md:15`
- `review/2026-04-14-review-13/userspace-architecture-review.md:46`
- `review/2026-04-14-review-15/userspace-architecture-review.md:23`
- `review/2026-04-14-review-16/userspace-architecture-review.md:24`

Reasoning:

- `review-10` explicitly said runtime sealing was complete enough to stop
  treating it as active debt, but `review-13`, `review-15`, and `review-16`
  each found real remaining boundary gaps and required more code changes.
- The same architecture thread then spread across many same-day review folders,
  even though the repo guidance says the newest review folder should remain the
  primary source of truth and work that belongs to an existing review should
  continue in that folder.
- The implementation quality ended up better than the intermediate notes, but
  the review chain itself was not decision-complete when it claimed closure.

Why this matters:

- the code is now in a good place, but the reviews over-closed the work several
  times before the compile gate and public/private seams were actually done
- that created avoidable churn and made the review notes less trustworthy than
  the code and tests
- future architecture work should treat “resolved” as meaning “mechanically
  enforced and re-audited”, not “directionally improved”

### Landed follow-up: the reviewed host runners now use explicit positive manifests instead of exclusion-driven support graphs

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

- The runner helper layer no longer derives these binaries by subtracting
  exclusions from `NOAH_COMMON_SOURCES`.
- It now defines one explicit shared base list plus exact runner-specific
  add-on lists for `runtime_debug`, `key_runtime_modifier_hold`, and
  `key_runtime_scenario`.
- Those selected sources are still checked against the canonical manifest
  variables, so the helper remains mechanically tied to the userspace source of
  truth without silently widening a runner when `NOAH_COMMON_SOURCES` grows.

Why this matters:

- the reviewed runner binaries are now explicit and auditable
- support-graph drift is harder to introduce by accident
- the helper model is now clearer than both the old hand-maintained
  mini-manifest and the later exclusion-driven variant

### Landed follow-up: `review-13` is now explicitly framed as audit snapshot plus later landed update

References:

- `review/2026-04-14-review-13/userspace-architecture-review.md:15`
- `review/2026-04-14-review-13/userspace-architecture-review.md:46`
- `review/2026-04-14-review-13/userspace-architecture-review.md:104`
- `review/2026-04-14-review-13/progress.md:48`

Reasoning:

- `review-13` now explicitly separates the later landed structure summary from
  the audit-time findings and labels the findings as the pre-remediation
  snapshot.
- That keeps the historical content intact without forcing readers to infer
  which claims were about the audit moment and which were about the later tree.

Why this matters:

- the review chain is now materially easier to read as an engineering record
- later review folders no longer need readers to mentally repair `review-13`
  before the sequence makes sense

## Strong Areas

### The runtime and key-runtime boundary work was materially good

References:

- `users/noah/lib/key/runtime/key_runtime_api.h:13`
- `users/noah/lib/key/runtime/key_runtime_api.h:14`
- `users/noah/lib/state/runtime/runtime_debug.h:14`
- `users/noah/lib/key/runtime/key_runtime_debug.c:40`
- `tests/host/key_runtime_scenario_harness.c:9`
- `tests/host/key_runtime_scenario_harness.c:205`
- `tests/host/run_feature_gate_compile_tests.sh:15`
- `tests/host/run_feature_gate_compile_tests.sh:69`

Assessment:

- The public seams are now meaningfully narrower than they were at the start of
  the review chain.
- `key_runtime_api.h` is minimal and purpose-built.
- `runtime_debug.h` is now a semantic observation surface instead of a storage
  dump.
- Higher-level scenario coverage drives the runtime through public seams, and
  the compile gate now enforces the repo-owned production boundary in a way that
  matches the intended architecture.

### The orchestration cleanup was good because it added order visibility without hiding the contracts

References:

- `users/noah/runtime_init.c:28`
- `users/noah/runtime_init.c:39`
- `users/noah/runtime_init.c:51`
- `users/noah/lib/key/runtime/key_runtime_process.c:130`
- `users/noah/lib/rgb/core/rgb_runtime.c:60`
- `users/noah/lib/rgb/core/rgb_runtime.c:77`
- `tests/host/runtime_init_order_test.c:92`
- `tests/host/rgb_layer_render_test.c:489`

Assessment:

- The explicit stage tables are a net improvement here.
- They did not create a fake generic framework; they made order-sensitive code
  local and readable, then pinned that order with tests.
- That is a good tradeoff for firmware code where ordering matters and a shared
  abstraction would likely have been more indirection than value.

### The action-kind refactor was one of the strongest code improvements in the sequence

References:

- `users/noah/lib/action/action_kind_registry_list.h:12`
- `users/noah/lib/action/action_dispatch.h:60`
- `users/noah/lib/action/action_kind.c:16`
- `tests/host/action_dispatch_test.c:229`

Assessment:

- The action-kind registry now gives definitions, matching priority, policy,
  and dispatch metadata one source of truth.
- That is a real reduction in synchronization risk compared with the older
  parallel-table shape.
- The host tests exercise the descriptor surface directly, which is the right
  seam for this kind of refactor.

## Overall Assessment

The code work over the last review sequence was good.

Most of the landed changes are real improvements:

- runtime-owned state and key-runtime seams are cleaner
- compile-gate enforcement is stronger and now matches the production boundary
- higher-level tests are better aligned to semantic seams
- orchestration order is easier to read and better protected
- the action-kind model is materially more coherent than before

The weakest part of the effort was not the code. It was the review discipline:
several folders declared the work “done enough” before the mechanical sealing and
follow-up audits were actually complete. The code ended up ahead of the review
process.

So the answer is:

- the refactor work itself was largely good and worth keeping
- the review chain was noisier and less reliable than it should have been
- future passes should be stricter about not closing an architecture thread
  until the compile gates, tests, docs, and active review notes all agree

## Verification

Commands run in this retrospective audit:

- `git status --short`
- `git log --since='3 days ago' --oneline --decorate --stat -- review tests/host users/noah keyboards/bastardkb/charybdis/4x6/keymaps/noah`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Current conclusion:

- no must-fix correctness regressions found in the current tree
- the strongest remaining issue from this whole sequence is review-process
  integrity, not runtime behavior
- the codebase is in better shape now than it was before the review chain began
