# Firmware Findings Implementation Progress

## Program metadata

- **Branch:** `sol`
- **Starting commit:** `5b20ed01` (`sol findings`)
- **Started:** 2026-07-13
- **Current focus:** [Finding 03 — VIA split buffer validation](03-via-split-buffer-validation.md)
- **Overall status:** In progress
- **Runtime review thread:** [`review/2026-05-08-review-01`](../review/2026-05-08-review-01/)

This file is the implementation record for the plans in this directory. It records what actually changed, why choices were made, what verification really ran, and what remains open. A plan is not marked verified until its targeted checks, the full host suite, the target firmware build, target-specific evidence, and required documentation all pass.

## Status legend

- **Planned:** implementation has not started.
- **In progress:** code, tests, or target evidence is actively being developed.
- **Implemented, verification incomplete:** code is present but at least one closure gate is missing or failing.
- **Verified:** the complete per-finding closure bar passed.
- **Blocked:** a concrete external blocker prevents meaningful progress and is recorded with its exact evidence.

## Finding dashboard

| ID | Finding | Priority | Status | Verification summary |
| --- | --- | --- | --- | --- |
| 01 | [Target stack safety](01-target-stack-safety.md) | Must fix | Verified | Fresh post-LTO target gate: 1,808 B worst main path in a 1,920 B budget; full host suite and firmware build pass |
| 02 | [Press-token rollover](02-press-token-rollover.md) | Must fix | Planned | Not run |
| 03 | [VIA split buffer validation](03-via-split-buffer-validation.md) | Must fix | Planned | Not run |
| 04 | [VIA macro byte validation](04-via-macro-byte-validation.md) | Must fix | Planned | Not run |
| 05 | [VIA split persistence](05-via-split-persistence.md) | Must fix | Planned | Not run |
| 06 | [Combo-origin cache lifecycle](06-combo-origin-cache-lifecycle.md) | Should fix | Planned | Not run |
| 07 | [Nonblocking macro playback](07-nonblocking-macro-playback.md) | Should fix | Planned | Not run |
| 08 | [Synthetic-key ownership](08-synthetic-key-ownership.md) | Should fix | Planned | Not run |
| 09 | [Pending-release sequence rollover](09-pending-release-sequence-rollover.md) | Should fix | Planned | Not run |
| 10 | [Pointing backlog bounds](10-pointing-backlog-bounds.md) | Should fix | Planned | Not run |
| 11 | [Dragscroll stall recovery](11-dragscroll-stall-recovery.md) | Should fix | Planned | Not run |
| 12 | [Split RPC failure backoff](12-split-rpc-failure-backoff.md) | Should fix | Planned | Not run |
| 13 | [RGB preview parity](13-rgb-preview-parity.md) | Should fix | Planned | Not run |
| 14 | [Macro-cache RAM](14-macro-cache-ram.md) | Optimize | Planned | Not run |
| 15 | [RGB render work](15-rgb-render-work.md) | Optimize | Planned | Not run |
| 16 | [Runtime lookup hot path](16-runtime-lookup-hot-path.md) | Optimize | Planned | Not run |
| 17 | [Split timer sampling](17-split-timer-sampling.md) | Optimize | Planned | Not run |

## Finding 01 — Target stack safety

### Objective

Remove capacity-sized automatic storage from deferred-release draining, preserve release lifecycle semantics, and prove a safe process-stack margin from a freshly linked target image.

### Required invariants for this pass

1. Deferred releases remain FIFO ordered and project exactly once.
2. Owner-token pending state clears only after the final queued release for that owner is removed.
3. Tap-commit feedback remains paired with its matching dispatch.
4. Work added during projection cannot create an unbounded re-entrant drain.
5. No process-record path allocates an automatic array proportional to the 120-entry pending-release capacity.
6. Closure requires target stack evidence, not only host behavior.

### Baseline

- Branch started clean on commit `5b20ed01`.
- The full host suite passed before firmware edits with `env PATH=/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin sh tests/host/run_all_host_tests.sh`.
- The available pre-change target image configured `__process_stack_size__` to 2,048 bytes.
- A conservative pre-change handled-release lower bound was 4,496 bytes: outer QMK process frames 472 B, `noah_process_record_user` 640 B, handled-key stage 592 B, release 1,032 B, and deferred drain 1,760 B.
- The old drain frame came from an automatic array sized to the complete pending-release queue. Queue capacity was unchanged by this pass.
- Baseline linked sections were recorded as `.text` 103,304 B, `.rodata` 15,460 B, `.data` 23,760 B, `.bss` 65,196 B, heap 173,184 B, and RAM4 288 B.
- Initial target verification was blocked by broken Homebrew QMK/ARM dependencies. `brew reinstall osx-cross/arm/arm-none-eabi-gcc@8 qmk/qmk/qmk` restored `qmk 1.1.8` and a working `arm-none-eabi-gcc` driver. Exact firmware compiles then passed during intermediate iterations.
- At the 2026-07-13 checkpoint, the final target rebuild was unavailable
  because sibling build writes were denied. That external blocker was resolved
  on 2026-08-15; the fresh target gate and ordinary firmware build now pass.
  No sibling source files were edited.

