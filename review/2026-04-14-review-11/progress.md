# Implementation Progress

This file tracks the follow-up remediation work recorded in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Runtime-sealing gap closure

Completed in this pass:

- started with `git status --short`
- removed the weak host fallback definitions from
  `users/noah/lib/state/runtime/runtime_shared_state.c`
- added `users/noah/lib/state/runtime/runtime_reset.h` as the public reset seam
- narrowed `users/noah/lib/state/runtime/runtime_debug.h` to key-runtime
  observation only
- migrated runtime-debug and integration tests off the old aggregate snapshot
  fields onto semantic/module-owned seams
- removed the public `pd_mode_runtime_shared_state.h` header
- introduced internal-only
  `users/noah/lib/pointing/runtime/pd_mode_runtime_shared_state_internal.h`
- tightened `tests/host/run_feature_gate_compile_tests.sh` to block removed pd
  runtime headers and leaked internal pd storage headers
- updated `docs/KEY_RUNTIME.md` and the active review-11 notes to match the
  landed code

Host harness fallout fixed in the same pass:

- standardized the reset-capable host fixture stubs in
  `tests/host/include/host_runtime_fixture.h`
- updated the lightweight pd-mode runners to use the shared reset-capable stub
  seam
- added explicit local clear/report/layer stubs to standalone host tests that
  previously linked only because the production runtime exported weak fallbacks

Verification run in this pass:

- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_key_runtime_index_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_layer_ownership_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

Results so far:

- all targeted host checks above passed
- no sibling workspace folders were edited

Status after this pass:

- the remediation pass completed its full host-suite and firmware-build
  verification after the targeted checks above
- hook/stage orchestration remained the next architecture target instead of
  reopening runtime sealing
- later debug/test seam cleanup moved into `review/2026-04-14-review-12/`
