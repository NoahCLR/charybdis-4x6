# Combo-Origin Candidate Lifecycle Progress

## Why This Review Exists

Review 05 is closed and immutable. Finding 06 starts a distinct QMK
compatibility lifecycle change, so work continues in this next sortable folder.

## 2026-08-15 — Baseline and Pinned-Fork Audit

### Baseline

- `sh tests/host/run_qmk_combo_origin_tests.sh` passed before implementation.
- A physically complete combo is cached immediately by the userspace pre-hook.
- Pending entries survive member release, which is necessary for legitimate
  release-triggered QMK output.
- Pending entries are otherwise cleared only after a matching emitted combo
  release; overlap-disabled candidates therefore have no terminal path.
- A full four-entry pending cache silently overwrites slot zero.
- Pending collection and cleanup are keycode-oriented, so two combo indices
  sharing one output can be unioned or cleared together.

### Pinned QMK Findings

- Overlap resolution marks the shorter buffered combo disabled and emits no
  event for it.
- QMK can emit during release processing or later from `combo_task()` after
  elapsed time becomes strictly greater than the effective combo term.
- `matrix_scan_user()` precedes `combo_task()` in a keyboard cycle, so expiry
  needs one crossed-deadline observation of grace.
- `post_process_record_user()` is skipped when combo processing consumes the
  original record and cannot be the only lifecycle seam.

## 2026-08-15 — Lifecycle Implementation

### Reproducer

- Added the overlap-disabled case first. The original implementation failed
  because the suppressed short candidate remained in the feedback bitmap.
- Retained the delayed-after-release control so cleanup cannot collapse the
  legal QMK output window into physical key lifetime.

### Implemented

- Identified every completion with a nonzero generation allocated away from
  all live pending and active entries, including counter wrap.
- Replaced keycode-oriented pending promotion and teardown with exact
  `(combo_index, generation)` entries. Same-output combos retain distinct
  origins and releases use the physical release member to select the matching
  active footprint.
- Added scan/physical reconciliation. Disabled candidates retire immediately;
  inactive candidates receive one final `combo_task()` cycle after their
  profile-wide legal wait is crossed, then expire at the next scan.
- Replaced silent slot-zero overwrite with conservative refusal and a
  diagnostic counter. No refused footprint enters pending state.
- Added snapshot diagnostics for current pending/active counts, pending
  high-water, suppression, expiry, full-cache refusal, and unmatched output.
- Wired reconciliation between VIA default seeding and key-runtime scan.

### Focused Verification

- `sh tests/host/run_qmk_combo_origin_tests.sh` passed normal and
  `EXTRA_SHORT_COMBOS` layouts; its timerless branch compile also passed.
- `sh tests/host/run_runtime_init_order_tests.sh` passed.
- `sh tests/host/run_key_runtime_scenario_tests.sh` passed.
- `sh tests/host/run_key_runtime_integration_harness_tests.sh` passed.
- `sh tests/host/run_hook_chaining_tests.sh` passed.
- `sh tests/host/run_split_runtime_sync_tests.sh` passed both variants.
- `sh tests/host/run_real_profile_validation_tests.sh` passed both variants.
- `sh tests/host/run_feature_gate_compile_tests.sh` passed.
- `sh tests/host/run_qmk_contract_checks.sh` passed normal/compact layout parity
  and pinned hook/scan/combo-task order assertions.

## Checkpoint Status

Finding 06 is **implemented, verification incomplete**. Focused behavior and
compile gates pass; full host, firmware, fresh target stack/resource evidence,
and final documentation reconciliation remain before closure.

## 2026-08-15 — Target Closure

### Verification

- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
  passed with both combo layouts and the new QMK-order assertions.
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passed.
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`
  passed from a clean instrumented target rebuild. The first sandboxed clean
  was denied access to sibling generated QMK artifacts; the approved rerun
  passed and no sibling source changed.
- The stack manifest now names the linked direct path `main -> matrix_scan ->
  combo_origin_pending_output_reconcile ->
  combo_origin_pending_output_entry_clear`: 280 B.
- Worst reviewed main path: 1,816 B of a 1,920 B budget. Worst split path:
  328 B of a 768 B budget.
- `python3 tools/profile_introspect.py --write` and `--check` passed after the
  authored keymap lifecycle comment; generated reports were already current.

### Measurements

- Final instrumented image: 144,172 B text and 245,592 B total BSS.
- Lifecycle symbols: pending cache 96 B, active cache 96 B, diagnostics 12 B,
  generation state 4 B. Physical-key shadow storage remains 720 B.
- Relative to the Finding 08 instrumented image, text increased 648 B and total
  BSS stayed constant; the linker reduced heap as explicit `.bss` grew.

## Closure Verdict

Finding 06 is **resolved and closed**. Code, mechanical QMK contracts, focused
and complete host tests, ordinary firmware, explicit target stack evidence,
measurements, docs, Sol records, and this review agree. This folder is now
immutable history.

## Next Steps

1. Open the next sortable review folder for Finding 07.
2. Define the nonblocking macro scheduler lifecycle before implementation.
3. Reuse Finding 08's owner-scoped leases for all retained macro outputs.
