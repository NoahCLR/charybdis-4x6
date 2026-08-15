# Runtime Change Guide

Use this guide when choosing where to edit and what to verify. It assumes the
root README remains user-facing and that architecture details live in this
directory and the existing domain docs.

## Common Changes

| Change | Edit here | Do not edit here | Required checks |
| --- | --- | --- | --- |
| Add or change a key behavior row | selected profile `keymap.c` `key_behaviors[]` | key-runtime reducer/planners unless semantics change | `profile_introspect.py --keymap <name> --write`, `profile_introspect.py --keymap <name> --check`, `run_profile_introspection_checks.sh`, key behavior lookup/validation, keymap validation, real profile validation |
| Change key behavior semantics | `users/noah/lib/key/behavior/` plus tests | authored profile rows only | key behavior lookup/validation, keymap validation, real profile validation |
| Change release, quick-release, fallback, or pending multi-tap behavior | `key/runtime/planning/`, with reducer support if state changes | `release.c` or `deferred_release.c` as decision owners | release matrix, scenario, integration harness, modifier-hold, PD integration, feature gate |
| Change scan-time hold or multi-tap expiry | `key/runtime/planning/scan_planner.*` or `tap_series*` | RGB or split code | key-runtime scenario, release matrix, runtime debug, integration harness |
| Change effect application | `key/runtime/projection/` | planners, unless the decision itself changes | key-runtime integration harness, layer-lock, PD integration, RGB/split tests as relevant |
| Change modifier preservation or replay | `state/modifiers/` | local ad hoc save/restore logic in callers | keyboard mod ownership, action dispatch, delayed action, modifier-hold, PD integration |
| Change layer lock or momentary layer ownership | `state/ownership/layer_ownership.*` and key-runtime ownership bridge | direct QMK layer writes from planners | layer ownership, key-runtime layer-lock, feature gate |
| Add or change a PD mode | `pointing/defs/`, `pointing/modes/`, `pointing/runtime/` | key runtime, unless key-runtime interaction semantics change | PD mode, handlers, PD runtime, pointer layer policy, PD/key-runtime integration, split sync, RGB render |
| Change combo origin or combo footprint behavior | `compat/qmk_combo_origin.*` and `key/runtime/slot/origin_registry.*` | key runtime reducers as a combo-specific workaround | `run_qmk_combo_origin_tests.sh`, key-runtime scenario if behavior changes, RGB render, split sync, real profile validation |
| Change RGB colors or LED groups | selected profile `rgb_config.c` | RGB runtime stage logic | profile introspection write/check for the selected profile, `run_profile_introspection_checks.sh`, RGB validation, RGB render, real profile validation |
| Change shared config shown in generated profile docs | `users/noah/config.h` or selected profile `config.h` | generated Markdown by hand | profile introspection write/check for the selected profile, `run_profile_introspection_checks.sh`, feature gate if config changes source boundaries |
| Change RGB render order or stage behavior | `users/noah/lib/rgb/core/` or `users/noah/lib/rgb/stages/` | authored RGB config | RGB validation, RGB render, split sync if remote state changes |
| Change split runtime mirroring | `users/noah/lib/split/runtime_sync.*` | PD/RGB owners unless the source truth changes | split runtime sync, runtime trace, RGB render as relevant |
| Change hardcoded macros | `keymap.c` `HARDCODED_MACROS` | macro parser unless payload language changes | macro dispatch, macro payload, real profile validation |
| Change VIA macro defaults or seeding | `keymap.c` `VIA_MACROS`, `macro/via_macro_defaults.*`, VIA contracts | hardcoded macro dispatch | VIA macro defaults, VIA macro action lifecycle, qmk_via_split_sync |
| Change VIA split command replay or dynamic-keymap writes | `compat/qmk_via_split_sync.*`, with capacities in `compat/qmk_via_storage_contract.h` | direct QMK storage calls before packet validation | qmk_via_split_sync (normal and sanitizer variants), QMK contracts, feature gate, target stack gate |
| Change hook wiring | `users/noah/hooks.c`, `users/noah/runtime_init.c` | keymap-local hooks without chaining | hook chaining, runtime init order, feature gate |
| Change source/build wiring | `users/noah/source_manifest.mk` and mirrored host support as needed | one-off test runner source lists | feature gate compile, targeted host tests, full host suite, firmware compile |
| Change stack-sensitive runtime phases or target stack configuration | runtime owner plus `tools/firmware_stack_budget.json` when call topology changes | per-function frame guesses or host-only `sizeof` checks | matching runtime tests, `run_firmware_stack_budget_tool_tests.sh`, full host suite, fresh instrumented firmware compile, `run_firmware_stack_budget_checks.sh` |

