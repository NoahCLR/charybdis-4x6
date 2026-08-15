# Runtime Flow

This document traces the runtime paths that start from user behavior or QMK
hooks and end in side effects. Labels in the diagrams use these meanings:

- Authoritative: owns runtime truth.
- Planned: decides behavior and emits explicit effects.
- Projected: applies planned effects to QMK-facing state or registries.
- Compatibility: adapts upstream QMK/fork behavior without owning runtime truth.

## QMK Hook Lifecycle

```mermaid
flowchart TD
    qmk["QMK *_user hooks"] --> weak["users/noah/hooks.c weak defaults"]
    weak --> runtime["users/noah/noah_runtime.h entry points"]
    runtime --> init["runtime_init.c init and scan orchestration"]
    runtime --> process["key/runtime/process.c key event flow"]
    runtime --> pointing["pointing/runtime/pd_runtime.c pointing hook"]
    runtime --> rgb["rgb/core/rgb_runtime.c RGB hook"]
    runtime --> layer["pointing runtime + layer ownership hook"]
    init --> via_defaults["macro/via_macro_defaults.c"]
    init --> combo_scan["compat/qmk_combo_origin.c lifecycle reconciliation"]
    init --> key_scan["key/runtime/scan.c"]
    init --> split_sync["split/runtime_sync.c"]
    init --> held_repeat["key/ownership/held_repeat.c"]
```

Keymap-local hook overrides may replace the QMK hook, but they must call the
matching `noah_*` helper when they still want shared userspace behavior.

## Key Press Flow

```mermaid
flowchart TD
    physical["Physical key press"] --> pre_user["pre_process_record_user"]
    pre_user --> origin["Compatibility: qmk_combo_origin observes physical member"]
    pre_user --> key_track["Applied: owned_keycode tracks physical report usage"]
    pre_user --> mod_track["Projected: keyboard modifier ownership tracks physical mods"]
    physical --> process_user["process_record_user"]
    process_user --> normalize["Compatibility: combo origin normalizes event key"]
    normalize --> observe["Authoritative: reducer observes physical event"]
    observe --> preflight["Planned: preflight interrupts or suppresses aggregate-owned defaults"]
    preflight --> pd_handler["PD key handler intercepts active mode keys"]
    pd_handler --> lookup["key/behavior handled_key_lookup"]
    lookup --> press_plan["Planned: reducer press effect plan"]
    press_plan --> transition["Transition plan transport"]
    transition --> projection["Projected: apply effects"]
    projection --> action["Action dispatch or macro"]
    projection --> held["Held action or repeat registry"]
    projection --> layer["Layer ownership"]
    projection --> pd["PD mode state"]
    projection --> feedback["Feedback pulse state"]
```

The reducer owns active press identity by physical `keypos_t`. Layer changes or
transparent resolution do not move that identity. If a runtime-handled pd-mode
key matches the currently locked mode, press planning emits an explicit
unlock request before registering the held action, so the key becomes a
momentary owner for the rest of the physical hold.

## Key Release Flow

```mermaid
flowchart TD
    release["Physical key release"] --> normalize["Compatibility: combo origin normalizes release"]
    normalize --> observe["Authoritative: reducer observes release"]
    observe --> recover["process.c recovers resolved press keycode"]
    recover --> lookup["key/behavior lookup if handled"]
    lookup --> release_planner["Planned: release_planner resolves semantics"]
    release_planner --> release_effects["Planned: active release or pending multi-tap effects"]
    release_effects --> defer["Adapter: deferred_release may queue blocked dispatch"]
    defer --> queue["Authoritative queue: pending_release_queue"]
    defer --> transition["Transition plan transport"]
    transition --> projection["Projected: action/layer/PD/feedback effects"]
    projection --> drain["Drain pending release dispatches after blockers clear"]
```

Quick release, fallback suppression, buffered base tap, active release, and
pending multi-tap release decisions belong to `key/runtime/planning/`. The
release planner also suppresses fallback or lock retoggle when a same-mode
pd lock was already consumed on press. The deferred release adapter may queue
or drain effects but must not re-decide release semantics.

## Matrix Scan Flow

```mermaid
flowchart TD
    scan["matrix_scan_user"] --> init["runtime_init.c"]
    init --> via["VIA macro default scan seeding"]
    init --> combo["Compatibility: retire suppressed/expired combo origins"]
    init --> key_scan["key/runtime/scan.c"]
    key_scan --> reducer_scan["Authoritative reducer scan state"]
    reducer_scan --> scan_plan["Planned: scan_planner and tap_series helpers"]
    scan_plan --> transition["Transition plan transport"]
    transition --> projection["Projected effects"]
    projection --> pending["Drain pending release queue"]
    init --> split["split/runtime_sync_tick"]
    scan --> housekeeping["housekeeping_task_user"]
    housekeeping --> repeat["Held repeat tick"]
    housekeeping --> diag["Watchdog refresh and boot-indicator expiry"]
```

Combo-origin reconciliation runs before key-runtime scan projection. It removes
QMK-disabled candidates immediately and expires inactive candidates only after
the first crossed-deadline scan has been followed by a `combo_task()` cycle.
Scan also owns hold threshold promotion, long-hold promotion, pending multi-tap
expiry, pending release draining, split heartbeats, and held repeat ticking.

## Reducer, Planner, Projection Boundary

```mermaid
flowchart LR
    event["QMK event"] --> reducer["Authoritative reducer state"]
    reducer --> planner["Planned effects"]
    planner --> transition["Stack-backed transition plan"]
    transition --> projector["Projected side effects"]
    projector --> qmk_state["QMK reports, layers, mods, macros, PD, RGB feedback"]
    qmk_state --> debug["Debug/trace snapshots"]
    debug -.read only.-> reducer
```

