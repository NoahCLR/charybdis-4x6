# Finding 07: Replace Blocking Macro Playback with a Scan-Driven Engine

## Plan metadata

- **Severity:** Should-fix (P1 responsiveness, input-loss, and watchdog risk)
- **Status:** Verified on 2026-08-15
- **Affected surfaces:** Hardcoded macros, VIA macros, macro IR execution, matrix scan ordering, synthetic-key ownership, diagnostics/watchdog
- **Primary files:** [macro_payload_run.c](../users/noah/lib/macro/macro_payload_run.c), [macro_payload.h](../users/noah/lib/macro/macro_payload.h), [macro_dispatch.c](../users/noah/lib/macro/macro_dispatch.c), [via_macro_provider.c](../users/noah/lib/macro/via_macro_provider.c), [runtime_init.c](../users/noah/runtime_init.c)
- **Prerequisites:** Complete or lock the API design in [Finding 08](08-synthetic-key-ownership.md); retain the byte-validation guarantees from [Finding 04](04-via-macro-byte-validation.md)
- **Recommended phase:** Phase 2, after synthetic ownership; combo-origin work may proceed independently

## Reconciliation note

Finding 07 is now implemented and verified. The problem statement, baseline
evidence, and proposed plan below preserve the pre-implementation audit that
guided the work; they no longer describe the current runtime. The landed
engine is summarized under **Implementation outcome**, and the acceptance
checklist records the final closure evidence.

## Implementation outcome

- Hardcoded and VIA macros now start one shared scan-driven engine and return
  immediately to QMK. One execution may be active; later triggers are consumed,
  counted as busy, and not queued.
- The engine performs one bounded state transition per matrix scan. Delays use
  a wrap-safe 32-bit start timestamp plus duration, and text/chord output uses
  lease-backed press, wait, and release phases.
- Provider cache entries are pinned while active. VIA invalidation marks a
  pinned entry stale, preserves its IR bytes through completion or cleanup, and
  reloads the slot on the next use.
- Reset requests cancellation. Successful, cancelled, and runtime-error exits
  release only execution-owned leases; cleanup releases at most one retained
  lease per scan.
- Firmware playback has no call to `wait_ms()`, `send_char()`,
  `send_char_with_delay()`, or `owned_keycode_tap()`. A host source gate enforces
  that boundary.
- The fresh linked target contains a 188-byte engine context and 24-byte
  diagnostics block. Firmware text is 145,020 bytes, an 848-byte increase over
  the Finding 06 checkpoint; total BSS remains 245,592 bytes.
- The fresh reviewed stack gate passes. The new macro paths are 276–440 bytes
  from `main`; the overall main-process worst case remains 1,816/1,920 bytes,
  and the split-thread worst case remains 328/768 bytes.

## Problem statement

Macro execution currently runs the entire IR synchronously inside the key event that triggered it. Explicit delays are implemented with wait_ms loops, and every key/text operation adds another interval. A legal 65,535 ms delay therefore keeps the keyboard task occupied for more than a minute.

The heartbeat calls inside the wait loop can keep one diagnostic alive, but they do not service matrix scanning, pointing reports, RGB work, split transport, VIA commands, or normal key release processing. A press and release that both occur during the blocked interval can be missed entirely.

Playback must become a bounded amount of work performed from normal scan ticks. The resulting engine also needs explicit concurrency, cancellation, ownership, and cache-invalidation rules; merely replacing wait_ms with a timer check inside the existing loop would leave those lifecycle questions unresolved.

## Baseline evidence and failure scenario

