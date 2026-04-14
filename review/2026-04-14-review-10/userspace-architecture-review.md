# Userspace Architecture Review

Date: 2026-04-14

Status: deep architecture review of the current `charybdis-4x6` userspace with
focus on software structure, extension seams, and long-term maintainability for
fixed hardware.

Scope:

- `users/noah/` runtime architecture and ownership boundaries
- authored keymap data surfaces under
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/`
- extension cost for new behaviors, layers, modes, and runtime features
- state management, testing, and debug surfaces

Out of scope:

- hardware changes
- switch/trackball physical redesign
- upstream QMK architecture outside the local userspace unless it directly
  shapes a userspace boundary

## Executive Summary

This userspace is substantially more disciplined than a typical QMK keymap.
The strongest parts of the design are:

- the hard boundary between authored keymap data and reusable runtime code
- the reducer/effect-plan shape inside the handled-key engine
- the pd-mode manifest and command/snapshot split
- the unusually strong host-test and compile-gate coverage

The main long-term risks are not correctness bugs in the current tree. They are
architecture scaling risks:

1. runtime state ownership now has a canonical singleton context, but
   `runtime_shared_state.h` still exists as a compatibility shim and should not
   regain status as a primary runtime surface
2. action-kind identity, classification, metadata, dispatch, and most handled-
   key runtime policy are now registry-backed, but a thin layer of explicit
   authored-resolution overrides and transparent-field extraction still lives
   in local handled-key code
3. hook-level orchestration is centralized and order-sensitive, so adding new
   subsystems still means editing core pipelines instead of registering
   capabilities
4. the authored keymap surface is intentionally data-only, but too much of that
   data still lives in one large `keymap.c`, which will get harder to review
   and evolve as behavior count grows

## What Is Already Working Well

### Strong runtime/data boundary

The repo-specific boundary between authored keymap data and shared runtime code
is real, not aspirational:

- `users/noah/noah_runtime.h:1-25`
- `users/noah/noah_keymap.h:1-17`
- `tests/host/run_feature_gate_compile_tests.sh:12-30`

That is one of the highest-value design decisions in the repo. It keeps the
QMK hook surface narrow and stops keymap-owned translation units from reaching
back into runtime internals.

### The handled-key engine is a real state machine

The key runtime is not just ad hoc `process_record_user()` branching. The slot
engine has:

- an event seam:
  `users/noah/lib/key/runtime/slot/key_runtime_slot_step.h:1-34`
- transition planning:
  `users/noah/lib/key/runtime/key_runtime_transition.c:70-121`
- explicit effect execution:
  `users/noah/lib/key/runtime/key_runtime_transition.c:78-120`

That is the correct direction for tap/hold, multi-tap, owned-layer, and repeat
behavior. Preserve this shape.

### Pd modes are the best extensibility model in the tree

`users/noah/lib/pointing/defs/pd_mode_manifest.h:5-10` and `:68-74` show the
cleanest additive surface in the codebase. New pointing modes are defined once,
then projected into ids, registry rows, traits, and lock actions. That pattern
is the best local template for other extensibility work.

### Test and compile discipline is strong

`tests/host/run_all_host_tests.sh:7-44` and
`tests/host/run_feature_gate_compile_tests.sh:12-58` give this userspace much
better regression resistance than most firmware codebases. The tests are broad
enough that architecture work can be done safely if the seams stay coherent.

### Runtime state now has a canonical owner

Milestone 1 of the runtime-state refactor is now landed:

- `users/noah/lib/state/runtime/runtime_context.h:1-57`
- `users/noah/lib/state/runtime/runtime_shared_state.h:16-32`
- `users/noah/lib/state/runtime/runtime_shared_state.c:7-38`
- `users/noah/lib/state/runtime/runtime_debug.c:118-226`

The runtime singleton now owns the shared key/pd aggregate, layer ownership,
held-action ownership, held-repeat ownership, keyboard modifier ownership, and
the runtime trace ring. The old public module APIs still exist, but they now
forward into that one owner instead of maintaining separate mutable statics.

### Action kinds now have a single source of truth

The action system has also moved in the right direction:

- `users/noah/lib/action/action_kind_registry_list.h:1-83`
- `users/noah/lib/action/action_kind.c:16-190`
- `users/noah/lib/action/action_kind_dispatch.c:19-248`

Enum identity, classification priority, metadata, dispatch ops, fallback-hold
eligibility, default tap routing, and descriptor-layer source semantics are now
generated from one registry list instead of being maintained as separate,
manually synchronized tables. That makes the action surface much closer to the
manifest-driven pd-mode pattern that was already the cleanest extensibility
surface in the tree.

## Findings

### 1. Milestone 1 is landed and the first Milestone 2 boundary cleanup is done, but the compatibility shim still needs to stay narrow

Severity: should-fix

References:

- `users/noah/lib/state/runtime/runtime_context.h:1-57`
- `users/noah/lib/state/runtime/runtime_shared_state.h:16-30`
- `users/noah/lib/state/runtime/runtime_shared_state.c:7-42`
- `users/noah/lib/state/runtime/runtime_debug.c:118-226`
- `users/noah/lib/pointing/runtime/pd_mode_state.c:13-20`
- `users/noah/lib/pointing/runtime/pd_mode_snapshot.c:45-53`

What I observed:

- runtime-owned mutable state now lives under one
  `noah_runtime_context_t` singleton
- the old `noah_runtime_shared_state` name is now a compatibility alias into
  that context rather than an independent owner
- `noah_runtime_debug_snapshot()` and
  `noah_runtime_context_reset_for_test()` now work over the context directly
  instead of assembling or resetting state by calling each subsystem manually
- repo-owned pd-mode runtime code no longer reads the compatibility alias
  directly; it now goes through `pd_mode_runtime_shared_state()`
- the remaining concern is policy, not mechanism: keep new code on the slice
  accessors and avoid letting the compatibility shim expand again

Why this matters:

- Milestone 1 removed the biggest scaling tax: new runtime-owned subsystems no
  longer need their own private static store plus bespoke debug/reset wiring.
- The remaining cost is boundary drift risk. If new runtime code keeps using
  the compatibility alias instead of slice accessors, the old ownership model
  will quietly grow back around the new context.
- That matters because Milestone 1 only pays off long term if the compatibility
  layer stays visibly secondary to the runtime context and its slice helpers.

Recommended direction:

- `runtime_shared_state.h` should become an explicitly temporary compatibility
  shim, not a surface that new code treats as a primary owner.
- New runtime code should reach shared key/pd state through the context-backed
  helpers and let only the compatibility layer expose the legacy alias.

### 2. The action-kind core now owns most handled-key runtime policy, but a few handled-key heuristics are still local

Severity: should-fix

References:

- `users/noah/lib/action/action_dispatch.h:59-177`
- `users/noah/lib/action/action_kind_registry_list.h:1-83`
- `users/noah/lib/action/action_kind.c:16-190`
- `users/noah/lib/action/action_kind_dispatch.c:19-248`
- `users/noah/lib/key/interaction/key_behavior_lookup.c:46-58`
- `users/noah/lib/key/interaction/key_behavior_lookup.c:120-146`
- `users/noah/lib/key/interaction/handled_key_policy.h:30-89`
- `users/noah/lib/key/interaction/handled_key_defaults.c:41-88`
- `users/noah/lib/key/interaction/handled_key_transparency.c:34-43`
- contrast:
  `users/noah/lib/pointing/defs/pd_mode_manifest.h:5-10`
- contrast:
  `users/noah/lib/pointing/defs/pd_mode_manifest.h:68-74`

What I observed:

- action kinds are now rooted in one definition list,
  `action_kind_registry_list.h`, which fans out into:
  - the enum in `action_dispatch.h`
  - the metadata and classification table in `action_kind.c`
  - the dispatch-op table in `action_kind_dispatch.c`
- the registry now also owns policy flags for:
  - direct runtime-handled keycodes
  - momentary-layer keycodes
  - authored layer-tap contracts
  - release-layer-before-action behavior
  - press-and-hold held-lifecycle behavior
  - fallback-hold eligibility
  - default tap extraction for literal and layer-tap actions
  - descriptor-layer source semantics for layer-hold and layer-tap actions
- downstream action policy still exists in:
  - explicit authored-resolution momentary-layer and layer-tap override
    handling in `handled_key_transparency.c`
  - field-level transparent-source extraction in
    `handled_key_transparency.c`
  - a small amount of lookup glue in `key_behavior_lookup.c`
- by comparison, pd modes use a single manifest row that fans out into the rest
  of the subsystem

Why this matters:

- The registry change removed a real scaling tax: enum membership,
  classification order, metadata, and dispatch ops no longer need manual
  synchronization across parallel core files.
- The latest pass removed another real scaling tax: handled-key runtime code no
  longer needs to know that “layer lock means release the momentary layer first”
  or that “pd-mode hold is directly handled” by hard-coded kind checks.
- The latest pass removed another narrow but real tax: handled-key defaults no
  longer maintain their own pure-modifier list or default-tap extraction
  logic, and transparent tap-source checks now reuse descriptor-backed default
  tap semantics.
- The remaining cost is now much narrower and more localized. It mostly lives
  in downstream handled-key code as authored-resolution overrides and
  transparent-field extraction rather than as open-coded action-family
  branching.

Practical examples of features that would feel expensive under the current
design:

- a new action class with custom hold-preview behavior
- a new emitted-action family with its own press/release lifecycle
- a richer declarative behavior like “chord window” or “deferred one-shot”
  without reusing existing action categories

Recommended direction:

- Keep `action_kind_registry_list.h` as the root and extend it one step
  further.
- Each action-kind spec should eventually own:
  - classification predicate
  - authored capabilities
  - dispatch hooks
  - hold-preview / feedback policy hooks
- Then decide whether the last authored-resolution and transparent-field
  helpers belong in the registry surface or whether they should stay local as
  intentionally non-action-family policy.
- That lets the next action-family addition be “add a row” much more often than
  “edit three policy files and re-derive behavior by hand”.

Example direction:

```c
typedef struct {
    noah_action_kind_t kind;
    bool (*matches)(uint16_t action, noah_action_desc_t *out);
    uint16_t caps;
    noah_action_kind_dispatch_ops_t ops;
    noah_action_policy_hooks_t policy;
} noah_action_kind_spec_t;

