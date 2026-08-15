# Source Map

This map groups the userspace runtime packages and repo-local profile tooling
by responsibility. It is meant to help a maintainer or agent route changes
without turning this document into a per-function encyclopedia.

## How To Use This Map

This is a lookup index, not a narrative guide. Use the first table to find
top-level runtime and tooling entry points, the source-to-doc matrix to route a
change by subsystem, and the file coverage sections when you need to check
whether a package inventory still matches the tree.

For behavior flow, use [runtime-flow.md](./runtime-flow.md). For "where should I
edit this?" decisions, use [change-guide.md](./change-guide.md).

## Top-Level Runtime Surfaces

These files are outside `users/noah/lib/`, but they are part of the source trace
for this pack because they define how QMK reaches the runtime.

| Surface | Responsibility | Authority | Tests or checks |
| --- | --- | --- | --- |
| `users/noah/source_manifest.mk` | Firmware source list for userspace runtime files | Build wiring | `run_feature_gate_compile_tests.sh`, full host suite, firmware compile |
| `users/noah/runtime_init.c` | Userspace init, scan, and housekeeping orchestration | Integration order | `run_runtime_init_order_tests.sh`, runtime debug/diag/trace tests |
| `users/noah/hooks.c` | Weak QMK hook defaults that call `noah_*` helpers | Hook integration | `run_hook_chaining_tests.sh`, feature gate compile |
| `users/noah/noah_runtime.h` | Public userspace entry surface for keymap hook chaining | Boundary contract | hook chaining and compile gates |
| `users/noah/config.h` | Shared userspace config defaults and feature knobs | Configuration input | profile introspection and real-profile validation when relevant |

## Tooling And Generated Docs

These files are not runtime packages, but they are part of the maintained
source trace because they rewrite or verify human-facing firmware docs.

| Surface | Responsibility | Writes | Primary checks |
| --- | --- | --- | --- |
| `tools/profile_introspect.py` | Parse selected authored profile inputs and render generated visual profile reports | default `docs/KEYMAP-OVERVIEW.md` plus optional `docs/profiles/<name>/KEYMAP-OVERVIEW.md` and matching SVG assets | `python3 tools/profile_introspect.py --check`, `python3 tools/profile_introspect.py --keymap <name> --check`, `run_profile_introspection_checks.sh`, full host suite |
| `tools/via_to_qmk_layout.py` | Convert VIA export JSON back into selected source-owned keymap blocks | optionally selected profile `keymap.c` `VIA_MACROS(MACRO)` and `keymaps[][]` | script preview/write review, real-profile validation, full host suite when source changes |
| `tools/charybdis-profile-studio/` | VS Code webview editor for selected Charybdis 4x6 profile surfaces, new-profile templates, and generated Studio screenshots | selected profile `config.h`, `keymap.c`, `rgb_config.c`, generated `rules.mk`; screenshot PNGs under `docs/media/profile-studio/` | `npm run check`, `npm run screenshots` for screenshot refresh, selected real-profile validation, profile introspection when noah authored data changes |
| `tools/check_firmware_stack_budget.py` and `tools/firmware_stack_budget.json` | Reconcile target stack symbols/artifacts and enforce reviewed stack contexts, call edges, indirect edges, and reserves | read-only report; no source output | `run_firmware_stack_budget_tool_tests.sh`; fresh instrumented target build plus `run_firmware_stack_budget_checks.sh` |

## Source-To-Doc Matrix

