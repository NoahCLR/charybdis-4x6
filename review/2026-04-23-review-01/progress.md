# Progress

## 2026-04-23

### Review Opened

- Opened a new full-codebase architecture review using
  `prompts/initial-architecture-review.md`.
- Did not continue `review/2026-04-22-review-01/` because that review is
  internally coherent and scoped to the prior combo-origin/RGB/runtime
  compaction thread, while this pass is a fresh full-userspace audit.

### Completed

- Checked the worktree with `git status --short`; it was clean before review
  edits.
- Read the latest review folder:
  - `review/2026-04-22-review-01/userspace-architecture-review.md`
  - `review/2026-04-22-review-01/progress.md`
- Audited:
  - root build and entry surfaces
  - authored keymap/RGB/config data
  - key runtime process, reducer, feedback, ownership, and transition layers
  - QMK/VIA/split/combo compatibility surfaces
  - PD-mode runtime, lifecycle, policy, and handlers
  - RGB render stages and split sync
  - macro payload/default/VIA dispatch surfaces
  - validation, feature-gate, and full host-suite runners
  - primary documentation under `README.md` and `docs/`
- Wrote the initial findings in
  `review/2026-04-23-review-01/userspace-architecture-review.md`.
- Regenerated `docs/KEYMAP-OVERVIEW.md` after the full host suite caught stale
  profile-introspection output. The authored RGB data now documents
  `PD_COLOR_MODE_RIGHT_HALF` consistently.
- Implemented all review findings:
  - effect fan-out paths now stream and chunk-drain transition effects instead
    of truncating on one fixed-size plan
  - key behavior, keymap, and hardcoded macro validation now return error
    counts, and the QMK build hard-fails invalid authored profiles through
    real-profile validation
  - PD mode tracks transient same-mode key owners by key position while keeping
    the single active/locked mode invariant
  - removed the no-op deferred-release blocker observer from the core API
- Added or updated host coverage for effect fan-out chunking, validation error
  counts, build validation wiring, runtime init order, and same-mode PD owner
  release order.
- Updated this review's architecture note with a reconciliation/remediation
  status section so the audit-time findings are not left stale.
- Updated `docs/HOOK_OVERRIDES.md` so the `keyboard_post_init_user()` helper
  contract documents runtime initialization and the build-time authored-profile
  gate.

### Verification

Passed:

- `sh tests/host/run_feature_gate_compile_tests.sh`
- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_macro_dispatch_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_macro_payload_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_hook_chaining_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

### Next Steps

- No current remediation items remain open for this review.
- Future key-runtime fan-out sources should add matching chunked-effect stress
  coverage when they are introduced.
- Future authored profiles that add duplicate physical or combo entry points
  for one momentary PD mode should add key-runtime integration coverage for
  that profile shape.

### Closure Verification

- Ran the follow-up and closure prompt checks using:
  - `prompts/follow-up-architecture-audit.md`
  - `prompts/closure-verification-review.md`
- Rechecked every major finding against code, tests, compile gates, docs, and
  this review folder.
- Kept explicit compile-gate coverage focused on active architecture contracts,
  not negative checks for deleted symbols.
- Closure verdict: close thread.
- Remaining open findings: none.
