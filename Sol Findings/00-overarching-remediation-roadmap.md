# Firmware Findings Remediation Roadmap

## Plan metadata

- **Status:** Implementation in progress; Phases 1 and 2 are verified. Phase 3 software work is complete, with Finding 05 awaiting its physical two-half matrix. Phase 4 software work is complete, with Finding 10 awaiting on-device timing. Phase 5 correctness work is verified and Finding 15 optimization remains. See [`implementation-progress.md`](implementation-progress.md).
- **Prepared:** 2026-07-13.
- **Scope:** Firmware runtime correctness, split reliability, target resource safety, and measured hot-path efficiency.
- **Source:** The deep firmware code review performed against the current `charybdis-4x6` tree.
- **Purpose:** Give future implementation passes a stable order, shared safety rules, verification gates, and a durable handoff record.
- **Default write boundary:** This repository. Inspect `../bastardkb-qmk` when an upstream QMK contract must be verified, but do not edit it unless a later task explicitly authorizes that scope.

This directory is a remediation pack, not proof that any finding is resolved. Each numbered finding has its own execution plan. The plans intentionally separate implementation work so failures can be attributed, reviewed, and reverted without coupling unrelated subsystems.

## How to use this roadmap

At the beginning of every remediation pass:

1. Run `git status --short` and preserve all unrelated worktree changes.
2. Read this roadmap, then read the complete plan for the selected finding.
3. Read the newest relevant folder under `review/` before architecture or runtime-boundary work.
4. If the relevant review is open, keep its `progress.md` coherent with the landed state. If it is closed, leave it immutable and open the next sortable review folder when architecture work actually begins.
5. Capture the failing scenario or current measurement before editing code.
6. Run the narrowest relevant host runner as a baseline.
7. Implement one small, testable phase at a time and rerun targeted tests throughout.
8. Finish the finding's acceptance checklist, the full host suite, and a target firmware build before calling it resolved.

Do not treat an attractive refactor, a green unit test, or a host-only result as closure. Target stack/RAM behavior, QMK integration, and split timing must be proven at their actual seams.

## Safety and design principles

### Preserve behavior while changing mechanisms

- Write down the externally visible contract before altering ownership, timing, persistence, or rendering.
- Keep authored keymap data under `keyboards/bastardkb/charybdis/4x6/keymaps/noah/` and reusable policy/runtime code under `users/noah/`.
- Keep QMK/VIA fork assumptions centralized under `users/noah/lib/compat/`.
- Continue weak-hook chains through the matching `noah_*` helper unless a later task explicitly and visibly replaces shared behavior.
- Do not introduce raw layer actions that bypass userspace layer ownership.

### Correctness precedes optimization

- Land validation and rollover fixes before reducing caches or introducing indexing.
- Establish synthetic-key ownership before making macro playback asynchronous.
- Establish split failure/backoff semantics before extending split persistence into a richer durable protocol.
- Establish RGB preview correctness before sharing or caching renderer work.
- Require measurements before and after every RAM, stack, scan-cost, or render-cost optimization.

### Bound all target work

Every scan-driven mechanism needs an explicit upper bound:

- stack bytes per call chain;
- queued or cached entries;
- synthetic actions emitted per scan;
- RPC attempts per scan and per outage interval;
- timer reads per subsystem tick;
- RGB semantic-map/partition builds per chunk or frame;
- time spent in a playback or deferred-release step.

If a proposed implementation cannot state its bound, it is not ready to land.

### Make wraparound a tested state

Counters used as identities or ordering values must define:

- whether zero is reserved;
- how the next value is chosen at wrap;
- whether live values may be reused;
- how ordering comparisons work across wrap;
- what happens when the identifier space is genuinely exhausted.

Tests must inject state immediately before wrap. Waiting for natural wrap is not verification.

### Fail closed at external boundaries

Malformed VIA/RPC input must be rejected before pointer arithmetic, EEPROM writes, cache mutation, playback, or mirroring. Rejection must leave the previous valid state intact and must not produce partial side effects.

## Finding index and dependency map

