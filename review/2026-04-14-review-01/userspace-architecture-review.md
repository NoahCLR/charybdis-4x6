# Userspace Architecture Review

Date: 2026-04-14

Status: fresh architecture review of the live `users/noah/` userspace after
the prior review lineage in `review/2026-04-13-review-07/`. This pass focuses
only on software structure, runtime boundaries, and long-term extensibility for
the fixed Charybdis 4x6 hardware.

Scope:

- `users/noah/` runtime architecture, interfaces, and ownership boundaries
- the authored-data boundary in
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/`
- host-test and debug surfaces where they define the real contract

Out of scope:

- hardware changes
- replacing QMK
- upstream fork redesign outside this repo

## Executive Summary

The userspace is already past the "custom firmware pile of hacks" stage. The
authored keymap/runtime split is real, the handled-key engine is built around
explicit slot state and deferred effects rather than ad hoc hook branching, pd
mode has a real command/state layer, and the host test surface is good enough
to support refactors without flying blind.

The main architectural risk is now narrower and more subtle: the runtime is
well organized by directories, but several extension seams still terminate in
shared policy code rather than mode-owned or behavior-owned contracts. The
result is a codebase that is maintainable today, but where adding genuinely new
interaction classes still means editing multiple core modules in lockstep.

If the goal is long-term extensibility, the next step is not a rewrite. The
right move is to keep the existing state-machine/effect-plan core and refactor
the remaining semantic choke points into explicit contracts:

1. make key-behavior hold semantics additive instead of cross-module
2. make handled-key resolution context explicit instead of hidden in globals
3. replace whole-table coordination passes with tiny explicit registries
4. sync pd-mode identity directly instead of reconstructing it from flags
5. unify macro sources behind one source/cache abstraction

## What Is Working Well

- The authored/runtime split is real. `users/noah/noah_keymap.h` is an
  authoring bundle, while runtime modules consume narrower headers such as
  `noah_runtime.h` and `noah_keymap_ids.h`.
- The build surface is disciplined. `users/noah/source_manifest.mk` and
  `users/noah/rules.mk` keep runtime sources centralized instead of scattering
  file lists through tests and keymap glue.
- The handled-key runtime is conceptually strong. Slot reducers emit ordered
  effects, and transition execution is separated from decision-making.
- Pd mode is no longer a hook soup. `pd_mode_apply_command(...)` is a real
  state-application surface rather than open-coded press/release branching.
- Testability is materially better than average firmware code. The repo has
  targeted host suites by subsystem plus `runtime_debug` and `runtime_trace`
  surfaces for higher-level state assertions.

## Priority Assessment

- Architecture & separation of concerns: good overall, but handled-key and
  pd-mode extension points still concentrate policy in shared runtime code.
- Modularity & extensibility: new authored data is easy; new semantics are not.
- Abstractions & interfaces: several interfaces are meaningful, but
  `handled_key` and split pd-mode display state still leak implementation
  details.
- Code organization & structure: directory structure is strong; a few files are
  becoming multi-responsibility navigation bottlenecks.
- State management & flow: explicit slot phases and pd-mode commands are good;
  cross-key coordination still depends on implicit global scans.
- Scalability: runtime cost is fine for fixed hardware; the main scaling risk is
  semantic sprawl and cross-module edit breadth.
- Testing & debuggability: strong host coverage and state snapshots; future
  refactors should add more contract-level tests, not fewer.

## Findings

### High Priority

#### 1. Key-behavior semantics are still encoded as a cross-module policy matrix, not one extensible contract

References:

- `users/noah/lib/key/interaction/handled_key.h:37-131`
- `users/noah/lib/key/interaction/handled_key.c:56-76`
- `users/noah/lib/key/interaction/handled_key.c:238-251`
- `users/noah/lib/key/runtime/key_runtime_interaction.h:98-181`
- `users/noah/lib/key/runtime/key_runtime_feedback.c:58-166`

Why this matters:

- The action vocabulary is now reasonably centralized in
  `action_dispatch.h`, but hold semantics are still spread across:
  - handled-key hold contract derivation
  - fallback/implicit-hold inference
  - release-contract construction
  - RGB feedback policy
- Adding a new hold style, changing how an existing style releases, or adding a
  new feedback mode still requires synchronized edits in several core files.
- This is the main reason "add a new key behavior" is still harder than "add a
  new authored row".

Concrete example:

- `handled_key_hold_contract_for_behavior(...)` decides threshold behavior,
  held-lifecycle reuse, preview layers, and feedback retention.
- `key_runtime_slot_release_contract_build(...)` independently decides how taps
  and release-time holds are selected.
- `key_feedback_pack_for_slot(...)` separately reconstructs whether the same
  hold should remain visible, flash, or stay quiet.

That is one semantic concept expressed three times.

Current extensibility impact:

- New tap/hold behavior: high-effort, core-runtime change
- New layer or pd-mode action using existing hold semantics: low-to-medium
- Tap-dance-like behavior with new release rules: high-effort, because it will
  have to thread through the same policy matrix

Recommended direction:

- Keep the existing state machine.
- Introduce one behavior-owned contract object that materializes everything the
  runtime needs for a resolved branch:
  - threshold kind
  - threshold action
  - release behavior
  - feedback profile
  - preview-layer behavior
  - held-lifecycle reuse rules
- Make `handled_key.c` produce that contract once, then let release/feedback
  read it rather than re-deriving policy.

Example shape:

```c
typedef struct {
    key_runtime_threshold_kind_t threshold_kind;
    uint16_t                     threshold_action;
    uint16_t                     release_action;
    bool                         uses_held_lifecycle;
    bool                         keeps_pending_feedback;
    bool                         keeps_registered_feedback;
    uint8_t                      preview_layer;
} noah_hold_semantics_t;

