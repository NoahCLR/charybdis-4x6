# Finding 12: Bound Split RPC Failure Retries

## Plan metadata

- Severity: high
- Status: planned
- Recommended phase: Phase 3 split resilience, after or alongside [Finding 17](17-split-timer-sampling.md)
- Affected surfaces:
  - users/noah/lib/split/runtime_sync.c
  - users/noah/lib/split/runtime_sync.h
  - users/noah/lib/split/runtime_sync_dirty.c
  - users/noah/lib/split/runtime_sync_dirty.h
  - users/noah/config.h
  - tests/host/split_runtime_sync_test.c
  - tests/host/run_split_runtime_sync_tests.sh
- Prerequisites:
  - Preserve the current four packet domains and their successful-send semantics.
  - Agree on a single per-tick timestamp API with [Finding 17](17-split-timer-sampling.md).
  - Keep this runtime sync transport separate from the durable VIA persistence protocol; it may reuse a generic retry helper only if the contracts stay explicit.

## Problem statement

The master has no outage-level retry gate. A failed transaction remains immediately eligible because its successful-send fields are not updated, and the same tick continues attempting later packet domains. A disconnected or unhealthy split can therefore turn every keyboard scan into several blocking serial timeouts.

This is more than wasted work. With the configured five-millisecond serial timeout, four attempted runtime RPC domains can consume roughly twenty milliseconds in one scan before the next scan immediately retries. That delays matrix handling, pointing reports, RGB work, and watchdog service precisely while the transport is degraded.

## Current evidence and failure scenario

- users/noah/lib/split/runtime_sync.c:177-193 returns false after a failed base RPC without advancing any retry timestamp.
- users/noah/lib/split/runtime_sync.c:196-211, 214-229, and 232-248 repeat that behavior for combo, semantic, and branch packets.
- users/noah/lib/split/runtime_sync.c:185-189, 204-208, 222-226, and 240-244 only update sent-once and last-send state after success.
- users/noah/lib/split/runtime_sync.c:380-405 attempts the remaining domains even when an earlier transaction has failed.
- users/noah/lib/split/runtime_sync.c:163-175 makes a never-successful or dirty domain due again on the next scan.
- users/noah/config.h:25-26 sets SERIAL_USART_TIMEOUT to 5 milliseconds.
- users/noah/lib/split/runtime_sync_dirty.c:12-19 retains domain dirty state, but there is no separate transport-failure state or next-retry deadline.

Failure scenario:

1. Disconnect the secondary half while all four domains are eligible.
2. The base RPC blocks until timeout and fails.
3. Combo, semantic, and branch RPCs are still attempted in the same scan.
4. No shared failure deadline is established.
5. The next matrix scan repeats the entire sequence, amplifying an outage into sustained main-loop starvation.

## Required invariants

1. One failed runtime RPC closes the transport attempt window for the remainder of that tick.
2. While in backoff, no runtime-sync domain invokes transaction_rpc_send.
3. Unsent state remains pending. Backoff must never clear a dirty bit or update a last-success payload.
4. Retry delay is bounded, deterministic, wrap-safe, and configurable within reviewed limits.
5. Recovery is automatic: the first successful probe exits backoff and drains the latest state without replaying obsolete intermediate snapshots.
6. Domain priority is deterministic and cannot permanently starve a later dirty domain.
7. Force means bypass unchanged/heartbeat suppression; it does not mean an unbounded retry loop during an outage.
8. Connected steady-state heartbeat behavior and packet contents remain unchanged.
9. All timing decisions use the one timestamp sampled for the current tick.

## Scope

- Introduce shared transport-health state for the runtime-sync packet family.
- Stop later domain sends after the first failure in a tick.
- Add bounded retry/backoff and a recovery probe policy.
- Retain latest-value/dirty semantics during the outage.
- Add failure, prolonged-outage, wraparound, and recovery tests.
- Add lightweight diagnostic counters or trace events if they can be compiled out with existing diagnostics gates.

## Non-goals

- Do not make runtime RGB feedback durable across power loss.
- Do not add acknowledgements or versioned snapshots to VIA persistence here.
- Do not change packet schemas merely to implement backoff.
- Do not treat silence as proof that the secondary half has applied a packet.
- Do not globally change QMK serial timeouts as the primary remedy.

## Implementation plan

### Phase 1: Capture a baseline and choose the retry contract

1. Extend the split runtime host fixture so transaction_rpc_send can return a scripted result per call and per domain.
2. Record, in tests, the current call count for a four-domain due tick when every send fails.
3. Define named policy constants:
   - initial retry delay
   - maximum retry delay
   - optional backoff multiplier or fixed-step schedule
   - maximum successful sends allowed in one tick, if a send budget is also adopted
4. Prefer a small deterministic schedule over randomized jitter; there is only one master initiating this RPC family.
5. Decide the recovery probe domain. The base packet is the preferred probe because it contains the most general live state and is already first.

### Phase 2: Model transport health explicitly

1. Add a small transport state object rather than overloading per-domain last-success timestamps. It should contain at least:
   - consecutive failure count or retry level
   - next retry deadline or last failure timestamp plus delay
   - whether the transport is currently suppressed
   - optional diagnostics counters for attempts, failures, suppressed ticks, and recoveries
2. Use unsigned subtraction or an established wrap-safe helper for deadline checks.
3. Reset this state in split_runtime_sync_init.
4. Do not infer transport health from sent-once flags; those flags describe payload history, not link status.

