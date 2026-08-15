# Firmware Findings Implementation Progress

## Program metadata

- **Branch:** `sol`
- **Starting commit:** `5b20ed01` (`sol findings`)
- **Started:** 2026-07-13
- **Current focus:** Findings 05 and 10 await their physical checks. All planned software optimization findings, including Finding 16, are verified.
- **Overall status:** In progress
- **Latest closed review:** [`review/2026-08-15-review-16`](../review/2026-08-15-review-16/)
- **Active implementation reviews:** [`review/2026-08-15-review-10`](../review/2026-08-15-review-10/) and [`review/2026-08-15-review-12`](../review/2026-08-15-review-12/) remain hardware-verification pending

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
| 02 | [Press-token rollover](02-press-token-rollover.md) | Must fix | Verified | Wrap/collision/owner-store/tiny-domain exhaustion coverage, full host, firmware, and explicit target stack path pass |
| 03 | [VIA split buffer validation](03-via-split-buffer-validation.md) | Must fix | Verified | Exhaustive normal/ASan/UBSan boundary matrix, full host suite, firmware build, and target stack regression gate pass |
| 04 | [VIA macro byte validation](04-via-macro-byte-validation.md) | Must fix | Verified | Exhaustive text-domain, whole-IR preflight, cache lifecycle, full host, firmware, and explicit target-stack paths pass |
| 05 | [VIA split persistence](05-via-split-persistence.md) | Must fix | In progress | Durable snapshot/ack reconciliation implemented; all software gates pass, including fresh main/split stack paths; physical two-half matrix pending |
| 06 | [Combo-origin cache lifecycle](06-combo-origin-cache-lifecycle.md) | Should fix | Verified | Exact generations, suppression/deadline/capacity lifecycle, normal/compact QMK contracts, full host, firmware, and explicit 280 B target stack path pass |
| 07 | [Nonblocking macro playback](07-nonblocking-macro-playback.md) | Should fix | Verified | Fake-timer/wrap, busy, pinning, cancellation, ownership, full host, firmware, and fresh target-stack gates pass |
| 08 | [Synthetic-key ownership](08-synthetic-key-ownership.md) | Should fix | Verified | Aggregate physical/managed report ownership, scoped persistent leases, strict macro balance, source guards, full host, firmware, and target stack gates pass |
| 09 | [Pending-release sequence rollover](09-pending-release-sequence-rollover.md) | Should fix | Verified | Explicit linked FIFO, 65,537-cycle blocked-head stress, structural corruption checks, full host, firmware, and target stack gate pass |
| 10 | [Pointing backlog bounds](10-pointing-backlog-bounds.md) | Should fix | Implemented, verification incomplete | 4-tap/32-step bounds, extremes, compile guards, full host, target build, and 1,280 B pointing path pass; flashed timing pending |
| 11 | [Dragscroll stall recovery](11-dragscroll-stall-recovery.md) | Should fix | Verified | 55/56 and 80/81 ms boundaries, first-report/no-motion/wrap/reset/pinch reuse, one-read timer budget, full host, firmware, and explicit 360 B stack path pass |
| 12 | [Split RPC failure backoff](12-split-rpc-failure-backoff.md) | Should fix | Verified | Stop-on-first-failure, 50–1,000 ms wrap-safe backoff, current-state recovery, bounded trace, full host, firmware, and target-stack gates pass |
| 13 | [RGB preview parity](13-rgb-preview-parity.md) | Should fix | Verified | Shared selected-layer renderer; universal/inherit/base-less/ordering/chunk parity, full host, firmware, and target-stack gates pass |
| 14 | [Macro-cache RAM](14-macro-cache-ram.md) | Optimize | Verified | 41,440 B per-slot IR storage reduced to 599 B shared storage; full host, firmware, memory, and stack gates pass |
| 15 | [RGB render work](15-rgb-render-work.md) | Optimize | Verified | Exact frame snapshot; 10→1 semantic and 20→1 combo projection builds, 5→3 local stage pipelines, full host, firmware, and target resource/stack gates pass |
| 16 | [Runtime lookup hot path](16-runtime-lookup-hot-path.md) | Optimize | Verified | One authored search per handled press, zero per matched release, 180→2 one-active-press slot visits, zero unchanged dirty marks, full host/firmware/resource/fresh-stack gates pass |
| 17 | [Split timer sampling](17-split-timer-sampling.md) | Optimize | Verified | One sampled tick timestamp, active auto-mouse elapsed-at compatibility, wrap/timer-budget tests, full host, firmware, and explicit target-stack paths pass |

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

## Finding 03 — VIA split buffer validation

### Objective

Treat the slave VIA replay callback as an untrusted binary boundary and prove
packet shape, payload availability, and destination range before any QMK
storage or rendering side effect.

### 2026-08-15 — Decoder and boundary coverage

**Implemented**

- Replaced parsing inside the side-effect switch with a pure typed decoder.
- Removed the narrowing `uint8_t` required-length calculation. Set-buffer now
  proves the four-byte header, uses `size_t` subtraction for available payload,
  enforces the 28-byte RPC payload maximum, and validates `size <= capacity -
  offset` after proving `offset <= capacity`.
- Added `noah_qmk_via_keymap_buffer_capacity()` in the QMK compatibility layer
  and a compile-time assertion that the configured keymap byte region fits
  VIA's 16-bit offset contract.
- Defined the padded-report contract: supported commands may carry trailing
  bytes because QMK raw-HID reports are commonly 32 bytes, but padding never
  expands a declared payload.
- Defined zero-length set-buffer as a valid no-op at offsets through the exact
  region end. It skips the storage call and payload pointer while preserving
  the command's RGB invalidation effect.
- Added layer/row/column validation for keycode writes and layer/encoder
  validation for encoder writes before QMK is called.
- Added all 256 encoded sizes across transport lengths 0 through 32, canaries,
  destination edges, valid 28-byte payload, reset padding, oversized transport,
  invalid coordinates, and an encoder-enabled variant.
- The concrete runner now executes normal, ASan/UBSan, and encoder-enabled
  builds and rejects reintroduction of the original narrowing expression.

**Focused verification**

- `sh tests/host/run_qmk_via_split_sync_tests.sh` — passed all three variants.
- `sh tests/host/run_qmk_contract_checks.sh` — passed.
- `sh tests/host/run_via_macro_defaults_tests.sh` — passed.
- `sh tests/host/run_action_lifecycle_tests.sh` — passed.
- `sh tests/host/run_feature_gate_compile_tests.sh` — passed.

### Current finding status

Finding 03 is **Verified**. The focused normal/sanitizer/encoder builds, full
host suite, ordinary firmware compile, compatibility documentation, and fresh
Finding 01 target stack regression gate pass on the same source tree. The
target gate retains a 1,808 B worst reviewed main path against a 1,920 B budget;
the reviewed VIA slave path is 88 B against a 768 B split-thread budget.

## Finding 04 — VIA macro byte validation

### Objective

Keep arbitrary high bytes out of QMK's 128-entry text lookup tables while
preserving the full byte grammar for VIA keycode operands, and reject every
malformed IR before its first output, wait, or key-ownership side effect.

### 2026-08-15 — Text-domain and whole-IR preflight

**Baseline reproduced**

- Added the exhaustive high-text decoder test first. The unmodified decoder
  failed on the first `0x80` case because it accepted the byte as text.

**Implemented**

- Added one shared `macro_payload_text_byte_is_supported()` predicate. Authored
  parsing, VIA/QMK-stream decoding, and playback structure validation now agree
  that text bytes are `0x01..0x7F`; zero remains the VIA stream terminator.
- Kept high-valued bytes legal in complete QMK tap/down/up command operand
  positions. Tests cover `0x80`, `0xFE`, and `0xFF` through decode and playback.
- Added a shared IR step decoder used by both a complete preflight pass and the
  execution pass. It validates opcode shape, text lengths/bytes, delays,
  key-action operands, tap-list counts, bounds, and final hold balance.
- Playback now completes preflight before sending text, waiting, or changing
  owned-key state. A valid prefix followed by a later malformed operation can
  no longer produce partial output.
- Proved invalid VIA slots are negatively cached: a second play performs no NVM
  reads; repairing storage has no effect until explicit invalidation; after
  invalidation, the repaired ASCII payload loads and plays.
- The concrete macro payload runner now repeats its coverage under ASan/UBSan.

**Focused verification**

