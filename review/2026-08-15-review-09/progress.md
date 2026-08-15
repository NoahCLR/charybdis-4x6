# Split Runtime Failure-Backoff Progress

## Why This Review Exists

Review 08 closed Finding 17 and established one sampled split-runtime clock.
Finding 12 now uses that seam to prevent repeated serial timeouts from starving
the firmware main loop during an outage.

## 2026-08-15 — Baseline and Contract

### Baseline Verification

Passed before implementation:

- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

### Baseline Failure Behavior

- A failed domain does not update its last-success payload or timestamp.
- Later domains are still attempted in the same tick.
- The next scan retries immediately because there is no shared transport-health
  state.
- With a 5 ms serial timeout, four failed runtime domains can consume about
  20 ms per scan.

### Chosen Policy

- Stop on the first failure.
- Back off 50, 100, 200, 400, 800, then 1,000 ms, capped thereafter.
- Suppressed ticks build no packets and send no RPCs.
- Use the current base packet as the recovery probe; after success, drain
  current eligible state in fixed base/combo/semantic/branch order.
- Preserve dirty state and per-domain last-success data until that domain
  succeeds.
- Reinitialization as master starts healthy with every domain pending.

## Checkpoint Status

Finding 12 is **closed and resolved**. The completed history and evidence are
recorded below.

## 2026-08-15 — Red Failure Budget

- Added scripted RPC outcomes, per-domain attempt counts, attempt timestamps,
  and packet-builder counters.
- Added the full four-domain forced outage case.
- `sh tests/host/run_split_runtime_sync_tests.sh` failed as expected because
  the old runtime attempted all four domains after the failed base RPC.

## 2026-08-15 — Shared Health Gate

- Added explicit transport-health state with last failure, current delay,
  saturating consecutive-failure count, and active-backoff flag.
- Added configurable 50 ms initial and 1,000 ms maximum delays with doubling
  and compile-time bounds.
- Stop after the first failure. Backoff ticks still sample Finding 17's one
  clock but perform no packet building, auto-mouse lookup, or RPC.
- Force does not bypass backoff.
- A due retry force-sends current base state. Success clears outage state and
  drains current later domains; a later failure starts a new bounded window.
- Failure leaves dirty and last-success state untouched. Master/slave
  reinitialization clears inherited transport health.
- Added failure and recovery trace events. Suppressed scans are silent.

## 2026-08-15 — Verification and Measurements

Passed:

- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`
- `git diff --check`

The first full-host attempt stopped on expected generated profile-document
drift after adding the two introspected config constants. Regeneration and the
required check passed; the complete suite then passed.

Measurements:

- fully due failed pass: four RPC attempts before, one after;
- suppressed pass: zero RPCs and zero packet/auto-mouse builders;
- retry delays: 50, 100, 200, 400, 800, 1,000 ms capped;
- outbound base-broadcast path: 480 B;
- worst reviewed main path: 1,816/1,920 B;
- worst reviewed split-slave path: 328/768 B;
- linked image: 145,204 B text and 245,592 B BSS.

## Closure Verdict

Finding 12 is **resolved**. Outage work is bounded, current state converges
after recovery, telemetry is bounded, and all required host, firmware, target,
generated-document, and hygiene gates pass. Review 09 is closed and immutable.

## Next Steps

Open the next sortable review folder for Finding 05. Keep its durable VIA
version/authority/reconciliation state separate from transient runtime-sync
transport health.
