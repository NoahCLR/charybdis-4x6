# Press-Token Identity Rollover Progress

## Why This Review Exists

`review/2026-08-15-review-02/` closed Finding 04 and is immutable. This next
sortable folder records Finding 02's reducer ownership-identity contract.

## 2026-08-15

### Baseline

- Confirmed press construction assigned `state->next_token_id++` directly.
- Confirmed owner queries reject zero, so the wrap allocation could not be
  found by held-action/repeat release planning.
- Enumerated all current owner-bearing reducer stores before implementation.

### Completed

- Added an explicit wrapping, collision-aware, finite-domain allocator.
- Defined active press, retained press, active lease, and active pending
  release as the owner-ID reservation set.
- Moved allocation ahead of press cancellation and all other event mutation.
- Added a saturating snapshot diagnostic and empty-plan handled-key failure
  policy for theoretical exhaustion.
- Added production-domain wrap/collision/cleanup tests and a second runtime
  build with a fully exhausted three-ID domain.
- Added the linked allocation call chain to the target stack manifest.
- Updated `docs/KEY_RUNTIME.md`, Finding 02, the overarching roadmap, and the
  implementation ledger.

### Focused Verification

- `sh tests/host/run_runtime_debug_tests.sh` — passed both domains.
- `sh tests/host/run_key_runtime_release_matrix_tests.sh` — passed.
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh` — passed.
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh` — passed both variants.
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh` — passed.
- `sh tests/host/run_key_runtime_scenario_tests.sh` — passed.
- `sh tests/host/run_key_runtime_integration_harness_tests.sh` — passed.
- `sh tests/host/run_held_action_tests.sh` — passed.
- `sh tests/host/run_action_lifecycle_tests.sh` — passed.
- `sh tests/host/run_feature_gate_compile_tests.sh` — passed.

### Closure Verification

- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
  — passed on the final runtime/test tree.
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` — passed.
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh`
  — all 15 fixtures passed.
- `sh tests/host/run_firmware_stack_budget_checks.sh` — passed from a fresh
  target build. Token allocation measures 1,104 B against the 1,920 B reviewed
  budget; the overall worst reviewed main path remains 1,808 B.

### Closure Verdict

Finding 02 is **resolved**. The allocator never issues zero, every current live
owner store blocks reuse, exhaustion is atomic and diagnosable, and code,
mechanical enforcement, docs, full host, target firmware, and target stack
evidence agree.

This review is closed and becomes immutable history. Finding 09 and later
architecture work must use the next sortable review folder.

## Next Steps

- Implement Finding 09's pending-release sequence rollover independently of
  token identity.
- Preserve the bounded FIFO drain and reviewed target stack reserve.
