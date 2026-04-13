# Userspace Architecture Review

Date: 2026-04-13

Status: new review created after
[2026-04-13-review-01](../2026-04-13-review-01/userspace-architecture-review.md).
This pass reviews the live userspace architecture for long-term software
extensibility. It does not propose hardware changes.

Scope:

- `users/noah/` runtime architecture and boundaries
- `keyboards/.../keymaps/noah/` as the authored data surface
- maintainer docs and host-test seams that define and protect the architecture

Out of scope:

- hardware changes
- upstream QMK redesign
- style-only cleanup with no architectural payoff

## Executive Summary

The userspace is in a better place than a typical personal firmware tree. It
already has three strong architectural decisions that are worth preserving:

1. keymap-owned authored data is mostly kept in `keymap.c`
2. userspace-owned runtime behavior is mostly kept under `users/noah/lib/`
3. host tests and compile gates are treated as first-class architecture
   protection, not as afterthoughts

The next round of work should not be another directory shuffle. The folder
layout is already serviceable. The main architectural risk now is semantic
duplication:

- key interaction semantics are resolved in more than one place
- action meaning is repeatedly inferred from raw `uint16_t` keycodes
- pd-mode policy is split across multiple runtime modules
- central state types leak domain details back across module boundaries

The result is a codebase that is understandable today, but where adding a new
interaction concept or policy dimension will still require edits in too many
places.

My recommendation is to keep the current ownership model, but refactor around
three stronger contracts:

1. a resolved interaction contract for any tap count, not only the first tap
2. a typed action descriptor instead of repeated raw-keycode classification
3. a computed pd-mode control-plane snapshot that centralizes mode policy

If those three seams are improved, the codebase should scale cleanly to more
behaviors, richer feedback, and more shared policy without turning the key
runtime into a permanent integration bottleneck.

## What Is Working Well

### 1. The authored-data vs runtime boundary is real

The repo is not treating `keymap.c` like a dumping ground. Authored profile
data lives in one obvious place:

- `VIA_MACROS(MACRO)`
- `HARDCODED_MACROS(MACRO)`
- `COMBOS(COMBO)`
- `key_behaviors[]`
- `keymaps[][]`

That is a good long-term decision. It means new concrete layout/profile work
usually starts as data, not runtime code.

### 2. The key engine has a real architecture, not ad hoc hook logic

The handled-key path is built around:

- resolved handled-key lookup
- slot-local reducers
- an explicit effect vocabulary
- ownership modules for layers, held actions, repeats, and modifiers

That is the right direction for firmware behavior that is richer than stock
QMK tap-hold. The transition-plan layer is especially important because it
prevents side effects from being fired directly out of every branch.

### 3. The repo takes architecture protection seriously

Three choices stand out:

- `users/noah/source_manifest.mk` gives the firmware build and compile gates a
  shared source inventory
- `tests/host/run_feature_gate_compile_tests.sh` protects header boundaries
  and build-surface drift
- the host suite covers runtime logic, validation, compatibility assumptions,
  and real-profile checks instead of only unit-level helpers

This is unusually strong for keyboard firmware and materially lowers the risk
of long-lived refactors.

### 4. The pd-mode manifest is the right abstraction seed

`users/noah/lib/pointing/defs/pd_mode_manifest.h` is doing valuable work:

- mode identity
- generated keycodes
- handler binding
- traits
- lifecycle hook ownership

That manifest-driven shape is a good base for future mode growth.

## Findings

### 1. Key interaction semantics are split across too many contracts

This is the most important architectural issue.

`handled_key_lookup()` resolves only the single-tap branch of a key into
`handled_key_view_t`. Later tap-count branches are then resolved through a
different path:

- `users/noah/lib/key/interaction/handled_key.c` resolves `behavior.single`
- `users/noah/lib/key/runtime/slot/key_runtime_slot.c` calls
  `key_behavior_step_lookup()` and `key_behavior_has_more_taps()` directly for
  pending multi-tap state
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c`
  rehydrates hold and long-hold behavior from the pending multi-tap payload and
  even writes back into `slot->binding.long_hold`

That means `handled_key_view_t` is not actually the full interaction contract.
It is only the first-step contract.

Why this matters:

- if you add a new per-tap semantic, you cannot rely on one resolver
- feedback semantics and runtime semantics can drift because later tap-count
  branches bypass the same abstraction
- downstream reducers still need knowledge of authored behavior structure
  instead of consuming a stable resolved contract

Today this is manageable because the model is still relatively compact. It will
get expensive when you try to add things like:

- per-tap interrupt policy
- per-tap feedback policy
- per-tap ownership hints
- different multi-tap branch metadata beyond tap/hold/long-hold

Recommended direction:

- keep `key_behavior_t` as the authoring schema
- add a resolved interaction API that can answer for any tap count
- store resolved step semantics in the slot instead of partially re-deriving
  them during multi-tap release/scan

Example shape:

```c
typedef struct {
    noah_action_desc_t tap;
    noah_action_desc_t hold;
    noah_action_desc_t long_hold;
    uint16_t tap_hold_term;
    uint16_t longer_hold_term;
    uint16_t multi_tap_term;
    uint16_t flags;
    uint8_t layer;
    pd_mode_mask_t pd_mode;
} resolved_interaction_step_t;

typedef struct {
    bool handled;
    bool has_more_taps;
    resolved_interaction_step_t step;
} resolved_interaction_view_t;

resolved_interaction_view_t interaction_resolve(uint16_t keycode, uint8_t tap_count);
```

With a contract like that:

- `handled_key_view_t` can disappear or become a thin alias
- multi-tap release/scan no longer needs special semantic re-resolution logic
- new behavior dimensions land in one place first

### 2. Action meaning is repeatedly inferred from raw keycodes

The codebase has several helper predicates, but the architecture still treats
most actions as raw `uint16_t` values whose meaning must be rediscovered by
each subsystem.

Examples:

- `action_dispatch.h` exposes classification predicates such as
  `action_dispatch_is_layer_lock()`, `action_dispatch_is_macro()`, and
  `action_dispatch_is_qmk_behavior_keycode()`
- `action_lifecycle.c` decides hold kind and dispatch path by repeatedly
  branching on raw action values
- `handled_key.c` decides fallback-hold behavior partly from raw keycode class
- `key_runtime_slot_policy.c` decides effect shape and feedback behavior from
  raw hold actions
- `pointer_layer_policy.c` infers pointer relevance from action/keycode class

This is better than open-coded branching everywhere, but it is still a leaky
interface. The actual semantic type of an action is not carried with it.

Why this matters:

- adding a new action family means touching many classifiers
- one subsystem can silently forget a new action kind while another handles it
- feedback, ownership, and dispatch policy remain coupled to QMK keycode
  encoding details

Recommended direction:

- introduce `noah_action_desc_t`
- resolve authored actions once near the interaction layer
- let reducers and policy code branch on descriptor kind/capabilities, not on
  QMK macros or numeric ranges

Example shape:

```c
typedef enum {
    NOAH_ACTION_LITERAL,
    NOAH_ACTION_LAYER_HOLD,
    NOAH_ACTION_LAYER_LOCK,
    NOAH_ACTION_PD_MODE_HOLD,
    NOAH_ACTION_PD_MODE_LOCK,
    NOAH_ACTION_MACRO,
    NOAH_ACTION_QMK_BEHAVIOR,
    NOAH_ACTION_KEYMAP_CUSTOM,
} noah_action_kind_t;