typedef struct {
    uint16_t                 tap_action;
    uint8_t                  tap_repeat_count;
    noah_hold_semantics_t    hold;
    noah_hold_semantics_t    long_hold;
    key_runtime_release_mode release_mode;
} noah_behavior_contract_t;
```

That makes new behavior kinds additive instead of cross-cutting.

#### 2. Handled-key resolution hides critical runtime context behind lookup helpers

References:

- `users/noah/lib/key/interaction/handled_key.c:122-235`
- `users/noah/lib/key/interaction/handled_key.c:178-225`
- `users/noah/lib/key/interaction/handled_key.c:380-427`

Why this matters:

- `handled_key_lookup(...)` looks like a pure authored-data lookup, but the
  `*_at_position(...)` helpers depend on:
  - global `layer_state`
  - physical key position
  - `keycode_at_keymap_location(...)`
  - the current active-layer stack
- That makes the interface leaky. A caller reading the header sees "resolution"
  accessors, but the real answer is "resolution plus implicit runtime context".
- This matters most for `KC_TRNS` handling and same-position fallthrough logic,
  where the current layer stack is effectively a hidden input parameter.

Architectural cost:

- Harder to reason about resolution purity
- Harder to unit-test the resolution layer without full global setup
- Harder to introduce new context sources later, such as modal overlays or
  per-origin metadata, because the context is implicit rather than typed

Recommended direction:

- Split authored resolution from contextual materialization.
- Keep `handled_key_lookup(...)` as the authored-table lookup.
- Add an explicit resolution context for layer stack and key position, then
  materialize transparent-source fallthrough from that context.

Example shape:

```c
typedef struct {
    layer_state_t active_layers;
    keypos_t      key_pos;
    int8_t        origin_layer;
} handled_key_resolution_ctx_t;