| ID | Priority | Finding | Intended outcome | Principal dependencies | Recommended phase |
| --- | --- | --- | --- | --- | --- |
| 01 | Must fix | [Target stack safety](01-target-stack-safety.md) | No reachable firmware call chain can overrun the process stack; target evidence enforces the budget. | None | 1 |
| 02 | Must fix | [Press-token rollover](02-press-token-rollover.md) | Token ID zero is never issued and a live identity is never silently reused. | None | 1 |
| 03 | Must fix | [VIA split buffer validation](03-via-split-buffer-validation.md) | Length/offset arithmetic is wide, bounded, and side-effect-free on malformed packets. | None | 1 |
| 04 | Must fix | [VIA macro byte validation](04-via-macro-byte-validation.md) | Invalid high bytes never reach QMK ASCII lookup tables or playback side effects. | None | 1 |
| 05 | Must fix | [VIA split persistence](05-via-split-persistence.md) | All persistent VIA state converges across reconnects, retries, resets, and role changes. | 03, 12; coordinate with 17 | 3 |
| 06 | Should fix | [Combo-origin cache lifecycle](06-combo-origin-cache-lifecycle.md) | Suppressed overlap candidates expire without discarding delayed legitimate QMK output. | None | 2 |
| 07 | Should fix | [Nonblocking macro playback](07-nonblocking-macro-playback.md) | Playback advances from scan tasks without blocking matrix, pointing, RGB, or split work. | 04, 08 | 2 |
| 08 | Should fix | [Synthetic-key ownership](08-synthetic-key-ownership.md) | Physical and synthetic sources share basic keys/modifiers without premature release or orphan teardown. | Coordinate with 02 | 2 |
| 09 | Should fix | [Pending-release sequence rollover](09-pending-release-sequence-rollover.md) | Deferred releases retain deterministic age/order across sequence wrap. | Coordinate with 01 | 1 |
| 10 | Should fix | [Pointing backlog bounds](10-pointing-backlog-bounds.md) | Large reports cannot monopolize a scan; `INT16_MIN` and residual overflow are defined. | 11 recommended first | 4 |
| 11 | Should fix | [Dragscroll stall recovery](11-dragscroll-stall-recovery.md) | The first report after a timing gap starts from expired gesture state. | None | 4 |
| 12 | Should fix | [Split RPC failure backoff](12-split-rpc-failure-backoff.md) | A disconnected half cannot cause a per-scan multi-domain retry storm. | Coordinate with 17 | 3 |
| 13 | Should fix | [RGB preview parity](13-rgb-preview-parity.md) | Preview and normal layer rendering resolve groups and inheritance identically. | None | 5 |
| 14 | Optimize | [Macro-cache RAM](14-macro-cache-ram.md) | Macro caching consumes materially less RAM with measured latency and unchanged invalidation. | 04 and preferably 07 | 6 |
| 15 | Optimize | [RGB render work](15-rgb-render-work.md) | Empty ranges and repeated semantic/partition work are eliminated with color parity. | 13 | 5 |
| 16 | Optimize | [Runtime lookup hot path](16-runtime-lookup-hot-path.md) | Key/active-state lookup and feedback dirtiness become measurement-driven and bounded. | 02, 08, 09 stable first | 6 |
| 17 | Optimize | [Split timer sampling](17-split-timer-sampling.md) | Split sync samples one timebase per tick and uses wrap-safe elapsed arithmetic. | None; coordinate with 12 and 05 | 3 |

Dependency direction is intentional. In particular:

- **04 → 08 → 07 → 14:** validate macro inputs, define ownership, make playback nonblocking, then reduce its memory footprint.
- **03 → 12/17 → 05:** harden packet bounds, establish transport behavior and a shared clock, then build convergence/reconciliation.
- **13 → 15:** prove rendering semantics before caching shared intermediate state.
- **02/08/09 → 16:** stabilize runtime identities and ownership before optimizing lookup paths around them.
- **11 → 10:** fix gesture expiry first so backlog/budget work is measured against correct gesture state.

## Execution phases

### Phase 0 — Reproduce and restore the target verification baseline