| Source group | Responsibility | Runtime authority | Inputs | Outputs or side effects | Primary tests | Related docs |
| --- | --- | --- | --- | --- | --- | --- |
| `action/` | Classify and dispatch action keycodes; aggregate physical and managed literal report ownership | Action metadata, dispatch policy, and owner-scoped literal-key leases | Runtime effects, physical key observations, direct action keys, macro actions | Aggregate-boundary QMK report transitions, synthetic records, PD keycode press/release, macro playback | `run_action_dispatch_tests.sh`, `run_action_lifecycle_tests.sh`, `run_owned_keycode_tests.sh`, `run_delayed_action_tests.sh` | [change-guide](./change-guide.md), [KEY_RUNTIME](../KEY_RUNTIME.md) |
| `compat/` | Centralize QMK/fork/VIA contracts | Compatibility-only | QMK combo records and state, VIA commands, QMK APIs | Bounded generation-aware combo origins, VIA split replication, sampled auto-mouse elapsed access, contract wrappers | `run_qmk_contract_checks.sh`, `run_qmk_combo_origin_tests.sh`, `run_qmk_via_split_sync_tests.sh`, `run_feature_gate_compile_tests.sh` | [runtime-flow](./runtime-flow.md) |
| `key/behavior/` | Resolve authored key behavior | Authored behavior interpretation | `key_behaviors[]`, resolved keycodes | `handled_key_resolution_t`, materialized runtime contracts, validation errors | `run_key_behavior_lookup_tests.sh`, `run_key_behavior_validation_tests.sh`, `run_keymap_validation_tests.sh`, `run_real_profile_validation_tests.sh` | [INTERACTION_MODEL](../INTERACTION_MODEL.md), [KEY_RUNTIME](../KEY_RUNTIME.md) |
| `key/ownership/` | Applied held-action and repeat registries | Projected applied state | Key-runtime effects, housekeeping tick | Registered held actions, repeating actions | `run_held_action_tests.sh`, `run_key_runtime_modifier_hold_integration_tests.sh` | [KEY_RUNTIME](../KEY_RUNTIME.md) |
| `key/runtime/` top level | QMK-facing key-runtime orchestration | Integration only | QMK key events and scan events | Reducer observations, transition plans, projected effects | key-runtime release/scenario/integration/layer-lock/modifier-hold suites | [KEY_RUNTIME](../KEY_RUNTIME.md), [runtime-flow](./runtime-flow.md) |
| `key/runtime/reducer/` | Canonical key-runtime state | Authoritative | Physical key events, scan events, bridge observations | Press tokens, tap series, leases, persistent intents, shadow projection | key-runtime release/scenario/integration/runtime debug/trace suites | [KEY_RUNTIME](../KEY_RUNTIME.md) |
| `key/runtime/planning/` | Release, scan, tap-series, and effect planning | Planned decisions | Reducer state, authored slot interaction, timer state | Effect plans, release decisions, pending multi-tap decisions | `run_key_runtime_release_matrix_tests.sh`, `run_key_runtime_scenario_tests.sh` | [KEY_RUNTIME](../KEY_RUNTIME.md), [runtime-flow](./runtime-flow.md) |
| `key/runtime/projection/` | Apply planned effects outward | Projected side effects | Effect plans | Action dispatch, held/repeat changes, layer/PD writes, feedback pulses | key-runtime integration, layer-lock, PD integration, RGB/split tests | [KEY_RUNTIME](../KEY_RUNTIME.md) |
| `key/runtime/queue/` | Pending release dispatch queue | Authoritative queue inside key runtime | Deferred dispatch effects, release blockers | Ordered pending release snapshots and drains | release matrix, runtime debug, scenario tests | [KEY_RUNTIME](../KEY_RUNTIME.md) |
| `key/runtime/slot/` | Per-key-position helpers | Helper data, not independent authority | Key positions, combo origins, materialized behavior | Packed key positions, origin bitmaps, slot interaction contracts | combo origin, PD, RGB, split, key-runtime tests | [KEY_RUNTIME](../KEY_RUNTIME.md) |
| `key/runtime/trace/` and top-level `trace.*` | Runtime tracing | Diagnostic only | Runtime events, transition plans, projection snapshots | Trace entries and optional console output | `run_runtime_trace_tests.sh`, key-runtime trace variants | [KEY_RUNTIME](../KEY_RUNTIME.md) |
| `macro/` | Hardcoded macros, VIA defaults, payload parser, pinned provider caches, and scan-driven playback | Macro payload validation and one-active execution lifecycle | `MACRO_n`, `VIA_MACRO_n`, payload strings, VIA EEPROM, matrix scans | Lease-backed macro output, bounded cleanup, VIA default seeding | macro dispatch/payload/engine/provider/defaults/VIA lifecycle tests | [KEYMAP](../KEYMAP.md), [runtime-flow](./runtime-flow.md), [VIA_TO_QMK](../tooling/VIA_TO_QMK.md) |
| `pointing/defs/` | PD mode manifest and generated keycodes | Authored PD identity | PD mode manifest macros | Mode ids, flags, keycodes, lock keycodes | PD mode and profile validation tests | [ADDING_PD_MODE](../ADDING_PD_MODE.md), [POINTER_MODES](../POINTER_MODES.md) |
| `pointing/modes/` | Individual trackball mode handlers | Mode behavior only | Mouse reports and mode lifecycle callbacks | Transformed mouse reports, mode-specific reset/DPI behavior | `run_pd_mode_handlers_tests.sh`, `run_pd_mode_tests.sh` | [POINTER_MODES](../POINTER_MODES.md) |
| `pointing/policy/` | Pointer layer and PD policy rules | Policy helper | Layer state, keycodes, PD traits | Pointer layer activation, mouse-record classification | `run_pointer_layer_policy_tests.sh`, PD/key-runtime integration tests | [POINTER_MODES](../POINTER_MODES.md) |
| `pointing/runtime/` | PD mode state, snapshots, lifecycle, key bridge | Authoritative PD state | PD key events, key-runtime effects, pointer reports, split snapshots | Local/display/remote PD state, active handler routing, DPI sync | PD runtime/mode/bridge integration tests | [ADDING_PD_MODE](../ADDING_PD_MODE.md), [runtime-flow](./runtime-flow.md) |
| `rgb/automouse/` | Auto-mouse RGB fade support | Projected UI | Auto-mouse timing and layer state | Fade frame and progress quantization | RGB layer render tests | [RGB_CONFIG](../RGB_CONFIG.md) |
| `rgb/core/` | RGB orchestration, helpers, validation | Render pipeline owner | Authored RGB config, runtime snapshots | LED frame application, config validation, map invalidation | `run_rgb_validation_tests.sh`, `run_rgb_layer_render_tests.sh` | [RGB_CONFIG](../RGB_CONFIG.md) |
| `rgb/stages/` | Individual RGB overlays | Projected UI | Layer state, combo bitmaps, key feedback, PD snapshots | Stage-specific LED painting | RGB render tests | [RGB_CONFIG](../RGB_CONFIG.md) |
| `split/` | Runtime split sync transport, outbound timing, and outage backoff | Transport only | One sampled tick timestamp plus PD, automouse, preview, combo, and key-feedback snapshots | Bounded master-to-slave runtime attempts, per-domain success state, and current-state recovery | `run_split_runtime_sync_tests.sh`, `run_runtime_trace_tests.sh` | [runtime-flow](./runtime-flow.md) |
| `state/shared/` | Internal runtime storage and test reset | Storage owner | Runtime owners | Shared singleton context and reset | runtime init/debug/trace/diag and feature gate tests | [KEY_RUNTIME](../KEY_RUNTIME.md) |
| `state/diagnostics/` | Runtime debug, diagnostics, trace | Diagnostic owner | Runtime stage scopes and state snapshots | Debug APIs, restart watchdog, boot indicator state, trace snapshots | `run_runtime_debug_tests.sh`, `run_runtime_diag_tests.sh`, `run_runtime_trace_tests.sh` | [runtime-flow](./runtime-flow.md) |
| `state/modifiers/` | Keyboard modifier snapshots and replay policy | Modifier policy helper | QMK mod state, action replay, delayed actions, PD masks | Saved/restored/filtered modifier state | keyboard mod ownership, action, delayed action, modifier-hold, PD integration tests | [KEY_RUNTIME](../KEY_RUNTIME.md) |
| `state/ownership/` | Applied layer and keyboard-mod ownership ledgers | Projected applied state plus lock bridge | Runtime effects, physical modifier observation, layer lock requests | QMK layer state, keyboard mod reports, core shadow updates | `run_layer_ownership_tests.sh`, `run_keyboard_mod_ownership_tests.sh`, layer-lock tests | [KEY_RUNTIME](../KEY_RUNTIME.md) |