typedef struct {
    noah_action_kind_t kind;
    uint16_t raw_keycode;
    uint16_t flags;
    union {
        uint8_t layer;
        pd_mode_mask_t pd_mode;
        uint16_t literal;
    } data;
} noah_action_desc_t;
```

Then:

- `noah_action_hold_kind()` becomes table logic over descriptor kind
- key-runtime feedback can reason about ownership and preview without checking
  `IS_QK_MOMENTARY()`
- pd-mode and layer semantics stop leaking as numeric action conventions

### 3. `state/runtime/` is not a clean state boundary yet

`users/noah/lib/state/runtime/runtime_shared_state.h` is presented as the
central shared-state owner, but it embeds key-domain behavior types directly:

- `hold_behavior_t`
- `multi_tap_t`
- slot-specific lifecycle enums and semantic fields

That makes `state/runtime/` less of a reusable state layer and more of an
integration header that other domains must include to get key-runtime types.

Why this matters:

- the state layer now depends on the interaction schema
- changes in key interaction types ripple into state consumers
- it becomes harder to move toward a narrower public key-runtime API because
  the storage details are defined in the shared-state surface

This shows up in practice because key-runtime code, interaction code, and some
test surfaces all need the same header for both storage and semantic types.

Recommended direction:

- move slot-local enums and binding structs under `users/noah/lib/key/runtime/`
- let `runtime_shared_state_t` own a `key_runtime_state_t` defined by the key
  runtime rather than defining key-runtime storage inside `state/runtime/`
- keep `state/runtime/` focused on cross-subsystem aggregates and snapshots,
  not on one subsystem's internal storage schema

This is not about adding indirection everywhere. It is about putting type
ownership where the real behavior ownership already lives.

### 4. Pd-mode policy is manifest-driven at the edge but fragmented in the core

The pd-mode manifest is a strong starting point, but the live control plane is
spread across:

- `pd_mode_state.c` for active/locked state, exclusivity, release behavior, and
  display snapshots
- `pd_mode_lifecycle.c` for DPI policy, exclusivity enforcement, auto-mouse
  anchoring, reset hooks, and key interception routing
- `pointer_layer_policy.c` for layer consequences of mode traits and auto-mouse
- `pd_mode_registry.c` for lifecycle-hook selection and registry queries

The result is that adding a new cross-cutting mode trait is still expensive.
The manifest row alone is not enough; the semantics are distributed.

Why this matters:

- mode policy is harder to inspect in one place
- the single-active-mode invariant is enforced procedurally in several loops
- display state, runtime state, and layer consequences are coupled but not
  represented by one snapshot

The current implementation is still manageable because `PD_MODE_COUNT` is
small. Architecturally, though, it already wants a controller-style seam.

Recommended direction:

- keep the manifest
- compute one `pd_mode_snapshot_t` from manifest + current raw state
- compute derived policy from that snapshot in one place
- have lifecycle/apply code consume the snapshot delta

Example shape:

```c
typedef struct {
    pd_mode_mask_t active_mode;
    pd_mode_mask_t locked_mode;
    bool prefer_typing_layer;
    bool keep_auto_mouse_anchor;
    bool enable_dragscroll_backend;
    uint16_t effective_dpi;
} pd_mode_snapshot_t;

pd_mode_snapshot_t pd_mode_compute_snapshot(pd_mode_runtime_shared_state_t raw);
void pd_mode_apply_snapshot(pd_mode_snapshot_t before, pd_mode_snapshot_t after);
```

That would let:

- `pointer_layer_policy_apply()` depend on snapshot semantics instead of mode
  loops and direct trait scans
- split-sync display state mirror the same computed model as local runtime
- new traits land in the snapshot computation first, not as ad hoc branches in
  several files

### 5. Preflight is becoming a catch-all integration phase

`key_runtime_preflight_record()` currently owns multiple unrelated concerns:

- modifier ownership tracking and default-path suppression
- interrupting active handled keys on other presses
- flushing unrelated pending multi-taps before non-handled presses
- direct layer-lock/pd-lock dispatch routing just after handled-key processing

This works, but it is a warning sign. Preflight is becoming the place where
"things that must happen before the normal path" accumulate.

Why this matters:

- future behavior changes are likely to land in preflight because it is the
  easiest interception point
- preflight scans the whole slot table for several policies
- the orchestration layer risks becoming more complex than the reducers it is
  supposed to protect

Because the hardware is fixed, the O(board) scans are not the real problem.
The architectural problem is that event policy is being centralized by habit
instead of by a clear extension seam.

Recommended direction:

- keep a top-level orchestration stage
- split preflight into named policy passes or transition producers

Example shape:

```c
typedef bool (*key_event_policy_fn)(
    uint16_t keycode,
    keyrecord_t *record,
    key_runtime_transition_plan_t *plan
);