The reducer stores truth. Planners decide. Projection writes outward. Debug and
trace read state but do not mutate reducer facts.

## Pointing-Device Flow

```mermaid
flowchart TD
    key["PD keycode or authored key behavior"] --> key_runtime["Key runtime effect planning"]
    key_runtime --> pd_projection["Projected: pd_projection lock tap, explicit lock state, or held action preemption"]
    pd_projection --> pd_state["Authoritative: pd_mode_state local/display/remote mode state"]
    pointer["pointing_device_task_user report"] --> pd_runtime["pd_runtime.c"]
    pd_runtime --> snapshot["pd_mode_snapshot"]
    snapshot --> handler["Active mode handler"]
    handler --> output["Mouse report output"]
    pd_state --> bridge["Bridge: pd_mode_key_runtime_bridge observes lock state"]
    bridge --> reducer_shadow["Reducer shadow lock state"]
    pd_state --> rgb["RGB PD mode stage"]
    pd_state --> split["Split runtime sync"]
```

PD mode state is PD-runtime-owned. Key runtime can request PD effects and observe
PD lock state through the bridge, but it does not own local/display/remote PD
mode storage.

## RGB Render Pipeline

```mermaid
flowchart TD
    rgb_hook["rgb_matrix_indicators_advanced_user"] --> diag["Runtime boot indicator (150 white)"]
    diag --> base["Layer base or automouse fade"]
    base --> combo_under["Combo underlay"]
    combo_under --> preview["Key preview layer"]
    preview --> pd["PD mode overlay"]
    pd --> combo_overlay["Combo overlay"]
    combo_overlay --> key_feedback["Key-behavior feedback overlay"]
    key_feedback --> leds["RGB matrix LEDs"]
    key_runtime["Key runtime feedback state"] --> preview
    key_runtime --> key_feedback
    combo["Combo origin/feedback bitmaps"] --> combo_under
    combo --> combo_overlay
    pd_state["PD mode snapshots"] --> pd
    authored["rgb_config.c authored tables"] --> base
    authored --> pd
    authored --> combo_under
    authored --> key_feedback
```

RGB rendering is projected UI. It must not create key-runtime, combo, or PD
truth; it consumes snapshots and authored color tables.

## Split Sync Flow

```mermaid
flowchart TD
    master["Master half"] --> build_base["Build base packet: automouse, PD, preview"]
    master --> build_combo["Build combo feedback packet"]
    master --> build_semantic["Build key-feedback semantic packet"]
    master --> build_branch["Build broad-owner/tap-branch packet"]
    build_base --> rpc["QMK split transaction RPC"]
    build_combo --> rpc
    build_semantic --> rpc
    build_branch --> rpc
    rpc --> slave["Slave remote snapshot"]
    slave --> rgb["Slave RGB rendering"]
    pd["PD mode state"] --> build_base
    feedback["Key feedback state"] --> build_base
    feedback --> build_semantic
    feedback --> build_branch
    combo["Combo origin bitmaps"] --> build_combo
```

Split sync is transport. It mirrors already-owned state to the other half and
does not make ownership decisions.

## Macro And VIA Flow

```mermaid
flowchart TD
    keymap["keymap.c VIA_MACROS and HARDCODED_MACROS"] --> hardcoded["macro_dispatch hardcoded slots"]
    keymap --> via_defaults["via_macro_defaults seeding"]
    action["Action lifecycle"] --> via_play["QMK VIA macro playback contract"]
    action --> hardcoded
    hardcoded --> payload["macro_payload parse/IR/playback"]
    via_defaults --> storage["QMK dynamic macro EEPROM"]
    via_command["VIA command"] --> via_split["qmk_via_split_sync compatibility"]
    via_split --> slave["Slave dynamic keymap/macro storage"]
    via_split --> rgb_invalidate["RGB layer map invalidation"]
```

Hardcoded macros are source-owned. VIA macros are QMK dynamic macro slots with
source-authored defaults and split mirroring.

Macro text is QMK ASCII, not arbitrary bytes: text positions accept
`0x01..0x7F`, while zero terminates a VIA slot. Bytes above `0x7F` remain valid
only where a complete QMK prefix command defines them as keycode operands. Both
authored and VIA decoders use the same predicate, and playback validates the
entire IR before its first text, wait, or key-ownership side effect. Invalid VIA
slots stay negatively cached until a VIA mutation invalidates the macro cache.

## Test Coverage Map

```mermaid
flowchart TD
    action["Action and macro dispatch"] --> action_tests["action, delayed_action, lifecycle, macro, VIA tests"]
    key_behavior["Authored key behavior"] --> behavior_tests["key_behavior, keymap, real_profile tests"]
    key_runtime["Key runtime reducer/planner/projection"] --> runtime_tests["release matrix, scenario, integration harness, modifier hold, layer lock"]
    pd["Pointing and PD runtime"] --> pd_tests["pd_mode, pd_runtime, handlers, key-runtime integration, pointer layer policy"]
    rgb["RGB rendering"] --> rgb_tests["rgb_validation, rgb_layer_render"]
    split["Split transport"] --> split_tests["split_runtime_sync, qmk_via_split_sync"]
    hooks["Hooks and boundaries"] --> boundary_tests["hook_chaining, feature_gate_compile, qmk_contract"]
    all["Whole userspace"] --> full["run_all_host_tests.sh and qmk compile"]
```