## File Coverage By Package

### `action/`

- Boundary files: `action_dispatch.c/h`, `action_lifecycle.c/h`,
  `owned_keycode.c/h`, `synthetic_record.c/h`
- Metadata files: `action_kind.c`, `action_kind_dispatch.c`,
  `action_kind_dispatch_internal.h`, `action_kind_internal.h`,
  `action_kind_registry_list.h`

### `compat/`

- Combo and split adapters: `qmk_combo_origin.c/h`,
  `qmk_via_split_sync.c/h`
- QMK contract wrappers: `qmk_contract.c`, `qmk_mod_contract.c/h`,
  `qmk_auto_mouse_contract.h`, `qmk_pointing_contract.h`,
  `qmk_via_contract.c`, `qmk_via_playback_contract.h`,
  `qmk_via_storage_contract.h`
- Split helpers: `split_half.h`, `split_role.c`

Combo-origin pending entries are bounded compatibility candidates, not runtime
press ownership. Each is keyed by combo index and physical completion
generation, reconciled against the pinned QMK active/disabled layout at the
scan boundary, and either promoted exactly once, suppressed, expired after the
legal buffered-output window, refused conservatively at capacity, or reset.

VIA packets replayed by the slave are untrusted input. `qmk_via_split_sync.c`
must decode and validate a complete typed command before it calls QMK storage
or applies RGB effects. Fixed-size commands may carry trailing raw-HID padding.
For set-buffer, padding does not enlarge the declared payload: the declared
size must fit both the received RPC bytes and the 28-byte transport payload.
The destination must fit the dynamic-keymap capacity exposed by
`qmk_via_storage_contract.h`. A zero-byte write is valid through the exact end
of that region, skips the QMK storage call, and still represents a successfully
applied command.

