# Userspace Architecture Review

Date: 2026-04-13

Status: new review created after
[2026-04-13-review-02](../2026-04-13-review-02/userspace-architecture-review.md).
This pass reviews the current live userspace after the earlier contract
cleanup work. Hardware is treated as fixed; this review is only about software
architecture, structure, and long-term extensibility.

Implementation status update:

- Recommendation 1 has landed: action dispatch now uses a tagged action-family
  contract instead of the earlier boolean-heavy descriptor.
- Recommendation 2 has landed: pd-mode writes now route through
  `pd_mode_apply_command(...)`, which owns mutation, lifecycle coordination,
  and split-sync intent.
- Recommendation 3 has its first implementation slice: slot / feedback /
  pending-multi-tap hold policy now consumes a shared resolved interaction
  contract.
- Recommendation 4 has landed: hardcoded authored macros now compile into a
  cached execution IR, while VIA default seeding stays on the encoded-string
  path.
- Recommendation 5 has its first implementation slice: the key-runtime
  scenario harness now exposes semantic builders for handled-key and pd-mode
  setup plus a runtime-debug snapshot helper, and scenario tests have started
  moving onto that seam.
- Recommendation 5 has a second implementation slice: process-record-driven
  integration tests now share a small semantic step/snapshot harness instead
  of rebuilding records, time advancement, and slot reads inline.
- The remaining open recommendation area is pushing more integration coverage
  onto those semantic builders beyond the current scenario-level slice.

Scope:

- `users/noah/` runtime architecture and module boundaries
- `keyboards/.../keymaps/noah/` as the authored data surface
- host-test and debug surfaces that protect or constrain refactors

Out of scope:

- hardware changes
- upstream QMK redesign
- style-only cleanup with no architectural payoff

## Executive Summary

This userspace now has a real architecture, not a pile of firmware hooks.
Four decisions are especially solid and should be preserved:

1. authored keymap data stays concentrated in `keymap.c`
2. shared runtime behavior lives under `users/noah/lib/`
3. the key engine runs through explicit transition plans and effect execution
4. source manifests, compat wrappers, and host tests are treated as
   architectural contracts

The biggest remaining risks are no longer directory layout problems. They are
contract-shape problems:

- action semantics are still represented as a bag of booleans instead of a
  stable action family model
- pd-mode reads are centralized through `pd_mode_snapshot()`, but pd-mode
  writes and side effects are still scattered
- key interaction is resolved centrally, but hold/release/feedback policy is
  still interpreted in several runtime modules
- hardcoded macros are still runtime-string-interpreted even though they are
  static authored data
- a large part of the host suite still couples itself to storage layout rather
  than only to semantic behavior

The codebase should scale well if the next refactors strengthen those seams.
It does not need a generic plugin system yet. It needs a few more typed
control-plane contracts.

## What Is Working Well

### 1. The authored-data boundary is real

`keymap.c` remains the clear authoring surface for:

- `VIA_MACROS(MACRO)`
- `HARDCODED_MACROS(MACRO)`
- `COMBOS(COMBO)`
- `key_behaviors[]`
- `keymaps[][]`

`noah_keymap.h` and `keymap_materialize.h` keep that surface discoverable
without leaking it into runtime modules. This is the right long-term shape for
fixed hardware with evolving software behavior.

### 2. The key runtime already uses an event/effect architecture

The key engine is not directly firing side effects from every reducer branch.
The current flow around:

- `key_runtime_transition_plan_t`
- `key_runtime_effect_t`
- `key_runtime_slot_step(...)`
- `key_runtime_transition_execute_plan(...)`

is a strong base. It is already the repo's most event-driven subsystem, and it
is the right architectural pattern for future key-behavior growth.

### 3. Compatibility and build ownership are explicit

Three files do particularly valuable architectural work:

- `users/noah/source_manifest.mk`
- `users/noah/rules.mk`
- `users/noah/lib/compat/qmk_contract.h`

They make source inclusion, fork-specific QMK assumptions, and build surfaces
discoverable in one place instead of leaving them implicit in scattered
includes and duplicated file lists.

### 4. PD mode has a useful read-side model now

The combination of:

- `pointing/defs/pd_mode_manifest.h`
- `pointing/defs/pd_mode_flags.h`
- `pointing/runtime/pd_mode_snapshot.c`

