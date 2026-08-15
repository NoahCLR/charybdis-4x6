# Finding 01: Restore a Provable Target Stack Margin

## Plan metadata

- **Severity:** Must fix — memory-safety and reset risk on ordinary handled-key release paths
- **Status:** Verified on 2026-08-15; see [`implementation-progress.md`](implementation-progress.md) for the implementation and verification record
- **Affected surfaces:** key-runtime release planning, deferred-release draining, QMK process-record call chain, target linker/build reporting
- **Primary files:** [`release.c`](../users/noah/lib/key/runtime/release.c), [`deferred_release.c`](../users/noah/lib/key/runtime/deferred_release.c), [`process.c`](../users/noah/lib/key/runtime/process.c), transition/effect-plan types, userspace build wiring
- **Prerequisites:** a reproducible firmware build with final linked disassembly/map artifacts; preserve a known-good release-behavior baseline
- **Recommended phase:** Phase 1, before functional remediation that adds state or call depth

## Problem statement

The audited firmware reserved a 2,048-byte process stack, while handled-key and scan paths contained nested frames that could not retain the required reserve. The largest avoidable allocation was a full-capacity deferred-release array on the stack. A release that reached that drain path could overwrite adjacent memory before any C-level capacity check could help.

This is not a theoretical “large frame” cleanup. The closure condition is a measured upper bound for a complete target call chain, including QMK callers and relevant interrupt/runtime reserve, with a mechanically enforced margin.

## Current evidence and failure scenario

- The audited linked image defined `__process_stack_size__ = 0x800`, or 2,048 bytes.
- Target disassembly/frame inspection found approximately:
  - `noah_process_record_user`: 640 bytes;
  - the handled-key process stage: 592 bytes;
  - `key_runtime_process_handled_key_release`: 1,032 bytes.
