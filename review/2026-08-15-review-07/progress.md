# Scan-Driven Macro Playback Progress

## Why This Review Exists

Review 06 is closed and immutable. Finding 07 is the next Phase 2 runtime
change and introduces a distinct macro scheduler lifecycle.

## 2026-08-15 — Baseline and Contract Audit

### Baseline Verification

The following passed before implementation:

- `sh tests/host/run_macro_payload_tests.sh`
- `sh tests/host/run_macro_dispatch_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_owned_keycode_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`

### Audit Findings

- The current interpreter executes the complete IR in the triggering key event.
- Explicit delay loops call `wait_ms()` in slices, but text output and owned
  taps also reach blocking QMK helpers.
- Hardcoded and VIA providers both execute cached IR synchronously.
- Cache invalidation clears IR bytes immediately, so an asynchronous engine
  needs a pin/deferred-invalidation contract.
- Finding 08's owner-scoped leases already provide the correct isolation for
  persistent holds and transient text/chord output.

### Chosen Contract

- One active execution, no queue, deterministic busy rejection.
- One bounded state transition per scan.
- Timestamp-plus-duration waits using the 32-bit QMK timer.
- Pinned cache bytes during active playback; VIA invalidation marks them stale.
- Lease-backed text and chord press/release phases with no delay-owning QMK
  playback helper.
- Scan-bounded cancellation/runtime-error cleanup.

## Checkpoint Status

At this baseline checkpoint, Finding 07 remained **open** and the runtime
contract had been established. The later closure section supersedes this
historical checkpoint.

## 2026-08-15 — First Red Test

- Added `run_macro_payload_engine_tests.sh` to the complete host runner.
- Its fake-timer scenario starts a 65,535 ms delay immediately before 32-bit
  timer wrap, asserts zero `wait_ms()` and output calls at start, and advances
  only through explicit scans.
- The runner fails to link because `macro_payload_start_ir()`, engine scan/init,
  and diagnostics do not exist yet. This is the expected pre-implementation
  failure and establishes the scheduler seam.

## 2026-08-15 — Scan Engine and Provider Lifetime

### Completed

- Replaced synchronous IR playback with one shared engine that advances one
  bounded state transition per matrix scan.
- Added typed started/busy/invalid/empty results and finish results for success,
  cancellation, and runtime failure.
- Added wrap-safe start-plus-duration waits, lease-backed text/tap phases,
  persistent hold tracking, reverse cleanup, and engine diagnostics.
- Removed every firmware macro call to `wait_ms()`, `send_char()`,
  `send_char_with_delay()`, and `owned_keycode_tap()`.
- Added pinned/stale cache state. Active hardcoded/VIA IR cannot be overwritten;
  deferred invalidation is applied by the finish callback.
- Migrated hardcoded and VIA dispatch to the shared start API. Reset requests
  cancellation, VIA reseeding invalidates the cache, and matrix scan advances
  the engine between key-runtime and split work.
- Expanded the reviewed-path stack manifest for macro preflight, text/chord
  press, persistent holds, release, and provider completion.

### Focused Verification

Passed:

- `sh tests/host/run_macro_payload_tests.sh`
- `sh tests/host/run_macro_payload_engine_tests.sh`
- `sh tests/host/run_macro_slot_provider_tests.sh`
- `sh tests/host/run_macro_dispatch_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_via_macro_defaults_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_owned_keycode_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh`

The new engine runner passes in normal and ASan/UBSan variants. The provider
runner proves active-byte immutability, and VIA integration proves a mid-delay
edit affects the next run rather than the current run.

## 2026-08-15 — Closure Verification

Passed:

- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`
- `git diff --check`

Fresh linked measurements:

- firmware text: 145,020 B;
- firmware BSS: 245,592 B;
- macro engine context: 188 B;
- macro diagnostics: 24 B;
- new macro reviewed paths: 276–440 B;
- worst reviewed main path: 1,816/1,920 B;
- worst reviewed split path: 328/768 B.

## Closure Verdict

Finding 07 is **resolved and this review is closed**. The synchronous playback
path is gone, all macro output and cleanup is bounded by scan transitions,
active IR lifetime is enforced by cache pinning, hardcoded and VIA providers
share one engine, and the required host, firmware, target-stack, and
documentation gates pass. No sibling source was edited; only generated target
artifacts under `../bastardkb-qmk/.build` were refreshed.

## Next Steps

Do not append later work to this closed review. Begin Phase 3 / Finding 17 in
the next sortable review folder, then continue to Findings 12 and 05.