- users/noah/lib/macro/macro_payload_run.c:9-22 slices a delay into repeated blocking wait_ms calls.
- users/noah/lib/macro/macro_payload_run.c:24-31 adds TAP_CODE_DELAY after every explicit delay.
- users/noah/lib/macro/macro_payload_run.c:34-62 blocks again after key-down, key-up, and tap-list operations.
- users/noah/lib/macro/macro_payload_run.c:65-72 delegates delayed text to send_char_with_delay, which also owns timing synchronously.
- users/noah/lib/macro/macro_payload_run.c:88-173 walks the complete IR before returning to QMK.
- users/noah/lib/macro/macro_payload.h:24-27 permits up to 512 IR bytes.
- The delay opcode stores uint16_t, so a single parsed delay can be 65,535 ms.
- users/noah/lib/macro/macro_dispatch.c:91-107 and users/noah/lib/macro/via_macro_provider.c:91-100 both invoke playback directly from dispatch.
- users/noah/runtime_init.c:44-46 already provides a shared matrix-scan seam suitable for advancing a runtime engine.

Concrete failure:

1. Trigger a macro containing a long delay.
2. Playback enters macro_payload_wait_ms and remains inside the original key event.
3. Move the pointing device, use VIA, or press and release another key during the delay.
4. Those tasks cannot run until the delay completes; a short physical key cycle may never be observed.
5. If playback exits abnormally, held synthetic keys must still be released, but the current cleanup exists only on the synchronous stack.

## Required invariants

1. No macro opcode calls wait_ms, send_char_with_delay, or another delay-owning helper.
2. Each scan advances at most one bounded atomic macro operation, independent of payload length or delay value.
3. A delay records a wrap-safe deadline and returns immediately to QMK.
4. At most one macro execution is active. A trigger received while busy is consumed but rejected deterministically; it is not silently queued without a documented bound.
5. The active execution sees an immutable IR for its lifetime, even if VIA edits invalidate provider caches.
6. Every synthetic hold acquired by an execution is released exactly once on success, parse/runtime failure, cancellation, reset, or provider invalidation.
7. Cleanup releases only leases acquired by that macro execution.
8. Hardcoded and VIA macros use the same engine; their text interval policy may differ but their lifecycle may not.
9. Completion, failure, and busy rejection are observable to tests and diagnostics.
10. Physical key processing, pointing, RGB, split sync, and diagnostics continue to run between macro operations.

## Scope

This pass should:

- introduce a small macro-execution state machine;
- advance it from noah_matrix_scan_user or an equivalently ordered runtime scan hook;
- define a single-active-execution/busy-reject policy;
- preserve existing IR opcodes and user-visible timing intent;
- make IR lifetime and VIA invalidation safe;
- integrate owner-scoped synthetic holds;
- remove all blocking waits from macro playback;
- cover reset, abort, timer wrap, and concurrent physical activity.

## Non-goals

- Running multiple macros concurrently.
- An unbounded macro queue.
- Changing the authored macro language or QMK VIA encoding.
- Replacing QMK's character-to-keycode translation.
- Solving macro-cache RAM usage in the same pass; that is [Finding 14](14-macro-cache-ram.md).
- Making every synthetic tap elsewhere in firmware nonblocking.

## Proposed execution contract

Use one engine context with these states:

- idle;
- ready to decode/execute the next opcode;
- waiting until a deadline;
- completing;
- aborting/cleaning up.

The public start API should return a typed result such as started, busy, invalid, or empty. Dispatchers continue to report the macro keycode as handled even when start returns busy or invalid, while diagnostics retain the reason.

The initial concurrency policy is deliberately simple:

- one active macro;
- no queued macro;
- a second trigger while busy is consumed and counted as busy;
- the active macro is not restarted or cancelled by the second trigger.

This keeps memory and ownership bounded. A queue should be considered only after real usage demonstrates a need and a bounded descriptor/lifetime design is documented.

## Detailed implementation plan

### Step 1: Split validation/start from execution

1. Keep compilation/decoding responsible for producing a validated IR.
2. Replace direct macro_payload_play_ir calls in providers with a start function that initializes the engine and returns immediately.
3. Preserve temporary compatibility wrappers only for host tooling that truly needs synchronous execution; do not link a blocking wrapper into firmware.
4. Make invalid/empty/busy results explicit so macro_dispatch and via_macro_provider cannot treat all failures identically.

### Step 2: Define immutable IR ownership

Choose one of these mechanically safe models:

1. pin the selected static cache slot for the execution lifetime and defer any reload that would mutate its IR bytes; or
2. copy the selected IR into one dedicated execution buffer owned by the engine.

