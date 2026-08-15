# Finding 17: Sample Split Runtime Time Once Per Tick

## Plan metadata

- Severity: low-to-medium optimization
- Status: verified on 2026-08-15
- Recommended phase: Phase 3 split runtime cleanup, immediately before or with [Finding 12](12-split-rpc-failure-backoff.md)
- Affected surfaces:
  - users/noah/lib/split/runtime_sync.c
  - users/noah/lib/split/runtime_sync.h
  - users/noah/lib/compat/qmk_auto_mouse_contract.h
  - tests/host/split_runtime_sync_test.c
  - tests/host/run_split_runtime_sync_tests.sh
  - ../bastardkb-qmk/quantum/pointing_device/pointing_device_auto_mouse.c and .h (explicitly authorized compatibility extension)
- Prerequisites:
  - Define one unsigned wrap-safe elapsed helper for 32-bit timestamps.
  - Decide whether strict one-sample behavior includes active auto-mouse elapsed. The plan below treats it as required for closure.
  - Obtain user authorization before editing the sibling QMK fork if its compatibility API must be extended. Authorization was granted for this implementation.

## Implementation outcome — 2026-08-15

Finding 17 is implemented and verified:

- initialized master tick/force entry points sample `timer_read32()` once;
- heartbeat/build/broadcast helpers use the passed `now` and do not read time;
- all successful domains in one forced tick store exactly the same timestamp;
- active-heartbeat boundaries pass across `UINT32_MAX` wrap;
- inactive, PD-suppressed, and RGB-gradient-disabled auto-mouse paths do not fetch elapsed time;
- active auto-mouse progress uses the shared tick sample through the centralized
  compatibility wrapper, including correct 16-bit wrap behavior;
- host tests enforce the read budget and expose a host-only clock snapshot.

With explicit user authorization, the sibling QMK fork now exposes
`auto_mouse_get_time_elapsed_at(uint16_t now)`. The original no-argument API
keeps its behavior by sampling once and delegating. The charybdis runtime calls
only the compatibility wrapper and passes the low 16 bits of the shared
32-bit tick sample.

The full host suite, ordinary firmware build, and fresh reviewed-path stack
gate pass. The new enforced target paths use 280 bytes for the shared clock
sample and 464 bytes for the outbound base broadcast. The overall worst main
path remains 1,816/1,920 bytes; the split-slave worst path remains 328/768
bytes. Linked firmware text is 145,060 bytes and BSS is 245,592 bytes, a
40-byte text increase from the Finding 07 checkpoint and no linked BSS change.

## Problem statement

One split runtime tick repeatedly asks the platform timer for elapsed time per packet domain and again when successful sends store their timestamp. On ChibiOS, timer_read32 takes the system lock, so redundant reads are not free.

The domains are making decisions for one logical tick but can observe slightly different timestamps. Besides wasted locking, this complicates deterministic heartbeat, retry, and test behavior. Auto-mouse progress adds another timer sample through the fork contract even when the value is not needed by the compiled base packet.

## Pre-change evidence

- users/noah/lib/split/runtime_sync.c:163-166 calls timer_elapsed32 inside the shared heartbeat helper.
- users/noah/lib/split/runtime_sync.c:177-248 invokes that heartbeat helper separately for base, combo, semantic, and branch paths.
- users/noah/lib/split/runtime_sync.c:185-189, 204-208, 222-226, and 240-244 calls timer_read32 again after each successful send.
- users/noah/lib/split/runtime_sync.c:169-175 can perform a heartbeat check before packet construction, followed by another check in the broadcast function.
- users/noah/lib/split/runtime_sync.c:343-363 samples timer_read32 during initialization.
- users/noah/lib/split/runtime_sync.c:370-377 and 412-419 fetch auto-mouse elapsed before entering the common tick path.
- users/noah/lib/compat/qmk_auto_mouse_contract.h:30-32 delegates to auto_mouse_get_time_elapsed.
- ../bastardkb-qmk/quantum/pointing_device/pointing_device_auto_mouse.c:110-112 implements that API with timer_elapsed, which takes another timer sample.
- ../bastardkb-qmk/platforms/chibios/timer.c:101-103 shows timer_read32 acquiring the ChibiOS system lock.

