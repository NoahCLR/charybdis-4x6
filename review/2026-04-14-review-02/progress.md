# Implementation Progress

This file tracks the follow-up quality review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Refactor follow-up audit

Completed in this pass:

- Started with `git status --short`.
- Re-read the newest existing review folder,
  `review/2026-04-14-review-01/`, before auditing the implementation.
- Audited the landed refactor directly in:
  - handled-key contract/materialization and transparency resolution
  - key-runtime index/registry state and consumers
  - pd-mode identity sync and split runtime transport
  - macro provider/cache, VIA default seeding, and live VIA playback
  - host tests that were added or updated during the refactor
- Wrote a new review folder for this critical follow-up audit.

Key findings recorded in this review:

- must-fix: transparent multi-tap wrappers still inherit `tap_action` from the
  lower source while keeping `has_more_taps` / `tap_resolves_on_press` from the
  upper authored row, which can change multi-tap runtime behavior
- should-fix: handled-key phase 2 is still transitional because the legacy
  `*_at_position(...)` helpers remain public and host tests still rebuild
  `handled_key_materialize(...)` from them
- should-fix: the macro provider/cache refactor did not actually subsume VIA
  default seeding, despite the prior review notes claiming it did
- optional cleanup: `handled_key_interaction_policy_t` and
  `interaction.policy` remain as dead duplicates of the new behavior contract

Areas assessed as solid in this pass:

- handled-key file splitting improved navigation
- pd-mode identity sync is cleaner than the old flag transport
- runtime debug now exposes registry state clearly
- QMK-stream decode is cleaner after being split from macro playback

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- full host suite passed
- firmware build passed

Checks intentionally skipped in this pass:

- none

Workspace scope:

- no sibling workspace folders were edited
- all changes in this pass are confined to
  `charybdis-4x6/review/2026-04-14-review-02/`

Next steps:

- fix transparent multi-tap materialization so continuation metadata comes from
  the same effective transparent source as the tap action
- remove or internalize the old handled-key `*_at_position(...)` helpers and
  update host suites to exercise `handled_key_materialize(...)` directly
- either move VIA default seeding behind the provider layer or narrow the prior
  review notes so they match the actual implementation

### Audit follow-up implementation

Completed in this pass:

- fixed handled-key materialization so `tap_action`, `tap_repeat_count`,
  `tap_has_more_taps`, and `tap_resolves_on_press` all come from one effective
  transparent tap source
- removed the public handled-key `*_at_position(...)` compatibility helpers
- cut runtime and host tests over to the explicit
  `handled_key_materialize(...)` seam, using one shared host helper for
  authored-only materialization stubs
- removed the dead `handled_key_interaction_policy_t` /
  `interaction.policy` duplicate surface
- added transparent multi-tap regression coverage for both press-resolve and
  release-resolve continuation behavior
- added a provider-owned macro encode helper and moved VIA default macro seeding
  onto the provider/cache layer
- updated host runners that now need the payload encoder linked with the macro
  provider helper

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_via_macro_defaults_tests.sh`
- `sh tests/host/run_macro_dispatch_tests.sh`
- `sh tests/host/run_macro_payload_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- all targeted host suites passed
- feature-gate compile checks passed
- full host suite passed
- firmware build passed

Checks intentionally skipped in this pass:

- none

Workspace scope:

- no sibling workspace folders were edited
- source changes are confined to `users/noah/`, `tests/host/`, and this review
  folder

Review note updates:

- `userspace-architecture-review.md` was left unchanged because the landed
  implementation matched the audit follow-up intent

Next steps:

- none