typedef struct {
    handled_key_resolution_t authored;
    uint16_t                 tap_action;
    hold_behavior_t          hold;
    hold_behavior_t          long_hold;
    uint8_t                  resolved_layer;
    pd_mode_mask_t           resolved_pd_mode;
    uint16_t                 resolved_flags;
} handled_key_materialized_t;
```

This would make the contextual dependency explicit and shrink the surface area
of `handled_key.c`.

#### 3. Cross-key coordination still depends on whole-table sweeps and broadcast interrupts

References:

- `users/noah/lib/key/runtime/key_runtime_preflight.c:40-78`
- `users/noah/lib/key/runtime/key_runtime_transition.c:144-215`
- `users/noah/lib/key/runtime/key_runtime_feedback.c:76-199`
- `users/noah/lib/key/runtime/slot/key_runtime_slot.c:173-192`
- `users/noah/lib/key/runtime/key_runtime.c:11-29`

Why this matters:

- On this fixed 24-key board, the raw runtime cost is acceptable. The problem
  is architectural, not performance-critical.
- Several important decisions still work by sweeping the entire slot table:
  - "is some other slot active?"
  - "does any unrelated pending multi-tap exist?"
  - "which slot owns preview feedback?"
  - "scan every active slot, then every pending multi-tap slot"
  - "activate the first pending fallback hold"
- Every new cross-key rule is likely to become another sweep plus another
  implicit convention about which scan or preflight pass owns the answer.

Why this becomes expensive later:

- Chorded rules, combo-aware interruption policy, or per-hand arbitration would
  all need more global passes.
- The ownership model becomes harder to observe because "active" and "pending"
  membership is derived from scans instead of being represented explicitly.
- Feedback and coordination code stay coupled to slot storage shape instead of
  depending on a narrower registry contract.

Recommended direction:

- Keep the fixed slot table for state storage.
- Add tiny explicit registries in shared state for:
  - active slot indices
  - pending multi-tap slot indices
  - preview-layer owner
  - pending fallback-hold owner
- Update those registries on slot lifecycle changes, then let preflight,
  feedback, and scan consume the registries instead of sweeping the raw table.

Example shape:

```c
typedef struct {
    uint8_t active_slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t active_slot_count;
    uint8_t pending_multi_tap_slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t pending_multi_tap_count;
    uint8_t preview_owner_slot;
    uint8_t pending_fallback_slot;
} key_runtime_index_state_t;
```

This would keep the current slot model but make cross-key coordination a first
class contract instead of an emergent property of repeated loops.

#### 4. Pd-mode state is exclusive locally, but split sync and display precedence are still flag-oriented

References:

- `users/noah/lib/pointing/policy/pd_mode_policy.h:12-33`
- `users/noah/lib/pointing/runtime/pd_mode_state.c:98-110`
- `users/noah/lib/pointing/runtime/pd_mode_state.c:198-243`
- `users/noah/lib/state/runtime/split_runtime_sync.h:16-22`
- `users/noah/lib/pointing/runtime/pd_mode_registry.c:20-55`
- `users/noah/lib/pointing/runtime/pd_mode_registry.c:74-80`

Why this matters:

- The runtime behaves like "one effective active mode plus one effective locked
  mode".
- The sync packet still ships `pd_mode_flags` and `pd_mode_locked_flags`.
- Remote display state then reconstructs one effective mode by picking the
  first matching manifest row.
- That means display precedence is still partly a function of registry order,
  even though exclusivity is now an explicit state-machine rule.

Architectural consequences:

- Adding overlay modes, display-only priorities, or half-local UI rules will
  require changing both state and policy helpers.
- Registry order remains a hidden contract for remote display behavior.
- `pd_mode_registry.c` still owns one shared auto-mouse lifecycle object,
  which is better than scattered hooks but still means some mode semantics are
  not fully mode-owned.

Recommended direction:

- Transport explicit identity in split sync:
  - `active_mode_id`
  - `locked_mode_id`
  - optional `display_mode_id` if local/display rules diverge later
- Keep flags only if they are still needed for secondary traits or analytics.
- Add explicit display priority to the manifest only if multiple-mode display
  semantics are intentionally supported in the future.

Example shape:

```c
typedef struct __attribute__((packed)) {
    uint16_t automouse_progress;
    uint8_t  active_mode_id;
    uint8_t  locked_mode_id;
    uint8_t  key_feedback_flags;
    uint8_t  key_preview_layer;
} split_runtime_sync_packet_t;
```

That would align the transport with the actual state model the runtime already
uses locally.

### Medium Priority

#### 5. The macro subsystem shares one IR, but not one source/cache abstraction

References:

- `users/noah/lib/macro/macro_dispatch.c:11-82`
- `users/noah/lib/macro/via_macro_defaults.c:48-148`
- `users/noah/lib/compat/qmk_contract.c:38-68`
- `users/noah/lib/macro/macro_payload.h:29-38`

Why this matters:

- The repo does have a useful common IR in `macro_payload_ir_t`.
- But the three macro surfaces still behave as three pipelines:
  - hardcoded macros compile and cache IR eagerly per slot
  - VIA default macros validate/encode authored text for EEPROM seeding
  - live VIA macros read QMK storage, decode the dynamic stream, then play it
- Any future change to validation, caching, metrics, or slot ownership will
  need parallel edits across those pipelines.

Extensibility impact:

- Adding a new macro source is harder than it should be.
- Adding cache invalidation or observability for macro playback will duplicate
  source-specific logic.
- The macro layer is reusable in pieces, but it is not yet one stable source
  abstraction.

Recommended direction:

- Add a `macro_source_t` or `macro_slot_provider_t` interface above the IR:
  - validate slot
  - materialize IR for slot
  - optionally seed defaults
  - invalidate cache when backing storage changes
- Keep hardcoded macros, VIA defaults, and live VIA playback as providers over
  the same contract.

Example shape:

```c
typedef struct {
    bool (*validate)(uint8_t slot);
    bool (*load_ir)(uint8_t slot, macro_payload_ir_t *out);
    void (*invalidate)(uint8_t slot);
} macro_slot_provider_t;
```

That would make macro sources extensible without changing macro playback code.

#### 6. A few modules are becoming mixed-responsibility navigation bottlenecks

Largest current files in the live userspace:

- `users/noah/lib/key/interaction/handled_key.c` at 490 lines
- `users/noah/lib/macro/macro_payload_run.c` at 473 lines
- `users/noah/lib/macro/macro_payload_keycodes.c` at 404 lines
- `users/noah/lib/pointing/runtime/pd_mode_state.c` at 331 lines
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c` at
  351 lines