Idle scenario:

1. The split tick checks unchanged base, combo, semantic, and branch heartbeats.
2. Each eligible path calls timer_elapsed32, which reads and locks the timer.
3. The prebuild and broadcast checks can duplicate those reads.
4. Auto-mouse elapsed is fetched under POINTING_DEVICE_AUTO_MOUSE_ENABLE even when the gradient field is compiled out or inactive.
5. All values represent effectively the same scan, but they are sampled separately.

## Required invariants

1. Each split runtime tick samples the system time once and passes that value through all domain decisions.
2. Heartbeat and retry elapsed calculations use unsigned subtraction: elapsed = now - then.
3. All successful domains in one tick record exactly the same now timestamp.
4. UINT32_MAX wrap preserves due/not-due behavior.
5. Packet eligibility, dirty clearing, active/idle heartbeat intervals, and force behavior remain unchanged except where Finding 12 deliberately changes failure policy.
6. Auto-mouse elapsed is not read when its gradient field is disabled or inactive.
7. For strict closure, active auto-mouse progress is derived from the same tick sample without a second platform timer read.
8. Fork-specific timer access remains centralized in users/noah/lib/compat.

## Scope

- Thread a sampled now value through heartbeat, should-build, broadcast, and retry helpers.
- Replace timer_elapsed32 calls in split runtime with wrap-safe arithmetic.
- Store the sampled now on successful sends.
- Remove unnecessary auto-mouse elapsed calls.
- If authorized, extend the QMK fork contract so active auto-mouse elapsed can be computed from a caller-provided timestamp.
- Add timer-read budgets and wrap tests.

## Non-goals

- Do not globally replace QMK timer helpers.
- Do not alter heartbeat interval values in this finding.
- Do not cache time across separate keyboard scans.
- Do not expose QMK auto-mouse internals directly outside the compat layer.
- Do not edit ../bastardkb-qmk without explicit scope approval.

## Implementation plan

### Phase 1: Instrument timer access

1. Extend the split host fixture with fake timer_read32 and timer_elapsed32 counters.
2. Count calls for initialization, idle tick, dirty tick, active heartbeat tick, forced sync, failed send, and successful four-domain send.
3. Give each fake read a distinct increasing value so tests reveal mixed timestamps.
4. Lock a failing baseline test that expects the improved one-read budget.

### Phase 2: Pass one now through runtime sync

1. Change split_runtime_sync_elapsed_internal to accept or sample a uint32_t now once.
2. Add a pure helper equivalent to (uint32_t)(now - last_send).
3. Pass now to:
   - split_runtime_sync_heartbeat_due
   - split_runtime_sync_should_build_packet
   - each broadcast helper
   - the shared failure/backoff gate from Finding 12
4. Remove timer_elapsed32 from those helpers.
5. On every successful send, assign last_send = now rather than calling timer_read32.
6. During initialization, sample once and initialize every domain timestamp from that value.
7. Keep public test seams narrow; a clock-injected internal function is preferable to global fake behavior.

### Phase 3: Avoid unnecessary auto-mouse sampling

1. Match the raw-elapsed fetch gate to the actual base-packet field gate: POINTING_DEVICE_AUTO_MOUSE_ENABLE and RGB_AUTOMOUSE_GRADIENT_ENABLE.
2. If the auto-mouse state is inactive and the quantized progress must be zero, skip elapsed lookup.
3. Confirm active-to-inactive progress clearing still makes the base packet dirty and reaches the secondary half.
4. Verify feature builds with auto mouse on/gradient off and auto mouse off/gradient on where legal.

### Phase 4: Achieve strict one-sample active progress

The local compatibility API currently exposes only elapsed-now, which samples the timer internally. It does not expose the active start timestamp, so strict one-sample active behavior cannot be implemented locally without a new fork contract or fragile duplicated state.

1. Request explicit user authorization for a narrowly scoped sibling change. Completed.
2. In the QMK fork, add an API such as auto_mouse_get_time_elapsed_at(uint16_t now) that subtracts the internal active timestamp from the caller's sampled low 16 bits.
3. Keep the existing no-argument API for upstream callers and implement it by sampling then delegating, if appropriate.
4. Add a compat wrapper in qmk_auto_mouse_contract.h; runtime_sync.c calls only that wrapper.
5. Add a QMK contract check that verifies the expected symbol/signature.
6. If sibling modification is not authorized, stop with the local one-read heartbeat improvement documented as partial resolution. Do not claim strict one-sample closure.