Goal: make later evidence comparable and ensure an environment failure is not mistaken for a firmware regression.

1. Record `git rev-parse HEAD`, `git status --short`, QMK version, compiler version, and the Python interpreter selected by the test runners.
2. Run `sh tests/host/run_all_host_tests.sh` before the first firmware edit.
3. Run `qmk compile -kb bastardkb/charybdis/4x6 -km noah` before the first firmware edit.
4. Record binary section sizes, configured stack size, and a map/symbol snapshot for the baseline image.
5. Keep all generated diagnostics outside source directories unless a checked-in test fixture is deliberately added.

Known local-environment observations from the 2026-07-13 audit:

- The default `/usr/bin/python3` was Python 3.9 and failed tooling that uses `str | Path`. The full host suite passed when run with `PATH=/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin`.
- The target compile was not completed. The local QMK/Pillow path first lacked TIFF/JPEG dependencies; after a temporary diagnostic workaround, the ARM toolchain failed to load `/opt/homebrew/opt/isl/lib/libisl.23.dylib`.
- Those are baseline-environment blockers, not permission to skip the target build at closure. Repair or document the toolchain before marking any firmware finding resolved.

Do not check temporary wrappers or local dependency workarounds into the firmware tree.

### Phase 1 — Eliminate immediate corruption and rollover risks

Recommended order: **01, 03, 04, 02, 09**.

- Start with target stack safety because an overflowing call chain can invalidate every higher-level behavioral observation.
- Harden VIA length and byte boundaries before extending any persistence or playback path.
- Fix both identity/ordering rollover cases with injectable near-wrap tests.
- Prefer small, independent changes and commits for each finding.

Exit criteria:

- target stack measurements show a documented safety margin;
- malformed VIA payloads have zero memory/EEPROM/playback side effects;
- token tests cross `0xFFFF → 0x0001` deterministically and pending-release
  FIFO tests exceed 65,536 operations without a sequence clock;
- all finding-specific tests, full host suite, and firmware compile are green.

### Phase 2 — Establish ownership and lifecycle foundations

Recommended order: **08 first; 06 and 07 may then proceed independently**.

1. Define who owns physical and synthetic basic/modifier registrations and how reference counts are reconciled.
2. Correct combo-origin lifecycle without assuming physical release means QMK can no longer emit a delayed combo event.
3. Replace blocking macro delays with a scan-driven state machine that uses the new ownership contract. This can proceed alongside combo-origin work once Finding 08 is stable.

Findings 08, 06, and 07 are verified. Macro playback is scan-driven, uses the
landed lease contract, pins active provider IR, and rejects overlapping starts
without queueing.

The macro scheduler must specify queueing, overlap, cancellation, reset, layer change, suspend, and aborted-playback cleanup before code is written. A scheduler that is nonblocking but leaks owned keys is not an improvement.

Exit criteria:

- overlapping physical/synthetic key scenarios preserve every live owner;
- orphan macro key-up policy is explicit and mechanically tested;
- overlap-suppressed combos neither poison feedback nor evict valid pending origins;
- macro delays do not stop concurrent scan, pointing, RGB, or split progress.

### Phase 3 — Make split transport bounded and convergent

Recommended order: **17, 12, 05**. Findings 12 and 17 may share a tightly scoped preparatory change if that is the cleanest way to pass one sampled `now` through the sync tick.

Findings 17 and 12 are verified: one wrap-safe sampled timestamp drives all
outbound split-domain timing, and a failed RPC now stops the send pass behind a
bounded 50–1,000 ms recovery backoff. Finding 05's durable versioned
reconciliation and all software gates are complete; only its physical
disconnect, power-cycle, reconnect, and USB-role-swap matrix remains.

1. Sample a single timestamp and pass it through split-domain scheduling.
2. Introduce shared outage gating and bounded retry/backoff while retaining dirty state.
3. Extend VIA mirroring to a durable, versioned reconciliation protocol covering keymap, layout options, encoder data, macro region, resets, and split RGB state where owned by this protocol.

The persistence design must distinguish:

- command receipt from successful application;
- transient delivery from durable convergence;
- the authoritative image from the replica image;
- a new write from a replay or snapshot;
- connected, disconnected, stale, and reconciled states;
- ordinary retry from rejoin/role-change reconciliation.

Exit criteria:

- an outage has a strict RPC-attempt and time budget per scan;
- dirty state survives failure and eventually converges after recovery;
- duplicate/reordered delivery is safe or rejected by version rules;
- role changes and slave rejoin are covered by host integration tests;
- macro set/reset persistence is included, not special-cased away.

### Phase 4 — Bound pointing work and reset stale gestures

Recommended order: **11, 10**.

Finding 11 is verified: dragscroll and pinch now expire prior lock/residual
state before accepting post-stall motion, and one sampled timestamp drives all
handler decisions. Finding 10 now enforces four taps per pointing poll, a
32-step retained backlog, exact residual preservation, deterministic overload
diagnostics, and correct `INT16_MIN` dominance. All software and target-build
gates pass; its flashed worst-case timing/feel check remains pending.

- Expire dragscroll state against the previous motion timestamp before accumulating the first post-gap report.
- Define per-scan output budgets and residual/backlog limits for every pointing-discrete mode.
- Promote absolute-value and accumulation math so `INT16_MIN` is representable.
- Choose and document an overload policy: carry bounded residual, saturate, coalesce, or intentionally drop. Silent unbounded loops are forbidden.

Exit criteria:

- maximum 16-bit reports have finite, tested scan cost;
- stale axis locks/residuals cannot leak across inactivity gaps;
- threshold configuration is compile-time validated;
- concurrent keyboard/split work continues while backlog drains.

### Phase 5 — Correct and streamline RGB rendering

Recommended order: **13, 15**.

Finding 13 is verified: normal activation and pending preview now consume one
selection-aware base/group renderer, including universal rows, inheritance,
base-less explicit groups, authored override order, and chunk parity. Finding
15 is the remaining Phase 5 optimization item.

- Extract or reuse one group/inheritance resolution path for normal and preview rendering.
- Add parity tests before introducing per-frame or per-chunk caches.
- Then skip empty LED ranges and share semantic-map/combined-partition work.
- Instrument build counts or equivalent work counters so the optimization has a falsifiable target.

Exit criteria:

- preview parity tests cover `RGB_LAYER_GROUP_ALL`, the inherit sentinel, and an explicit nonblack group over a black base;
- master-right empty chunks short-circuit;
- semantic/partition work meets the documented budget;
- rendered colors remain byte-for-byte equivalent for representative states.

### Phase 6 — Recover RAM and reduce runtime lookup cost

Recommended order: **14, 16**.

- Measure current `.bss`, cache hit rate, parse latency, behavior comparisons, active-slot scans, and feedback dirty events.
- Select the smallest design that meets an explicit latency and memory budget; do not begin with a generic runtime rewrite.
- Preserve invalidation, malformed-payload caching semantics, ownership, and release behavior.

Exit criteria:

- target map evidence shows the intended RAM reduction;
- cache invalidation and playback latency remain within stated bounds;
- hot-path counters show fewer comparisons/scans/dirty rebuilds in representative scenarios;
- behavior and full integration tests remain unchanged except for new enforcement.

## Per-finding implementation loop

Use this loop for every numbered file:

1. **Claim the scope.** Name the finding, files likely to change, and contracts that must remain stable.
2. **Capture baseline evidence.** Add or run a reproducer that fails for the reason stated in the plan.
3. **Choose the narrow design.** Record rejected alternatives when the tradeoff affects RAM, flash, timing, persistence, or API boundaries.
4. **Add enforcement first where practical.** Near-wrap injection, malformed-packet tables, fake-RPC failures, work counters, and stack reports should make the risk observable.
5. **Implement one phase.** Keep source manifests, mirrored host runners, and compile gates synchronized with any new source file or boundary.
6. **Run targeted checks.** Stop on the first failure and find its root cause; never weaken a test merely to get green output.
7. **Inspect target effects.** For stack/RAM/timing findings, compare target artifacts or counters to baseline.
8. **Update human-facing state.** Update `README.md`/`docs/` when behavior or workflow changes and update the active review record for architecture work.
9. **Run repository closure gates.** Full host suite, firmware compile, and `git diff --check`.
10. **Record the handoff.** Exact commands, results, measurements, residual risks, and the next finding that is now unblocked.