gives the repo a real read-only pd-mode contract. That is a meaningful
improvement over raw-flag consumers rediscovering mode state independently.

### 5. The test surface is unusually strong for firmware

The host suite is broad and intentional:

- subsystem runners are specific instead of generic
- `runtime_debug.h` exposes a useful aggregate snapshot
- `runtime_trace.h` gives a structured trace sink
- the key-runtime scenario harness supports sequence-level testing
- real-profile validation protects authored data, not only helpers

That is a major reason architectural refactors here are realistic.

## Findings

### 1. `noah_action_desc_t` is still a boolean bag, not a durable action taxonomy

`users/noah/lib/action/action_dispatch.h` moved the codebase forward, but the
current descriptor still carries semantics mostly as overlapping booleans:

- `is_layer_lock`
- `is_raw_qmk_layer_action`
- `is_layer_tap`
- `is_macro`
- `is_qmk_behavior_keycode`
- `is_keymap_custom`
- `is_pd_mode_lock`
- `is_owned_momentary_layer`

Consumers then reconstruct policy from different boolean subsets:

- `action_lifecycle.c`
- `handled_key.c`
- `key_behavior_lookup.c`
- `keymap_validation.c`
- `key_runtime_preflight.c`
- `key_runtime_slot_policy.c`

This is better than raw keycode classification everywhere, but it still leaks
policy through descriptor shape. It also allows invalid or irrelevant
combinations in principle, which means precedence still lives in consumer
control flow instead of in the descriptor itself.

Why this matters:

- adding a new action family still requires editing several boolean-based
  branches
- capability questions such as "press-only", "layer-owning", and
  "requires per-key hold ownership" are derived in multiple places
- higher layers still need to know too much about how action kinds overlap

Recommended direction:

- promote the descriptor from "fields + booleans" to "kind + capabilities"
- keep `layer` and `pd_mode` as typed payload, but make the family explicit
- let helpers answer capability questions from one normalized descriptor

Example shape:

```c
typedef enum {
    NOAH_ACTION_LITERAL,
    NOAH_ACTION_LAYER_LOCK,
    NOAH_ACTION_LAYER_HOLD,
    NOAH_ACTION_PD_MODE_HOLD,
    NOAH_ACTION_PD_MODE_LOCK,
    NOAH_ACTION_MACRO,
    NOAH_ACTION_QMK_BEHAVIOR,
    NOAH_ACTION_KEYMAP_CUSTOM,
    NOAH_ACTION_UNSUPPORTED_LAYER_ACTION,
} noah_action_kind_t;

typedef enum {
    NOAH_ACTION_CAP_PRESS_ONLY          = 1u << 0,
    NOAH_ACTION_CAP_REQUIRES_KEY_OWNER  = 1u << 1,
    NOAH_ACTION_CAP_LAYER_AFFECTING     = 1u << 2,
    NOAH_ACTION_CAP_PD_MODE_AFFECTING   = 1u << 3,
} noah_action_cap_t;

typedef struct {
    noah_action_kind_t kind;
    uint16_t           raw;
    uint16_t           caps;
    union {
        uint8_t        layer;
        pd_mode_mask_t pd_mode;
    } target;
} noah_action_desc_t;
```

This would make new action families cheaper to add than the current
"descriptor booleans plus consumer precedence" pattern.

### 2. PD-mode reads are centralized, but the write path is still fragmented

The read side is good:

- `pd_mode_snapshot()` computes effective local/display state
- `pointer_layer_policy.c`, `pd_runtime.c`, and RGB code can consume that view

The write side is still spread across several modules:

- `pd_mode_state.c` mutates active/locked flags and sometimes triggers sync
- `pd_mode_lifecycle.c` owns DPI, lifecycle hooks, auto-mouse anchoring, and
  key interception
- `action_lifecycle.c` triggers sync for lock actions
- `split_runtime_sync.c` mirrors remote state into the raw runtime store

That means one conceptual operation such as "activate this mode" is still
implemented as a multi-file convention rather than a single write contract.

Why this matters:

- adding a new shared pd-mode side effect means touching state, lifecycle, and
  often sync
- write-side invariants are harder to audit than read-side invariants
- the current `pd_mode_snapshot_t` helps consumers, but the code that produces
  new pd state is still procedural and distributed

Recommended direction:

- introduce one pd-mode command/apply seam
- make it responsible for state mutation, lifecycle hooks, DPI refresh, and
  sync intent
- keep `pd_mode_snapshot_t` as the read contract, but pair it with a write
  contract

Example shape:

```c
typedef enum {
    PD_MODE_COMMAND_KEY_PRESS,
    PD_MODE_COMMAND_KEY_RELEASE,
    PD_MODE_COMMAND_LOCK_TOGGLE,
    PD_MODE_COMMAND_REMOTE_SNAPSHOT,
} pd_mode_command_kind_t;

typedef struct {
    pd_mode_command_kind_t kind;
    uint16_t               keycode;
    pd_mode_mask_t         mode;
    pd_mode_mask_t         active_flags;
    pd_mode_mask_t         locked_flags;
} pd_mode_command_t;

typedef struct {
    pd_mode_snapshot_t before;
    pd_mode_snapshot_t after;
    bool               local_state_changed;
    bool               split_sync_required;
} pd_mode_apply_result_t;

pd_mode_apply_result_t pd_mode_apply_command(pd_mode_command_t command);
```

This would let the repo extend pd modes the same way the key runtime already
extends handled-key behavior: through a typed control-plane step instead of
distributed conventions.

### 3. Interaction resolution is centralized, but interaction policy is still spread across reducers

The earlier review work fixed an important problem: slot state now caches a
resolved handled-key view, and later tap counts no longer bypass the resolver.

The remaining issue is that several modules still interpret that interaction
view independently:

- `key_runtime_slot_policy.c` decides threshold behavior and owned-state
  changes
- `key_runtime_slot_pending_multi_tap.c` decides pending-hold release/scan
  semantics
- `key_runtime_feedback.c` decides when the interaction should stay visible
- `handled_key.c` still decides some default hold strategy and fallback rules

That means the codebase now has one resolved data contract but still multiple
policy interpreters.

Why this matters:

- adding a new hold semantic or interrupt policy will still fan out across
  several files
- feedback behavior and release behavior can drift if they keep interpreting
  the same interaction differently
- the runtime is still organized mainly by event phase instead of by resolved
  capability

Recommended direction:

- compute one "resolved interaction policy" object per active slot
- make it explicit whether the interaction:
  - emits on threshold
  - registers while held
  - emits on release
  - repeats while held
  - needs preview feedback
  - needs layer release before some actions

Example shape:

```c
typedef struct {
    bool threshold_dispatches;
    bool threshold_registers_held;
    bool release_dispatches;
    bool repeats_while_held;
    bool keep_feedback_visible;
    bool release_layer_before_action;
} resolved_hold_contract_t;

typedef struct {
    handled_key_view_t        interaction;
    resolved_hold_contract_t  hold;
    resolved_hold_contract_t  long_hold;
} resolved_interaction_policy_t;
```

Then `slot_policy`, `pending_multi_tap`, and `feedback` can consume the same
policy object instead of translating `hold_behavior_t` again in each module.

### 4. Hardcoded macros are still runtime-string interpreted instead of compiled authored data

The macro code is physically split well now:

- `macro_payload_parse.c`
- `macro_payload_run.c`
- `macro_payload_encode.c`
- `macro_payload_keycodes.c`

That was the right cleanup. The remaining architectural limitation is that
hardcoded macros are still represented only as strings and reinterpreted at
runtime:

- `keymap.c` authors macro strings
- `macro_dispatch.c` validates and dispatches those strings
- `macro_payload_run.c` reparses on playback
- `via_macro_defaults.c` re-encodes the same authored strings for VIA seeding

This is flexible, but it keeps the macro subsystem parser-first even for
static authored data.

Why this matters:

- hardcoded macros pay parse/validation complexity every time they run
- adding richer macro commands will keep touching parse, run, and encode paths
- debugging macro behavior still means reasoning from source strings instead of
  from a stable execution IR

Recommended direction:

- keep the string DSL as the authoring format
- add a compact intermediate representation for execution
- compile hardcoded macros once, while leaving VIA payload seeding on the
  encoded path

Example shape:

```c
typedef enum {
    MACRO_IR_TEXT,
    MACRO_IR_DELAY,
    MACRO_IR_KEY_DOWN,
    MACRO_IR_KEY_UP,
    MACRO_IR_TAP_LIST,
} macro_ir_opcode_t;

typedef struct {
    macro_ir_opcode_t op;
    uint16_t          arg0;
    uint16_t          arg1;
} macro_ir_op_t;
```