- `sh tests/host/run_macro_payload_tests.sh` — passed normal and ASan/UBSan variants.
- `sh tests/host/run_macro_dispatch_tests.sh` — passed.
- `sh tests/host/run_via_macro_defaults_tests.sh` — passed.
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh` — passed.
- `sh tests/host/run_action_lifecycle_tests.sh` — passed.
- `sh tests/host/run_qmk_contract_checks.sh` — passed.
- `sh tests/host/run_feature_gate_compile_tests.sh` — passed.

### Current finding status

Finding 04 is **Verified**. Focused macro/VIA checks, ASan/UBSan, the final full
host suite, ordinary target firmware build, documentation, and the fresh stack
gate pass on one coherent tree. The expanded target manifest measures the new
hardcoded preflight route at 760 B and the handled VIA preflight route at
1,152 B against the 1,920 B main-process budget. The overall worst reviewed
path remains 1,808 B. The final linked image reports `.text` 102,104 B,
`.rodata` 15,460 B, `.data` 23,760 B, and `.bss` 65,204 B.

## Finding 02 — Press-token rollover

### Objective

Guarantee that press identity never uses zero or collides with an owner still
referenced by reducer state, including after `uint16_t` wrap.

### 2026-08-15 — Total allocator and owner-liveness enforcement

**Implemented**

- Replaced the unchecked `next_token_id++` assignment with a total allocator
  that explicitly advances `0xFFFF -> 1`, normalizes invalid candidates, and
  stops after one complete finite-domain cycle.
- Defined the reservation set as active press tokens, inactive tokens retained
  for deferred settlement, active leases of every kind, and active pending
  releases. A maintenance comment on the core owner stores requires future
  owner-bearing state to join the predicate and its test.
- Kept the common path inexpensive: owner arrays are skipped entirely when
  their authoritative counts are zero. Live-state scans are bounded by the
  fixed press-token, lease, and pending-release capacities.
- Moved allocation before cancellation, tap-series updates, interruption
  flags, token/count mutation, feedback sequencing, or lease attachment.
- Added a saturating allocation-failure diagnostic to projection snapshots.
  Exhausted handled presses are consumed with an empty effect plan; unhandled
  QMK presses remain unowned. No zero identity is substituted.
- Added an explicit target stack-manifest path through the no-inline allocator.
  The fresh linked frame is 40 B and the complete reviewed allocation path is
  1,104 B against the 1,920 B main-process budget.

**Boundary coverage**

- Allocations at `0xFFFE`, `0xFFFF`, and post-wrap `1`.
- A live low ID at wrap, proving selection skips to `2`.
- Independent reservation through active press, retained press, held-action
  lease, repeat lease, and pending-release storage.
- Balanced cleanup for wrapped held-action and repeat owners.
- Deferred owner settlement at `0xFFFF` alongside a new post-wrap owner.
- A second host build with `KEY_RUNTIME_CORE_TOKEN_ID_MAX=3` that reserves all
  identities, proves no partial press/lease mutation, consumes the handled
  press without effects, and exposes the diagnostic count.

**Focused verification**

- `sh tests/host/run_runtime_debug_tests.sh` — passed production and tiny-domain builds.
- `sh tests/host/run_key_runtime_release_matrix_tests.sh` — passed.
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh` — passed.
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh` — passed both variants.
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh` — passed.
- `sh tests/host/run_key_runtime_scenario_tests.sh` — passed.
- `sh tests/host/run_key_runtime_integration_harness_tests.sh` — passed.
- `sh tests/host/run_held_action_tests.sh` — passed.
- `sh tests/host/run_action_lifecycle_tests.sh` — passed.
- `sh tests/host/run_feature_gate_compile_tests.sh` — passed.

### Current finding status

Finding 02 is **Verified**. Focused ownership/release checks, the complete host
suite, ordinary firmware compile, explicit fresh target stack path, developer
documentation, Sol plan, and closed review agree on the landed identity
contract. The final linked image reports `.text` 102,464 B, `.rodata` 15,460 B,
`.data` 23,760 B, and `.bss` 65,204 B.

## Finding 09 — Pending-release FIFO rollover

### Objective

Remove the wrapping 16-bit age counter from deferred-release ordering while
preserving bounded drain batches, blocker policy, oldest-match behavior, and
owner-token settlement.

### 2026-08-15 — Explicit linked FIFO and long-uptime proof

**Implemented**

- Replaced stored sequence numbers with one-byte next indices plus explicit
  core-state head and tail indices. `UINT8_MAX` is the invalid sentinel, and a
  static assertion keeps configured capacity below it.
- Added constant-time tail append and capacity-bounded traversals for ordinal
  snapshots, first eligible drain selection, and oldest matching completion.
- Centralized every removal through one unlink helper responsible for list
  repair, head/tail state, slot clearing, count changes, and owner-token cleanup.
- Kept active-owner entries linked in their original positions while allowing
  the first eligible later entry to drain.
- Added queue high-water and saturating structural-validation-failure fields to
  projection diagnostics. Full structural scanning is host-only.
- Added a focused queue runner to the full host suite. It covers slot reuse;
  head/middle/tail/only unlink; blocked-owner skipping and later eligibility;
  oldest duplicate matching; partial/final owner cleanup; full capacity,
  overflow, drain, and complete reuse; cycles/orphans; and 65,537 cycles with a
  long-lived blocked head.

**Measurements**

- `pending_release_slot_t`: 12 B before and after.
- `pending_release_t`: 14 B to 12 B.
- Host `key_runtime_core_state_t`: 21,780 B before and after.
- Target `.bss`: unchanged at 65,204 B.
- Linked bounded deferred-drain frame: 120 B, down from the prior 136 B record.
- Reviewed target maximum: unchanged at 1,808 B against a 1,920 B budget.
- Final linked sections: `.text` 102,704 B, `.rodata` 15,460 B, `.data`
  23,760 B, `.bss` 65,204 B, and `.ram4` 288 B.

### Current finding status

Finding 09 is **Verified**. No sequence counter remains in queue order, every
removal follows the same list-maintenance contract, direct and integration
coverage pass, the target image links, and the fresh reviewed-path stack gate
retains its required reserve.

## Finding 08 — Synthetic-key ownership

### Objective

Make physical and synthetic producers share aggregate report ownership, migrate
persistent acquisitions to explicit leases, and prevent one producer from
releasing another's key.

### 2026-08-15 — Strict macro-local hold checkpoint

**Implemented**

- Changed macro hold balance so key-up without an earlier unmatched key-down is
  invalid across authored parsing, QMK/VIA decoding, and IR preflight.
- Reordered playback to consume macro-local balance before unregistering. Even
  a malformed IR now fails without issuing a key release.
- Replaced the old encode/round-trip fixture that intentionally blessed an
  orphan Shift release with a balanced explicit Shift hold.
- Added direct decoder and malformed-IR tests proving orphan release has zero
  output side effects.
- Updated macro syntax documentation.

### 2026-08-15 — Aggregate ownership and lease closure

**Implemented**

- Added compact physical and managed counts for basic, system, consumer, and
  mouse usages at the literal action boundary.
- Added explicit owner-scoped leases. Modded actions prevalidate all components,
  share the same basic usage counts, and delegate modifier counts to the
  existing modifier ledger.
- Observed physical usages before QMK default handling and suppressed defaults
  while a managed owner keeps the usage live. Tightened the handled-release
  bypass to actual handled press tokens.
- Migrated held literal actions, macro holds/chords/taps/abort cleanup, and PD
  arrow-selection Shift to retained leases.
- Added saturation, underflow, unsupported-action, and idempotent-release
  diagnostics plus a test-only aggregate reset seam.
- Added mechanical guards for raw QMK and legacy unscoped ownership callers.

**Coverage**

- Direct ownership tests cover transition edges, physical-first and
  managed-first overlap, two managed owners, shared modded basics, mouse,
  consumer, system, saturation, underflow, idempotence, and atomic failure.
- The real runtime scenario harness proves physical `KC_C` survives a
  synthetic tap in both arrival orders.
- Held-action, macro, VIA, PD, runtime debug/trace, layer-lock, hook, profile,
  QMK-contract, and feature variants cover the migrated seams.

**Measurements**

- Baseline commit `5100f2a5`: text 142,364 B, BSS 245,840 B, runtime singleton
  19,752 B, arrow hold flag 1 B.
- Final instrumented image: text 143,524 B, BSS 245,592 B,
  `owned_keycode_state` 456 B, runtime singleton 20,000 B, arrow lease 4 B.
- Explicit ownership storage cost is 707 B. The whole-image BSS difference is
  recorded separately because LTO can reshape unrelated linked storage.