### Phase 3: Centralize attempt arbitration

1. Sample now once at entry to split_runtime_sync_elapsed_internal, as specified by Finding 17.
2. Before building expensive packets, check the shared retry gate.
3. If backoff is active and the deadline is not due, return without building packets or calling transaction_rpc_send.
4. When the deadline becomes due, permit one recovery attempt.
5. Have each broadcast function return a richer internal result such as skipped, succeeded, or failed, or centralize the raw send in a helper with that result.
6. On the first failed send:
   - preserve the domain dirty/pending condition
   - advance the shared backoff state
   - stop processing later domains in this tick
7. On a successful recovery probe:
   - clear transport-failure state
   - update only that domain's last-success snapshot
   - continue with a deliberately chosen drain policy
8. Prefer draining at most a bounded number of additional domains in the recovery tick. If all domains may drain immediately, document and test the worst-case healthy recovery cost.

### Phase 4: Preserve latest-state and fairness semantics

1. Rebuild a pending packet from current state when retrying; do not retain a queue of stale snapshots.
2. Clear each dirty bit only after its own successful send.
3. Keep unchanged heartbeat suppression based on last successful payload and last successful time.
4. If a per-tick send budget can postpone later domains, add a rotating cursor or explicit priority rule and prove bounded service for combo, semantic, and branch updates.
5. Verify that an active-to-idle transition remains pending through an outage so stale remote overlays are eventually cleared.
6. Define split role-change behavior: initializing as master starts with all domains pending and no inherited backoff.

### Phase 5: Add diagnostics and enforcement

1. Add trace events for first failure, suppressed retry window, and recovery, using the existing split-sync trace domain.
2. Keep counters saturating or wide enough that diagnostics cannot introduce their own rollover bug.
3. Add compile-time validation that retry delays are nonzero, ordered, and within the timer's unambiguous comparison range.
4. Document any user-tunable retry constants in the relevant runtime documentation.

## Test and verification plan

Extend tests/host/split_runtime_sync_test.c with these cases:

- A base failure prevents combo, semantic, and branch attempts in the same tick.
- Ticks before the retry deadline make zero RPC calls and avoid expensive packet builders.
- Dirty bits survive all suppressed ticks and all failures.
- Retry cadence follows the configured schedule and caps at the maximum delay.
- A successful recovery probe exits backoff.
- After recovery, the newest state is sent; obsolete intermediate values are not replayed.
- A pending active-to-idle clear reaches the remote after recovery.
- A later dirty domain is not starved by repeated base updates.
- Force does not bypass an active failure backoff unless an explicit, separately tested recovery API is chosen.
- Retry deadline comparisons work across UINT32_MAX wrap.
- One tick uses one clock sample, coordinated with Finding 17.

Run targeted checks repeatedly:

1. sh tests/host/run_split_runtime_sync_tests.sh
2. sh tests/host/run_runtime_trace_tests.sh
3. sh tests/host/run_runtime_debug_tests.sh
4. sh tests/host/run_feature_gate_compile_tests.sh

Closure gates:

1. sh tests/host/run_all_host_tests.sh
2. qmk compile -kb bastardkb/charybdis/4x6 -km noah

Do not mark this finding resolved if the firmware compile is blocked or skipped.

## Observability and measurements

- Record maximum RPC attempts in one tick for healthy, first-failure, backoff, and recovery cases.
- Record the retry timestamps produced by the host fixture.
- With a deliberately disconnected secondary half, measure worst-case matrix-scan delay before and after the change.
- Confirm trace volume is bounded during a long outage; one event per suppressed scan would itself be wasteful.
- Capture the final retry constants and the rationale for their latency versus load tradeoff.

## Risks, tradeoffs, and fallbacks

- Longer backoff lowers outage cost but increases remote-display recovery latency.
- Stopping after the first failure can delay later domains; fairness must be explicit.
- A send budget reduces scan spikes but can extend full-state convergence after reconnect.
- A transport API may report failure transiently even while the half is present. The policy must tolerate isolated failures without entering an excessive delay.
- If richer result plumbing causes an overly broad rewrite, first land the minimal shared gate and stop-on-first-failure behavior, with tests, then add adaptive backoff in a second reviewed pass.

## Documentation and review-note updates

- Update the active open runtime architecture review progress.md in the implementation pass. If the relevant review has closed, open the next sortable review folder instead of modifying closed history.
- Document the retry constants and recovery semantics in the relevant split-runtime documentation under docs/ if they are intended as supported configuration.
- Reconcile any existing claim that runtime sync is immediate so it acknowledges bounded recovery during transport outages.
- Record measured before/after attempt counts and scan delay in the review note.

## Acceptance checklist

- [ ] First failed RPC ends the current tick's send attempts.
- [ ] Backoff ticks perform no RPC calls and do not build avoidable packets.
- [ ] Dirty and active-to-idle state survives the outage.
- [ ] Recovery sends current state and clears only successfully transmitted domains.
- [ ] Retry cadence, cap, fairness, role change, and timer wrap are host-tested.
- [ ] Failure/recovery telemetry is bounded and documented.
- [ ] Targeted split, trace, debug, and feature-gate checks pass.
- [ ] The full host suite passes.
- [ ] The target QMK compile passes.
- [ ] Runtime docs and the active review note match the landed behavior.

## Next action

Add scripted per-domain RPC failures and a timer-read counter to the split runtime host fixture. Use those tests to lock the desired stop-on-first-failure and retry cadence before changing runtime_sync.c.
