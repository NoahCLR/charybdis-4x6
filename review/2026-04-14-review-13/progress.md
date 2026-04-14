# Implementation Progress

This file tracks the follow-up audit recorded in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Refactor quality audit

Completed in this pass:

- started with `git status --short`
- read the newest prior review folder in `review/2026-04-14-review-12/`
- re-audited the landed runtime-sealing, runtime-debug, host-fixture, and
  orchestration work in the current tree
- checked the current code, tests, and docs against the design claims recorded
  in the prior review folders
- recorded the follow-up findings in `review/2026-04-14-review-13/`

Verification run in this audit pass:

- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_index_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- all targeted checks above passed
- the full host suite passed
- the firmware compile passed
- no sibling workspace folders were edited

Next steps:

- narrow the public key-runtime API so non-key-runtime modules and higher-level
  host harnesses stop depending on `key_runtime_internal.h`
- decide whether the remaining orchestration debt should be resolved by a shared
  stage contract or by flattening the current order-sensitive control flow
- fix the stale helper names in `docs/KEY_RUNTIME.md` when the key-runtime API
  cleanup lands

### Boundary and orchestration remediation

Completed in this pass:

- added `users/noah/lib/key/runtime/key_runtime_api.h` as the narrow
  cross-module production seam for key-runtime-owned behavior
- renamed the broad private headers to
  `users/noah/lib/key/runtime/key_runtime_internal.h` and
  `users/noah/lib/key/runtime/key_runtime_process_internal.h`
- moved the `runtime_debug.h` implementation into
  `users/noah/lib/key/runtime/key_runtime_debug.c` while keeping
  `users/noah/lib/state/runtime/runtime_debug.h` public and stable
- updated production callers so `users/noah/runtime_init.c` and
  `users/noah/lib/action/action_dispatch.c` no longer depend on key-runtime
  storage/process internals
- reworked the higher-level host harnesses and integration suites so they stay
  on `noah_process_record_user()`, `key_runtime_api.h`, `runtime_reset.h`, and
  `runtime_debug.h`
- tightened `tests/host/run_feature_gate_compile_tests.sh` to reject the
  removed header names and to allow key-runtime internals only in key-runtime
  owner code, `runtime_shared_state_internal.h`, and the low-level white-box
  host suites
- rewrote `users/noah/runtime_init.c`,
  `users/noah/lib/key/runtime/key_runtime_process.c`, and
  `users/noah/lib/rgb/core/rgb_runtime.c` around explicit local stage tables
  without changing their stage order
- added `tests/host/runtime_init_order_test.c` and
  `tests/host/run_runtime_init_order_tests.sh`
- extended `tests/host/rgb_layer_render_test.c` with an explicit full-order
  precedence scenario
- updated `docs/KEY_RUNTIME.md` and the active review note to match the landed
  seams

Verification run in this implementation pass:

- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_hook_chaining_tests.sh`
- `sh tests/host/run_key_runtime_index_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_action_dispatch_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- all targeted checks above passed
- the full host suite passed with the new runtime-init-order runner included
- the firmware compile passed
- no sibling workspace folders were edited

Next steps:

- keep any future cross-module key-runtime entry points on
  `key_runtime_api.h` instead of widening the internal headers again
- update `tests/host/runtime_init_order_test.c` and
  `tests/host/rgb_layer_render_test.c` in the same pass as any intentional
  stage-order changes

### Review-history reconciliation

Completed in this pass:

- annotated `review/2026-04-14-review-13/userspace-architecture-review.md` so
  the landed structure summary and the audit-time findings are no longer framed
  as simultaneous claims about the same tree
- clarified that the findings section is the pre-remediation audit snapshot and
  that the later landed state is recorded by the structure update and the
  follow-up reviews

Result:

- `review-13` now reads as a historical checkpoint plus later landed update,
  instead of an internally contradictory source-of-truth document
