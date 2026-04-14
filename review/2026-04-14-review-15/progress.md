# Implementation Progress

This file tracks the follow-up audit recorded in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Refactor quality audit

Completed in this pass:

- started with `git status --short`
- read the newest existing review folder in `review/2026-04-14-review-14/`
- re-audited the landed index-boundary and host-runner cleanup in the current
  tree
- checked the compile gate against the actual repo boundary it claims to seal
- compared the new shared host-runner helper against the remaining public-seam
  runner wiring
- recorded the follow-up findings in `review/2026-04-14-review-15/`

Verification run in this audit pass:

- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- all targeted checks above passed
- the full host suite passed
- the firmware compile passed
- no sibling workspace folders were edited

### Compile-gate scope and runner-helper cleanup

Completed in this pass:

- broadened the key-runtime feature-gate production checks so the active keymap
  under `keyboards/bastardkb/charybdis/4x6/keymaps/noah/` is treated as
  repo-owned production code alongside `users/noah`
- updated the removed-header and key-runtime internal-header checks to share
  that production path scope
- replaced the manual host runner mini-manifest with a manifest-derived helper
  layer in `tests/host/noah_source_manifest.sh`
- moved `run_runtime_debug_tests.sh`,
  `run_key_runtime_modifier_hold_integration_tests.sh`, and
  `run_key_runtime_scenario_tests.sh` onto runner-specific derived helpers
- updated this review note to record the landed cleanup while leaving the
  older `review-13` history issue open

Verification run in this pass:

- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- the compile gate now enforces the key-runtime internal-header boundary across
  both userspace-owner and keymap-owned production code
- the reviewed public-seam runners now derive their support graphs from
  `NOAH_COMMON_SOURCES`
- all targeted checks above passed
- the full host suite passed
- the firmware compile passed

Next steps:

- optionally reconcile `review/2026-04-14-review-13/` now that newer review
  folders have corrected its stale findings
