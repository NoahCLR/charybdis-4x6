# Progress

## 2026-04-14 Initial Review Start

- Started a fresh active review thread in `review/2026-04-14-review-18/`.
- This folder was created instead of continuing an older review because the prior review history was intentionally removed and the user explicitly requested a fresh start.
- Review prompt used: `prompts/initial-architecture-review.md`.

## Completed Work

- Audited the current userspace/runtime structure across `users/noah/`, the keymap-owned authoring surface under `keyboards/bastardkb/charybdis/4x6/keymaps/noah/`, and the build/test enforcement scripts under `tests/host/`.
- Wrote the initial architecture review in `userspace-architecture-review.md`.
- Captured a verified baseline for the current tree before opening any new refactor thread work.
- Landed reliability remediation for two confirmed runtime bugs:
  - moved held-repeat ticking out of `matrix_scan_user()` and into the userspace housekeeping hook so repeat dispatch now runs after QMK event processing,
  - extended the shared emit seam with masked synthetic-QMK tap support and moved arrow-mode vertical taps onto that helper.
- Landed pd-mode buffered tap replay remediation for mode-owned real modifiers:
  - added a mode-owned buffered-tap masking hook on the private pd-mode hook surface and used it from `PINCH_MODE`,
  - kept delayed tap snapshot timing in `multi_tap_engine.c` but removed the need for pinch-specific runtime branching there,
  - added a generic `keyboard_mod_ownership_managed_only_mask(...)` helper so buffered taps only strip managed-only modifiers and preserve physically held GUI.
- Extended host coverage for the new hook surface, masked emit contract, and dropped-backlog repeat policy.
- Extended host coverage for pinch buffered tap replay, managed-only modifier masking, and the direct pd-mode policy query.
- Applied a follow-up architecture audit using `prompts/follow-up-architecture-audit.md`.
- Confirmed the remediation landed cleanly with no new `must-fix` or remediation-specific `should-fix` findings; the original maintainability findings remain open.
- Restored the real-profile host-test baseline by adding the missing host `keycodes.h` shim used by the direct keymap compilation runners.
- Applied another follow-up architecture audit using `prompts/follow-up-architecture-audit.md` to the buffered-tap implementation itself.
- That audit found no new `must-fix` issues, but it did identify:
  - one `should-fix`: the new buffered-tap policy query is broader than needed because it currently leaks through the public pd-mode header,
  - one `optional cleanup`: the pinch regression coverage proves the masking mechanism but does not yet exercise the real authored `KC_TRNS` path from the shipped profile.
- Landed the follow-up seam narrowing for buffered-tap policy:
  - removed `pd_mode_buffered_tap_masked_real_mods(...)` from the public pd-mode header,
  - introduced the dedicated internal header `users/noah/lib/pointing/runtime/pd_mode_buffered_tap_internal.h`,
  - narrowed production use of that header to pd runtime owner modules plus `multi_tap_engine.c`,
  - added compile-gate enforcement so host tests and production code cannot casually depend on that internal seam.
- Applied a closure-verification assessment using `prompts/closure-verification-review.md`.
- Confirmed that the buffered-tap seam work is genuinely resolved and mechanically enforced, but the thread is still not ready to close because the original `should-fix` findings on the registry DSLs and `key_runtime_internal.h` remain open.
- Confirmed the active review folder, `docs/ADDING_PD_MODE.md`, and the landed pd-mode/runtime code all describe the same ownership model.
- This assessment-only pass did not run new verification commands; it relies on the already-green buffered-tap seam-narrowing baseline recorded below.
- Landed the optional buffered-tap coverage cleanup:
  - `tests/host/pd_mode_key_runtime_integration_test.c` now authors `PINCH_MODE` as `TAP_SENDS(KC_TRNS)` like the shipped profile,
  - the integration harness now provides a local pointer-layer-to-base-layer transparent source path with a lower raw `LT(..., KC_J)` key,
  - the pinch replay assertions now verify the delayed action resolves to `KC_J` through transparent lookup instead of using a synthetic `KC_C` stand-in.
- Landed concurrent keyboard-event masking for mode-owned real modifiers:
  - added `noah_pre_process_record_user(...)` and `noah_post_process_record_user(...)` to the shared hook surface so userspace can track physical modifier presses before `process_record_user()` and restore any temporarily masked mode-owned real mods after the event,
  - added a private pd-mode keyboard-event masking query and used it from `key_runtime_process.c` so concurrent key processing hides only the managed-only subset of active mode-owned real modifiers,
  - kept `PINCH_MODE` as the owner of the managed-only `GUI` policy for both buffered tap replay and concurrent keyboard events,
  - removed the redundant file-static pinch registration latch and moved transient keyboard-event mask state into key-runtime shared state so resets and test fixtures do not depend on translation-unit statics,
  - updated `docs/ADDING_PD_MODE.md` so mode authors know that lifecycle hooks can also own concurrent keyboard-event masking policy.