Practical path for this repo:

- hardcoded authored macros: parse once at startup or generate IR at build time
- VIA defaults: continue to encode the string form for EEPROM seeding
- runtime playback: execute IR, not source text

That would keep flexibility while reducing the structural cost of future macro
growth.

### 5. The host suite is excellent, but too much of it is white-box against storage layout

The repo has broad coverage, but a large number of tests still operate by
directly constructing or mutating runtime storage:

- `tests/host/key_runtime_slot_test.c`
- `tests/host/key_runtime_transition_test.c`
- `tests/host/key_runtime_feedback_test.c`
- `tests/host/runtime_debug_test.c`

Examples include direct assertions on:

- `interaction.view.*`
- `pending_multi_tap.*`
- `lifecycle.held_action_keycode`
- raw `noah_runtime_shared_state` fields

This is reasonable for low-level slot/storage tests. It becomes expensive when
integration-level tests also depend on those details.

Why this matters:

- architectural refactors require broad fixture rewrites even when behavior is
  preserved
- storage shape becomes a de facto public API for tests
- adding new behavior concepts increases the amount of setup boilerplate

Recommended direction:

- keep white-box tests for true storage-owning modules
- move more integration tests onto scenario-level and debug-snapshot-level
  builders
- treat `key_runtime_scenario_harness` as the seed of a higher-level semantic
  test DSL

Concrete next step:

- add small host-side builders for "pressable handled key", "pending multi-tap
  chain", and "pd mode state" so tests stop hand-constructing deep structs
- keep `runtime_debug.h` as the public snapshot seam for integration checks

## Modularity And Extensibility Assessment

### Adding new key behaviors

Adding new rows to `key_behaviors[]` is easy.

Adding new behavior families is still moderately expensive.

Today a new behavior concept that changes hold/release/feedback semantics would
still likely require edits in:

- `key_behavior.h`
- `handled_key.c`
- `key_runtime_slot_policy.c`
- `key_runtime_slot_pending_multi_tap.c`
- `key_runtime_feedback.c`
- corresponding host fixtures

The limiting factor is not folder layout. It is that the authored behavior
schema and the resolved runtime policy are still fairly closed.

### Adding new layers or modes

Layers are in good shape:

- layer ownership is explicit
- invalid raw layer actions are rejected
- lock vs momentary semantics are not mixed

PD modes are good for simple mode additions but only fair for shared-policy
growth. A new mode with a new trait or lifecycle rule still requires touching
more than the manifest row.

### Adding new feature families

The current design scales well for:

- more authored tap/hold variations
- more pd modes under existing policy
- more RGB consumers of current runtime state

It scales less well for:

- new action families
- new interaction semantics
- richer macro execution features

This is why I would not introduce a generic plugin framework yet. The repo
still benefits more from stronger typed contracts than from generalized
registration machinery.

## Abstractions And Interfaces

Strong abstractions:

- `noah_runtime.h` as the shared hook surface
- `source_manifest.mk` as a build contract
- `qmk_contract.h` as the fork-compat layer
- `key_runtime_transition_plan_t` / `key_runtime_effect_t`
- `pd_mode_snapshot_t`
- `runtime_debug.h`

Leaky abstractions:

- `noah_action_desc_t` because it still encodes semantics as overlapping
  booleans
- pd-mode mutation because read-side and write-side contracts are asymmetric
- hold semantics because multiple reducers still interpret the same interaction
  contract differently
- integration tests that depend directly on storage layout

## Code Organization And Structure

The directory layout is already good enough:

- `action/`, `compat/`, `macro/`
- `key/interaction`, `key/ownership`, `key/runtime`
- `pointing/defs`, `pointing/runtime`, `pointing/policy`, `pointing/modes`
- `state/ownership`, `state/runtime`
- `rgb/core`, `rgb/stages`, `rgb/automouse`

The main structural issues are now intra-module rather than top-level:

- action family semantics want a stronger descriptor contract
- pd-mode wants a stronger write controller
- key interaction wants a stronger resolved policy layer
- macro execution wants an IR instead of source strings at runtime