## Anti-Patterns

- Do not duplicate runtime truth because a local file needs a convenient answer.
  Add a query surface or projection snapshot instead.
- Do not decide release semantics in `release.c`, `deferred_release.c`, RGB, or
  split sync. Release decisions belong to the key-runtime planners.
- Do not write projected state directly from planners. Emit effects and project
  them through the projection layer.
- Do not scatter QMK or fork assumptions through unrelated modules. Put them in
  `compat/` and cover them with contract tests.
- Do not treat a slave VIA RPC as trusted because the normal sender is local
  firmware. Prove command shape, declared payload, coordinates, and destination
  capacity before any storage or rendering side effect.
- Do not include `noah_keymap.h` from runtime modules or `noah_runtime.h` from
  keymap-owned translation units.
- Do not add raw QMK layer-action outputs to authored data when they bypass the
  userspace layer ownership model.
- Do not treat split sync or RGB as owners. They mirror or render existing
  runtime truth.

## Verification Shortcuts

Use the narrowest matching set while working, then run the required broader
checks before handoff when source/build behavior changed.

| Area touched | Targeted checks |
| --- | --- |
| Key runtime | `run_key_runtime_release_matrix_tests.sh`, `run_key_runtime_modifier_hold_integration_tests.sh`, `run_pd_mode_key_runtime_integration_tests.sh`, `run_key_runtime_layer_lock_integration_tests.sh`, `run_key_runtime_scenario_tests.sh`, `run_key_runtime_integration_harness_tests.sh` |
| PD runtime or pointing policy | `run_pd_mode_tests.sh`, `run_pd_mode_handlers_tests.sh`, `run_pd_runtime_tests.sh`, `run_pointer_layer_policy_tests.sh`, `run_split_runtime_sync_tests.sh` |
| Authored profile data | `profile_introspect.py --keymap <name> --write`, `profile_introspect.py --keymap <name> --check`, `run_profile_introspection_checks.sh`, `run_key_behavior_lookup_tests.sh`, `run_key_behavior_validation_tests.sh`, `run_keymap_validation_tests.sh`, `run_all_profile_validation_tests.sh`, `run_real_profile_thumb_layer_lock_integration_tests.sh` |
| RGB | `run_rgb_validation_tests.sh`, `run_rgb_layer_render_tests.sh` |
| Hooks or ownership | `run_hook_chaining_tests.sh`, `run_keyboard_mod_ownership_tests.sh`, `run_owned_keycode_tests.sh`, `run_held_action_tests.sh`, `run_layer_ownership_tests.sh` |
| Macros, VIA, QMK contracts | `run_qmk_contract_checks.sh`, `run_action_lifecycle_tests.sh`, `run_macro_dispatch_tests.sh`, `run_macro_payload_tests.sh`, `run_macro_payload_engine_tests.sh`, `run_macro_slot_provider_tests.sh`, `run_via_macro_defaults_tests.sh`, `run_via_macro_action_lifecycle_tests.sh`, `run_qmk_via_split_sync_tests.sh` |
| Shared runtime or tracing | `run_runtime_init_order_tests.sh`, `run_runtime_debug_tests.sh`, `run_runtime_diag_tests.sh`, `run_runtime_trace_tests.sh` |
| Source manifests or boundaries | `run_feature_gate_compile_tests.sh` |
| Target stack topology or budget | `run_firmware_stack_budget_tool_tests.sh`; after a fresh instrumented target build, `run_firmware_stack_budget_checks.sh` |

For docs-only changes, run `git diff --check` and a stale-path audit. Host tests
and firmware compile can be skipped when no runtime source, authored input,
source manifest, build wiring, or generated firmware input changed.


## Definition Of Done

- The owner of each changed runtime fact is still singular.
- The closest domain doc and this architecture pack still agree with the code.
- Targeted host tests pass for the area touched.
- `run_feature_gate_compile_tests.sh` passes when source boundaries, manifests,
  or include rules changed.
- `run_all_host_tests.sh` and `run_all_profile_compile_tests.sh` pass for
  runtime, authored profile, build, or firmware-behavior changes.
