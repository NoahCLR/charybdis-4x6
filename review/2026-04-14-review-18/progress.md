# Progress

## 2026-04-14 Initial Review Start

- Started a fresh active review thread in `review/2026-04-14-review-18/`.
- This folder was created instead of continuing an older review because the prior review history was intentionally removed and the user explicitly requested a fresh start.
- Review prompt used: `prompts/initial-architecture-review.md`.

## Completed Work

- Audited the current userspace/runtime structure across `users/noah/`, the keymap-owned authoring surface under `keyboards/bastardkb/charybdis/4x6/keymaps/noah/`, and the build/test enforcement scripts under `tests/host/`.
- Wrote the initial architecture review in `userspace-architecture-review.md`.
- Captured a verified baseline for the current tree before opening any new refactor thread work.
- Landed reliability remediation for two confirmed runtime bugs:
  - moved held-repeat ticking out of `matrix_scan_user()` and into the userspace housekeeping hook so repeat dispatch now runs after QMK event processing,
  - extended the shared emit seam with masked synthetic-QMK tap support and moved arrow-mode vertical taps onto that helper.
- Extended host coverage for the new hook surface, masked emit contract, and dropped-backlog repeat policy.
- Applied a follow-up architecture audit using `prompts/follow-up-architecture-audit.md`.
- Confirmed the remediation landed cleanly with no new `must-fix` or remediation-specific `should-fix` findings; the original maintainability findings remain open.

## Findings Snapshot

- `must-fix`: none in the current tree.
- `should-fix`: the positional registry DSLs are now the main maintainability risk; `key_runtime_internal.h` is still too broad for an internal seam.
- `optional cleanup`: the keymap materialization macros and mixed-responsibility pointing bridge are acceptable now but are the next likely growth hotspots.

## Verification

- Passed: `sh tests/host/run_all_host_tests.sh`
- Passed: `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during remediation:
  - `sh tests/host/run_action_dispatch_tests.sh`
  - `sh tests/host/run_pd_mode_handlers_tests.sh`
  - `sh tests/host/run_held_action_tests.sh`
  - `sh tests/host/run_hook_chaining_tests.sh`
  - `sh tests/host/run_runtime_init_order_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_runtime_trace_tests.sh`
- Sibling workspace folders touched: none

## Next Steps

1. Re-audit whether the physical modifier ownership preflight/remapped-release hazard is reachable in any real authored profile, then either prove the invariant or patch it.
2. Resume the original architecture thread by narrowing `key_runtime_internal.h` or making the registry rows more explicit; those `should-fix` items are still open.
3. Keep this review folder as the active thread history for both reliability follow-ups and future architecture cleanup on the same userspace seam.
