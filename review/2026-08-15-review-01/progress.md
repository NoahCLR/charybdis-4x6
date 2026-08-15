# VIA Split Replay Trust-Boundary Progress

## Why This Review Exists

Finding 03 is not continued in `review/2026-05-11-review-01/` because that open
thread owns multi-profile tooling. It is not appended to
`review/2026-05-08-review-01/` because that thread owns runtime loop performance
and stack safety. This folder tracks the separate VIA RPC trust boundary.

## 2026-08-15

### Completed

- Reproduced the narrowing bug in the existing decoder: casting `4 + size` to
  `uint8_t` lets encoded sizes 252 through 255 wrap below a valid transport
  length.
- Added a pure typed decoder ahead of the slave application switch.
- Changed set-buffer validation to widened subtraction-based checks for header,
  available payload, the 28-byte RPC maximum, and destination capacity.
- Added a compatibility helper for the dynamic-keymap byte capacity with a
  compile-time 16-bit contract assertion.
- Defined padded-report handling and zero-length write behavior explicitly.
- Added coordinate validation for set-keycode and optional set-encoder replay.
- Added exhaustive size/length coverage, guarded packet canaries, destination
  edge cases, reset padding, oversized transport rejection, and an
  encoder-enabled build.
- Added required ASan/UBSan execution to the concrete host runner and a narrow
  source guard against the original narrowing expression.

### Verification

- `sh tests/host/run_qmk_via_split_sync_tests.sh` — passed normal,
  ASan/UBSan, and encoder-enabled variants.
- `sh tests/host/run_qmk_contract_checks.sh` — passed.
- `sh tests/host/run_via_macro_defaults_tests.sh` — passed.
- `sh tests/host/run_action_lifecycle_tests.sh` — passed.
- `sh tests/host/run_feature_gate_compile_tests.sh` — passed.
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
  — passed.
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` — passed.
- `sh tests/host/run_firmware_stack_budget_checks.sh` — passed after a fresh
  target build. The reviewed VIA slave callback path is 88 B against a 768 B
  split-thread budget; the worst reviewed main path remains 1,808 B against a
  1,920 B budget.

### Closure Verdict

Finding 03 is **resolved**. The userspace decoder now enforces the intended
trust boundary, tests mechanically cover the former wraparound and all encoded
sizes, compatibility docs match the implemented padding/zero-length policy,
and every closure gate passed on the same tree.

This review is closed and becomes immutable history. Finding 04 or any later
architecture work must use the next sortable review folder.