Naming and discoverability are mostly good. `source_manifest.mk`,
`keymap_materialize.h`, and the `compat/` layer help a lot.

## State Management And Flow

State ownership is mostly explicit now:

- key slots and feedback under `key/runtime`
- pd-mode raw flags under `state/runtime`
- layers under `state/ownership/layer_ownership.c`
- held actions and repeats under `key/ownership`
- keyboard mods under `state/ownership/keyboard_mod_ownership.c`

The remaining state-management issue is write-side coordination:

- pd-mode raw state is still mutated procedurally in several places
- key-runtime policy is still derived in several reducers from the same slot
  interaction

The solution is not more global state. The solution is fewer places that are
allowed to interpret or mutate the same semantics.

## Scalability Outlook

On this fixed board, runtime cost is not the main concern. Architectural
scalability is.

If the codebase keeps growing without new contract work, the likely bottlenecks
are:

1. action-family growth increasing boolean-descriptor branching
2. pd-mode feature growth increasing write-side fragmentation
3. new interaction semantics touching too many slot/policy/feedback modules
4. macro growth increasing parser/executor complexity
5. refactors slowing down because integration tests depend on storage layout

## Testing And Debuggability

What is strong today:

- direct subsystem runners
- full host suite
- feature-gate compile checks
- runtime trace ring buffer
- runtime debug snapshot
- scenario harness
- real-profile validation

What should improve next:

- move more integration tests to semantic helpers and scenario builders
- keep white-box tests local to storage-owning modules
- keep `runtime_trace.h` enum-based; avoid sliding back to stringly typed trace
  metadata elsewhere
- once pd-mode gains a write controller, add command/result tests at that seam

## Concrete Recommendations

### Recommendation 1: upgrade the action descriptor into a true action family contract

Goal:

- make action meaning explicit and stable
- stop encoding policy primarily as overlapping booleans

First targets:

- `users/noah/lib/action/action_dispatch.h`
- `users/noah/lib/action/action_lifecycle.c`
- `users/noah/lib/key/interaction/handled_key.c`
- `users/noah/lib/key/interaction/key_behavior_lookup.c`

### Recommendation 2: add a pd-mode write controller, not only a pd-mode snapshot

Goal:

- centralize local mutation, lifecycle side effects, and sync intent

First targets:

- `users/noah/lib/pointing/runtime/pd_mode_state.c`
- `users/noah/lib/pointing/runtime/pd_mode_lifecycle.c`
- `users/noah/lib/action/action_lifecycle.c`

### Recommendation 3: compute resolved hold policy once per slot interaction

Goal:

- make threshold/release/feedback behavior consume one policy object

First targets:

- `users/noah/lib/key/runtime/slot/key_runtime_slot_policy.c`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c`
- `users/noah/lib/key/runtime/key_runtime_feedback.c`

### Recommendation 4: introduce macro IR for static authored macros

Goal:

- keep the string DSL for authoring, but stop treating hardcoded authored
  macros as runtime source text

First targets:

- `users/noah/lib/macro/macro_dispatch.c`
- `users/noah/lib/macro/macro_payload_parse.c`
- `users/noah/lib/macro/macro_payload_run.c`
- `users/noah/lib/macro/via_macro_defaults.c`

### Recommendation 5: shift more host integration tests upward to semantic builders

Goal:

- preserve strong coverage while lowering refactor friction

First targets:

- `tests/host/key_runtime_scenario_harness.*`
- `tests/host/key_runtime_transition_test.c`
- `tests/host/key_runtime_slot_test.c`
- `tests/host/runtime_debug_test.c`

## Suggested Refactor Order

1. Strengthen `noah_action_desc_t` into a tagged action family contract.
2. Add a pd-mode write controller so state mutation and sync become coherent.
3. Introduce a resolved hold-policy object consumed by slot, multi-tap, and
   feedback reducers.
4. Add macro IR for hardcoded macros while preserving the current string DSL.
5. Move more integration tests onto scenario/debug-snapshot helpers.

## Bottom Line

This repo is already well past the point where "clean up the folders" is the
right advice. The important next work is contract work:

- a stronger action taxonomy
- a stronger pd-mode write model
- a stronger resolved hold-policy seam
- a stronger macro execution representation
- slightly less test dependence on raw storage shape

If those land, the userspace should stay maintainable as it grows richer,
without needing a premature plugin architecture.