### Work log

#### 2026-07-13 — Baseline, target reproduction, and design investigation

**Completed**

- Re-read the overarching roadmap, the complete Finding 01 plan, and the open runtime review under `review/2026-05-08-review-01/`.
- Captured the host baseline, linked target memory sections, configured process-stack symbol, dominant frames, and complete nested release lower bound.
- Repaired the local QMK/ARM toolchain and confirmed intermediate exact target builds.
- Audited the actual QMK call structure instead of summing only userspace functions. This exposed press-time materialization and VIA reseeding as additional stack-critical paths.

**Decisions**

- Keep the pending-release queue capacity unchanged. The defect was transport storage on the process stack, not queue capacity.
- Use a four-entry drain batch, enforced by compile-time byte and item caps.
- A drain detaches one entry-time batch, projects it in FIFO order, and leaves anything enqueued during projection for the next release/scan boundary.
- Reject synchronous drain re-entry with a single-threaded in-progress guard. This bounds callbacks and synthetic replay without static scratch storage.
- Treat a 512-byte/25% reserve as a complete-call-chain requirement, not a per-function threshold.
- Preserve the reducer's shadow-inclusive layer context separately from the transition layer's live-only recovery context.
- Keep full target proof open until a fresh post-change ELF/map/disassembly passes a reconciled stack gate.

#### 2026-07-13 — Constant-stack deferred release transport

**Implemented**

- Replaced the queue-capacity automatic array in `key_runtime_deferred_release_drain_dispatches()` with `KEY_RUNTIME_DEFERRED_RELEASE_DRAIN_BATCH_CAPACITY == 4`.
- Added compile-time guards limiting the batch to four records and 64 transport bytes.
- Added the in-progress guard so projection-triggered drain calls return immediately.
- Moved the release-triggered drain to `key_runtime_process_stage_handled_key()`. Planning, tracing, and projection now return from the large release frame before the drain begins.
- Retained scan-driven draining for backlogs larger than one batch.

**Behavioral coverage added**

- Full configured-capacity fill, overflow rejection, repeated bounded batches, FIFO order, and no duplicate empty drain.
- Enqueue-during-projection plus synchronous re-entry: the entry snapshot projects once, the new record remains pending, and the next drain projects it once.
- Two owners spanning a batch boundary. Each owner remains release-pending after the first batch and clears only after its last record drains.
- Exact preservation of real, weak, one-shot, and locked one-shot modifier snapshots.
- Tap-commit feedback adjacency on the last item of one batch and first item of the next.
- Repeated production `noah_key_runtime_scan()` calls and a no-op third scan.

#### 2026-07-13 — Release and behavior stack phases

**Implemented**

- Split handled-key release into planning/defer and trace/projection phases so planner-local storage unwinds before effects execute.
- Split reducer release handling into no-inline pending-multi-tap, active-token, and unmatched-recovery helpers. Mutually exclusive release plans/resolutions no longer share one target frame.
- Replaced the unmatched-release full materialization with a narrow HOLD-source query that computes only the momentary-layer recovery fact.
- Kept active release decisions sourced from the press token. Tests prove a momentary fallback resolution cannot inject a layer release into a non-layer token and a plain fallback resolution cannot suppress a layer token's release.
- Preserved the reducer fallback context (`live | core shadow | base`) and the transition recovery context (`live | base`) as separate policies. A shadow-only lower-layer test mechanically covers the distinction.
- Converted handled-key lookup and materialization hot seams to pointer/out parameters where target ABI copies were measurable.
- Split tap, hold, and long-hold materialization into no-inline field phases. Each phase owns one temporary source resolution, then unwinds before the next field resolves.
- Changed release planning's interaction field to a borrowed pointer and documented that settlement/state mutation invalidates it. Resolution, planning, and settlement remain synchronous.

**Measured intermediate target frames**

- Bounded drain: approximately 1,760 B to 136 B.
- Process observer split: aggregate observer path 552 B to 528 B before field-phase splitting.
- Transparency resolver: 168 B to 32 B.
- Materializer before field-phase splitting: 360 B to 336 B.
- Release before branch-phase splitting: 856 B to 832 B.
- These are intermediate measurements from pre-final ELFs. They guide the refactor but are not final closure evidence.

