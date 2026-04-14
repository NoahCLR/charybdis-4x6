# Implementation Progress

This file tracks the review-12 audit and cleanup work recorded in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Refactor quality audit

Completed in this pass:

- started with `git status --short`
- read the newest prior review folder in `review/2026-04-14-review-11/`
- re-audited the runtime debug surface, host fixture shape, compile gate, and
  review/doc integrity
- recorded the follow-up findings in `review-12`

Verification run in this audit pass:

- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`

Results:

- both targeted checks above passed

### Live runtime-debug and host-fixture cleanup

Completed in this pass:

- removed the public `noah_runtime_debug_snapshot_t` contract and
  `noah_runtime_debug_snapshot(...)`
- changed `users/noah/lib/state/runtime/runtime_debug.h` into a live semantic
  query surface with direct key-runtime readers only
- rewrote `users/noah/lib/state/runtime/runtime_debug.c` to answer directly from
  key-runtime slot/index helpers instead of copying a matrix-sized aggregate
- removed snapshot-shaped runtime-debug wrappers from the key-runtime
  integration and scenario harnesses
- migrated runtime-debug and integration tests onto the live query helpers
- split `tests/host/include/host_runtime_fixture.h` into:
  - `tests/host/include/host_runtime_reset_fixture.h`
  - `tests/host/include/host_pd_fixture.h`
- deleted the old umbrella fixture header
- migrated host tests onto the narrower reset vs pd helper headers
- tightened `tests/host/run_feature_gate_compile_tests.sh` to fail on includes
  of the removed `host_runtime_fixture.h`
- updated `docs/KEY_RUNTIME.md`
- corrected the stale next-step note in
  `review/2026-04-14-review-11/progress.md`
- updated the active review-12 notes to reflect the landed cleanup

Verification run in this cleanup pass:

- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_key_runtime_index_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`

Results so far:

- all targeted host checks above passed
- no sibling workspace folders were edited

Next steps:

- run the full host suite
- run the firmware compile after the full host suite is green
- treat hook/stage orchestration as the next architecture target