#define NOAH_ACTION_KIND_LIST(X) \
    X(LITERAL, literal_spec) \
    X(LAYER_LOCK, layer_lock_spec) \
    X(LAYER_HOLD, layer_hold_spec) \
    X(MACRO, macro_spec) \
    X(PD_MODE_LOCK, pd_mode_lock_spec)
```

The pd-mode manifest is the local proof that this repo already benefits from
single-source definition tables. Reusing that pattern here would remove the
largest “modify core logic to add capability” bottleneck in the userspace.

### 3. Hook-level orchestration is centralized and order-sensitive

Severity: should-fix

References:

- `users/noah/lib/key/runtime/key_runtime_process.c:44-156`
- `users/noah/runtime_init.c:28-40`
- `users/noah/lib/rgb/core/rgb_runtime.c:23-64`

What I observed:

- `noah_process_record_user()` is a hard-coded ordered stage pipeline
- `noah_matrix_scan_user()` and `noah_keyboard_post_init_user()` manually
  sequence subsystem work
- RGB rendering order is also centrally hard-coded in one function

Why this matters:

- The order is semantically important, but the order is encoded as imperative
  code in several different places.
- New subsystem insertion becomes risky because behavior depends on exact
  placement relative to existing stages.
- This means adding a feature like a new event interceptor, a tracing adapter,
  a mode-local preflight rule, or a new RGB overlay still requires editing
  central orchestration code instead of declaring participation.

Why the current key runtime does not fully solve this:

- Inside the handled-key engine, you already have a good reducer/effect-plan
  architecture.
- Outside that engine, the top-level userspace still behaves like a hand-wired
  chain of privileged modules.

Recommended direction:

- Introduce small compile-time hook registries for:
  - process-record phases
  - matrix-scan tasks
  - post-init tasks
  - RGB render stages
- Keep ordering explicit, but make it data-driven rather than open-coded.

Example direction:

```c
typedef enum {
    NOAH_PHASE_SYNTHETIC,
    NOAH_PHASE_PREFLIGHT,
    NOAH_PHASE_MODE_INTERCEPT,
    NOAH_PHASE_HANDLED_KEY,
    NOAH_PHASE_DIRECT_ACTION,
    NOAH_PHASE_MACRO,
} noah_process_phase_t;

