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

### Follow-up Remediation

- Fixed combo-origin fallback semantics in
  `users/noah/lib/compat/qmk_combo_origin.c` so an unresolved combo press no
  longer leaks fake QMK key position `(0,0)` into userspace locality.
- Unresolved combo presses now reuse the latest observed physical key as the
  representative owner and install an explicit broad fallback footprint that
  resolves to both halves.
- Added cached release coverage for that unresolved fallback so combo release
  follows the same representative owner instead of drifting back to `(0,0)`.
- Fixed `noah_qmk_combo_origin_reset()` to clear the full combo cache state
  instead of leaving stale `active`, `keycode`, or owner fields behind.
- Fixed the later `9e8faca44c481af53b76dc9e87293523a7eebb01` key-runtime
  regression by shrinking the two hot replay payloads that had started carrying
  full `keypos_t` values through stack-backed transition plans.
- Added `users/noah/lib/key/runtime/keypos_codec.h` so dispatch and delayed
  replay effects now store a packed matrix index instead of a full `keypos_t`
  while keeping the same runtime semantics at projection time.
- Added a compile-time guard in `users/noah/lib/key/runtime/effects/effect.h`
  so `key_runtime_effect_t` cannot silently grow past the size that keeps the
  transition plans small enough for the firmware stack budget.

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
- `sh tests/host/run_qmk_combo_origin_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_delayed_action_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_action_dispatch_tests.sh`

### Notes

- No sibling workspace folders were edited.
- One production compile seam surfaced late: `combo_ref_from_layer()` needed a
  local declaration in the compat module because upstream exposes it as a weak
  hook without a public header declaration.
- One host-stub seam surfaced late: `pgm_read_word` had to become an inline
  function instead of a macro so mixed stub/real-QMK host tests would not hit a
  macro redefinition failure.

### Next Steps

- Flash the packed-key runtime build onto hardware and retest the original
  right-thumb slash and rapid punctuation repros to confirm the watchdog reset
  was the stack growth from `9e8faca...`.
- If hardware instability persists after this runtime shrink, instrument the
  handled-key release/deferred-dispatch path on-device next; combo-origin is no
  longer the leading suspect for that reboot.
- Overlapping active combos that share the same output keycode still deserve a
  dedicated release-path audit if that authored pattern becomes important.