Assessment:

- The directory layout is still good, so this is not a repo-wide organization
  problem.
- The issue is discoverability inside a few important files:
  - `handled_key.c` mixes fallback-hold policy, transparent lookup, default tap
    behavior, and contextual materialization
  - `macro_payload_run.c` mixes runtime execution, IR writing helpers, and QMK
    dynamic-stream decoding
  - `pd_mode_state.c` mixes command application, snapshot comparison, and split
    sync triggering rules

Recommended direction:

- Split by responsibility, not by arbitrary size:
  - `handled_key_defaults.c`, `handled_key_transparency.c`,
    `handled_key_materialize.c`
  - `macro_payload_decode_qmk.c` separate from payload runtime execution
  - `pd_mode_sync.c` or `pd_mode_apply.c` if pd-mode state grows again

This is secondary to the contract changes above, but it will improve
maintainer speed immediately.

## Testing And Debuggability

Current assessment:

- Strong: the host suite is broad and aligned to subsystem boundaries.
- Strong: `runtime_debug` gives whole-userspace snapshots for tests.
- Strong: `runtime_trace` gives a small cross-subsystem causal trace without
  requiring console logging everywhere.
- Risk: the trace schema is intentionally tiny, so it is best at confirming a
  suspected flow, not at replacing richer state contracts.

Recommended improvements:

- When refactoring the hold contract, add contract-level tests that assert one
  materialized behavior contract rather than only end-to-end runtime outcomes.
- If registry-based key-runtime coordination lands, add tests that assert
  registry membership transitions directly.
- If pd-mode sync moves to identity transport, add tests that assert sync
  packet semantics independent of registry order.

## Concrete Refactor Sequence

Recommended order:

1. Extract one materialized behavior contract for hold/release/feedback.
2. Make handled-key contextual resolution explicit through a typed context.
3. Add key-runtime coordination registries while preserving the existing slot
   table and effect executor.
4. Change pd-mode split transport from flags to explicit effective identity.
5. Add a macro source/provider abstraction over hardcoded, VIA-default, and
   live VIA-backed slots.

This ordering keeps each step local, keeps the current runtime architecture in
place, and reduces the number of core files touched when the next real feature
arrives.