typedef struct {
    noah_process_phase_t phase;
    key_runtime_process_stage_fn_t fn;
} noah_process_stage_spec_t;
```

This does not need a heavyweight event bus. A small declarative table would be
enough. The important improvement is that extending the pipeline stops meaning
“edit the core owner function and manually reason about every surrounding
stage”.

### 4. The authored keymap surface is data-driven, but too consolidated

Severity: optional but worthwhile

References:

- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c:5-10`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c:41-140`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c:142-500`
- `users/noah/noah_keymap.h:5-17`

What I observed:

- one translation unit owns:
  - custom keycodes
  - VIA macro defaults
  - hardcoded macro payloads
  - combos
  - all key behaviors
  - all layer layouts
- the file is still data-only, which is good, but it is carrying too many
  distinct authoring concerns in one place

Why this matters:

- Changes to macros, combos, and layer layouts all collide in one file
  structurally and in review
- discoverability is lower than it should be for a supposedly declarative
  authoring surface
- adding new layers or large behavior tables will keep increasing diff noise

Recommended direction:

- Keep the authored-data boundary exactly where it is today, but split the data
  itself into domain files or `.inc` fragments.
- One reasonable structure:
  - `keymap_data/custom_keycodes.inc`
  - `keymap_data/via_macros.inc`
  - `keymap_data/hardcoded_macros.inc`
  - `keymap_data/combos.inc`
  - `keymap_data/key_behaviors.inc`
  - `keymap_data/layers.inc`