Auto-mouse elapsed time is also fork-specific. Split runtime passes its one
sampled tick timestamp through `qmk_auto_mouse_contract.h`; the compatibility
wrapper converts it to the fork's 16-bit clock and preserves unsigned wrap
semantics without taking another platform timer sample.

### `key/behavior/`

- Public authored behavior API: `key_behavior.h`, `handled_key.h`,
  `key_behavior_lookup.h`, `keymap_validation.h`
- Lookup and materialization: `handled_key_lookup.c`,
  `handled_key_materialize.c`, `handled_key_defaults.c`,
  `handled_key_transparency.c`, `handled_key_resolution_accessors.c`
- Internal behavior policy: `handled_key_internal.h`,
  `handled_key_policy.h`
- Validation: `key_behavior_lookup.c`, `keymap_validation.c`

### `key/ownership/`

- Held action registry: `held_action.c/h`
- Held repeat registry: `held_repeat.c/h`

### `key/runtime/`

- Hook-facing wrappers: `process.c`, `preflight.c`, `press.c`, `release.c`,
  `scan.c`, `api.c/h`, `process_internal.h`
- Effect transport and diagnostics: `transition.c/h`, `trace.c/h`,
  `debug.c`, `deferred_release.c/h`, `delayed_action.c/h`, `feedback.c/h`,
  `feedback_kind.h`, `types.h`
- Reducer state: `reducer/runtime.c/h`, `reducer/ownership_state.c/h`,
  `reducer/state_query.c/h`
- Planning: `planning/effect.h`, `planning/effect_queue.h`,
  `planning/effect_plan.c/h`, `planning/release_internal.h`,
  `planning/release_planner.c/h`, `planning/scan_planner.c/h`,
  `planning/tap_series.h`, `planning/tap_series_flush.c`
- Projection: `projection/projection.c/h`,
  `projection/feedback_projection.c/h`, `projection/pd_projection.c/h`
- Queue, slot, trace helpers: `queue/pending_release_queue.c/h`,
  `slot/keypos_codec.h`, `slot/origin_registry.c/h`,
  `slot/slot_interaction.h`, `trace/core_trace.c/h`

### `macro/`

- Dispatch and providers: `macro_dispatch.c/h`, `macro_slot_provider.c/h`,
  `via_macro_provider.c/h`, `via_macro_defaults.c/h`
- Payload parser/encoder/runtime: `macro_payload.c/h`,
  `macro_payload_internal.h`, `macro_payload_decode_qmk.c`,
  `macro_payload_encode.c`, `macro_payload_keycodes.c`,
  `macro_payload_parse.c`, `macro_payload_run.c`

### `pointing/`

- Definitions: `defs/pd_mode_flags.h`, `defs/pd_mode_manifest.h`,
  `defs/pd_modes.h`
- Mode handlers: `modes/pd_mode_arrow.c`, `modes/pd_mode_brightness.c`,
  `modes/pd_mode_dragscroll.c`, `modes/pd_mode_handler_common.h`,
  `modes/pd_mode_handlers.h`, `modes/pd_mode_pinch.c`,
  `modes/pd_mode_volume.c`, `modes/pd_mode_zoom.c`
- Policy: `policy/pd_mode_policy.h`, `policy/pointer_layer_policy.c/h`
- Runtime: `runtime/pd_runtime.c`, `runtime/pd_mode_state.c`,
  `runtime/pd_mode_snapshot.c`, `runtime/pd_mode_registry.c`,
  `runtime/pd_mode_lifecycle.c`, `runtime/pd_mode_key_runtime_bridge.c/h`,
  `runtime/pd_mode_buffered_tap_internal.h`,
  `runtime/pd_mode_internal.h`,
  `runtime/pd_mode_keyboard_event_internal.h`,
  `runtime/pd_mode_registry_internal.h`,
  `runtime/pd_mode_runtime_shared_state_internal.h`

### `rgb/`