## Verification policy

### Targeted tests are iterative checkpoints

Use the exact subsystem runners named in each finding plan. The main families are:

- key runtime and release lifecycle: `run_key_runtime_release_matrix_tests.sh`, `run_key_runtime_modifier_hold_integration_tests.sh`, `run_pd_mode_key_runtime_integration_tests.sh`, `run_key_runtime_layer_lock_integration_tests.sh`, `run_key_runtime_scenario_tests.sh`, `run_key_runtime_integration_harness_tests.sh`;
- pointing and split: `run_pd_mode_tests.sh`, `run_pd_mode_handlers_tests.sh`, `run_pd_runtime_tests.sh`, `run_pointer_layer_policy_tests.sh`, `run_split_runtime_sync_tests.sh`;
- authored profile: `run_key_behavior_lookup_tests.sh`, `run_key_behavior_validation_tests.sh`, `run_keymap_validation_tests.sh`, `run_real_profile_validation_tests.sh`;
- RGB: `run_rgb_validation_tests.sh`, `run_rgb_layer_render_tests.sh`;
- hooks/ownership: `run_hook_chaining_tests.sh`, `run_keyboard_mod_ownership_tests.sh`, `run_owned_keycode_tests.sh`, `run_held_action_tests.sh`, `run_layer_ownership_tests.sh`;
- macro/VIA/QMK contracts: `run_qmk_contract_checks.sh`, `run_action_lifecycle_tests.sh`, `run_macro_dispatch_tests.sh`, `run_macro_payload_tests.sh`, `run_via_macro_defaults_tests.sh`, `run_via_macro_action_lifecycle_tests.sh`;
- shared runtime/tracing: `run_runtime_init_order_tests.sh`, `run_runtime_debug_tests.sh`, `run_runtime_trace_tests.sh`.

Run commands from the repository root, for example:

```sh
sh tests/host/run_macro_payload_tests.sh
```

Do not replace concrete runners with a guessed umbrella command while iterating.

### Conditional repository gates

- If authored keymap, combo, macro, or RGB data changes, run `sh tests/host/run_real_profile_validation_tests.sh`.
- If runtime wiring, source lists, compatibility surfaces, or header boundaries change, run `sh tests/host/run_feature_gate_compile_tests.sh`.
- If an authored input to `tools/profile_introspect.py` changes, run both:

```sh
python3 tools/profile_introspect.py --write
python3 tools/profile_introspect.py --check
```

- If a new userspace firmware source file is introduced, add it to `users/noah/source_manifest.mk` and update mirrored host/compile-gate surfaces in the same pass.

### Mandatory closure gates

Every firmware finding must finish with:

```sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
git diff --check
```

Run the firmware build only after relevant host checks are green. A failed or skipped command must be reported explicitly. The closure bar cannot be lowered because the local toolchain is inconvenient.

## Evidence required by finding class

| Finding class | Minimum evidence in addition to behavior tests |
| --- | --- |
| Stack | Target `.su`/disassembly/map evidence for the reachable call chain, configured stack size, and a documented safety margin. |
| RAM/cache | Before/after `.data` and `.bss`, cache structure sizes, representative hit/miss/parse latency, and invalidation tests. |
| Rollover | Injectable counter state immediately before wrap, coexistence of pre/post-wrap live entries, and exhaustion behavior. |
| External input | Boundary-value table, malformed/truncated cases, no-side-effect assertions, and valid maximum payload coverage. |
| Split transport | Fake send failures, retry-attempt counters, reconnect/rejoin, duplicate delivery, role change, and eventual convergence. |
| Timing/scheduler | Fake clock or deterministic tick advancement, per-scan work bound, concurrency progress, cancellation/cleanup. |
| Rendering | Semantic parity fixtures, exact color output where feasible, and instrumented work/build counts. |
| Hot path | Before/after counters for comparisons, slot scans, dirtiness, timer reads, or equivalent target-relevant operations. |

