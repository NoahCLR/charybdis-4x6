# Split Runtime Failure-Backoff Architecture Review

Review 08 is closed and immutable. Finding 12 continues Phase 3 by bounding
runtime-sync transport work during a disconnected or unhealthy split.

## Review Scope

- End a tick's outbound attempts after the first failed runtime RPC.
- Suppress all runtime-sync packet building and sends inside a shared backoff
  window.
- Reuse Finding 17's single sampled `now` and wrap-safe elapsed convention.
- Preserve per-domain last-success snapshots and dirty state across failure.
- Probe recovery with current base state, then drain current domain state in
  deterministic priority order.

## Prior Finding Status

| Prior finding | Status | Required preservation |
| --- | --- | --- |
| Finding 12: split RPC failure backoff | resolved | Shared health state, stop-on-first-failure, bounded retry, recovery, tracing, host tests, firmware, and fresh target evidence pass. |
| Finding 17: split timer sampling | resolved | Use the existing single `now`; no helper may resample time. |
| Finding 05: split persistence | open | Preserve latest-state and dirty semantics so durable convergence can build on them later. |
| Finding 01: target stack safety | resolved | Reconcile the outbound split target path after health-state control flow changes. |

## Chosen Contract

- The first failed runtime RPC stops later domain work in that tick.
- Retry delays start at 50 ms and double to a 1,000 ms cap.
- A backoff deadline is represented as `last_failure` plus a delay and tested
  with unsigned `now - last_failure >= delay`, including across wrap.
- Force bypasses payload/heartbeat suppression but never active backoff.
- A due retry force-sends the current base packet as the recovery probe. Probe
  success clears shared failure state, after which the same tick may drain all
  other currently eligible domains. A later failure stops that drain.
- No stale packet queue is retained. Every retry rebuilds current state.
- Dirty flags and last-success snapshots change only after their own successful
  transmission.
- Failure and recovery produce one trace event per attempt transition; silent
  backoff ticks do not fill the trace ring.

## Closure Bar

Closure requires scripted red/green failure tests, retry schedule/cap/wrap
coverage, no-build suppressed ticks, current-state recovery, dirty and
active-to-idle preservation, deterministic later-domain service, bounded
telemetry, targeted split/trace/debug/feature checks, the complete host suite,
ordinary firmware build, fresh target evidence, and reconciled documentation.

## Landed Boundaries and Enforcement

- `users/noah/lib/split/runtime_sync.c` owns transport health and arbitration;
  `runtime_sync_dirty.c` remains the per-domain pending-state owner.
- `users/noah/config.h` exposes reviewed 50 ms initial and 1,000 ms maximum
  delays. Compile-time assertions require nonzero, ordered, unambiguous values.
- `tests/host/split_runtime_sync_test.c` scripts failure results and enforces
  stop, suppression, schedule/cap/wrap, current-state recovery, active-to-idle
  clearing, later-domain drain, and role reset.
- `tests/host/runtime_trace_test.c` proves one failure and one recovery event
  with no trace growth across suppressed scans.
- The existing stack manifest's outbound base-broadcast path covers the changed
  control flow and passes at 480 B.

## Closure Verdict — 2026-08-15

**Closed: Finding 12 is resolved.** Every closure requirement passes. A fully
due outage tick now performs one failed RPC instead of four; suppressed ticks
perform none. The overall reviewed main path remains 1,816/1,920 B and the
split-slave worst path remains 328/768 B. Review 09 is now immutable.
