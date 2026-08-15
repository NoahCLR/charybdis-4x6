# Pending-Release FIFO Progress

## Why This Review Exists

`review/2026-08-15-review-03/` closed Finding 02 and is immutable. This next
sortable folder records Finding 09's queue-ordering contract.

## 2026-08-15

### Baseline

- Confirmed each pending release stored a 16-bit sequence number.
- Confirmed ordered lookup used ordinary numeric comparison from a zero
  sentinel, making a wrapped zero record invisible and misordering live records
  across wrap.
- Recorded host layouts: 12-byte compact slot, 14-byte public snapshot, and
  21,780-byte core state.

### Completed

- Replaced sequence age with explicit one-byte next links and core head/tail
  indices.
- Added constant-time tail append and bounded FIFO traversal.
- Centralized every removal through one unlink helper.
- Preserved active-owner skipping without changing remaining order.
- Added host-only structure validation, high-water telemetry, and a saturating
  validation-failure diagnostic.
- Added direct coverage for slot reuse, all unlink positions, matching order,
  owner lifetime, full capacity, corruption, and 65,537 operations behind one
  long-lived entry.
- Added the focused runner to the complete host suite and updated developer,
  plan, roadmap, and implementation records.

### Measurements

- Compact slot: 12 B to 12 B.
- Public snapshot: 14 B to 12 B.
- Host core state: 21,780 B to 21,780 B.
- Target `.bss`: 65,204 B, unchanged from the prior closure image.
- Linked bounded-drain frame: 120 B, down from the prior 136 B record.
- Worst reviewed process path: 1,808 B in a 1,920 B budget.

### Focused Verification

- `sh tests/host/run_pending_release_queue_tests.sh` — passed.
- `sh tests/host/run_key_runtime_release_matrix_tests.sh` — passed.
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh` — passed.
- `sh tests/host/run_key_runtime_scenario_tests.sh` — passed.
- `sh tests/host/run_key_runtime_integration_harness_tests.sh` — passed.
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh` — passed both variants.
- `sh tests/host/run_runtime_debug_tests.sh` — passed both token-domain variants.
- `sh tests/host/run_feature_gate_compile_tests.sh` — passed.

### Closure Verification

- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
  — passed, including the new focused runner and 15 stack-tool fixtures.
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` — passed.
- `sh tests/host/run_firmware_stack_budget_checks.sh` — passed after an
  approved fresh clean/rebuild of sibling QMK artifacts. The first sandboxed
  attempt was unable to clean that artifact directory and was not accepted as
  evidence.
- `git diff --check` — passed.

### Closure Verdict

Finding 09 is **resolved**. Pending-release order no longer depends on a
wrapping counter; list invariants, blocker semantics, owner cleanup, memory
layout, full integration, target linking, and reviewed stack reserve are all
mechanically covered.

This review is closed and becomes immutable history.

## Next Steps

- Begin Finding 08's synthetic-key ownership contract in the next sortable
  review folder.
- Preserve the linked FIFO and bounded drain transport while ownership policy
  evolves.