## Change and commit strategy

- Prefer one finding per branch/commit series. Shared infrastructure is acceptable only when its contract and consumers are explicit.
- Keep refactors separate from behavior changes when that makes review and bisection clearer.
- Do not combine critical boundary fixes with optional RAM/performance cleanup.
- Never stage or commit unrelated user changes.
- Inspect the complete staged diff before committing and describe the whole diff in the commit message.
- When a finding changes an architecture boundary, update tests and the appropriate open review record in the same pass.

A useful commit progression for a complex finding is:

1. deterministic reproducer/enforcement;
2. minimal implementation;
3. target measurement gate or integration coverage;
4. docs/review reconciliation.

Squashing is a later review decision; maintaining understandable intermediate states is more valuable during implementation.

## Finding status lifecycle

Use these states consistently in future updates:

- **Planned:** evidence and implementation path are documented; no fix claimed.
- **Ready:** baseline reproduction and required design decisions are complete.
- **In progress:** implementation has started; name the completed and remaining phases.
- **Implemented, verification incomplete:** code exists but one or more closure gates or target measurements are missing.
- **Verified:** all per-finding acceptance criteria, full host suite, target build, docs, and review integrity requirements pass.
- **Blocked:** only after a concrete external blocker prevents meaningful progress; preserve the exact failing command and evidence.

Do not use “resolved” as a synonym for “code looks cleaner.” For seam, boundary, or API findings, verified enforcement and coherent documentation are part of the implementation.

## Cross-finding regression scenarios

Before closing the full remediation program, add or retain scenarios that cross subsystem boundaries:

1. A physical key remains held while a VIA macro taps or holds the same basic key, including cancellation during a macro delay.
2. A press token crosses wrap while held actions and deferred releases are live,
   and the explicit pending-release FIFO survives more than 65,536 operations
   behind a long-lived blocked entry.
3. A malformed VIA macro/buffer command is received during split outage and recovery; neither half mutates partial state.
4. A split half disconnects during a macro/keymap persistence update, rejoins, changes role, and converges without retry storm.
5. Maximum pointing input drains under budget while keyboard releases, split sync, and RGB tasks continue to progress.
6. RGB preview renders a grouped/inherited layer while semantic feedback is active, with cached and uncached paths producing identical colors.
7. Runtime lookup optimization processes combo, repeat, layer lock, PD-mode, and synthetic ownership interactions without changing release planning.

These do not replace subsystem tests. They protect the seams created by the remediation order.

## Program-level definition of done

The remediation program is complete only when all of the following are true:

- [ ] Every numbered finding is either verified or explicitly rejected with a documented reason and equivalent risk mitigation.
- [ ] All must-fix findings have deterministic regression tests.
- [ ] Stack and RAM claims are supported by a freshly linked target image, not an old artifact.
- [ ] Work per scan is bounded for deferred release, macro playback, pointing emission, split retry, timer sampling, and RGB rendering. Deferred release and macro playback are verified; the remaining domains belong to later phases.
- [ ] Counter wrap behavior is specified and tested for every affected identity/order field.
- [ ] Split state converges after failure, reconnect, and role change.
- [ ] `README.md`, relevant `docs/`, and the active review folder agree with the landed architecture and behavior.
- [ ] `sh tests/host/run_all_host_tests.sh` passes on the final combined tree.
- [ ] `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passes on the final combined tree.
- [ ] `git diff --check` passes and no sibling workspace source was modified unintentionally.
- [ ] Remaining risks and deferred optional work are stated plainly in the final handoff.

## Recommended next action

Perform [05 — VIA split persistence](05-via-split-persistence.md)'s physical
two-half verification matrix when hardware is available. In parallel, proceed
to [10 — pointing backlog bounds](10-pointing-backlog-bounds.md); Finding 11 is
verified. Finding 01 remains the stack-safety baseline that later runtime
changes must continue to satisfy.
