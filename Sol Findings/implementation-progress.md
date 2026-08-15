# Firmware Findings Implementation Progress

## Program metadata

- **Branch:** `sol`
- **Starting commit:** `5b20ed01` (`sol findings`)
- **Started:** 2026-07-13
- **Current focus:** [Finding 08 — synthetic-key ownership](08-synthetic-key-ownership.md)
- **Overall status:** In progress
- **Latest closed review:** [`review/2026-08-15-review-04`](../review/2026-08-15-review-04/)

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
| 05 | [VIA split persistence](05-via-split-persistence.md) | Must fix | Planned | Not run |
| 06 | [Combo-origin cache lifecycle](06-combo-origin-cache-lifecycle.md) | Should fix | Planned | Not run |
| 07 | [Nonblocking macro playback](07-nonblocking-macro-playback.md) | Should fix | Planned | Not run |
| 08 | [Synthetic-key ownership](08-synthetic-key-ownership.md) | Should fix | Planned | Not run |
| 09 | [Pending-release sequence rollover](09-pending-release-sequence-rollover.md) | Should fix | Verified | Explicit linked FIFO, 65,537-cycle blocked-head stress, structural corruption checks, full host, firmware, and target stack gate pass |
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

## Cross-cutting decisions and deferred work

- The implementation order follows [`00-overarching-remediation-roadmap.md`](00-overarching-remediation-roadmap.md).
- Finding 09 replaced pending-release sequence ordering without changing Finding
  01's four-record transport batch or re-entry policy.
- The stack checker must model vendor split callbacks under `SlaveThread`, not under the main process stack. Stack-context correctness is part of the gate contract.
- No sibling workspace source was edited; only QMK build artifacts were produced under `../bastardkb-qmk/.build`.

## Next program action

Begin Finding 08 by specifying one reference-counted ownership contract for
physical and synthetic basic/modifier registrations. Preserve current macro and
combo behavior until that foundation has direct overlap and orphan-release
coverage.