- Stack closure remains 1,808 B of a 1,920 B main-process budget and 328 B of a
  768 B split-thread budget.

### Current finding status

Finding 08 is **Verified**. Aggregate ownership, scoped leases, strict macro
balance, failure handling, mechanical caller boundaries, complete host
coverage, target firmware, resource measurements, and review documentation all
meet the closure bar.

## Finding 06 — Combo-origin cache lifecycle

### Objective

Bound QMK combo-origin candidates without losing attribution for legitimate
outputs emitted after physical member release, and prevent overlap-suppressed
origins from poisoning feedback or cache capacity.

### 2026-08-15 — Pinned contract and failing reproduction

- Audited the pinned QMK pre-hook, `process_combo()`, matrix-scan, and
  `combo_task()` order. The userspace pre-hook runs before combo processing;
  userspace scan runs before matrix event processing and the following
  `combo_task()` cycle.
- Recorded that current profile output is legal strictly after `COMBO_TERM`
  (50 ms) and that `post_process_record_user()` is not reliable for records
  consumed by QMK combo processing.
- Added the suppressed-overlap test first. The old code failed because the
  disabled short candidate remained in the pressed-combo bitmap.

### Implemented

- Added nonzero physical-completion generations allocated away from every live
  pending and active entry, making equality safe across counter wrap.
- Keyed pending/active state by exact combo index and generation. Emitted
  presses promote one candidate; same-output combos no longer union origins or
  clear each other. Release matching uses the triggering physical member.
- Reconciled pending candidates at later physical observations and at the scan
  boundary. Disabled candidates retire immediately. Inactive candidates keep
  one crossed-deadline grace scan for the following QMK `combo_task()`, then
  expire if no output arrived.
- Derived the deadline from the profile-wide maximum QMK wait contract,
  including per-combo term and hold/tap feature variants. Timerless builds do
  not invent an illegal expiry.
- Refused new candidates when all four pending slots are live instead of
  overwriting slot zero. Refused origins never enter pending feedback state.
- Added snapshot diagnostics for current pending/active counts, high-water,
  suppressed retirement, deadline expiry, cache-full refusal, and unmatched
  delayed output.
- Wired reconciliation between VIA default scan work and key-runtime scan.

### Coverage and measurements

- Focused tests cover suppression, delayed output, deadline edges, uint16 timer
  wrap, full-cache refusal and recovery, feedback bitmap cleanup, exact
  same-output generations, unmatched output, reset, normal/compact QMK layouts,
  and the timerless compile branch.
- Runtime init ordering, pinned QMK source ordering/layout parity, hook chaining,
  key-runtime scenarios/integration, split sync, real profile, and feature
  compile gates pass.
- Final instrumented image: 144,172 B text, 245,592 B total BSS. Exact lifecycle
  symbols are 96 B pending cache, 96 B active cache, 12 B diagnostics, and 4 B
  generation state. Text is 648 B above the prior Finding 08 instrumented image;
  fixed-layout total BSS is unchanged and the explicit `.bss` growth reduces
  linked heap.
- The explicit combo-origin retirement stack path is 280 B. The worst reviewed
  main path is 1,816 B of 1,920 B; the split worst remains 328 B of 768 B.
- No sibling source was edited; only pinned-QMK inspection and generated target
  artifacts crossed the repository boundary.

### Current finding status

Finding 06 is **Verified**. Focused and complete host coverage, ordinary target
firmware, fresh explicit stack evidence, measurements, compatibility docs, Sol
records, and closed review 06 agree.

## Finding 07 — Nonblocking macro playback

### Objective

Move macro timing and output out of the triggering key event, preserve complete
IR validation and QMK text translation, and make execution lifetime, overlap,
invalidation, cancellation, and synthetic ownership explicit and bounded.

### Baseline and design

- The pre-change interpreter walked the complete IR synchronously. Explicit
  delay, text, and tap paths all reached helpers that called `wait_ms()`.
- A first fake-timer test intentionally failed to link before the engine API
  existed. It established that a 65,535 ms delay started before timer wrap must
  return immediately with no output or wait call.
- Review 07 fixed the lifecycle contract before implementation: one active
  execution, no queue, deterministic busy rejection, one bounded transition
  per scan, pinned provider IR, and lease-scoped cleanup.

### Landed implementation

- Replaced the synchronous player with a shared idle/ready/waiting/output/
  cleanup engine advanced after key-runtime scan and before split sync.
- Preserved complete IR preflight before the first side effect. Delays use
  unsigned timestamp-plus-duration comparisons across 32-bit wrap.
- Reimplemented QMK ASCII output through the exported keycode, shift, AltGr,
  and dead-key lookup tables. Text and tap-list output acquire exact
  `owned_keycode_lease_t` values, hold them across scan deadlines, and release
  them in reverse acquisition order.
- Persistent key-down operations retain exact leases. Runtime failure and
  cancellation release one retained lease per scan; success requires an empty
  persistent hold set.
- Replaced provider playback with a typed start API. Hardcoded and VIA macros
  use the same engine; busy, invalid, empty, cancellation, and runtime-error
  results remain distinct.
- Added provider pin/stale state. Invalidation cannot overwrite active IR;
  completion applies deferred invalidation before a later load.
- Reset requests cancellation before EEPROM/default stages. VIA default
  reseeding invalidates cached IR after the EEPROM image is updated.
- Added diagnostics for public engine state, active source/slot, current and
  high-water holds, operation/completion/busy/cancellation/error counts, and
  maximum deadline lateness.
- Added explicit fresh-linked stack paths for macro text, chord, persistent
  hold, owned release, provider completion, hardcoded/VIA preflight, and VIA
  default seeding.

### Test and enforcement additions

- `run_macro_payload_engine_tests.sh` runs normal plus ASan/UBSan fake-timer
  tests and mechanically rejects blocking playback helpers in firmware source.
- `run_macro_slot_provider_tests.sh` proves pinned invalidation preserves bytes,
  reloads after completion, and does not pin a busy candidate.
- VIA lifecycle coverage now advances time through explicit scans and proves
  start is side-effect free, long delay/text/chord playback has no wait call,
  active IR survives mid-delay invalidation, and a busy trigger does not restart
  the active slot.
- Runtime order tests pin engine init, reset cancellation, and the scan position
  between key-runtime and split work. The full host runner includes both new
  focused suites.

### Target measurements

- Fresh linked firmware: 145,020 B text and 245,592 B BSS.
- Engine context: 188 B BSS; engine diagnostics: 24 B BSS.
- Text increased by 848 B from the Finding 06 checkpoint. Total BSS did not
  increase at the linked-image level.
- New reviewed main-process macro paths are 276–440 B. The overall reviewed
  worst path remains 1,816/1,920 B; split remains 328/768 B.

### Closure status

Finding 07 is **verified**. Focused normal/sanitizer tests, ownership and action
integration, runtime order and contract gates, feature variants, the full host
suite, ordinary firmware build, and fresh instrumented target stack gate all
pass. User, architecture, review, and Sol documentation describe the landed
one-active scan-driven lifecycle. No sibling source was edited; target commands
only refreshed generated artifacts under `../bastardkb-qmk/.build`.

## Finding 17 — Split timer sampling

### Objective

Use one system-time sample for each initialized master split-runtime tick so
heartbeat, packet construction, successful-send timestamps, and active
auto-mouse progress describe one coherent scan without repeated ChibiOS timer
locks.

### Implemented

- Sample `timer_read32()` once at each initialized master tick, force, init, or
  explicit-elapsed entry and pass `now` through heartbeat, eligibility, packet
  construction, and every broadcast helper.
- Replace `timer_elapsed32()` calls with unsigned `now - last_send` arithmetic;
  all successful domains store the same sampled `now`.
- Skip auto-mouse elapsed lookup when its gradient field is absent, auto mouse
  is inactive, or PD lock suppresses its packet value.
- Centralize active elapsed lookup in
  `noah_qmk_contract_auto_mouse_elapsed_at(now)`, using the sampled timestamp's
  low 16 bits with wrap-safe subtraction.
- With explicit user authorization, extend the sibling QMK fork with
  `auto_mouse_get_time_elapsed_at(uint16_t now)`. Its original no-argument API
  remains compatible by sampling once and delegating.
- Add timer-read and elapsed-helper budgets, same-timestamp assertions,
  heartbeat and 16-bit auto-mouse wrap cases, disabled/inactive feature checks,
  and a no-gradient linked-symbol guard.
- Add reviewed target paths for the matrix-scan clock sample and outbound base
  broadcast.

