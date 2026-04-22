# Progress

## 2026-04-22

### Completed

- Added `users/noah/lib/compat/qmk_combo_origin.c` to normalize combo outputs
  from fake QMK position `(0,0)` into:
  - one representative owner key
  - one full combo locality footprint
- Added `users/noah/lib/key/runtime/origin_registry.c` as the shared bitmap
  registry for single-key and combo-owned locality.
- Replaced key-feedback split sync from one packed owner key to a bitmap.
- Replaced PD trigger ownership from a single half to a side mask.
- Updated RGB key-feedback and PD-mode renderers to honor combo footprints.
- Added combo-member ambiguity validation for authored combo rows.
- Added host coverage for:
  - combo-origin normalization and release caching
  - duplicate-output combo footprint broadening
  - combo-driven PD both-side ownership
  - combo-driven RGB key and key-half rendering
- Updated user-facing docs and authored comments to describe combo footprints,
  both-halves broadening, and trigger-half semantics.

### Verification

Passed:

- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_qmk_combo_origin_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_profile_introspection_checks.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

### Notes

- No sibling workspace folders were edited.
- One production compile seam surfaced late: `combo_ref_from_layer()` needed a
  local declaration in the compat module because upstream exposes it as a weak
  hook without a public header declaration.
- One host-stub seam surfaced late: `pgm_read_word` had to become an inline
  function instead of a macro so mixed stub/real-QMK host tests would not hit a
  macro redefinition failure.

### Next Steps

- No immediate follow-up is required for closure.
- If combo authoring grows more complex later, keep validating that each combo
  row uses unique member keycodes and extend the combo-origin tests alongside
  any new locality-sensitive behavior.