- Automouse fade: `automouse/rgb_automouse.c/h`,
  `automouse/rgb_automouse_stage.c/h`
- Core rendering and authored config helpers: `core/rgb_runtime.c/h`,
  `core/rgb_config_defaults.c`, `core/rgb_config_helpers.h`,
  `core/rgb_helpers.h`, `core/rgb_validation.c/h`
- Stages: `stages/rgb_layer_stage.c/h`,
  `stages/rgb_combo_feedback_stage.c/h`,
  `stages/rgb_key_feedback_stage.c/h`,
  `stages/rgb_pd_mode_stage.c/h`,
  `stages/rgb_preview_stage.c/h`

### `split/`

- Runtime transport: `runtime_sync.c/h`

### `state/`

- Shared storage: `shared/runtime_context_internal.h`,
  `shared/runtime_reset.h`, `shared/runtime_shared_state.c`,
  `shared/runtime_shared_state_internal.h`
- Diagnostics: `diagnostics/runtime_debug.h`,
  `diagnostics/runtime_diag.c/h`, `diagnostics/runtime_trace.c/h`
- Modifiers: `modifiers/keyboard_mod_policy.c/h`,
  `modifiers/keyboard_mod_state.c/h`
- Ownership ledgers: `ownership/keyboard_mod_ownership.c/h`,
  `ownership/layer_ownership.c/h`

## Authored Profile Inputs

The authored profile boundary is intentionally outside `users/noah/lib/`:

- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c` owns layers,
  combos, hardcoded macro payloads, VIA macro defaults, key behaviors, and
  keymap-local custom keycodes.
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h` owns keymap-facing
  timing, layer ids, pointer tuning, and RGB feature flags.
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c` owns authored
  RGB colors, LED groups, and render-stage configuration.

Changes to these authored inputs must run profile introspection and real-profile
validation as described in [change-guide.md](./change-guide.md).

## Host Test Runner Inventory

Use the runner that matches the behavior touched while iterating, then use
`run_all_host_tests.sh` before handing back source or firmware-behavior changes.

| Area | Runners |
| --- | --- |
| Action lifecycle and dispatch | `run_action_dispatch_tests.sh`, `run_action_lifecycle_tests.sh`, `run_delayed_action_tests.sh`, `run_owned_keycode_tests.sh` |
| Key behavior, authored profile validation, and generated overview | `run_profile_introspection_checks.sh`, `run_key_behavior_lookup_tests.sh`, `run_key_behavior_validation_tests.sh`, `run_keymap_validation_tests.sh`, `run_all_profile_validation_tests.sh`, `run_real_profile_thumb_layer_lock_integration_tests.sh` |
| Key runtime | `run_key_runtime_release_matrix_tests.sh`, `run_key_runtime_modifier_hold_integration_tests.sh`, `run_pd_mode_key_runtime_integration_tests.sh`, `run_key_runtime_layer_lock_integration_tests.sh`, `run_key_runtime_scenario_tests.sh`, `run_key_runtime_integration_harness_tests.sh` |
| Hooks, ownership, and boundaries | `run_hook_chaining_tests.sh`, `run_keyboard_mod_ownership_tests.sh`, `run_held_action_tests.sh`, `run_layer_ownership_tests.sh`, `run_feature_gate_compile_tests.sh` |
| Macro, VIA, and QMK compatibility | `run_macro_dispatch_tests.sh`, `run_macro_payload_tests.sh`, `run_via_macro_defaults_tests.sh`, `run_via_macro_action_lifecycle_tests.sh`, `run_qmk_combo_origin_tests.sh`, `run_qmk_via_split_sync_tests.sh` |
| Pointing and PD mode | `run_pd_mode_tests.sh`, `run_pd_mode_handlers_tests.sh`, `run_pd_runtime_tests.sh`, `run_pointer_layer_policy_tests.sh` |
| RGB and split | `run_rgb_validation_tests.sh`, `run_rgb_layer_render_tests.sh`, `run_split_runtime_sync_tests.sh` |
| Runtime diagnostics and tracing | `run_runtime_init_order_tests.sh`, `run_runtime_debug_tests.sh`, `run_runtime_diag_tests.sh`, `run_runtime_trace_tests.sh` |
| Firmware stack budget tooling | `run_firmware_stack_budget_tool_tests.sh` (host fixtures), `run_firmware_stack_budget_checks.sh` (fresh target artifacts) |
| Full host suite | `run_all_host_tests.sh` |
| All-profile firmware compile | `run_all_profile_compile_tests.sh` |