- Those nested frames total roughly 2,264 bytes before accounting for outer QMK frames.
- [`key_runtime_process_handled_key_release()`](../users/noah/lib/key/runtime/release.c#L11) owns a transition plan and calls deferred-release drain after executing it.
- [`key_runtime_deferred_release_drain_dispatches()`](../users/noah/lib/key/runtime/deferred_release.c#L54) allocates `pending_release_t pending[KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY]`. With capacity 120, the audited target frame contribution was about 1,768 bytes by itself.
- [`key_runtime_process_stage_handled_key()`](../users/noah/lib/key/runtime/process.c#L127) keeps the release call nested under process-stage and process-record state.

Representative failure: a handled key is released while one or more foreign deferred-release blockers exist. Release planning queues deferred dispatches, then the release flow drains them. The stack pointer crosses the configured process-stack boundary before or during projection, corrupting state and causing nondeterministic stuck actions, watchdog resets, or later crashes.

## Required invariants

1. No normal or error release path allocates storage proportional to the 120-entry pending-release capacity on the process stack.
2. Deferred dispatches retain their current FIFO/sequence ordering, owner cleanup, modifier snapshot, tap-feedback pairing, and exactly-once projection semantics.
3. Dispatches queued while a drain is in progress have an explicit policy; they cannot create an unbounded re-entrant drain loop.
4. The measured worst credible process-stack call chain, including QMK frames, fits within 75% of the configured stack and leaves at least 512 bytes of headroom. If these two limits differ, satisfy the larger reserve.
5. The stack limit is checked from target compiler/link artifacts in CI or the standard verification workflow. A host-only `sizeof` assertion is insufficient.
6. Increasing the configured stack may be used as a temporary containment measure only after RAM impact is measured; it is not the sole fix for the full-capacity local array.

## Scope

### In scope

- Remove or sharply bound large automatic arrays and large by-value copies on the handled-key press/release path.
- Reshape deferred-release consumption into a constant-stack operation.
- Measure the complete target call chain and introduce a repeatable stack-budget gate.
- Add behavioral tests for draining at capacity, ordering, nested enqueue behavior, and cleanup.
- Adjust process-stack configuration only if the measured post-refactor chain still needs a justified reserve.

### Non-goals

- Rewriting the reducer, transition planner, or all key-runtime APIs.
- Reducing queue capacity merely to make the stack array smaller.
- Treating a successful host test run as evidence that target stack use is safe.
- Using a shared static scratch array without proving initialization, re-entry, and split/interrupt ownership.

## Implementation plan

### Step 0 — Reproduce and freeze the baseline

1. Restore a working ARM/QMK toolchain and produce a clean `noah` firmware ELF, map file, and disassembly from the current tree.
2. Record the configured process-stack symbol, `.bss`/`.data` totals, and the frame sizes for the complete process-record-to-deferred-projection chain.
3. Enable deterministic target stack analysis without changing normal release
   optimization. The implemented gate uses `-fno-shrink-wrap` plus final linked
   post-LTO disassembly; this is more directly auditable than pre-LTO `.su`
   records for the linked call paths.
4. Add a focused release scenario that reaches `key_runtime_deferred_release_drain_dispatches()` with multiple pending entries. Preserve its trace/order output as the behavioral baseline.
5. Document any indirect-call edges that static call-graph tooling cannot resolve; these must be supplied explicitly to the budget checker.

### Step 1 — Make deferred draining constant-stack

1. Add a queue API that removes one oldest eligible pending release at a time, or a deliberately small compile-time batch such as two to four entries.
2. Prefer a snapshot-count drain contract:
   - capture the number eligible at drain entry;
   - consume at most that many in order;
   - leave entries enqueued during projection for the next explicit drain/scan.
3. Keep removal and owner-token cleanup atomic at the reducer API boundary. Projection should receive a single copied `pending_release_t`, not a pointer into a slot that has already been cleared or can be reused.
4. Replace the 120-element local array in `deferred_release.c` with the single-item/small-batch transport.
5. Verify full-capacity behavior. Queue capacity and functional behavior must remain unchanged.
6. Add a guard/diagnostic for an impossible non-progressing drain so corruption cannot become an infinite process-record loop.

### Step 2 — Collapse avoidable nested frame weight

1. Regenerate target stack reports and identify the remaining dominant objects in:
   - `noah_process_record_user`;
   - the process context and handled-key stage;
   - press/release transition plans;
   - transition execution and trace helpers.
2. Remove large by-value effect copies where a const reference or narrow field copy is safe. Confirm generated code actually improves; do not optimize from source appearance.
3. Evaluate giving one caller ownership of the transition-plan buffer instead of creating overlapping plans at nested layers. Preserve the boundary that reducer planning completes before projection side effects execute.
4. If a reusable scratch plan is considered, define single-thread/re-entry ownership and reset-on-every-entry rules, then test recursion/nested callbacks. Prefer caller-owned bounded storage when practical.
5. Stop when the target budget is met; avoid a generic runtime rewrite.

### Step 3 — Add a target stack-budget gate

1. Add a deterministic checker, for example `tests/host/run_firmware_stack_budget_checks.sh`, that parses the target ELF/map/disassembly and a maintained reviewed-path/indirect-edge manifest.
2. Make the checker fail when:
   - the configured process-stack symbol is missing or unexpectedly smaller;
   - any enumerated process-record path exceeds the agreed budget;
   - a userspace function on those paths has unknown/unbounded usage;
   - the required 512-byte and 25% reserve is not present.
3. Include press, handled release, non-handled release cleanup, scan projection, macro dispatch, and split callback paths in the report even if handled release remains the worst case.
4. Print the top frames and the worst chain on every run so regressions are diagnosable.
5. If compiler artifacts cannot conservatively include QMK frames, supplement them with target disassembly and an explicit QMK call-chain manifest. Do not silently ignore unresolved edges.

### Step 4 — Decide whether stack configuration changes

1. Recalculate RAM after code changes and after any proposed stack increase.
2. Increase process stack only when the post-refactor worst chain still lacks the required reserve and the resulting global RAM margin is acceptable.
3. Record the reason, before/after stack symbols, worst-chain measurements, and remaining RAM in the active architecture review.
4. Keep the target gate tied to the configured symbol so a later linker/config change cannot invalidate the proof.

## Test and verification plan

### New focused coverage to add

- Empty, one-entry, small-batch, and full-120-entry drain cases.
- FIFO ordering across multiple owners and wrapped pending-release sequences once Finding 09 lands.
- Entry enqueued during projection: it remains pending and is projected exactly once on the next permitted drain.
- Owner-token/pending markers clear only after the last dispatch for that owner.
- Tap-commit feedback remains adjacent to its dispatch and is neither duplicated nor dropped.
- A forced projection failure/non-progress test terminates safely and reports diagnostics.
- Target stack gate fixture tests for a passing chain, over-budget chain, missing symbol, and unresolved edge.

### Targeted existing runners

Run repeatedly while changing the release subsystem:

```sh
sh tests/host/run_key_runtime_release_matrix_tests.sh
sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh
sh tests/host/run_pd_mode_key_runtime_integration_tests.sh
sh tests/host/run_key_runtime_layer_lock_integration_tests.sh
sh tests/host/run_key_runtime_scenario_tests.sh
sh tests/host/run_key_runtime_integration_harness_tests.sh
sh tests/host/run_runtime_debug_tests.sh
sh tests/host/run_runtime_trace_tests.sh
```

Because runtime source/build instrumentation changes are likely:

```sh
sh tests/host/run_feature_gate_compile_tests.sh
```

### Closure gates

```sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
```

Also run the new target stack-budget checker against the just-built ELF. A compile without a passing stack report does not close this finding.

## Measurements and observability

- Keep a before/after table for configured stack, worst full chain, five largest frames, `.bss`, `.data`, and free-RAM estimate.
- In debug builds, consider a process-stack high-water mark/canary sampled by existing runtime diagnostics. It is corroborating runtime evidence, not a substitute for the static gate.
- Emit deferred-drain start count, drained count, and non-progress count through existing trace/diagnostic surfaces only when runtime debug is enabled.

## Risks, tradeoffs, and fallbacks

- **One-at-a-time drain costs more reducer calls.** Measure latency. A two-to-four-entry batch is acceptable if it retains the stack margin.
- **Draining entries added during projection can loop indefinitely.** Use the entry snapshot count and defer newly queued work.
- **Moving plans to static storage can hide stack use but introduce re-entry corruption.** Do not use that fallback without an enforced ownership contract.
- **Compiler stack reports can omit indirect recursion or assembly frames.** Maintain explicit call edges and validate the result against target disassembly.
- **A larger stack reduces global RAM headroom.** Only accept it with linked-memory evidence and the macro-cache RAM plan considered.

## Documentation and review-note updates

- Update the active open firmware architecture review's `progress.md` with the design, landed implementation, exact measurements, and verification. If the relevant review is closed, create the next sortable review folder instead of editing history.
- Update `userspace-architecture-review.md` if transition-plan ownership or drain semantics change.
- Document the stack-budget command and toolchain requirements in the relevant developer documentation.
- User-facing README changes are needed only if diagnostics/build setup exposed to users changes.

## Acceptance checklist

- [x] The 120-entry automatic array is gone from every process-stack path.
- [x] Deferred release ordering, ownership cleanup, and feedback pairing are covered at capacity.
- [x] Entries queued during drain follow a tested, bounded policy.
- [x] A fresh target build reports the configured process-stack size.
- [x] The worst complete chain satisfies both the 25% and 512-byte reserve rules.
- [x] The target stack-budget gate fails on an injected over-budget fixture.
- [x] All targeted host runners pass.
- [x] `sh tests/host/run_feature_gate_compile_tests.sh` passes.
- [x] `sh tests/host/run_all_host_tests.sh` passes.
- [x] `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passes.
- [x] Active review notes and build documentation match the landed tree.

## Closure measurements

| Measurement | Before | Verified tree |
| --- | ---: | ---: |
| Configured process stack | 2,048 B | 2,560 B |
| Required reserve | 512 B | 640 B |
| Reviewed-path budget | 1,536 B | 1,920 B |
| Worst reviewed main path | over budget | 1,808 B |
| Worst reviewed split path | not separated | 328 B of 768 B budget |
| `.text` | 103,304 B | 101,712 B |
| `.rodata` | 15,460 B | 15,460 B |
| `.data` | 23,760 B | 23,760 B |
| `.bss` | 65,196 B | 65,204 B |
| Linked heap | 173,184 B | 173,176 B |

The process stack increased by 512 bytes only after constant-stack draining,
press/release phase isolation, behavior materialization isolation, and VIA
seed de-duplication. The fresh post-LTO report found a remaining credible
1,808-byte nested fallback-settlement path, so 2,560 bytes is the smallest
reviewed configuration that meets the 25% reserve policy in 256-byte steps.

## Next action

Keep `sh tests/host/run_firmware_stack_budget_checks.sh` as the target closure
gate for later runtime work. Continue with Finding 03.
