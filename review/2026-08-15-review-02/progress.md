# VIA Macro Text and IR Preflight Progress

## Why This Review Exists

`review/2026-08-15-review-01/` closed Finding 03 and is immutable. This next
sortable folder records Finding 04's distinct macro decode/playback boundary.

## 2026-08-15

### Baseline

- Added exhaustive rejection expectations before the source fix.
- `sh tests/host/run_macro_payload_tests.sh` failed on the first `0x80` text
  byte because the VIA decoder produced text IR instead of rejecting it.

### Completed

- Added a shared 7-bit, non-NUL macro text predicate.
- Applied it to authored parsing, VIA stream decoding, and IR playback walking.
- Added a shared structural IR iterator and a complete side-effect-free
  preflight pass before execution.
- Preserved eight-bit QMK keycode operands and covered high-value examples.
- Added exhaustive high-text cases, late-malformation cases, invalid-slot
  negative caching, repair/invalidation, and ASan/UBSan execution.
- Updated macro architecture and keymap documentation with the byte-domain and
  cache contracts.

### Focused Verification

- `sh tests/host/run_macro_payload_tests.sh` — passed normal and ASan/UBSan.
- `sh tests/host/run_macro_dispatch_tests.sh` — passed.
- `sh tests/host/run_via_macro_defaults_tests.sh` — passed.
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh` — passed.
- `sh tests/host/run_action_lifecycle_tests.sh` — passed.
- `sh tests/host/run_qmk_contract_checks.sh` — passed.
- `sh tests/host/run_feature_gate_compile_tests.sh` — passed.

### Closure Verification

- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
  — passed on the final tree.
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` — passed.
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh`
  — all 15 fixtures passed.
- `sh tests/host/run_firmware_stack_budget_checks.sh` — passed from a fresh
  linked image. Newly explicit hardcoded and VIA macro preflight paths measure
  760 B and 1,152 B respectively against the 1,920 B reviewed budget. The
  overall worst reviewed main path remains 1,808 B.

### Closure Verdict

Finding 04 is **resolved**. Code, mechanical enforcement, macro documentation,
the Sol finding record, full host suite, target build, and target stack evidence
all match the intended byte-domain and preflight design.

This review is closed and becomes immutable history. Finding 02 and later
architecture work must use the next sortable review folder.