The recommended first implementation is a pinned cache generation because only one execution can run and existing provider slots are static. Add these rules:

- invalidation may mark a pinned slot stale but may not overwrite its bytes;
- after completion, the next lookup reloads a stale slot;
- a provider reset may request engine cancellation, but cleanup runs before the slot becomes reusable;
- tests simulate a VIA write/invalidation while playback is waiting.

If pinning complicates the later RAM-cache remediation, use one dedicated execution buffer and account for its exact BSS cost in Finding 14.

### Step 3: Implement the scan-driven interpreter

Store in the engine context:

- IR pointer/generation or owned snapshot;
- cursor and end offset;
- text subcursor/count;
- current text-output mode and interval;
- wrap-safe next-due timestamp;
- owner-scoped leases for macro key-down opcodes;
- status and diagnostic result.

On each scan:

1. sample the timer once;
2. if waiting and the deadline has not elapsed, return;
3. execute at most one atomic unit;
4. record the next due time when an interval or explicit delay applies;
5. return immediately.

Atomic units are:

- one text character;
- one key-down;
- one key-up;
- one tap-list chord, with its temporary modifier/basic leases released before returning;
- completion or one bounded cleanup action.

Use unsigned subtraction/deadline helpers proven across timer wrap. A 65,535 ms delay must neither fire immediately nor require a blocking loop.

### Step 4: Preserve timing without delay-owning QMK helpers

1. Replace send_char_with_delay with send_char followed by an engine deadline.
2. Replace macro_payload_wait_interval with the same deadline mechanism.
3. Decide whether an explicit delay includes the legacy post-delay TAP_CODE_DELAY. Preserve the current behavior initially and encode it as a state transition, not a nested wait.
4. Keep caps-lock-specific timing only where existing owned tap semantics require it; model it as a deadline.
5. Document timing granularity: an opcode runs on the first scan at or after its due time.

### Step 5: Make hold cleanup owner-scoped

Use the lease API planned by Finding 08:

1. key-down acquires a lease and stores it in the active execution's hold set;
2. key-up must find and release the matching lease;
3. an orphan key-up is an invalid IR condition and cannot touch another subsystem's ownership;
4. duplicate key-down fails without changing ownership;
5. completion is successful only with an empty hold set;
6. abort releases remaining leases in reverse acquisition order, one bounded step per scan if necessary.

Do not perform a large unbounded cleanup loop merely because normal playback is scan-driven.

### Step 6: Wire runtime lifecycle and ordering

1. Add the engine scan call to noah_matrix_scan_user.
2. Initialize/reset it through the shared runtime initialization path.
3. Define cancellation on eeconfig reset, runtime reinitialization, and any provider reset that invalidates active storage.
4. Keep normal physical event processing ahead of or behind macro advancement consistently; document the chosen order and add an init/scan-order test.
5. Call the runtime heartbeat from the normal scan path, not from synthetic delay loops.
6. If a new firmware source file is added, wire it into users/noah/source_manifest.mk and mirrored host compile surfaces in the same pass.

Recommended ordering is to process physical records normally and advance one macro operation during matrix scan. This prevents a macro scan from monopolizing a physical event hook.

### Step 7: Update both dispatch providers

Hardcoded and VIA providers should:

- load/validate the slot;
- request engine start;
- consume the macro action regardless of start result;
- log invalid and busy results separately;
- never call a synchronous player.

VIA cache invalidation tests must prove an active pinned/snapshotted macro remains deterministic while a later trigger sees the updated payload.

## Test plan

### New focused tests

Refactor macro_payload_test.c around a fake timer and explicit scan calls. Add:

- starting a long-delay macro returns immediately with zero wait_ms calls;
- no scan emits more than one atomic operation;
- a 65,535 ms delay fires only when due;
- due-time comparisons across uint32_t wrap;
- delayed text emits one character per due interval without send_char_with_delay;
- physical press/release callbacks and pointing/split test callbacks execute while a macro waits;
- second trigger while busy is consumed, counted, and does not restart/cancel the active macro;
- hardcoded and VIA providers produce identical lifecycle transitions;
- VIA invalidation during a delay obeys the IR pin/snapshot contract;
- malformed opcode, duplicate down, orphan up, reset, and explicit cancellation release only execution-owned leases;
- cleanup itself is bounded;
- completion leaves the engine idle and ownership/report state quiescent.

### Targeted runners during implementation

~~~sh
sh tests/host/run_macro_payload_tests.sh
sh tests/host/run_macro_dispatch_tests.sh
sh tests/host/run_via_macro_action_lifecycle_tests.sh
sh tests/host/run_action_lifecycle_tests.sh
sh tests/host/run_owned_keycode_tests.sh
sh tests/host/run_keyboard_mod_ownership_tests.sh
sh tests/host/run_held_action_tests.sh
sh tests/host/run_runtime_init_order_tests.sh
~~~

Because scan wiring, source lists, and headers are likely to change:

~~~sh
sh tests/host/run_feature_gate_compile_tests.sh
sh tests/host/run_qmk_contract_checks.sh
~~~

### Closure gates

~~~sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
~~~

## Observability and measurements

Expose under existing diagnostics:

- engine state and active provider/slot;
- operations executed;
- busy-trigger count;
- cancellation and runtime-error count;
- maximum observed lateness relative to a due timestamp;
- active-hold count/high-water mark.

Add a host assertion that every playback path records zero wait_ms and zero send_char_with_delay calls. On target, use existing trace/heartbeat facilities to show scans continue during a representative multi-second macro.

## Risks, tradeoffs, and fallback

- **Changed timing feel:** scan granularity adds sub-scan jitter. Preserve minimum intervals and measure lateness instead of attempting busy waits.
- **IR invalidation race:** a provider can overwrite active bytes if pinning is incomplete. Treat pin state as part of the provider API and test VIA writes mid-playback.
- **Held keys across user input:** explicit key-down/delay/key-up macros intentionally overlap physical activity. Owner-scoped leases must prevent cross-release.
- **Cleanup latency:** one-release-per-scan cleanup is bounded but may leave holds for several scans. With at most 16 holds this is acceptable; if immediate cleanup is required, cap and prove the cleanup loop's worst case.
- **Busy rejection surprises users:** it is deterministic and safe. Document it. A bounded queue is a future product decision, not an implicit implementation detail.
- **Compatibility fallback:** retain a synchronous runner only in host-only code if encoding round trips require it. Firmware dispatch must remain nonblocking.

## Documentation and review-note updates

When implementation lands:

- correct any keymap or docs language that calls playback queued unless the documented busy policy matches reality;
- document macro concurrency, timing granularity, cancellation, and VIA edit behavior in README.md and the relevant docs page;
- update the active runtime review's architecture note if the macro lifecycle/API boundary changes;
- add verification history and measurements to progress.md;
- if the relevant review is closed, create the next sortable review folder rather than editing closure history.

## Acceptance checklist

- [x] Firmware macro playback contains no blocking wait_ms or send_char_with_delay path.
- [x] One scan performs a bounded amount of macro work.
- [x] Long delays and timer wrap are correct.
- [x] Physical, pointing, RGB, split, and diagnostics work can run between macro operations.
- [x] The one-active/busy-reject policy is enforced and documented.
- [x] IR bytes remain immutable for the active execution.
- [x] Success, failure, reset, cancellation, and invalidation leave no owned holds.
- [x] Hardcoded and VIA macros share the same lifecycle engine.
- [x] Source manifest and feature-gate mirrors include any new module. No new firmware source file was added, so the existing manifest entry remains authoritative.
- [x] All targeted runners pass.
- [x] The full host suite passes.
- [x] The firmware compile passes.
- [x] User docs and the active review describe the landed behavior.

## Next action

Proceed to Phase 3 with [Finding 17](17-split-timer-sampling.md), then Finding
12 and Finding 05. Finding 14 remains the dedicated follow-up for reducing the
existing hardcoded/VIA macro cache footprint.