### Measurements

- Normal and forced initialized master ticks: one `timer_read32()` call and no
  `timer_elapsed32()` calls.
- Shared-clock target path: 280 B.
- Outbound split base-broadcast target path: 464 B.
- Overall reviewed main-process worst path: 1,816/1,920 B.
- Split-slave reviewed worst path: 328/768 B.
- Linked firmware: 145,060 B text and 245,592 B BSS. This is +40 B text and no
  linked BSS change from the Finding 07 checkpoint.

### Closure status

Finding 17 is **verified**. Focused split, compatibility, pointing, RGB, and
feature checks pass, as do the complete host suite, ordinary firmware build,
fresh stack-budget build, and diff hygiene. Review 08 records the closure. The
change crossed into the authorized sibling QMK fork only for the two-file
auto-mouse compatibility extension.

## Finding 12 — Split RPC failure backoff

### Objective

Prevent a disconnected secondary half from turning every matrix scan into up
to four consecutive 5 ms serial timeouts, while preserving current state for
automatic recovery.

### Implemented

- Added explicit shared transport-health state rather than overloading
  per-domain successful-send timestamps.
- Stop the base/combo/semantic/branch send pass immediately after its first
  failed RPC.
- Suppress packet builders, auto-mouse elapsed lookup, and all runtime RPCs
  until a wrap-safe retry deadline becomes due.
- Use a configurable exponential schedule of 50, 100, 200, 400, 800, then
  1,000 ms capped.
- Force-send the current base packet as the recovery probe. On success, clear
  outage state and drain currently eligible later domains in deterministic
  order; stop again on any new failure.
- Preserve dirty flags and last-success packets/timestamps until the matching
  domain succeeds. Reinitialization clears inherited outage state.
- Emit bounded failure and recovery trace events without emitting one event per
  suppressed scan.
- Regenerated `docs/KEYMAP-OVERVIEW.md` because the retry constants are
  introspected authored configuration.

### Measurements

- Fully due failed tick: four RPC attempts before, one after.
- Suppressed tick: zero RPC attempts and zero packet/auto-mouse builders.
- Retry window: 50 ms initial, 1,000 ms maximum.
- Outbound split base-broadcast target path: 480 B, up from 464 B.
- Overall reviewed main-process worst path: unchanged at 1,816/1,920 B.
- Split-slave reviewed worst path: unchanged at 328/768 B.
- Linked firmware: 145,204 B text and 245,592 B BSS, +144 B text and no BSS
  change from Finding 17.

### Closure status

Finding 12 is **verified**. Scripted failure, schedule/cap/wrap, no-build
suppression, current-state recovery, active-to-idle clearing, role reset, and
bounded trace coverage pass. The complete host suite, ordinary firmware build,
fresh target stack gate, generated-doc check, and diff hygiene pass. Review 09
records the closure.

## Finding 05 — VIA split persistence

### Objective

Replace best-effort pre-apply command replay with durable committed-state
reconciliation that survives loss, reboot, reconnect, and role changes.

### 2026-08-15 — Command, metadata, and framing foundation

**Implemented**

- Added validated full-packet mutation classification for keymap, encoder,
  macro, reset, EEPROM-reset, and layout-option writes.
- Removed outbound raw replay from the pre-QMK hook. The hook now retains only
  owned effect flags and never stores a borrowed HID packet pointer.
- Reserved the existing atomic 32-bit user-eeconfig word for schema, dirty
  state, and a nonzero 27-bit serial generation. This does not move VIA data
  or require an EEPROM migration.
- Persist dirty state before returning to upstream QMK; coalesce repeated edits
  without extra EEPROM writes; refuse to publish dirty/unknown boot state.
- Made macro-default seeding success explicit. Reset publishes clean
  generation 1 only after successful seeding and remains dirty on failure.
- Added a fixed 32-byte, endian-stable, CRC-8 protected frame envelope for
  metadata, snapshot begin, chunk push/pull, commit, ack, and error messages.
  Strict shape/range validation precedes all future storage effects.

**Current boundary reconciliation**

That foundation checkpoint has now advanced to a live durable implementation.
Canonical region readback/digest, post-QMK clean completion, acknowledged
one-fragment snapshot push/pull, bounded retry and periodic rejoin, role and
generation conflict policy, receiver digest commit, and post-commit RGB/macro
cache invalidation have landed. Shared callback/scan state uses short atomic
snapshots and epoch cancellation so a replacement transfer cannot publish an
older verification result.

Focused normal, sanitizer, encoder, state, and feature-gate checks pass. Full
host and ordinary firmware also pass on the final source state. The ordinary
target is 150,748 B text and 245,584 B BSS (+5,544 B text and -8 B BSS versus
the Finding 12 baseline). A fresh instrumented target build passes every
reviewed path: 1,904/1,920 B for the worst main-process path and 336/768 B for
the worst split-worker path. Physical two-half power-cycle and USB-role-swap
verification remains intentionally pending and is the only remaining Finding
05 closure gate.

## Finding 11 — Dragscroll stall recovery

### 2026-08-15 — Verified

- Moved prior lock/residual expiry before current motion accumulation, with
  inclusive 55/56 ms lock and 80/81 ms buffer boundaries.
- Derived buffer, rate, and lock ages from one wrap-safe `timer_read32()`
  sample; the host fixture proves zero `timer_elapsed32()` calls.
- Preserved immediate new-axis output, exact rate behavior, no-motion expiry,
  reset, and the manifest-owned DRAGSCROLL/PINCH handler/reset pairing.