- Aligned the wider host test matrix with the new hook/runtime ownership:
  - added a weak no-op fallback for the private pd-mode keyboard-event masking query in `key_runtime_process.c` so host runners that compile key runtime without the full pd registry still link cleanly,
  - added `keyboard_mod_state.c` to the shared host key-runtime source bundle,
  - updated the scenario, layer-lock, and real-profile host harnesses with only the ownership/state stubs they still own locally,
  - updated `key_runtime_preflight_test.c` so it now expects physical modifier tracking to happen in `pre_process_record_user()` instead of inside preflight.
- Landed final-outcome cleanup for concurrent keyboard-event masking:
  - added `noah_process_record_user_finalize(...)` to the shared runtime hook surface so cleanup now follows the final QMK event result instead of the helper-local `noah_process_record_user(...)` result,
  - updated the weak `process_record_user()` wrapper and the documented chaining pattern so keymap-local overrides that narrow the shared result to `false` finalize immediately instead of leaking the temporary keyboard-event mask,
  - moved the common key-runtime integration and scenario harnesses onto full QMK-style `pre/process/finalize-or-post` flow instead of calling `noah_process_record_user()` directly,
  - dropped the bespoke pre/post wrappers from the pd-mode integration test and kept only the direct low-level helper calls that intentionally inspect mid-event masking state.
- Closed the follow-up review gaps in that final-outcome cleanup:
  - broadened the shared hook contract comment so any override that chains `noah_process_record_user(...)` and returns a final `false` must finalize first, not just `&&`-style narrowing overrides,
  - extended `tests/host/hook_chaining_test.c` with an explicit pass-through-false chaining shape so a future `if (!noah_process_record_user(...)) return false;` override still has mechanical coverage for the finalize-before-return rule,
  - updated the weak default `noah_post_process_record_user(...)` in `tests/host/key_runtime_integration_harness.c` to finalize the `true` path just like production,
  - added `tests/host/key_runtime_integration_harness_test.c` plus `run_key_runtime_integration_harness_tests.sh` so the harness default `post -> finalize(true)` behavior is now enforced by the host suite instead of relying on inspection.
- Re-ran the follow-up architecture audit on that last cleanup using `prompts/follow-up-architecture-audit.md`.
- The rerun found no new remediation-specific findings. The contract wording, strong-override coverage, default harness behavior, host-suite enforcement, and active review notes now agree on the landed structure.
- Re-ran the closure review using `prompts/closure-verification-review.md`.
- Closure verdict is unchanged: this thread is still not ready to close because the original `should-fix` findings on the registry DSLs and `key_runtime_internal.h` remain open, even though the remediation-specific hook/harness findings are now resolved.

## Findings Snapshot

- `must-fix`: none in the current tree.
- `should-fix`: the positional registry DSLs are still the main maintainability risk; `key_runtime_internal.h` is still too broad for an internal seam.
- `optional cleanup`: the keymap materialization macros and mixed-responsibility pointing bridge are acceptable now but are the next likely growth hotspots.
- `closure verdict`: keep this thread open until the remaining `should-fix` items are either resolved or explicitly downgraded out of the closure bar.

## Verification

- Passed: `sh tests/host/run_all_host_tests.sh`
- Passed: `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during remediation:
  - `sh tests/host/run_action_dispatch_tests.sh`
  - `sh tests/host/run_pd_mode_handlers_tests.sh`
  - `sh tests/host/run_held_action_tests.sh`
  - `sh tests/host/run_hook_chaining_tests.sh`
  - `sh tests/host/run_runtime_init_order_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_runtime_trace_tests.sh`
- Passed during pd-mode buffered tap remediation:
  - `sh tests/host/run_keyboard_mod_ownership_tests.sh`
  - `sh tests/host/run_pd_mode_tests.sh`
  - `sh tests/host/run_pd_runtime_tests.sh`
  - `sh tests/host/run_real_profile_validation_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_key_runtime_transition_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during buffered-tap seam narrowing:
  - `sh tests/host/run_pd_mode_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during transparent-path coverage cleanup:
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during concurrent keyboard-event masking remediation:
  - `sh tests/host/run_pd_mode_tests.sh`
  - `sh tests/host/run_hook_chaining_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
- Passed during host-matrix follow-up for concurrent keyboard-event masking:
  - `sh tests/host/run_runtime_debug_tests.sh`
  - `sh tests/host/run_runtime_trace_tests.sh`
  - `sh tests/host/run_key_runtime_release_matrix_tests.sh`
  - `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_key_runtime_preflight_tests.sh`
  - `sh tests/host/run_key_runtime_scenario_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during final-outcome cleanup for concurrent keyboard-event masking:
  - `sh tests/host/run_hook_chaining_tests.sh`
  - `sh tests/host/run_key_runtime_integration_harness_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_key_runtime_scenario_tests.sh`
  - `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Sibling workspace folders touched: none

## Next Steps

1. Re-audit whether any other pd mode will need mode-owned modifier masking for buffered replay or concurrent keyboard events before adding more special policy to the private pd-mode hook surfaces.
2. Resume the original architecture thread by narrowing `key_runtime_internal.h` or making the registry rows more explicit; those `should-fix` items are still open.
3. Keep this review folder as the active thread history for both reliability follow-ups and future architecture cleanup on the same userspace seam.