#### 2026-07-13 — VIA reseed stack path

**Implemented**

- The initial stack-gate audit found a separate matrix-scan path over the 1,536-byte reviewed-chain budget: `seed_via_default_macros()` called `via_macro_payload_slot_is_valid()`, which owned a 512-byte IR, and then loaded/compiled the same payload again into another 512-byte IR owned by the seed loop.
- Removed the redundant validation/compile call from seeding. The seed loop now compiles once, logs the same invalid-payload diagnostic on failure, writes the slot terminator, and continues with the existing storage behavior.
- Added a compile-call budget assertion to the VIA defaults host test. A direct EEPROM seed compiles the two non-empty authored payloads exactly twice, not four times.
- The standalone post-init validation remains intact and sequential; it no longer nests under seeding.

#### 2026-07-13 — Target stack gate

**Implemented**

- Added opt-in target flags for a linked map and non-shrink-wrapped final
  disassembly. Ordinary firmware builds retain their existing flags.
- Added a Python reviewed-path checker with ELF/map stack-symbol
  reconciliation, ARM Thumb frame extraction, linked direct-call/tail-edge
  parsing, documented indirect-edge assertions, reserve calculation,
  diagnostic top-frame reporting, and host fixtures.
- Separated the 2,048-byte main process stack from the vendor `SlaveThread`
  context. The split context validates the linked 1,200-byte working area,
  derives its 1,024 usable bytes, and reserves 25%/256 bytes.
- Added source-reviewed main paths for press materialization, press/release
  planning and projection, all release-planning branches, bounded deferred
  emission, non-handled cleanup, scan projection/draining, process macros, and
  VIA seeding, plus the five split RPC callbacks.
- Enforced a 512-byte/25% reserve on the main context and a 256-byte/25%
  reserve on the split context.
- Added a host-only checker test runner to the full host suite and kept the target compile/report runner separate.

**Audit history**

- The first manifest/checker revision was rejected as non-authoritative because it summed named functions without proving call edges or reachable-path coverage.
- The audit also separated the main 2,048-byte process stack from the vendor serial `SlaveThread` context; split callbacks must not be evaluated as fictitious descendants of `main`.
- The replacement is intentionally labeled a reviewed-path regression gate,
  not a proof of the global call-graph maximum. Every manifest adjacency must
  be a linked direct call/tail edge or a documented indirect assertion backed
  by a linked register-indirect callsite.
- The initial source-level manifest did fail closed against the fresh linked
  image because LTO had inlined or renamed several expected symbols. No failed
  adjacency was waived: the manifest was reconciled to linked direct edges and
  documented register-indirect callsites.

#### 2026-08-15 — Fresh target closure

**Completed**

- The first fresh linked audit exposed a real handled-press projection over the
  original 1,536-byte reviewed-path budget. This was not treated as mere
  manifest drift.
- Split handled-press planning into a target-visible no-inline phase, matching
  the existing release planning/projection boundary. The linked press wrapper
  fell from 488 bytes to 176 bytes; its planner owns a separate 336-byte frame.
- Reconciled every main-process path to the final linked call graph, including
  QMK callers, stage-table callbacks, action-kind callbacks, nested fallback
  settlement, release-decision branches, bounded drains, scan paths, macro
  compilation, and VIA seeding.
- The reconciled 2,048-byte report then proved five reviewed paths exceeded the
  1,536-byte path budget. The worst complete path was 1,808 bytes.
- Set `USE_PROCESS_STACKSIZE = 0xA00` (2,560 bytes). The policy reserve is now
  640 bytes and the reviewed-path budget is 1,920 bytes. The worst main path
  passes at 1,808 bytes; the worst split callback path passes at 328 bytes in
  its independent 768-byte budget.
- Linked sections are `.text` 101,712 B, `.rodata` 15,460 B, `.data` 23,760 B,
  `.bss` 65,204 B, `.ram4` 288 B, and heap 173,176 B. The justified 512-byte
  process-stack increase did not consume the linked heap region on this RP2040
  layout.
- The checker remains deliberately scoped to reviewed paths. A pass is not
  represented as a global maximum-stack proof.

### Current finding status

Finding 01 is **Verified**. The fresh instrumented target gate, full host suite,
ordinary firmware compile, documentation, and active review reconciliation all
pass on the same source tree.

## Verification ledger