- Left all gesture tuning and Finding 10 backlog policy unchanged.
- Added an explicit post-LTO pointing-task path: 360/1,920 B. Ordinary target
  size remains 150,748 B text and 245,584 B BSS; overall reviewed maxima remain
  1,904/1,920 B main and 336/768 B split.

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
| 2026-08-15 | 03 | `sh tests/host/run_qmk_via_split_sync_tests.sh` | Passed | Normal, ASan/UBSan exhaustive matrix, and encoder-enabled variants |
| 2026-08-15 | 03 | `sh tests/host/run_qmk_contract_checks.sh` | Passed | Compatibility contract remains coherent |
| 2026-08-15 | 03 | `sh tests/host/run_via_macro_defaults_tests.sh` | Passed | VIA defaults behavior remains intact |
| 2026-08-15 | 03 | `sh tests/host/run_action_lifecycle_tests.sh` | Passed | Action lifecycle remains intact |
| 2026-08-15 | 03 | `sh tests/host/run_feature_gate_compile_tests.sh` | Passed | Compatibility header/build variants compile |
| 2026-08-15 | 03 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Final complete host suite with normal, sanitizer, and encoder VIA replay variants |
| 2026-08-15 | 03 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Required ordinary target firmware build |
| 2026-08-15 | 03 | `sh tests/host/run_firmware_stack_budget_checks.sh` | Passed | Fresh clean target build; 1,808 B worst reviewed main path and 88 B reviewed VIA split path |
| 2026-08-15 | 03 | `git diff --check` | Passed | Source, tests, compatibility docs, Sol ledger, and closed Finding 03 review |
| 2026-08-15 | 04 | `sh tests/host/run_macro_payload_tests.sh` before implementation | Failed as expected | First `0x80` text case reproduced the unsafe decoder acceptance |
| 2026-08-15 | 04 | `sh tests/host/run_macro_payload_tests.sh` | Passed | Normal and ASan/UBSan exhaustive text-domain and whole-IR preflight coverage |
| 2026-08-15 | 04 | `sh tests/host/run_macro_dispatch_tests.sh` | Passed | Hardcoded macro dispatch remains coherent |
| 2026-08-15 | 04 | `sh tests/host/run_via_macro_defaults_tests.sh` | Passed | Authored VIA defaults remain valid |
| 2026-08-15 | 04 | `sh tests/host/run_via_macro_action_lifecycle_tests.sh` | Passed | Negative cache, repair/invalidation, and owned action lifecycle pass |
| 2026-08-15 | 04 | `sh tests/host/run_action_lifecycle_tests.sh` | Passed | General action lifecycle remains intact |
| 2026-08-15 | 04 | `sh tests/host/run_qmk_contract_checks.sh` | Passed | QMK compatibility contracts remain coherent |
| 2026-08-15 | 04 | `sh tests/host/run_feature_gate_compile_tests.sh` | Passed | Macro interface and feature variants compile |
| 2026-08-15 | 04 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh` | Passed | All 15 stack-tool schema/edge fixtures pass after adding macro preflight paths |
| 2026-08-15 | 04 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Required ordinary target firmware build |
| 2026-08-15 | 04 | `sh tests/host/run_firmware_stack_budget_checks.sh` | Passed | Fresh build; hardcoded macro preflight 760 B, VIA macro preflight 1,152 B, worst reviewed path unchanged at 1,808 B |
| 2026-08-15 | 04 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Final same-tree complete host suite, including normal and sanitizer macro payload variants |
| 2026-08-15 | 04 | `git diff --check` | Passed | Source, tests, stack manifest, macro docs, Sol ledger, and closed Finding 04 review |
| 2026-08-15 | 02 | `sh tests/host/run_runtime_debug_tests.sh` | Passed | Production wrap/collision/store coverage and three-ID exhaustion build |
| 2026-08-15 | 02 | `sh tests/host/run_key_runtime_release_matrix_tests.sh` | Passed | Release ownership remains coherent |
| 2026-08-15 | 02 | `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh` | Passed | Modifier hold ownership remains balanced |
| 2026-08-15 | 02 | `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh` | Passed | Both PD integration variants pass |
| 2026-08-15 | 02 | `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh` | Passed | Layer lock ownership remains coherent |
| 2026-08-15 | 02 | `sh tests/host/run_key_runtime_scenario_tests.sh` | Passed | Scenario matrix remains coherent |
| 2026-08-15 | 02 | `sh tests/host/run_key_runtime_integration_harness_tests.sh` | Passed | Production integration harness passes |
| 2026-08-15 | 02 | `sh tests/host/run_held_action_tests.sh` | Passed | Held/repeat registry behavior remains balanced |
| 2026-08-15 | 02 | `sh tests/host/run_action_lifecycle_tests.sh` | Passed | General action lifecycle remains intact |
| 2026-08-15 | 02 | `sh tests/host/run_feature_gate_compile_tests.sh` | Passed | Header and build variants compile |
| 2026-08-15 | 02 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh` | Passed | All 15 stack-tool fixtures pass with allocation path manifest |
| 2026-08-15 | 02 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Final complete host suite, including both allocator-domain builds |
| 2026-08-15 | 02 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Required ordinary target firmware build |
| 2026-08-15 | 02 | `sh tests/host/run_firmware_stack_budget_checks.sh` | Passed | Fresh build; allocator path 1,104 B, overall reviewed worst 1,808 B |
| 2026-08-15 | 02 | `git diff --check` | Passed | Source, tests, stack manifest, runtime docs, Sol ledger, and closed Finding 02 review |
| 2026-08-15 | 09 | `sh tests/host/run_pending_release_queue_tests.sh` | Passed | Layout, FIFO/reuse, all unlink positions, blocked owners, oldest duplicate, owner cleanup, capacity, corruption, and 65,537-cycle stress |
| 2026-08-15 | 09 | `sh tests/host/run_key_runtime_release_matrix_tests.sh` | Passed | Release ownership matrix remains coherent |
| 2026-08-15 | 09 | `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh` | Passed | Layer-lock deferred releases remain balanced |
| 2026-08-15 | 09 | `sh tests/host/run_key_runtime_scenario_tests.sh` | Passed | Scenario matrix retains release order |
| 2026-08-15 | 09 | `sh tests/host/run_key_runtime_integration_harness_tests.sh` | Passed | End-to-end runtime integration remains coherent |
| 2026-08-15 | 09 | `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh` | Passed | Both PD-mode integration variants pass |
| 2026-08-15 | 09 | `sh tests/host/run_runtime_debug_tests.sh` | Passed | Both token-domain variants retain deferred queue behavior |
| 2026-08-15 | 09 | `sh tests/host/run_feature_gate_compile_tests.sh` | Passed | Header/state/build variants compile with linked FIFO state |
| 2026-08-15 | 09 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Final complete host suite includes the new focused runner |
| 2026-08-15 | 09 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Required ordinary target firmware build |
| 2026-08-15 | 09 | `sh tests/host/run_firmware_stack_budget_checks.sh` | Blocked, then passed | Initial sandboxed clean could not modify sibling QMK artifacts; approved fresh clean/rebuild passed with 1,808 B worst reviewed path |
| 2026-08-15 | 09 | `git diff --check` | Passed | Source, focused test/runner, runtime docs, Sol records, and closed Finding 09 review |
| 2026-08-15 | 08 | `sh tests/host/run_macro_payload_tests.sh` | Passed | Normal and sanitizer variants reject authored, decoded, and malformed-IR orphan key-up before side effects |
| 2026-08-15 | 08 | `sh tests/host/run_via_macro_action_lifecycle_tests.sh` | Passed | Existing valid VIA macro ownership lifecycle remains coherent |
| 2026-08-15 | 08 | `sh tests/host/run_macro_dispatch_tests.sh` | Passed | Hardcoded macro dispatch remains coherent under strict balance |
| 2026-08-15 | 08 | `sh tests/host/run_via_macro_defaults_tests.sh` | Passed | Authored default macro payloads remain valid |
| 2026-08-15 | 08 | `sh tests/host/run_action_lifecycle_tests.sh` | Passed | General action lifecycle remains coherent |
| 2026-08-15 | 08 | `sh tests/host/run_qmk_contract_checks.sh` | Passed | QMK macro-stream contracts remain coherent |
| 2026-08-15 | 08 | `sh tests/host/run_feature_gate_compile_tests.sh` | Passed | Macro/runtime feature variants compile |
| 2026-08-15 | 08 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Complete post-checkpoint host suite |
| 2026-08-15 | 08 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Ordinary target firmware build after strict balance checkpoint |
| 2026-08-15 | 08 | `git diff --check` | Passed | Strict balance source, tests, keymap docs, Sol checkpoint, and active review |
| 2026-08-15 | 08 | `sh tests/host/run_owned_keycode_tests.sh` | Passed | Aggregate edges, overlap orders, shared basics, report domains, diagnostics, and raw-caller guards |
| 2026-08-15 | 08 | `sh tests/host/run_keyboard_mod_ownership_tests.sh` | Passed | Modifier capacity and unregister preflight remain atomic |
| 2026-08-15 | 08 | `sh tests/host/run_held_action_tests.sh` | Passed | Per-owner literal leases and failed-acquisition teardown are fail-closed |
| 2026-08-15 | 08 | `sh tests/host/run_macro_payload_tests.sh` | Passed | Macro persistent holds, chords, and abort cleanup release exact leases |
| 2026-08-15 | 08 | `sh tests/host/run_key_runtime_scenario_tests.sh` | Passed | Real hook ordering preserves physical-first and managed-first `KC_C` ownership |
| 2026-08-15 | 08 | `sh tests/host/run_feature_gate_compile_tests.sh` | Passed | Runtime, ownership, and feature variants compile with final interfaces |
| 2026-08-15 | 08 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Final same-tree complete host suite |
| 2026-08-15 | 08 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Required ordinary target build on final source tree |
| 2026-08-15 | 08 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh` | Passed | Fresh target artifacts; 1,808 B worst reviewed main path and 328 B split path |
| 2026-08-15 | 08 | `git diff --check` | Passed | Final source, tests, runtime docs, Sol records, and closed review 05 |
| 2026-08-15 | 06 | `sh tests/host/run_qmk_combo_origin_tests.sh` before implementation | Failed as expected | Overlap-disabled short candidate remained in the pressed-combo feedback bitmap |
| 2026-08-15 | 06 | `sh tests/host/run_qmk_combo_origin_tests.sh` | Passed | Normal/compact layouts; suppression, deadline/wrap, capacity, identity, diagnostics, release footprint; timerless compile |
| 2026-08-15 | 06 | `sh tests/host/run_runtime_init_order_tests.sh` | Passed | Combo reconciliation is between VIA default scan and key-runtime scan |
| 2026-08-15 | 06 | `sh tests/host/run_key_runtime_scenario_tests.sh` | Passed | Key-runtime scenarios remain coherent |
| 2026-08-15 | 06 | `sh tests/host/run_key_runtime_integration_harness_tests.sh` | Passed | End-to-end runtime integration remains coherent |
| 2026-08-15 | 06 | `sh tests/host/run_hook_chaining_tests.sh` | Passed | Shared hook delegation remains coherent |
| 2026-08-15 | 06 | `sh tests/host/run_split_runtime_sync_tests.sh` | Passed | Both split-sync variants preserve combo feedback transport |
| 2026-08-15 | 06 | `sh tests/host/run_real_profile_validation_tests.sh` | Passed | Both real-profile variants retain authored combo validity |
| 2026-08-15 | 06 | `sh tests/host/run_feature_gate_compile_tests.sh` | Passed | Compatibility header and runtime feature variants compile |
| 2026-08-15 | 06 | `sh tests/host/run_qmk_contract_checks.sh` | Passed | Normal/compact combo layout parity and pinned hook/scan/task order |
| 2026-08-15 | 06 | `python3 tools/profile_introspect.py --write && python3 tools/profile_introspect.py --check` | Passed | Authored keymap comment changed; generated profile outputs remained current |
| 2026-08-15 | 06 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Complete host suite with new lifecycle and contract variants |
| 2026-08-15 | 06 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Required ordinary target firmware build |
| 2026-08-15 | 06 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh` | Blocked, then passed | Initial sandboxed clean could not modify sibling generated QMK artifacts; approved fresh rebuild passed with explicit 280 B combo path, 1,816 B worst main path, and 328 B split path |
| 2026-08-15 | 07 | `sh tests/host/run_macro_payload_engine_tests.sh` before implementation | Failed as expected | Fake-timer wrap/nonblocking contract could not link before the engine API existed |
| 2026-08-15 | 07 | `sh tests/host/run_macro_payload_tests.sh` | Passed | Parser/compiler/encoder/decoder validation passes in normal and sanitizer variants |
| 2026-08-15 | 07 | `sh tests/host/run_macro_payload_engine_tests.sh` | Passed | Normal and sanitizer variants cover immediate start, 65,535 ms wrap, lease-backed text, busy rejection, bounded cancellation, runtime failure, malformed IR, and blocking-helper source guards |
| 2026-08-15 | 07 | `sh tests/host/run_macro_slot_provider_tests.sh` | Passed | Pinned invalidation and busy no-pin contracts pass |
| 2026-08-15 | 07 | `sh tests/host/run_macro_dispatch_tests.sh` | Passed | Hardcoded macro dispatch uses the typed shared-engine start contract |
| 2026-08-15 | 07 | `sh tests/host/run_via_macro_action_lifecycle_tests.sh` | Passed | Scan-driven VIA tap/down/up/text/delay/chord, cache invalidation, and busy behavior pass |
| 2026-08-15 | 07 | `sh tests/host/run_via_macro_defaults_tests.sh` | Passed | VIA defaults remain valid and reseeding invalidates provider state |
| 2026-08-15 | 07 | `sh tests/host/run_runtime_init_order_tests.sh` | Passed | Init, reset cancellation, and matrix-scan engine ordering are pinned |
| 2026-08-15 | 07 | `sh tests/host/run_owned_keycode_tests.sh`, `run_keyboard_mod_ownership_tests.sh`, `run_held_action_tests.sh`, and `run_action_lifecycle_tests.sh` | Passed | Aggregate owner isolation and surrounding action lifecycles remain coherent |
| 2026-08-15 | 07 | `sh tests/host/run_feature_gate_compile_tests.sh` | Passed | Macro/runtime feature and header variants compile |
| 2026-08-15 | 07 | `sh tests/host/run_qmk_contract_checks.sh` | Passed | Pinned QMK/VIA action and scan contracts remain coherent |
| 2026-08-15 | 07 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh` | Passed | All 15 stack-manifest fixtures pass with new macro edge/path coverage |
| 2026-08-15 | 07 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Complete host suite includes the two new focused runners |
| 2026-08-15 | 07 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Required ordinary target build on final firmware source |
| 2026-08-15 | 07 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh` | Passed | Fresh linked macro paths pass; 1,816 B worst reviewed main path and 328 B split path |
| 2026-08-15 | 17 | `sh tests/host/run_split_runtime_sync_tests.sh` before implementation | Failed as expected | The existing tick exceeded the new one-read `timer_read32()` budget |
| 2026-08-15 | 17 | `sh tests/host/run_split_runtime_sync_tests.sh` | Passed | Normal/no-gradient variants enforce one sampled clock, no elapsed helpers, coherent success timestamps, inactive gates, and 32/16-bit wrap behavior |
| 2026-08-15 | 17 | `sh tests/host/run_qmk_contract_checks.sh` | Passed | Authorized QMK elapsed-at declaration, wrap-safe implementation, and backward-compatible delegating wrapper are pinned |
| 2026-08-15 | 17 | `sh tests/host/run_pd_runtime_tests.sh` | Passed | PD state behavior remains coherent |
| 2026-08-15 | 17 | `sh tests/host/run_rgb_layer_render_tests.sh` | Passed | Auto-mouse RGB progress behavior remains coherent |
| 2026-08-15 | 17 | `sh tests/host/run_feature_gate_compile_tests.sh` | Passed | Split and compatibility feature variants compile |
| 2026-08-15 | 17 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh` | Passed | All 15 schema-v2 path and edge fixtures pass after manifest expansion |
| 2026-08-15 | 17 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Complete host suite passes with the shared-clock contract |
| 2026-08-15 | 17 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Required ordinary build against the authorized sibling QMK extension |
| 2026-08-15 | 17 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh` | Passed | Fresh clock path 280 B, base-broadcast path 464 B, worst main 1,816/1,920 B, split 328/768 B |
| 2026-08-15 | 12 | `sh tests/host/run_split_runtime_sync_tests.sh` before implementation | Failed as expected | A fully forced pass continued after the first failed base RPC instead of stopping at one attempt |
| 2026-08-15 | 12 | `sh tests/host/run_split_runtime_sync_tests.sh` | Passed | Both feature variants cover failure stop, force suppression, no-build backoff, schedule/cap/wrap, current-state recovery, active-to-idle clear, and role reset |
| 2026-08-15 | 12 | `sh tests/host/run_runtime_trace_tests.sh` | Passed | Failure/recovery events are emitted and twenty suppressed scans add no trace entries |
| 2026-08-15 | 12 | `sh tests/host/run_runtime_debug_tests.sh` | Passed | Runtime diagnostics remain coherent in both variants |
| 2026-08-15 | 12 | `sh tests/host/run_feature_gate_compile_tests.sh` | Passed | Split, trace, and header feature variants compile with the health-state API |
| 2026-08-15 | 12 | `python3 tools/profile_introspect.py --write && python3 tools/profile_introspect.py --check` | Passed | Regenerated the retry constants in the authored configuration overview |
| 2026-08-15 | 12 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Complete host suite passes after generated documentation reconciliation |
| 2026-08-15 | 12 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Required ordinary target firmware build |
| 2026-08-15 | 12 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh` | Passed | Fresh outbound path 480 B; worst main 1,816/1,920 B and split 328/768 B |
| 2026-08-15 | 05 | `sh tests/host/run_qmk_via_command_classifier_tests.sh` | Passed | Complete validated mutation table and malformed/read-only rejection |
| 2026-08-15 | 05 | `sh tests/host/run_qmk_via_sync_metadata_tests.sh` | Passed | Schema, dirty, nonzero generation, wrap, and half-range ordering |
| 2026-08-15 | 05 | `sh tests/host/run_qmk_via_sync_state_tests.sh` | Passed | Dirty-before-complete, coalescing, recovery, reset outcome, and rollover |
| 2026-08-15 | 05 | `sh tests/host/run_qmk_via_sync_protocol_tests.sh` | Passed | All frame kinds plus corruption, truncation, schema, range, and shape rejection |
| 2026-08-15 | 05 | `sh tests/host/run_qmk_via_split_sync_tests.sh` | Passed | Normal, sanitizer, and encoder transitional split-sync variants |
| 2026-08-15 | 05 | `sh tests/host/run_via_macro_defaults_tests.sh` | Passed | Explicit seed success/failure and deferred reseed lifecycle |
| 2026-08-15 | 05 | `sh tests/host/run_runtime_init_order_tests.sh` | Passed | Reset metadata finalization occurs after macro default seeding |
| 2026-08-15 | 05 | `sh tests/host/run_qmk_contract_checks.sh` and `sh tests/host/run_hook_chaining_tests.sh` | Passed | QMK and weak-hook contracts remain coherent |
| 2026-08-15 | 05 | `sh tests/host/run_feature_gate_compile_tests.sh` | Passed | New manifest sources and feature variants compile |
| 2026-08-15 | 05 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Complete host suite at the durable-state/frame-codec checkpoint |
| 2026-08-15 | 05 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Ordinary target build at the checkpoint; protocol remains intentionally inactive |
| 2026-08-15 | 05 | `git diff --check` | Passed | Current uncommitted Finding 05 checkpoint |
| 2026-08-15 | 05 | `sh tests/host/run_qmk_via_split_sync_tests.sh` | Passed | Final live reconciliation tests in normal, ASan/UBSan, and encoder variants, including dirty-recovery reseed failure |
| 2026-08-15 | 05 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Final complete host suite after live snapshot transport and recovery correction |
| 2026-08-15 | 05 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Final ordinary target: 150,748 B text, 245,584 B BSS |
| 2026-08-15 | 05 | `python3 tools/profile_introspect.py --write` and `--check` | Passed | Authored config input regenerated and verified; generated content remained coherent |
| 2026-08-15 | 05 | `sh tests/host/run_firmware_stack_budget_checks.sh` | Blocked | Sandbox could not recreate sibling QMK build artifacts; escalation rejected after app usage limit, with no budget assertion executed |
| 2026-08-15 | 05 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh` | Passed | 15 schema-v2 fixtures after manifest reconciliation to the new main/slave paths |
| 2026-08-15 | 05 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh` | Passed | Fresh post-LTO target: 1,904/1,920 B worst reviewed main path and 336/768 B worst reviewed split path; all Finding 05 digest/RPC/storage/commit/recovery/ack paths pass |
| 2026-08-15 | 11 | `sh tests/host/run_pd_mode_handlers_tests.sh` before implementation | Failed as expected | The first report after a 56 ms gap emitted the retained horizontal step because current motion refreshed stale state before expiry |
| 2026-08-15 | 11 | `sh tests/host/run_pd_mode_handlers_tests.sh` | Passed | 55/56 and 80/81 ms boundaries, immediate fresh-axis output, no-motion expiry, rate boundary, wrap, reset, and one-read timer budget |
| 2026-08-15 | 11 | `sh tests/host/run_pd_mode_tests.sh` | Passed | DRAGSCROLL and PINCH remain pinned to the shared handler and reset callback |
| 2026-08-15 | 11 | `sh tests/host/run_pd_runtime_tests.sh`, `run_pd_mode_key_runtime_integration_tests.sh`, and `run_pointer_layer_policy_tests.sh` | Passed | Surrounding pointing runtime, key-runtime integration variants, and layer policy remain coherent |
| 2026-08-15 | 11 | `sh tests/host/run_feature_gate_compile_tests.sh` | Passed | Pointing/config feature variants compile |
| 2026-08-15 | 11 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Complete host suite on the final formatted source |
| 2026-08-15 | 11 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Ordinary target remains 150,748 B text and 245,584 B BSS |
| 2026-08-15 | 11 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh` | Passed | All 15 manifest/schema fixtures pass |
| 2026-08-15 | 11 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh` | Passed | Explicit dragscroll path 360/1,920 B; overall main 1,904/1,920 B and split 336/768 B |
| 2026-08-15 | 10 | `sh tests/host/run_pd_mode_handlers_tests.sh` before implementation | Failed as expected | After correcting the host's stale 8-bit report model, one maximum report overflowed the eight-entry tap log in the old unbounded loop |
| 2026-08-15 | 10 | `sh tests/host/run_pd_mode_handlers_tests.sh` | Passed | All signs/modes, 4-tap ceiling, 32-step cap, exact residual, zero draining, sustained overload, reversal/reset, saturating diagnostics, extreme dominance, and compile-fail guards |
| 2026-08-15 | 10 | `sh tests/host/run_pd_mode_tests.sh`, `run_pd_runtime_tests.sh`, `run_pd_mode_key_runtime_integration_tests.sh`, `run_pointer_layer_policy_tests.sh`, and `run_owned_keycode_tests.sh` | Passed | Registry/reset, zero-report dispatch, surrounding key runtime/layer policy, and ownership contracts remain coherent |
| 2026-08-15 | 10 | `sh tests/host/run_feature_gate_compile_tests.sh` | Passed | Standalone and feature-gated pointing variants compile with guarded shared defaults |
| 2026-08-15 | 10 | `python3 tools/profile_introspect.py --write && python3 tools/profile_introspect.py --check` | Passed | Generated profile overview includes the 4/32 authored limits |
| 2026-08-15 | 10 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Complete host suite on the final shared implementation |
| 2026-08-15 | 10 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Ordinary target is 150,908 B text and 245,584 B BSS |
| 2026-08-15 | 10 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh` | Passed | Fresh generic path 1,240 B, vertical-arrow path 1,280 B, worst main 1,904/1,920 B, split 336/768 B |
| 2026-08-15 | 13 | `sh tests/host/run_rgb_layer_render_tests.sh` before implementation | Failed as expected | Five fresh processes independently exposed inherit, universal, base-less explicit, override-order, and chunk-parity mismatches |
| 2026-08-15 | 13 | `sh tests/host/run_rgb_layer_render_tests.sh`, `run_rgb_validation_tests.sh`, `run_split_runtime_sync_tests.sh`, and `run_feature_gate_compile_tests.sh` | Passed | Shared selection renderer, host scan budget, empty authored groups, split preview source, and feature-disabled header variants pass |
| 2026-08-15 | 13 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Complete host suite including all five layer-group parity scenarios |
| 2026-08-15 | 13 | `qmk compile -kb bastardkb/charybdis/4x6 -km noah` | Passed | Ordinary target is 150,896 B text and 245,592 B BSS; no second preview frame allocation |
| 2026-08-15 | 13 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh` | Passed | Fresh reviewed maxima remain 1,904/1,920 B main and 336/768 B split |
| 2026-08-15 | 15 | `sh tests/host/run_rgb_layer_render_tests.sh` before implementation | Failed as expected | New 58-LED work budget first failed on repeated semantic projection; audited baseline was 10 semantic and 20 combo projection invocations per five chunks |
| 2026-08-15 | 15 | Targeted RGB, validation, split, key-runtime scenario, runtime-trace, and feature-gate runners | Passed | Left/right parity, physical filtering, local/remote frame coherence, flash refresh, source reuse, stage order, and compile variants pass |
| 2026-08-15 | 15 | `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh` | Passed | Complete host suite includes the 58-LED workload and Finding 13 parity oracle |
| 2026-08-15 | 15 | `qmk compile -c -kb bastardkb/charybdis/4x6 -km noah` | Passed | Ordinary target is 151,000 B text, 0 B data, and 245,592 B BSS; linked source snapshot is 80 B with a 96 B compile ceiling |
| 2026-08-15 | 15 | `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh` | Passed | Fresh reviewed maxima remain 1,904/1,920 B main and 336/768 B split; snapshot adds no automatic array |

## Finding 10 — Pointing backlog bounds

### Objective

Prevent extreme signed 16-bit pointing reports from monopolizing the main loop
or overflowing accumulated motion, and make `INT16_MIN` dominant-axis behavior
exact without changing normal thresholds or mappings.

### Implemented

- Added a global four-tap per-poll budget and 32-whole-tap retained-debt cap.
- Preserved exact sub-threshold residual while deterministically discarding
  only excess whole taps.
- Kept a single active-mode budget; arrow still services only its selected
  dominant axis.
- Preserved direction-reversal clearing and made mode reset clear accumulator,
  direction, backlog, and per-activation diagnostics.
- Added per-axis saturation, dropped-tap, current/past backlog, and maximum
  emitted-tap snapshots with saturating counters.
- Added compile-time rejection for every invalid budget, cap, threshold, and
  accumulator-headroom configuration covered by the plan.
- Widened arrow magnitude to `int32_t` before negation.
- Corrected the host QMK report fixture to honor extended X/Y and H/V widths.
- Proved active modes receive zero-motion reports through the existing userspace
  path, so no duplicate scan scheduler was introduced.

### Target decision

The initial 4/32 policy is retained. It replaces up to roughly 820 synchronous
taps with a four-tap hard bound and an eight-successful-poll maximum tail. A
measured force-inline experiment was rejected because it added 312 B of text
and did not remove software division. The final shared version adds 160 B over
Finding 11, leaves BSS unchanged, and uses one combined quotient/remainder
operation per handler call.

### Remaining work

Software, host, ordinary target-build, and reviewed stack gates all pass.
Finding 10 remains **implemented, verification incomplete** until flashed
hardware records worst-case handler duration and confirms the 4/32 policy feels
acceptable in arrow, zoom, volume, and brightness modes. Review 12 stays open.

## Finding 13 — RGB preview parity

### Objective

Make a pending layer preview resolve exactly the same authored base color,
layer groups, inheritance, row order, painted state, and chunk boundaries as
normal activation of that selected layer.

### Implemented

- Replaced preview's partial direct-to-driver renderer with the normal layer
  stage's selection-aware two-phase frame algorithm.
- Kept normal effective-layer filtering and preview single-layer selection as
  inputs to one renderer instead of duplicating color rules.
- Reused the runtime's existing primary frame after base application, avoiding
  both main-stack growth and a second RGB-sized BSS buffer.
- Removed the preview-only group intersection and direct HSV conversion paths.
- Added a synthetic layer-group host variant with independently runnable
  inheritance, universal, no-base, override-order, and chunk scenarios.
- Added a host-only scan counter proving preview performs one group-table pass
  in addition to the normal base pass.
- Preserved the existing combo-underlay, preview, PD, combo-overlay, and
  key-feedback stage order.

### Target result

The ordinary target is 150,896 B text, 0 B data, and 245,592 B BSS: 12 B less
text and an 8 B linked-layout BSS difference from Finding 10, with no new
production frame or diagnostic allocation. Fresh reviewed stack maxima remain
1,904/1,920 B main and 336/768 B split.

### Closure

Finding 13 is **verified**. Review 13 records the red evidence, shared contract,
target evidence, documentation, and all passing closure gates.

## Finding 14 — Macro cache RAM

### Objective

Keep 64 VIA and 16 hardcoded logical slots without reserving a full 512-byte
decoded program for every slot, while preserving negative caching and stable
scan-driven playback ownership.

### Implemented

- Replaced each per-slot IR/cache object with one byte of validation metadata.
- Added one shared 514-byte decoded IR, pinned until the single active macro
  finishes or cancels.
- Preserved negative caching, explicit VIA invalidation, active mutation
  deferral, engine busy diagnostics, and provider independence.
- Made valid playback re-decode once per invocation and added deterministic
  compile/read-count tests.
- Added exact 512-byte capacity and over-capacity tests.
- Added a target memory checker for named macro storage, static BSS, and linker
  heap, plus host fixtures and developer documentation.
- Reconciled the target stack manifest with the linked provider → engine
  preflight edge.

### Target result

Named macro storage falls from 41,440 B to 599 B, reclaiming 40,841 B. Static
BSS is 25,524 B and the recovered RAM appears as 212,608 B linker heap. GNU
`size` remains 245,592 B BSS because that figure includes the linker-reserved
heap. Ordinary text is 151,024 B, a 24 B increase. Fresh reviewed stack maxima
remain 1,904/1,920 B main and 336/768 B split.

### Closure

Finding 14 is **verified**. Review 15 records the ownership contract, red
evidence, resource-policy reconciliation, target measurements, documentation,
and all passing closure gates.

## Finding 15 — RGB render work

### Objective

Stop rebuilding identical combo and key-feedback source projections for every
12-LED callback, and reject chunks that cannot touch the current physical half
without changing a single authored RGB result.

### Implemented

- Added exact per-frame invalidation from QMK's `rgb_matrix_get_limits(0)`
  contract, checked before right-half inverted-range rejection.
- Added top-level physical-half normalization and early exit.
- Added one bounded, lazy, runtime-owned source snapshot.
- Reused one semantic map for flash visibility and one combined combo
  projection for underlay, overlay, and suppression.
- Changed combo and key-feedback stage seams to consume immutable maps.
- Added a 58-LED/five-chunk host fixture for both physical roles, full/chunk
  color parity, work counts, empty/nonlocal ranges, idle probing, and local and
  remote mid-frame coherence.
- Preserved Finding 13's color-parity fixture and all existing stage-order
  coverage.

### Target result

Five-chunk semantic projection falls from 10 builds to 1, combined combo
projection from 20 invocations to 1, and stage-pipeline entry from 5 chunks to
the 3 chunks that intersect either physical half. The source snapshot is 80 B
and compile-time bounded to 96 B. Ordinary target size is 151,000 B text, 0 B
data, and 245,592 B BSS; linked BSS is unchanged from Finding 13. Fresh reviewed
stack maxima remain 1,904/1,920 B main and 336/768 B split.

### Closure

Finding 15 is **verified**. Review 14 records the QMK frame contract, red
evidence, coherent snapshot policy, work budget, target measurements, and all
passing closure gates.

## Finding 16 — Runtime lookup hot path

### Objective

Bound authored behavior resolution and runtime scan work to the actual event
and active entries, then stop invalidating split feedback on unchanged active
scans without losing deadline-driven visual updates.

### Implemented

- Added host-only authored-search, row-comparison, active-slot-visit,
  bitmap-consistency, and feedback-dirty counters.
- Derived a complete handled resolution from one located authored config row.
- Made the observed position-owned press token authoritative for normal
  preflight, handled press routing, and matched release settlement.
- Preserved explicit fallback lookup for unmatched releases and token-capacity
  failure.
- Cached authored continuation independently from transparent materialized
  tap-source continuation.
- Added two compact 60-slot active bitmaps while keeping the position-indexed
  arrays authoritative and mechanically checking their lifecycle consistency.
- Replaced scan-wide feedback invalidation with transition-plan invalidation
  plus feedback-sequence detection for visible no-plan deadline changes.
- Reconciled release and non-handled-cleanup stack paths against a fresh linked
  image; no false or undocumented adjacency was accepted.

### Measured result

- A direct handled resolution performs one authored search instead of two on
  the first tap and three on later taps.
- A normal handled press performs one authored search across the whole event
  pipeline; its matched release performs zero.
- One active press falls from 180 matrix-slot visits to one refresh plus one
  press visit. A pending tap series costs one refresh plus one series visit;
  idle costs zero.
- An unchanged active scan emits zero feedback dirty marks. A tested
  release-hold deadline crossing emits exactly one, and the next unchanged scan
  returns to zero.
- The final target is 152,232 B text, 25,524 B static BSS, 212,352 B linker
  heap, and 245,336 B ELF BSS including linker-reserved heap. The bitmaps add
  exactly 16 B to the key-runtime state.
- Fresh reviewed stack maxima are 1,912/1,920 B main and 336/768 B split.

### Verification and closure

- Focused lookup, real-profile, release, scenario, split-sync, runtime trace,
  RGB, feature-gate, and 15-fixture stack-tool checks passed.
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
  passed.
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passed.
- `PYTHON=/usr/bin/python3 sh tests/host/run_firmware_memory_budget_checks.sh`
  passed.
- `PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`
  passed from a clean instrumented target build.
- `git diff --check` passed.

Finding 16 is **verified**. Review 16 records the selected architecture,
rejected lookup-index tradeoff, red evidence, enforcement, target measurements,
and closure verdict. No sibling QMK source was edited.

## Cross-cutting decisions and deferred work

- The implementation order follows [`00-overarching-remediation-roadmap.md`](00-overarching-remediation-roadmap.md).
- Finding 09 replaced pending-release sequence ordering without changing Finding
  01's four-record transport batch or re-entry policy.
- Finding 08 establishes the only aggregate literal-report ownership path for
  physical events, held actions, macro playback, and PD persistent shortcuts.
  Finding 07 reuses those leases during nonblocking playback.
- Finding 06 keeps QMK combo candidates compatibility-owned and bounded. Its
  scan reconciliation is sequential with key-runtime scan, not nested runtime
  ownership, and its target path is explicitly tracked by the stack gate.
- Finding 07 makes hardcoded and VIA macro output scan-driven. It deliberately
  supports one active execution with no queue, pins provider IR across
  invalidation, and leaves macro-cache footprint reduction to Finding 14.
- Finding 17 establishes one sampled, wrap-safe clock for every outbound split
  domain. Finding 12 reuses that seam for shared outage/backoff scheduling.
- Finding 12 bounds runtime-sync outages independently of the durable VIA
  convergence protocol. Finding 05 may reuse timing concepts but must keep its
  version, authority, acknowledgement, and persistence state explicit.
- Finding 11 establishes expiry-before-accumulation and one sampled timestamp
  for the shared DRAGSCROLL/PINCH handler. Finding 10 now bounds discrete tap
  backlogs separately without changing that gesture lifecycle.
- The stack checker must model vendor split callbacks under `SlaveThread`, not under the main process stack. Stack-context correctness is part of the gate contract.
- Finding 17 explicitly crossed into `../bastardkb-qmk` for the authorized
  two-file auto-mouse elapsed-at compatibility extension. No other sibling
  source was edited.

## Next program action

Execute Finding 05's physical disconnect/power-cycle/role-swap matrix and
Finding 10's flashed timing/feel check when hardware is available. All planned
software findings are verified; these two physical checks are the remaining
program-level closure work.
