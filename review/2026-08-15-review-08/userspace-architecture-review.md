# Split Runtime Shared-Clock Architecture Review

Review 07 is closed and immutable. Finding 17 begins the Phase 3 split-runtime
work and changes the timing seam used by every outbound runtime domain.

## Review Scope

- Sample one 32-bit system timestamp for each public split-runtime tick.
- Use unsigned `now - then` arithmetic for heartbeat and later retry decisions.
- Store the same sampled timestamp for every successful domain in a tick.
- Avoid auto-mouse elapsed work when its packet field is disabled or inactive.
- Keep fork-specific auto-mouse timing access inside `lib/compat/`.

## Prior Finding Status

| Prior finding | Status | Required preservation |
| --- | --- | --- |
| Finding 17: repeated split timer sampling | resolved | `runtime_sync.c` samples once; timer-budget, timestamp-coherence, wrap, contract, full-host, firmware, and fresh stack gates enforce the result. |
| Finding 12: split RPC retry backoff | open | The shared `now` seam must be suitable for a later common failure/backoff gate without changing failure policy here. |
| Finding 05: split persistence | open | Preserve current packet eligibility and dirty-state behavior for the later convergence protocol. |
| Finding 01: target stack safety | resolved | Reconcile any linked frame or edge drift after timing signatures change. |

## Baseline

The split, QMK-contract, PD-runtime, RGB-render, and feature-gate runners pass.
One local tick can call `timer_elapsed32()` separately for base, combo, semantic,
and branch heartbeat checks, then call `timer_read32()` after each successful
send. Active auto-mouse progress additionally reaches the fork's 16-bit
`timer_elapsed()` through `auto_mouse_get_time_elapsed()`.

## Intended Local Contract

Every public tick/force entry samples `timer_read32()` once after confirming the
runtime is initialized and this half is master. That `now` value is threaded
through packet eligibility and broadcast helpers. Heartbeat checks use
`(uint32_t)(now - last_send)`, and successful broadcasts store exactly `now`.
Helpers below the public entry points do not read the system timer.

Initialization samples once and uses that value for every domain timestamp. A
test-only elapsed entry may still accept authored auto-mouse elapsed, but it
must sample one 32-bit `now` and otherwise follow the same path.

Auto-mouse elapsed is only relevant when both auto-mouse and the RGB gradient
field are compiled, no local PD mode suppresses the field, and auto mouse is
active. Inactive/disabled paths produce zero without asking the fork for
elapsed time.

## Sibling Boundary

The user explicitly authorized the narrow sibling compatibility extension.
`../bastardkb-qmk/quantum/pointing_device/pointing_device_auto_mouse.c` and
`.h` now expose `auto_mouse_get_time_elapsed_at(uint16_t now)`. The original
no-argument API preserves its contract by sampling and delegating. Charybdis
access remains centralized in `lib/compat/qmk_auto_mouse_contract.h`; no other
sibling source was changed.

## Landed Boundaries and Enforcement

- `users/noah/lib/split/runtime_sync.c` owns the single 32-bit sample and passes
  it through all domain eligibility and broadcast decisions.
- `users/noah/lib/compat/qmk_auto_mouse_contract.h` is the sole userspace seam
  for sampled 16-bit auto-mouse elapsed time.
- `tests/host/split_runtime_sync_test.c` enforces timer-call budgets, coherent
  success timestamps, inactive gates, and 32/16-bit wrap behavior.
- `tests/host/run_qmk_contract_checks.sh` pins the authorized fork declaration,
  wrap-safe subtraction, and backward-compatible wrapper.
- `tools/firmware_stack_budget.json` tracks the matrix-scan clock sample and
  outbound base-broadcast call chain.

## Closure Bar

Closure requires red/green timer budgets, timestamp-coherence and 32-bit-wrap
tests, inactive/disabled auto-mouse coverage, an authorized and centralized
active auto-mouse elapsed-at contract with 16-bit-wrap tests, targeted runners,
the complete host suite, ordinary firmware build, fresh target evidence, and
reconciled user/review/Sol documentation.

## Closure Verdict — 2026-08-15

**Closed: Finding 17 is resolved.** All closure requirements above pass. The
fresh reviewed paths are 280 B for the shared clock and 464 B for the outbound
base broadcast; the overall main-process worst path remains 1,816/1,920 B and
the split-slave worst path remains 328/768 B. Review 08 is now immutable.