### Phase 5: Enforce wrap and sample coherence

1. Test last_send near UINT32_MAX with now after wrap for just-before and exactly-at heartbeat thresholds.
2. Test the 16-bit auto-mouse timestamp wrap independently if the elapsed-at API is added.
3. Assert all four successful last-send values equal the one fake now.
4. Assert broadcast helpers do not call timer functions.
5. Coordinate retry-deadline wrap tests with Finding 12 rather than creating a second timing convention.

## Test and verification plan

Extend tests/host/split_runtime_sync_test.c with:

- One timer sample for idle, dirty, forced, failure, recovery, and four-success ticks.
- No timer_elapsed32 calls inside split runtime.
- Same last-send timestamp for all successful domains in one tick.
- Active/idle heartbeat boundary and UINT32 wrap cases.
- Semantic/branch prebuild checks do not resample.
- Auto-mouse disabled and gradient-disabled builds do not fetch elapsed.
- Inactive auto mouse does not fetch elapsed and reports zero progress.
- Active auto-mouse progress uses the shared sample after the fork contract is available.

Run targeted checks repeatedly:

1. sh tests/host/run_split_runtime_sync_tests.sh
2. sh tests/host/run_qmk_contract_checks.sh
3. sh tests/host/run_pd_runtime_tests.sh
4. sh tests/host/run_rgb_layer_render_tests.sh
5. sh tests/host/run_feature_gate_compile_tests.sh

Closure gates:

1. sh tests/host/run_all_host_tests.sh
2. qmk compile -kb bastardkb/charybdis/4x6 -km noah

If the fork contract changes, run the same target compile against that exact sibling QMK tree and report that the work crossed the repo boundary.

## Observability and measurements

- Timer-read count per split tick across all host scenarios.
- ChibiOS lock acquisitions attributable to split runtime before and after, measured by call count or temporary instrumentation.
- Timestamp coherence across sent domains.
- Firmware text-size change from passing now through helpers.
- Active auto-mouse progress parity against the old elapsed API across normal and 16-bit-wrap cases.

## Risks, tradeoffs, and fallbacks

- Passing a 32-bit now through many helpers changes signatures but should remain a local mechanical seam.
- Unsigned elapsed is correct for wrap as long as configured intervals stay well below half the timer range; enforce reasonable bounds.
- Using now's low 16 bits for an upstream 16-bit active timer requires exact wrap semantics and tests.
- Duplicating auto-mouse timer state locally would drift when QMK resets it on pointer activity; do not use that fallback.
- If sibling scope is unavailable, land the local heartbeat consolidation as partial remediation and retain this finding as open for the active auto-mouse second read.

## Documentation and review-note updates

- Document the one-sample timing convention and wrap-safe arithmetic in the split runtime architecture notes.
- Update the active open review progress.md during implementation, or open the next sortable review folder if the relevant review is closed.
- If the QMK fork changes, document the compat contract and explicitly report the sibling edit.
- Record before/after timer-call counts and exact verification commands.

## Acceptance checklist

- [x] One now value drives all heartbeat, build, and send decisions in a tick; Finding 12 will reuse it for retry decisions.
- [x] Broadcast success stores the shared timestamp without resampling.
- [x] UINT32 and auto-mouse UINT16 wrap cases pass.
- [x] Disabled or inactive auto-mouse paths perform no elapsed read.
- [x] Active auto-mouse progress uses the shared tick sample through a centralized compat contract.
- [x] Timer-call budgets are enforced in host tests.
- [x] Targeted split, contract, pointing, RGB, and feature-gate checks pass.
- [x] The full host suite passes.
- [x] The target QMK compile passes.
- [x] The sibling QMK change was explicitly authorized and is reported in the implementation record.
- [x] Architecture notes describe the shared-clock contract.

## Next action

Proceed to Finding 12 and reuse the sampled `now` for a shared, wrap-safe split
RPC failure/backoff gate. Do not introduce a second timing convention.