- Keep `MATERIALIZE_KEYMAP_DATA()` in one small assembly TU so the runtime
  symbols remain easy to find.

This is not about style. It is about reducing the size of the “one file you
must touch for every kind of authored change”.

## Requested Priority Assessment

### 1. Architecture & Separation of Concerns

Assessment:

- Good inside the handled-key runtime and pd-mode subsystem.
- Less good at the whole-userspace level because state and orchestration remain
  partly centralized and partly hidden.

Most important architectural boundary that should be preserved:

- `noah_runtime.h` vs `noah_keymap.h`

Most important architectural boundary that should improve:

- keep the runtime context as the only real state owner and avoid drifting back
  toward direct compatibility-alias usage

### 2. Modularity & Extensibility

Adding a new pd mode:

- relatively good today because of the manifest-driven design

Adding a new key behavior or action family:

- still invasive because action classification, capability metadata, dispatch,
  and hold semantics are spread across multiple core files

Adding a new hook-level subsystem:

- still requires editing top-level orchestration in `runtime_init.c`,
  `key_runtime_process.c`, and sometimes `rgb_runtime.c`

### 3. Abstractions & Interfaces

Strong abstractions:

- hook-entry boundary in `noah_runtime.h`
- authored-data boundary in `noah_keymap.h`
- pd-mode command/snapshot split

Leaky abstractions:

- the compatibility `runtime_shared_state` surface is still visible enough that
  new code could regress toward aggregate access if the boundary is not kept
  narrow
- action-kind abstractions are much healthier now, but buffered-modifier
  fallback and field-level transparent-source policy still sit outside the
  registry-backed descriptor surface

### 4. Code Organization & Structure

Good:

- domain-oriented layout under `users/noah/lib/`
- slot runtime decomposition is consistent
- compat surfaces are centralized under `users/noah/lib/compat/`

Needs improvement:

- `keymap.c` is still too dense as an authored-data home
- top-level orchestration lives in small files, but each one owns too much
  privilege over module ordering

### 5. State Management & Flow

Good:

- key runtime slot phases are explicit
- pd-mode commands and snapshots make transitions readable

Risky:

- the runtime context solved the old “private mutable statics everywhere”
  problem, but the compatibility alias still needs to remain secondary or that
  ownership model will drift back
- action lifecycle policy is mostly registry-backed now, but buffered-modifier
  fallback and field-level transparent-source rules still live in downstream
  handled-key code

### 6. Scalability of the Design

Likely to scale well:

- new authored rows in existing key-behavior/pd-mode patterns
- more tests
- more slot-level key-runtime nuance

Likely bottlenecks:

- new action kinds
- new hook-level subsystems
- more cross-cutting ownership/state modules
- further growth of the monolithic authored keymap TU

### 7. Testing & Debuggability

Assessment:

- test coverage is a major strength
- compile gates correctly enforce header boundaries and feature wiring

Specific improvement area:

- move from “global reset plus broad debug snapshot” toward module-local query
  seams backed by one explicit runtime context
- keep `runtime_trace` small and structured; it is the right debugging style

## Concrete Refactoring Sequence

If I were sequencing architecture work here, I would do it in this order:

1. Keep the runtime context as the only primary state owner and continue
   narrowing the compatibility shim so new code cannot drift back onto legacy
   aggregate access.
2. Finish pulling the remaining buffered-modifier fallback and transparent-
   source edge-case policy into the action registry so per-kind behavior is as
   declarative as the current design allows.
3. Replace top-level imperative hook ordering with small declarative stage
   tables for process-record, scan, post-init, and RGB render.
4. Split the authored keymap data into smaller data-only files without moving
   runtime logic back into the keymap layer.

That order matters:

- state unification already reduced future cross-module wiring cost
- the landed action registry removed the worst synchronization debt, and the
  next pass should finish pulling the remaining handled-key policy into that
  same declarative surface
- pipeline registration then becomes easier because subsystems have cleaner
  interfaces
- authoring-file decomposition is valuable, but it is lower risk and can land
  independently after the runtime seams improve

## Verification

Commands run for this review:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- full host suite passed
- firmware build passed and produced `.build/bastardkb_charybdis_4x6_noah.uf2`

Workspace scope:

- no sibling workspace folders were edited
