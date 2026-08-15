# Synthetic-Key Ownership Progress

## Why This Review Exists

The prior pending-release review is closed and immutable. Finding 08 begins a
new ownership boundary and therefore uses this next sortable review folder.

## 2026-08-15

### Baseline

- Confirmed ordinary synthetic 8-bit keys call QMK register/unregister directly.
- Confirmed QMK intentionally forces a fresh basic-key press if the usage is
  already present, then an unregister removes it regardless of another owner.
- Confirmed modifiers already have separate physical/managed counts.
- Confirmed macro hold balance treated orphan key-up as valid and playback
  unregistered before discarding the failed ownership lookup.

### Completed Checkpoint: Strict Macro Balance

- Orphan macro key-up now fails balance validation.
- Authored compilation, QMK/VIA decoding, and IR preflight share the rule.
- Playback proves and consumes macro-local ownership before unregistering.
- Replaced unsafe encode/round-trip fixtures with a balanced explicit hold.
- Added decoded-orphan and malformed-IR zero-side-effect coverage.
- Updated `docs/KEYMAP.md` with the strict local-balance contract.

### Verification

- `sh tests/host/run_macro_payload_tests.sh` — passed normal and sanitizer builds.
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh` — passed.
- `sh tests/host/run_macro_dispatch_tests.sh` — passed.
- `sh tests/host/run_via_macro_defaults_tests.sh` — passed.
- `sh tests/host/run_action_lifecycle_tests.sh` — passed.
- `sh tests/host/run_qmk_contract_checks.sh` — passed.
- `sh tests/host/run_feature_gate_compile_tests.sh` — passed.
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
  — passed after the checkpoint commit.
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` — passed after the
  checkpoint commit.
- `git diff --check` — passed.

## Current Status

Finding 08 is **in progress**. Strict macro balance is complete; aggregate
physical/managed report ownership and scoped lease migration remain open.

## Next Steps

1. Add the failing physical `KC_C` plus synthetic tap integration case.
2. Define normalized action components and an inactive/active lease contract.
3. Implement aggregate transition logic and failure diagnostics.
4. Migrate persistent producers, then run the full closure workflow.
