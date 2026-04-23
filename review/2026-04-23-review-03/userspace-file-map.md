# Userspace File Map

## Scope

- Frozen corpus command:
  `find users/noah -type f \( -name '*.c' -o -name '*.h' \) | sort`
- Scoped corpus: 160 files, 76 `.c`, 84 `.h`, 19,677 LOC.
- Scope includes only `users/noah/**/*.c` and `users/noah/**/*.h`.
- Supporting tests, docs, keymap data, source manifests, and QMK contracts were
  inspected for context but are not mapped file-by-file here.

## Verification Keys

- `feature gate`: `sh tests/host/run_feature_gate_compile_tests.sh`
- `full host`: `sh tests/host/run_all_host_tests.sh`
- `firmware`: `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Targeted references name the host runner area that exercises the contract.

## Root

| File | Purpose | Owner / role | Build / feature gate | Key deps / coverage | Verdict |
| --- | --- | --- | --- | --- | --- |
| `users/noah/config.h` | Shared firmware config, split transaction IDs, RGB geometry, pointing and VIA settings. | Root config, public to QMK build. | Header-only QMK config surface. | QMK config macros, feature gate, firmware. | Makes sense. |
| `users/noah/hooks.c` | Weak QMK hook wrappers that chain into `noah_*` runtime helpers. | Root runtime hook bridge, QMK-facing. | `NOAH_COMMON_SOURCES`. | `noah_runtime.h`; hook chaining tests. | Makes sense. |
| `users/noah/keymap_materialize.h` | Includes authored keymap data and exposes materialized behavior/combo/macro tables. | Root keymap authoring bridge, public to keymap. | Header-only keymap-owned include. | `noah_keymap_ids.h`, authored keymap data; real profile validation. | Makes sense. |
| `users/noah/noah_keymap.h` | Authoring umbrella for keymap-owned files. | Root public keymap API. | Header-only; must not be used by runtime modules. | Keymap `keymap.c` and `rgb_config.c`; boundary check. | Makes sense. |
| `users/noah/noah_keymap_ids.h` | Layer, custom keycode, macro, and PD-mode keycode IDs. | Root shared ID contract. | Header-only. | PD manifest, macro dispatch, keymap materialization; validation tests. | Makes sense. |
| `users/noah/noah_runtime.h` | Runtime hook entrypoint declarations. | Root public runtime API for QMK hook wrappers. | Header-only; used by `hooks.c` and `runtime_init.c`. | Runtime init order and hook chaining tests. | Makes sense. |
| `users/noah/runtime_init.c` | Coordinates eeconfig, matrix scan, housekeeping, post-init, and RGB/key/PD service ordering. | Root runtime lifecycle owner. | `NOAH_COMMON_SOURCES`. | Runtime diag, key runtime, split sync, VIA defaults, RGB; runtime init order tests. | Makes sense. |

## Compat

| File | Purpose | Owner / role | Build / feature gate | Key deps / coverage | Verdict |
| --- | --- | --- | --- | --- | --- |
| `users/noah/lib/compat/qmk_auto_mouse_contract.h` | Central wrappers for QMK auto-mouse functions and fallbacks. | Compat public contract. | Header-only, gated by auto-mouse feature macros. | Pointer policy, PD lifecycle, RGB automouse; PD/pointer tests. | Makes sense. |
| `users/noah/lib/compat/qmk_combo_origin.c` | Tracks combo origin key positions and feedback bitmaps around QMK combo processing. | Compat runtime owner. | `NOAH_COMMON_SOURCES`, internally gated for `COMBO_ENABLE`. | Key origin bitmap, combo feedback, split sync; combo and RGB feedback tests. | Makes sense; complex but contained. |
| `users/noah/lib/compat/qmk_combo_origin.h` | Public combo-origin and bitmap query API. | Compat public contract. | Header-only with no-op stubs when combos are off. | RGB combo stage, split sync, key runtime feedback. | Makes sense. |
| `users/noah/lib/compat/qmk_contract.c` | QMK/VIA macro playback bridge. | Compat QMK contract owner. | `NOAH_COMMON_SOURCES`. | VIA macro provider; QMK contract/action lifecycle tests. | Makes sense. |
| `users/noah/lib/compat/qmk_mod_contract.c` | Overrides QMK mod registration paths into owned modifier tracking. | Compat QMK mod contract owner. | `NOAH_COMMON_SOURCES`. | Keyboard mod ownership; keyboard mod ownership tests. | Makes sense. |
| `users/noah/lib/compat/qmk_mod_contract.h` | Declarations for QMK mod override surface. | Compat public contract. | Header-only. | `keyboard_mod_ownership`; feature gate. | Makes sense. |
| `users/noah/lib/compat/qmk_pointing_contract.h` | Wrappers for Charybdis pointer CPI, sniping, and dragscroll backend APIs. | Compat public pointing contract. | Header-only, pointing-feature gated. | PD lifecycle/runtime; PD runtime tests. | Makes sense. |
| `users/noah/lib/compat/qmk_via_contract.c` | VIA command effect classification and storage wrappers. | Compat VIA contract owner. | `NOAH_COMMON_SOURCES`, gated by `VIA_ENABLE` where needed. | VIA macro defaults, split sync; qmk VIA tests. | Makes sense. |
| `users/noah/lib/compat/qmk_via_playback_contract.h` | VIA macro playback buffer declarations/fallbacks. | Compat public VIA playback contract. | Header-only, `VIA_ENABLE` gated. | VIA macro provider; VIA macro lifecycle tests. | Makes sense. |
| `users/noah/lib/compat/qmk_via_split_sync.c` | Mirrors VIA keymap writes to the other split half. | Compat split/VIA owner. | `NOAH_COMMON_SOURCES`, `VIA_ENABLE` and split gated. | VIA storage contract, split sync; qmk VIA split sync tests. | Makes sense. |
| `users/noah/lib/compat/qmk_via_split_sync.h` | Public VIA split-sync command/init API. | Compat public contract. | Header-only with feature-gated stubs. | VIA macro defaults; qmk VIA split sync tests. | Makes sense. |
| `users/noah/lib/compat/qmk_via_storage_contract.h` | Wrappers for VIA macro buffer and dynamic keymap storage. | Compat public VIA storage contract. | Header-only, `VIA_ENABLE` gated. | VIA defaults/provider; VIA tests. | Makes sense. |
| `users/noah/lib/compat/split_half.h` | Split side/half helpers and keypos-to-half mapping. | Compat public split helper. | Header-only. | PD ownership, combo/RGB sides; split runtime and PD tests. | Makes sense. |
| `users/noah/lib/compat/split_role.c` | Optional forced split master/slave role override. | Compat QMK hook owner. | `NOAH_COMMON_SOURCES`, config-gated. | Split role hook; feature gate and firmware. | Makes sense. |

## State

| File | Purpose | Owner / role | Build / feature gate | Key deps / coverage | Verdict |
| --- | --- | --- | --- | --- | --- |
| `users/noah/lib/state/ownership/keyboard_mod_ownership.c` | Refcounted physical and managed modifier ownership. | State ownership runtime owner. | `NOAH_COMMON_SOURCES`. | QMK mod contract, action dispatch, arrow/pinch modes; keyboard mod ownership tests. | Makes sense. |
| `users/noah/lib/state/ownership/keyboard_mod_ownership.h` | Public modifier ownership and debug API. | State ownership public contract. | Header-only. | Mod contract/action/PD lifecycle tests. | Makes sense. |
| `users/noah/lib/state/ownership/layer_ownership.c` | Refcounted momentary and locked layer ownership. | State ownership runtime owner. | `NOAH_COMMON_SOURCES`. | Action dispatch, key runtime, runtime trace; layer ownership tests. | Makes sense. |
| `users/noah/lib/state/ownership/layer_ownership.h` | Public layer ownership and debug API. | State ownership public contract. | Header-only. | Layer ownership and key runtime layer-lock tests. | Makes sense. |
| `users/noah/lib/state/runtime/keyboard_mod_state.c` | Captures/restores QMK real, weak, and oneshot mod state. | State runtime helper. | `NOAH_COMMON_SOURCES`. | Delayed action and key runtime core; delayed action/key runtime tests. | Makes sense. |
| `users/noah/lib/state/runtime/keyboard_mod_state.h` | Snapshot type and apply/capture declarations. | State runtime public helper. | Header-only. | Macro/key runtime tests. | Makes sense. |
| `users/noah/lib/state/runtime/runtime_context_internal.h` | Concrete shared runtime context layout. | State runtime internal storage contract. | Header-only internal. | Shared state accessors; feature gate and runtime tests. | Makes sense. |
| `users/noah/lib/state/runtime/runtime_debug.h` | Runtime debug query facade over key runtime and ownership state. | State runtime public diagnostics contract. | Header-only. | `debug.c`, runtime debug tests. | Makes sense. |
| `users/noah/lib/state/runtime/runtime_diag.c` | Watchdog/reboot diagnostic state, scope tracking, and test backend. | State runtime diagnostics owner. | `NOAH_COMMON_SOURCES`, watchdog feature gated. | Runtime init/RGB diag/macro waits; runtime diag tests. | Makes sense. |
| `users/noah/lib/state/runtime/runtime_diag.h` | Public diagnostics, heartbeat, and test backend API. | State runtime public diagnostics contract. | Header-only with feature-gated stubs. | Runtime diag and RGB tests. | Makes sense. |
| `users/noah/lib/state/runtime/runtime_reset.h` | Reset-for-test declaration. | State runtime internal/test support. | Header-only. | Host harnesses and shared state reset. | Makes sense. |
| `users/noah/lib/state/runtime/runtime_shared_state.c` | Owns singleton runtime context and subsystem state accessors. | State runtime storage owner. | `NOAH_COMMON_SOURCES`. | Key runtime core, PD runtime, trace; feature gate/runtime tests. | Makes sense. |
| `users/noah/lib/state/runtime/runtime_shared_state_internal.h` | Internal accessor declarations for shared runtime storage. | State runtime internal contract. | Header-only internal. | Runtime context and PD state. | Makes sense. |
| `users/noah/lib/state/runtime/runtime_trace.c` | Optional shared runtime trace ring buffer. | State runtime tracing owner. | `NOAH_COMMON_SOURCES`, `NOAH_RUNTIME_TRACE_ENABLE` gated. | Runtime trace and key runtime core trace tests. | Makes sense. |
| `users/noah/lib/state/runtime/runtime_trace.h` | Public trace event API and no-op stubs. | State runtime public trace contract. | Header-only with feature-gated stubs. | Runtime trace tests and firmware. | Makes sense. |
| `users/noah/lib/state/runtime/split_runtime_sync.c` | Custom split transport payload and remote state application. | State runtime split-sync owner. | `NOAH_COMMON_SOURCES`, split/RPC gated. | PD snapshots, key feedback, combo feedback, automouse; split runtime sync tests. | Makes sense. |
| `users/noah/lib/state/runtime/split_runtime_sync.h` | Public split runtime sync state and service API. | State runtime split public contract. | Header-only. | Split sync, PD, RGB feedback tests. | Makes sense. |

## Action

| File | Purpose | Owner / role | Build / feature gate | Key deps / coverage | Verdict |
| --- | --- | --- | --- | --- | --- |
| `users/noah/lib/action/action_dispatch.c` | Emits tap, held, layer, PD lock, and literal actions under runtime policy. | Action runtime owner. | `NOAH_COMMON_SOURCES`. | Action kind, owned keycode, layer ownership, PD lock, synthetic records; action dispatch tests. | Makes sense. |
| `users/noah/lib/action/action_dispatch.h` | Action descriptor and emit policy API. | Action public contract. | Header-only. | Key runtime, handled-key policy, pointer policy; action/key runtime tests. | Makes sense. |
| `users/noah/lib/action/action_kind.c` | Central metadata table for QMK/custom action kinds. | Action metadata owner. | `NOAH_COMMON_SOURCES`. | Registry list and keycode macros; action dispatch and lifecycle tests. | Makes sense. |
| `users/noah/lib/action/action_kind_dispatch.c` | Action-kind dispatch operation table. | Action dispatch internals owner. | `NOAH_COMMON_SOURCES`. | Action kind internals; action lifecycle tests. | Makes sense. |
| `users/noah/lib/action/action_kind_dispatch_internal.h` | Internal action-kind dispatch callbacks. | Action internal contract. | Header-only internal. | `action_kind_dispatch.c`, lifecycle. | Makes sense. |
| `users/noah/lib/action/action_kind_internal.h` | Internal action metadata structs and lookup helpers. | Action internal contract. | Header-only internal. | Action kind, dispatch, handled-key policy. | Makes sense. |
| `users/noah/lib/action/action_kind_registry_list.h` | Single list of action-kind metadata rows. | Action registry data include. | Header-only internal include. | Action kind/dispatch compile coverage. | Makes sense. |
| `users/noah/lib/action/action_lifecycle.c` | High-level key action press/release/tap lifecycle. | Action runtime lifecycle owner. | `NOAH_COMMON_SOURCES`. | Macro dispatch, VIA playback, PD mode, action kind dispatch; action lifecycle tests. | Makes sense. |
| `users/noah/lib/action/action_lifecycle.h` | Public action lifecycle entrypoints. | Action public contract. | Header-only. | Key runtime process; action lifecycle and macro tests. | Makes sense. |
| `users/noah/lib/action/owned_keycode.c` | Owns QMK key registration/unregistration for runtime-held keycodes. | Action ownership helper. | `NOAH_COMMON_SOURCES`. | Keyboard mod ownership, pointer policy, raw QMK register APIs; owned keycode tests. | Makes sense. |
| `users/noah/lib/action/owned_keycode.h` | Owned keycode API. | Action public helper. | Header-only. | Macro payload runner, held action, action dispatch. | Makes sense. |
| `users/noah/lib/action/synthetic_record.c` | Synthetic QMK/custom record bridge with recursion guard. | Action QMK bridge owner. | `NOAH_COMMON_SOURCES`. | Action dispatch and process path; action dispatch/lifecycle tests. | Makes sense. |
| `users/noah/lib/action/synthetic_record.h` | Synthetic record API. | Action public helper. | Header-only. | Action dispatch tests. | Makes sense. |

## Key Interaction And Ownership

| File | Purpose | Owner / role | Build / feature gate | Key deps / coverage | Verdict |
| --- | --- | --- | --- | --- | --- |
| `users/noah/lib/key/interaction/handled_key.h` | Public handled-key materialized contract and semantics helpers. | Key interaction public contract. | Header-only. | Key runtime core, profile validation; key behavior/runtime tests. | Makes sense. |
| `users/noah/lib/key/interaction/handled_key_defaults.c` | Defaults and conversions from authored behavior to materialized contracts. | Key interaction owner. | `NOAH_COMMON_SOURCES`. | Action descriptors, key behavior schema; key behavior lookup/validation tests. | Makes sense. |
| `users/noah/lib/key/interaction/handled_key_internal.h` | Internal materialization helpers. | Key interaction internal contract. | Header-only internal. | Materialize, accessors, transparency, defaults. | Makes sense. |
| `users/noah/lib/key/interaction/handled_key_lookup.c` | Looks up authored behavior by physical key and tap count. | Key interaction lookup owner. | `NOAH_COMMON_SOURCES`. | `key_behaviors[]`, QMK keypos; key behavior lookup tests. | Makes sense. |
| `users/noah/lib/key/interaction/handled_key_materialize.c` | Resolves authored behavior into runtime-ready materialized contract. | Key interaction materializer owner. | `NOAH_COMMON_SOURCES`. | Defaults, transparency, policy; key behavior lookup/runtime tests. | Makes sense. |
| `users/noah/lib/key/interaction/handled_key_policy.h` | Inline policy builder from action descriptors to key-runtime semantics. | Key interaction policy helper, internal-by-use. | Header-only. | Action kind internals; key behavior/runtime tests. | Makes sense. |
| `users/noah/lib/key/interaction/handled_key_resolution_accessors.c` | Accessors for resolved tap, hold, terms, and flags. | Key interaction accessor owner. | `NOAH_COMMON_SOURCES`. | Handled-key internals; key behavior tests. | Makes sense. |
| `users/noah/lib/key/interaction/handled_key_transparency.c` | Lower-layer transparent tap/hold/long-hold resolution. | Key interaction transparency owner. | `NOAH_COMMON_SOURCES`. | QMK keymap lookup, handled-key internals; key behavior lookup tests. | Makes sense. |
| `users/noah/lib/key/interaction/key_behavior.h` | Authored key behavior schema and helper macros. | Key interaction public authoring contract. | Header-only. | Keymap materialization, validation; profile validation. | Makes sense. |
| `users/noah/lib/key/interaction/key_behavior_lookup.c` | Key behavior lookup and duplicate/unreachable validation. | Key interaction validation owner. | `NOAH_COMMON_SOURCES`. | Authored tables, keymap validation; key behavior tests. | Makes sense. |
| `users/noah/lib/key/interaction/key_behavior_lookup.h` | Public lookup/validation declarations. | Key interaction public contract. | Header-only. | Runtime process, profile validation. | Makes sense. |
| `users/noah/lib/key/interaction/keymap_validation.c` | Validates raw layer actions, combo members/outputs, and key behavior reachability. | Key interaction authored-profile validator. | `NOAH_COMMON_SOURCES`, combo gated where needed. | Authored keymap and combos; keymap/real profile validation. | Makes sense. |
| `users/noah/lib/key/interaction/keymap_validation.h` | Keymap validation entrypoint. | Key interaction public validator. | Header-only. | Real profile validation. | Makes sense. |
| `users/noah/lib/key/ownership/held_action.c` | Runtime-held action registration and release ownership. | Key ownership runtime owner. | `NOAH_COMMON_SOURCES`. | Owned keycodes, layer ownership, runtime context; held action tests. | Makes sense. |
| `users/noah/lib/key/ownership/held_action.h` | Held action API and survival policy declaration. | Key ownership public contract. | Header-only. | Key runtime core and action dispatch. | Makes sense. |
| `users/noah/lib/key/ownership/held_repeat.c` | Repeat action scheduler and pointer-layer action anchors. | Key ownership runtime owner. | `NOAH_COMMON_SOURCES`. | Owned keycodes, runtime context, timer; held action/repeat coverage in host suite. | Makes sense. |
| `users/noah/lib/key/ownership/held_repeat.h` | Held repeat API. | Key ownership public contract. | Header-only. | Key runtime core. | Makes sense. |

## Key Runtime

| File | Purpose | Owner / role | Build / feature gate | Key deps / coverage | Verdict |
| --- | --- | --- | --- | --- | --- |
| `users/noah/lib/key/runtime/api.c` | Public settle-pending-fallback-hold wrapper. | Key runtime API owner. | `NOAH_COMMON_SOURCES`. | Transition/core plan execution; key runtime tests. | Makes sense. |
| `users/noah/lib/key/runtime/api.h` | Public key runtime API. | Key runtime public contract. | Header-only. | Action dispatch, runtime init. | Makes sense. |
| `users/noah/lib/key/runtime/core/projection.h` | Projection snapshot type for runtime state comparison. | Key runtime core public debug contract. | Header-only. | Core runtime, trace tests. | Makes sense. |
| `users/noah/lib/key/runtime/core/release_internal.h` | Core release planning types and settlement enum. | Key runtime core internal contract. | Header-only internal. | Core runtime only. | Makes sense. |
| `users/noah/lib/key/runtime/core/runtime.c` | Core reducer for press tokens, tap series, leases, release plans, scan plans, and projection snapshots. | Key runtime core owner. | `NOAH_COMMON_SOURCES`. | Handled-key, action, ownership, PD, split/debug; key runtime release matrix/scenario/integration tests. | Makes sense; optional decomposition target due size. |
| `users/noah/lib/key/runtime/core/runtime.h` | Public core state types, effect plan API, reducers, queries, and debug helpers. | Key runtime core public contract. | Header-only. | Runtime wrappers, debug, feedback, tests. | Makes sense, broad by design. |
| `users/noah/lib/key/runtime/core/trace.c` | Structured core projection trace emission. | Key runtime core trace owner. | `NOAH_COMMON_SOURCES`, trace gated. | Runtime trace; trace tests. | Makes sense. |
| `users/noah/lib/key/runtime/core/trace.h` | Core trace declarations and no-op stubs. | Key runtime core trace contract. | Header-only, trace gated. | Runtime trace tests. | Makes sense. |
| `users/noah/lib/key/runtime/debug.c` | Public runtime debug queries over core state. | Key runtime debug facade owner. | `NOAH_COMMON_SOURCES`. | Core runtime and origin registry; runtime debug tests. | Makes sense. |
| `users/noah/lib/key/runtime/delayed_action.c` | Dispatches delayed actions under captured modifier state. | Key runtime delayed action helper. | `NOAH_COMMON_SOURCES`. | Keyboard mod state, action lifecycle; delayed action and key runtime tests. | Makes sense. |
| `users/noah/lib/key/runtime/delayed_action.h` | Delayed action API. | Key runtime public helper. | Header-only. | Core runtime and release path. | Makes sense. |
| `users/noah/lib/key/runtime/effects/effect.h` | Runtime effect type umbrella. | Key runtime effect contract. | Header-only. | Core runtime effects. | Makes sense. |
| `users/noah/lib/key/runtime/effects/effect_queue.h` | Small inline effect queue helper. | Key runtime effect helper. | Header-only. | Transition/core plan execution. | Makes sense. |
| `users/noah/lib/key/runtime/feedback.c` | Preview layer, combo feedback, semantic key feedback, and flash metadata. | Key runtime feedback owner. | `NOAH_COMMON_SOURCES`, combo/RGB feature gated. | Core/debug/split/RGB stages; RGB layer and split sync tests. | Makes sense. |
| `users/noah/lib/key/runtime/feedback.h` | Public feedback query/update API and semantic map helpers. | Key runtime feedback contract. | Header-only. | RGB key/combo stages, split sync. | Makes sense. |
| `users/noah/lib/key/runtime/interaction.h` | Inline runtime slot interaction and release decision helpers. | Key runtime policy helper. | Header-only. | Core runtime release planning; key runtime release matrix tests. | Makes sense. |
| `users/noah/lib/key/runtime/keypos_codec.h` | Packs/unpacks keypos for compact runtime state. | Key runtime helper. | Header-only. | Core runtime, split sync. | Makes sense. |
| `users/noah/lib/key/runtime/origin_registry.c` | Tracks origin side bitmap for physical key positions. | Key runtime origin owner. | `NOAH_COMMON_SOURCES`. | Split-half helpers, PD owner sides, combo feedback; split and PD tests. | Makes sense. |
| `users/noah/lib/key/runtime/origin_registry.h` | Origin registry and bitmap API. | Key runtime public helper. | Header-only. | Combo origin, split sync, RGB combo feedback. | Makes sense. |
| `users/noah/lib/key/runtime/preflight.c` | Preflight processing before runtime-owned key handling. | Key runtime process stage owner. | `NOAH_COMMON_SOURCES`. | Core flush/interrupt, PD masks; key runtime integration tests. | Makes sense. |
| `users/noah/lib/key/runtime/press.c` | Handled-key press wrapper into transition planner. | Key runtime process stage owner. | `NOAH_COMMON_SOURCES`. | Transition/core; key runtime tests. | Makes sense. |
| `users/noah/lib/key/runtime/process.c` | QMK process-record orchestration for synthetic, preflight, PD, handled, direct, and macro actions. | Key runtime process owner. | `NOAH_COMMON_SOURCES`. | Action lifecycle, PD, macro, core, diag; key runtime/action lifecycle tests. | Makes sense. |
| `users/noah/lib/key/runtime/process_internal.h` | Internal process-stage declarations. | Key runtime internal contract. | Header-only internal. | Preflight/press/release/scan/process. | Makes sense. |
| `users/noah/lib/key/runtime/release.c` | Handled and non-handled release processing and pending release drain. | Key runtime process stage owner. | `NOAH_COMMON_SOURCES`. | Transition/core, delayed action; release matrix tests. | Makes sense. |
| `users/noah/lib/key/runtime/scan.c` | Matrix scan runtime planning and PD DPI sync service. | Key runtime scan owner. | `NOAH_COMMON_SOURCES`. | Transition/core, delayed action, PD lifecycle; key runtime and PD tests. | Makes sense. |
| `users/noah/lib/key/runtime/slot/release_resolver.h` | Inline release decision reducer for slot interactions. | Key runtime slot policy helper. | Header-only. | Core runtime release planning; release matrix tests. | Makes sense. |
| `users/noah/lib/key/runtime/trace.c` | Optional text/structured runtime trace around process and effect plans. | Key runtime trace owner. | `NOAH_COMMON_SOURCES`, trace/console gated. | Runtime trace tests. | Makes sense. |
| `users/noah/lib/key/runtime/trace.h` | Key runtime trace declarations and no-op stubs. | Key runtime trace contract. | Header-only, trace gated. | Runtime trace tests. | Makes sense. |
| `users/noah/lib/key/runtime/transition.c` | Builds and executes runtime effect plans for press/release/scan/flush transitions. | Key runtime transition owner. | `NOAH_COMMON_SOURCES`. | Core runtime effects, delayed action, layer/held ownership; key runtime tests. | Makes sense. |
| `users/noah/lib/key/runtime/transition.h` | Transition API. | Key runtime public-ish process contract. | Header-only. | Process stages and API wrapper. | Makes sense. |
| `users/noah/lib/key/runtime/types.h` | Shared runtime action/effect payload types. | Key runtime type contract. | Header-only. | Core/runtime wrappers. | Makes sense. |

## Macro

| File | Purpose | Owner / role | Build / feature gate | Key deps / coverage | Verdict |
| --- | --- | --- | --- | --- | --- |
| `users/noah/lib/macro/macro_dispatch.c` | Dispatches hardcoded authored macro slots through the slot provider. | Macro hardcoded dispatch owner. | `NOAH_COMMON_SOURCES`. | `noah_keymap_ids.h`, payload IR, slot provider; macro dispatch tests. | Makes sense. |
| `users/noah/lib/macro/macro_dispatch.h` | Hardcoded macro dispatch/validation API. | Macro public contract. | Header-only. | Action lifecycle and profile validation. | Makes sense. |
| `users/noah/lib/macro/macro_payload.c` | Public validate/play wrappers over payload compile and IR playback. | Macro payload facade owner. | `NOAH_COMMON_SOURCES`. | Payload parser/run; macro payload tests. | Makes sense. |
| `users/noah/lib/macro/macro_payload.h` | Payload IR type and encode/decode/play API. | Macro payload public contract. | Header-only. | Hardcoded macros, VIA macros, tests. | Makes sense. |
| `users/noah/lib/macro/macro_payload_decode_qmk.c` | Decodes QMK send-string macro stream into payload IR. | Macro payload decoder owner. | `NOAH_COMMON_SOURCES`. | QMK send-string tokens; macro payload and VIA lifecycle tests. | Makes sense. |
| `users/noah/lib/macro/macro_payload_encode.c` | Encodes payload IR back to QMK macro stream. | Macro payload encoder owner. | `NOAH_COMMON_SOURCES`. | Payload parser/IR; macro payload and VIA defaults tests. | Makes sense. |
| `users/noah/lib/macro/macro_payload_internal.h` | Internal payload command parser and keycode lookup declarations. | Macro payload internal contract. | Header-only internal. | Payload parse/decode/run/encode. | Makes sense. |
| `users/noah/lib/macro/macro_payload_keycodes.c` | Maps textual KC names and aliases to QMK send-string keycodes. | Macro payload keycode map owner. | `NOAH_COMMON_SOURCES`. | QMK send-string keycode aliases; macro payload tests. | Makes sense. |
| `users/noah/lib/macro/macro_payload_parse.c` | Parses authored payload text into compiled IR. | Macro payload parser owner. | `NOAH_COMMON_SOURCES`. | Keycode lookup, IR writer; macro payload tests. | Makes sense. |
| `users/noah/lib/macro/macro_payload_run.c` | Plays compiled payload IR with owned keycodes and watchdog heartbeats. | Macro payload runtime owner. | `NOAH_COMMON_SOURCES`. | Owned keycode, runtime diag; macro payload/VIA lifecycle tests. | Makes sense. |
| `users/noah/lib/macro/macro_slot_provider.c` | Reusable macro slot cache/load/play/encode provider. | Macro shared provider owner. | `NOAH_COMMON_SOURCES`. | Payload IR; macro dispatch and VIA defaults tests. | Makes sense. |
| `users/noah/lib/macro/macro_slot_provider.h` | Slot provider/cache API. | Macro public helper. | Header-only. | Hardcoded/VIA macro providers. | Makes sense. |
| `users/noah/lib/macro/via_macro_defaults.c` | Validates and seeds authored default VIA macro payloads; handles VIA command effects. | Macro VIA defaults owner. | `NOAH_COMMON_SOURCES`, `VIA_ENABLE` gated. | VIA storage/split sync, RGB invalidation; VIA macro defaults tests. | Makes sense. |
| `users/noah/lib/macro/via_macro_defaults.h` | VIA default macro lifecycle hooks and stubs. | Macro public VIA contract. | Header-only, `VIA_ENABLE` gated. | Runtime init order and VIA tests. | Makes sense. |
| `users/noah/lib/macro/via_macro_provider.c` | Reads VIA dynamic macro buffer, decodes IR, and plays VIA macros. | Macro VIA provider owner. | `NOAH_COMMON_SOURCES`, `VIA_ENABLE` gated. | VIA playback contract, slot provider; VIA macro action lifecycle tests. | Makes sense. |
| `users/noah/lib/macro/via_macro_provider.h` | VIA macro playback/invalidation API and stubs. | Macro public VIA contract. | Header-only, `VIA_ENABLE` gated. | Action lifecycle and VIA tests. | Makes sense. |

## Pointing

| File | Purpose | Owner / role | Build / feature gate | Key deps / coverage | Verdict |
| --- | --- | --- | --- | --- | --- |
| `users/noah/lib/pointing/defs/pd_mode_flags.h` | PD mode IDs, flags, snapshots, and read-only query declarations. | Pointing defs public lightweight contract. | Header-only. | Manifest, split side helpers; PD/split/RGB tests. | Makes sense. |
| `users/noah/lib/pointing/defs/pd_mode_manifest.h` | Single source of truth for PD mode registry rows. | Pointing manifest contract. | Header-only. | Keycode IDs, registry, RGB validation; profile/PD tests. | Makes sense. |
| `users/noah/lib/pointing/defs/pd_modes.h` | Full public PD mode API, definitions, commands, and runtime entrypoints. | Pointing public contract. | Header-only, pointing types gated for no-pointing builds. | Registry/state/lifecycle/runtime; PD tests. | Makes sense. |
| `users/noah/lib/pointing/modes/pd_mode_arrow.c` | Arrow-mode pointer-to-arrows and mouse-button shortcuts. | Pointing mode owner. | `NOAH_POINTING_SOURCES`, `POINTING_DEVICE_ENABLE` gated. | Action dispatch, keyboard mod ownership; PD handlers/runtime tests. | Makes sense. |
| `users/noah/lib/pointing/modes/pd_mode_brightness.c` | Brightness-mode vertical threshold handler. | Pointing mode owner. | `NOAH_POINTING_SOURCES`, pointing gated. | Common handler helper; PD handlers tests. | Makes sense. |
| `users/noah/lib/pointing/modes/pd_mode_dragscroll.c` | Dragscroll axis-lock and scroll conversion handler. | Pointing mode owner. | `NOAH_POINTING_SOURCES`, pointing gated. | Timer, QMK scroll config; PD mode handlers/runtime tests. | Makes sense. |
| `users/noah/lib/pointing/modes/pd_mode_handler_common.h` | Shared pointer-mode axis and tap helpers. | Pointing mode internal helper. | Header-only, pointing types. | Mode handlers and action dispatch. | Makes sense. |
| `users/noah/lib/pointing/modes/pd_mode_handlers.h` | Public declarations for mode handlers and resets. | Pointing mode public contract. | Header-only. | Registry and handler tests. | Makes sense. |
| `users/noah/lib/pointing/modes/pd_mode_pinch.c` | Pinch lifecycle side effects for GUI ownership and dragscroll-like lock behavior. | Pointing mode lifecycle owner. | `NOAH_POINTING_SOURCES`. | Registry internals, keyboard mod ownership; PD mode tests. | Makes sense. |
| `users/noah/lib/pointing/modes/pd_mode_volume.c` | Volume-mode vertical threshold handler. | Pointing mode owner. | `NOAH_POINTING_SOURCES`, pointing gated. | Common handler helper; PD handlers tests. | Makes sense. |
| `users/noah/lib/pointing/modes/pd_mode_zoom.c` | Zoom-mode vertical threshold handler. | Pointing mode owner. | `NOAH_POINTING_SOURCES`, pointing gated. | Common handler helper; PD handlers tests. | Makes sense. |
| `users/noah/lib/pointing/policy/pd_mode_policy.h` | Trait-based PD mode policy helpers. | Pointing policy helper. | Header-only. | PD registry traits, pointer policy, key runtime core. | Makes sense. |
| `users/noah/lib/pointing/policy/pointer_layer_policy.c` | Auto-mouse layer arbitration and mouse-record classification. | Pointing policy owner. | `NOAH_POINTING_SOURCES`, auto-mouse gated with stubs. | PD snapshots, auto-mouse contract; pointer layer policy tests. | Makes sense. |
| `users/noah/lib/pointing/policy/pointer_layer_policy.h` | Pointer-layer policy API and debug snapshot type. | Pointing public policy contract. | Header-only. | PD runtime, action/held repeat, debug projection. | Makes sense. |
| `users/noah/lib/pointing/runtime/pd_mode_buffered_tap_internal.h` | Internal query for mode-owned buffered tap modifier masking. | Pointing runtime internal contract. | Header-only internal. | Registry and key runtime core. | Makes sense. |
| `users/noah/lib/pointing/runtime/pd_mode_internal.h` | Internal PD mode mutators and transition declarations. | Pointing runtime internal contract. | Header-only internal. | State/lifecycle/registry; PD tests. | Makes sense. |
| `users/noah/lib/pointing/runtime/pd_mode_keyboard_event_internal.h` | Internal active-mode keyboard event modifier mask query. | Pointing runtime internal contract. | Header-only internal. | Registry and key runtime process. | Makes sense. |
| `users/noah/lib/pointing/runtime/pd_mode_lifecycle.c` | PD mode activation/deactivation/lock/unlock side effects and DPI sync. | Pointing lifecycle owner. | `NOAH_POINTING_SOURCES`. | PD state, registry hooks, QMK pointing/auto-mouse contracts; PD runtime tests. | Makes sense. |
| `users/noah/lib/pointing/runtime/pd_mode_registry.c` | Materializes PD registry, trait lookup, lifecycle callbacks, and mode key lookup. | Pointing registry owner. | `NOAH_POINTING_SOURCES`. | Manifest, mode handlers, lifecycle internals; PD mode tests. | Makes sense. |
| `users/noah/lib/pointing/runtime/pd_mode_registry_internal.h` | Internal lifecycle hook struct and registry hook API. | Pointing registry internal contract. | Header-only internal. | Registry, lifecycle, pinch mode. | Makes sense. |
| `users/noah/lib/pointing/runtime/pd_mode_runtime_shared_state_internal.h` | Concrete PD mode runtime state stored in shared runtime context. | Pointing runtime internal storage contract. | Header-only internal. | Shared state, state/snapshot. | Makes sense. |
| `users/noah/lib/pointing/runtime/pd_mode_snapshot.c` | Builds local/display PD mode snapshot views. | Pointing snapshot owner. | `NOAH_POINTING_SOURCES`. | Runtime shared state, split master role; PD/split/RGB tests. | Makes sense. |
| `users/noah/lib/pointing/runtime/pd_mode_state.c` | PD command state machine for activate/deactivate/lock/unlock/key/remote snapshot. | Pointing state owner. | `NOAH_POINTING_SOURCES`. | Key origin registry, key runtime lock mirror, trace; PD/split/key-runtime integration tests. | Makes sense. |
| `users/noah/lib/pointing/runtime/pd_runtime.c` | QMK pointing init/task, mouse-record hook, and layer-state hook integration. | Pointing runtime QMK hook owner. | `NOAH_POINTING_SOURCES`, pointing/auto-mouse/idle-noise gated. | PD mode registry, pointer policy, QMK pointing contract; PD runtime tests. | Makes sense. |

## RGB

| File | Purpose | Owner / role | Build / feature gate | Key deps / coverage | Verdict |
| --- | --- | --- | --- | --- | --- |
| `users/noah/lib/rgb/automouse/rgb_automouse.c` | Computes current auto-mouse RGB timeout progress, local or split-mirrored. | RGB automouse owner. | `NOAH_AUTOMOUSE_SOURCES`, RGB/auto-mouse gated. | Auto-mouse contract, PD lock state, split sync; RGB layer tests. | Makes sense. |
| `users/noah/lib/rgb/automouse/rgb_automouse.h` | Auto-mouse RGB progress and fade math helpers. | RGB automouse public helper. | Header-only with RGB/auto-mouse stubs. | RGB automouse stage and tests. | Makes sense. |
| `users/noah/lib/rgb/automouse/rgb_automouse_stage.c` | Renders auto-mouse gradient fade between layer/base-effect destinations. | RGB stage owner. | `NOAH_COMMON_SOURCES`, RGB/auto-mouse/gradient gated. | RGB layer frame, QMK WS2812 buffer; RGB layer render tests. | Makes sense. |
| `users/noah/lib/rgb/automouse/rgb_automouse_stage.h` | Auto-mouse stage lifecycle/render API and stubs. | RGB stage contract. | Header-only, feature gated. | RGB runtime. | Makes sense. |
| `users/noah/lib/rgb/core/rgb_config_defaults.c` | Weak defaults for optional authored RGB group tables. | RGB config defaults owner. | `NOAH_COMMON_SOURCES`, RGB gated. | Keymap RGB config override; RGB validation tests. | Makes sense. |
| `users/noah/lib/rgb/core/rgb_config_helpers.h` | Authoring macros for RGB config tables. | RGB keymap authoring helper. | Header-only. | Keymap `rgb_config.c`, profile validation. | Makes sense. |
| `users/noah/lib/rgb/core/rgb_helpers.h` | RGB types and split-safe LED painting helpers. | RGB public helper. | Header-only, RGB stubs when disabled. | All RGB stages; RGB tests and feature gate. | Makes sense. |
| `users/noah/lib/rgb/core/rgb_runtime.c` | RGB post-init validation and render-stage ordering. | RGB runtime owner. | `NOAH_COMMON_SOURCES`, RGB gated. | Runtime diag and all RGB stages; RGB layer render tests. | Makes sense. |
| `users/noah/lib/rgb/core/rgb_runtime.h` | RGB runtime hook declarations. | RGB public runtime contract. | Header-only. | Runtime init and VIA invalidation. | Makes sense. |
| `users/noah/lib/rgb/core/rgb_validation.c` | Validates authored RGB layer, PD, combo, key feedback, and automouse config. | RGB validation owner. | `NOAH_COMMON_SOURCES`, feature gated by RGB/PD/combo/key feedback. | RGB config tables, PD registry; RGB validation/real profile validation. | Makes sense. |
| `users/noah/lib/rgb/core/rgb_validation.h` | RGB validation entrypoint. | RGB public validation contract. | Header-only. | RGB runtime, real profile validation. | Makes sense. |
| `users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c` | Renders combo underlay/overlay feedback from local or split bitmaps. | RGB combo stage owner. | `NOAH_COMMON_SOURCES`, RGB/combo gated. | Key runtime feedback, split sync, origin bitmap; RGB layer tests. | Makes sense. |
| `users/noah/lib/rgb/stages/rgb_combo_feedback_stage.h` | Combo feedback stage API and stubs. | RGB stage contract. | Header-only, RGB/combo gated. | RGB runtime. | Makes sense. |
| `users/noah/lib/rgb/stages/rgb_key_feedback_stage.c` | Renders key runtime semantic feedback by key, half, or board. | RGB key feedback stage owner. | `NOAH_COMMON_SOURCES`, RGB/key feedback gated. | Key feedback semantic map, split sync; RGB layer tests. | Makes sense. |
| `users/noah/lib/rgb/stages/rgb_key_feedback_stage.h` | Key feedback stage API and stubs. | RGB stage contract. | Header-only, RGB/key feedback gated. | RGB runtime. | Makes sense. |
| `users/noah/lib/rgb/stages/rgb_layer_stage.c` | Builds layer base frames and key-mapped LED coverage maps. | RGB layer stage owner. | `NOAH_COMMON_SOURCES`, RGB gated. | QMK keymap introspection, authored layer colors/groups; RGB base/layer tests. | Makes sense. |
| `users/noah/lib/rgb/stages/rgb_layer_stage.h` | RGB frame type and layer-stage API. | RGB stage contract. | Header-only, RGB gated. | RGB runtime and automouse/preview stages. | Makes sense. |
| `users/noah/lib/rgb/stages/rgb_pd_mode_stage.c` | Renders active/display PD mode colors and LED groups. | RGB PD stage owner. | `NOAH_COMMON_SOURCES`, RGB/pointing gated. | PD snapshot, authored PD colors/groups; RGB layer/validation tests. | Makes sense. |
| `users/noah/lib/rgb/stages/rgb_pd_mode_stage.h` | PD mode stage API and stubs. | RGB stage contract. | Header-only, RGB/pointing gated. | RGB runtime. | Makes sense. |
| `users/noah/lib/rgb/stages/rgb_preview_stage.c` | Renders preview-layer overlay from key runtime feedback state. | RGB preview stage owner. | `NOAH_COMMON_SOURCES`, RGB/key feedback gated. | Key feedback preview layer, split sync, layer stage; RGB layer tests. | Makes sense. |
| `users/noah/lib/rgb/stages/rgb_preview_stage.h` | Preview stage API and stubs. | RGB stage contract. | Header-only, RGB/key feedback gated. | RGB runtime. | Makes sense. |