static const key_event_policy_fn preflight_policies[] = {
    key_runtime_policy_track_modifiers,
    key_runtime_policy_interrupt_other_slots,
    key_runtime_policy_flush_multi_tap,
};
```

This would make new cross-cutting behavior an extension of a known list rather
than another special-case branch inside one large function.

### 6. The macro DSL runtime is functionally good but structurally monolithic

`users/noah/lib/macro/macro_payload.c` currently owns:

- the key-name lexicon
- token/key lookup
- command parsing
- runtime playback
- encoding for VIA seeding
- the visitor used by validate/play/encode

The file is coherent, but it is too large for the size of the abstraction.
This is the clearest code-organization issue in the repo.

Why this matters:

- parser changes and playback changes are tightly coupled in one file
- the large key-name table obscures the actual DSL mechanics
- future additions such as better diagnostics, richer commands, or alternate
  encoders will expand an already dense module

Recommended direction:

- keep the DSL
- split the module by responsibility:
  - `macro_payload_lexicon.c`
  - `macro_payload_parse.c`
  - `macro_payload_run.c`
  - `macro_payload_encode.c`
- keep `macro_payload.h` as the stable façade

If desired, the lexicon table can later be generated or replaced by a smaller
lookup structure, but that is secondary. The main architectural win is
separating grammar, execution, and serialization.

## Modularity And Extensibility Assessment

### Adding new key behaviors

Adding a new authored behavior row is easy.

Adding a new behavior concept is only moderately easy.

What is easy today:

- new concrete `key_behaviors[]` rows
- new tap/hold/long-hold combinations inside the existing schema
- new timing values and repeat rates

What is still expensive:

- new per-tap metadata
- new output semantics that are not well described by raw action keycodes
- new interaction policy that depends on interrupt/flush/feedback behavior

The reason is not lack of modularization. The reason is that the semantic
contract is not centralized enough.

### Adding new layers or modes

Layers are reasonably extensible because:

- the layer enum is explicit
- locks are owned through one layer-ownership module
- validation rejects the layer actions that would bypass ownership

Pd modes are moderately extensible:

- adding a simple new mode is straightforward through the manifest
- adding a mode with new shared policy is more expensive because the policy is
  distributed across registry, lifecycle, state, and pointer-layer logic

### Adding new feature families

The repo is in a decent position for:

- more authored tap/hold behaviors
- more pd modes
- more RGB overlays

It is in a weaker position for:

- plugin-like action families
- interaction features with new semantic metadata
- cross-cutting policy that touches key runtime, feedback, and pointer policy

That is why I do not think a plugin system is the right next move yet. The
codebase first needs stronger core contracts. After that, extension mechanisms
become realistic.

## Abstractions And Interfaces

### Strong abstractions

- `source_manifest.mk` as the build/compile-gate source contract
- `noah_runtime.h` and `hooks.c` as the userspace hook surface
- `pd_mode_manifest.h` as the definition source for shared pd modes
- `key_runtime_effect_t` as the executable effect vocabulary
- `runtime_debug.h` as a high-value test/debug seam

### Leaky abstractions

- `handled_key_view_t`: looks like the full resolved interaction contract but
  only covers the first tap-count branch
- raw `uint16_t action`: meaning is repeatedly recovered instead of carried
- `runtime_shared_state.h`: state ownership and key-runtime internals are mixed
- pd-mode trait usage: manifest rows exist, but trait consequences are not
  computed through one stable policy layer

## Code Organization And Structure

The current top-level structure is broadly correct:

- `compat/`
- `action/`
- `macro/`
- `key/interaction`, `key/ownership`, `key/runtime`
- `pointing/defs`, `pointing/runtime`, `pointing/policy`, `pointing/modes`
- `state/ownership`, `state/runtime`
- `rgb/core`, `rgb/automouse`, `rgb/stages`

The main remaining organization issues are inside modules, not between
directories:

- `macro_payload.c` is too large
- the key-runtime orchestration/reducer boundary is good, but some semantic
  resolution still leaks through it
- pd-mode runtime is physically split well, but conceptually the control plane
  is still fragmented

## State Management And Flow

The codebase is strongest when state is explicitly owned:

- layer state through `layer_ownership.c`
- held action ownership through `held_action.c`
- repeat scheduling through `held_repeat.c`
- keyboard modifier ownership through `keyboard_mod_ownership.c`

The main state-management risk is not "too much global state". It is "too many
places understand the same state differently".

Examples:

- key interaction state is split between handled-key resolution, slot state,
  pending multi-tap state, and action classification
- pd-mode state is split between raw flags, derived trait consequences, and
  display snapshots

The cure is not a giant global state machine. The cure is better derived
contracts.

## Scalability Outlook

If the codebase keeps growing without contract refactors, I expect these
bottlenecks first:

1. new interaction features will require touching too many key-runtime files
2. new action families will expand classification logic in several modules
3. new pd-mode traits will spread policy branches across runtime files
4. macro DSL growth will accumulate in one already large module

I do not see performance as the main risk on this fixed board. Technical debt
accumulation is more likely to come from semantic duplication than from runtime
cost.

## Testing And Debuggability

This is one of the strongest parts of the repo.

What is already good:

- direct subsystem runners
- full host-suite runner
- feature-gate compile checks
- runtime-debug snapshot
- runtime trace ring buffer
- scenario harness for interaction sequences
- real-profile validation coverage

What would improve it further:

- promote more tests toward resolved-contract tests instead of reducer-internal
  knowledge once the interaction contract is centralized
- replace stringly typed key-runtime trace stages with enums so trace semantics
  are refactor-safe
- consider snapshot-level pd-mode policy tests once a controller/snapshot seam
  exists

## Concrete Refactoring Recommendations

### Recommendation 1: centralize interaction resolution

Do next.

Goal:

- one resolver for any tap count
- slot state stores resolved semantics, not partial authored fragments

Expected payoff:

- easier new behavior families
- less multi-tap special-case code
- cleaner feedback/runtime contract

### Recommendation 2: introduce typed action descriptors

Do after or together with recommendation 1.

Goal:

- stop treating most semantics as rediscovered keycode classes

Expected payoff:

- less classifier duplication
- easier new action kinds
- less QMK encoding leakage into business logic

### Recommendation 3: move key-runtime storage types under key/runtime

Do as a boundary cleanup after 1 and 2 start landing.

Goal:

- let `state/runtime/` own aggregates and snapshots, not key-runtime internals

Expected payoff:

- clearer module ownership
- fewer include dependencies that cross layers awkwardly

### Recommendation 4: build a pd-mode controller snapshot

Do before adding materially richer pd-mode policy.

Goal:

- one place computes active/locked/display/policy consequences

Expected payoff:

- easier new traits
- easier display/policy consistency
- less procedural loop duplication

### Recommendation 5: split the macro DSL implementation

Do opportunistically; it is valuable but less urgent than the contract work.

Goal:

- keep the DSL stable while separating lexicon, parse, run, and encode logic

Expected payoff:

- easier diagnostics
- easier future commands
- lower cognitive load when editing macro behavior

## Suggested Refactor Order

1. Introduce `resolved_interaction_view_t` without changing authored behavior.
2. Convert multi-tap release/scan to consume that contract.
3. Introduce `noah_action_desc_t` and migrate key-runtime policy/helpers to it.
4. Relocate key-runtime storage types out of `state/runtime/`.
5. Introduce `pd_mode_snapshot_t` and migrate pointer-layer policy to it.
6. Split `macro_payload.c` once the higher-priority contracts are stable.

## Bottom Line

The repo already has a real software architecture. The problem is not lack of
structure. The problem is that some of the most important semantics are still
represented indirectly:

- first-tap contract vs later-tap contract
- raw action code vs action meaning
- raw pd-mode flags vs effective pd-mode policy

Fix those three seams and the codebase should remain maintainable even as the
userspace grows more expressive.