| Date | Finding | Command | Result | Notes |
| --- | --- | --- | --- | --- |
| 2026-07-13 | 01 | `git status --short` | Passed | Clean baseline before implementation |
| 2026-07-13 | 01 | `env PATH=/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin sh tests/host/run_all_host_tests.sh` | Passed | Pre-change host baseline |
| 2026-07-13 | 01 | `brew reinstall osx-cross/arm/arm-none-eabi-gcc@8 qmk/qmk/qmk` | Passed | Restored local QMK and ARM compiler dependencies; machine setup only, no repo files |
| 2026-07-13 | 01 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Intermediate exact builds after the bounded-drain and early frame-isolation iterations; predates final source state |
| 2026-07-13 | 01 | `sh tests/host/run_key_behavior_lookup_tests.sh` | Passed | Pointer/out materialization, field phases, narrow momentary query, same-resolution dual contexts |
| 2026-07-13 | 01 | `sh tests/host/run_runtime_debug_tests.sh` | Passed | Capacity/batch/re-entry/owner/modifier/feedback/scan and release-fallback coverage |
| 2026-07-13 | 01 | `sh tests/host/run_key_runtime_release_matrix_tests.sh` | Passed | Final source state for current pass |
| 2026-07-13 | 01 | `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh` | Passed | Final source state for current pass |
| 2026-07-13 | 01 | `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh` | Passed | Final source state for current pass |
| 2026-07-13 | 01 | `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh` | Passed | Final source state for current pass |
| 2026-07-13 | 01 | `sh tests/host/run_key_runtime_scenario_tests.sh` | Passed | Includes follow-up scan/no-duplicate deferred release scenario |
| 2026-07-13 | 01 | `sh tests/host/run_key_runtime_integration_harness_tests.sh` | Passed | Final source state for current pass |
| 2026-07-13 | 01 | `sh tests/host/run_runtime_trace_tests.sh` | Passed | Release trace/projection/drain phase changes preserved trace contracts |
| 2026-07-13 | 01 | `sh tests/host/run_via_macro_defaults_tests.sh` | Passed | Single-compile seed behavior and authored bytes |
| 2026-07-13 | 01 | `sh tests/host/run_via_macro_action_lifecycle_tests.sh` | Passed | VIA macro lifecycle unchanged |
| 2026-07-13 | 01 | `sh tests/host/run_feature_gate_compile_tests.sh` | Passed | Header/API/build variants after runtime signature and rules changes |
| 2026-07-13 | 01 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh` | Passed | 15 schema-v2 parsing, edge, context, reserve, and fail-closed fixtures |
| 2026-07-13 | 01 | `env PATH=/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Final combined host suite, including the new stack-tool fixtures |
| 2026-07-13 | 01 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Blocked | Final rebuild requires sibling `.build` writes; approval service rejected escalation due account usage limit, not a compiler/test failure |
| 2026-07-13 | 01 | `git diff --check` | Passed | Final source, test, tooling, Sol Findings, developer docs, and active review tree |
| 2026-08-15 | 01 | `sh tests/host/run_key_runtime_release_matrix_tests.sh` | Passed | Focused check after press planning split |
| 2026-08-15 | 01 | `sh tests/host/run_key_runtime_scenario_tests.sh` | Passed | Focused behavior check after press planning split |
| 2026-08-15 | 01 | `sh tests/host/run_key_runtime_integration_harness_tests.sh` | Passed | Focused integration check after press planning split |
| 2026-08-15 | 01 | `sh tests/host/run_feature_gate_compile_tests.sh` | Passed | Runtime/build boundary variants with final stack configuration |
| 2026-08-15 | 01 | `sh tests/host/run_firmware_stack_budget_checks.sh` | Passed | Fresh clean target build; 2,560 B main stack, 640 B reserve, 1,808 B worst reviewed main path; split path 328 B of 768 B budget |
| 2026-08-15 | 01 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Final complete host suite, including 15 stack-tool fixtures |
| 2026-08-15 | 01 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Required ordinary firmware build on final source tree |

## Cross-cutting decisions and deferred work

- The implementation order follows [`00-overarching-remediation-roadmap.md`](00-overarching-remediation-roadmap.md).
- Finding 09 may later replace pending-release sequence ordering. Finding 01 must not pre-empt that design except where a narrow queue API is required for constant-stack draining.
- The stack checker must model vendor split callbacks under `SlaveThread`, not under the main process stack. Stack-context correctness is part of the gate contract.
- No sibling workspace source was edited; only QMK build artifacts were produced under `../bastardkb-qmk/.build`.

## Next program action

Start Finding 03 with its malformed-packet baseline and preserve Finding 01's
target gate as a regression check for later runtime changes.
