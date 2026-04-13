# Userspace Architecture Review

Date: 2026-04-13

Status: active review created after
[2026-04-13-review-03](../2026-04-13-review-03/userspace-architecture-review.md).
This pass reviews the current live userspace after the earlier contract cleanup
work. Hardware is treated as fixed; this review is only about software
architecture, structure, and long-term extensibility.

Implementation update later the same day: the first slice of Finding 1 and the
first slice of Finding 2 have landed, followed by the authored/runtime seam
migration, the removal of the last public handled-key compatibility alias, and
the removal of the slot interaction's anonymous direct-field mirror. Active
slot storage now uses
`key_runtime_slot_interaction_t` as the slot-owned cached interaction contract,
and authored lookup now has an explicit `handled_key_resolution_t` surface with
`handled_key_resolution_*` accessors and explicit slot conversion helpers. The
active-release reducer now also executes a typed release contract derived from
that interaction instead of reconstructing all release semantics directly from
raw hold flags, and runtime consumers now read cached authored semantics
through `interaction.resolution` instead of through a flat mirrored struct.

This review is intentionally not a repeat of the earlier action-family,
pd-mode write-controller, macro IR, and test-harness recommendations. Those
changes materially improved the codebase and are now part of the baseline for
this pass. The remaining pressure points are subtler: a few contracts still do
too many jobs at once, some state models still advertise more flexibility than
the runtime actually allows, and a few compatibility and test surfaces still
leak implementation details upward.

Scope:

- `users/noah/` runtime architecture and module boundaries
- `keyboards/.../keymaps/noah/` as the authored data surface
- host-test and debug surfaces that shape future refactors

Out of scope:

- hardware changes
- upstream QMK redesign
- style-only cleanup with no architectural payoff

## Executive Summary

This codebase is in a good place structurally. It already has several strong
architectural properties that many firmware repos never reach:

1. authored keymap data is concentrated in `keymap.c`
2. runtime behavior is mostly centralized under `users/noah/lib/`
3. the key engine already uses an effect-plan model instead of ad hoc side
   effects
4. build surfaces, docs, and host tests are treated as real contracts

The next scaling risks are no longer "everything is tangled together". They are
"one contract is carrying too many meanings" risks:

- the handled-key resolution/interaction boundary only recently split, and one
  slot contract still carries more than one concern
- release behavior is still resolved by a large feature matrix inside one
  reducer instead of by a narrower release contract
- pd modes are represented as composable bitmasks even though the runtime
  enforces one effective active mode and one effective lock
- compatibility boundaries still mix unrelated fork assumptions
- macro semantics still cross two partially separate execution models
- the test suite is excellent, but too much higher-level coverage still treats
  storage layout like public API

This repo does not need a generic plugin system. It should continue to lean
into typed command/state-machine contracts, because that is already the
architecture that works here.

## What Is Working Well

### 1. The authored-data boundary is still real

`keymap.c` remains the clear authoring surface for:

- `VIA_MACROS(MACRO)`
- `HARDCODED_MACROS(MACRO)`
- `COMBOS(COMBO)`
- `key_behaviors[]`
- `keymaps[][]`

`noah_keymap.h`, `noah_keymap_ids.h`, and the docs under `docs/` still make
the authored/runtime split discoverable. That is the right long-term shape for
fixed hardware with changing software behavior.

### 2. The key runtime is still the strongest subsystem

The handled-key engine keeps the best architectural shape in the repo:

- explicit slot ownership by physical key position
- transition-plan batching before effect execution
- separate ownership modules for held actions, repeats, layers, and mods
- broad host coverage

The key runtime does not need a rewrite. It needs a tighter contract around the
data that flows through it.

### 3. PD-mode manifest-driven registration is a good pattern

`pd_mode_manifest.h` and `pd_modes[]` still give the pointing subsystem one
discoverable registration surface. Adding a new mode is much cheaper than it
would be in a switch-heavy design.

### 4. Build and compatibility contracts are taken seriously

`source_manifest.mk`, `rules.mk`, `hooks.c`, `noah_runtime.h`, and the
compile-gate runners show good repo discipline. This makes architectural
refactors realistic instead of aspirational.

## Findings

### 1. The handled-key resolution/interaction seam still carries too many meanings

The handled-key contract is more normalized than it used to be, but one seam is
still doing too much:

- `handled_key_lookup_tap_count(...)` builds `handled_key_resolution_t` from
  authored config in `handled_key.c`
- `handled_key_resolution_t` still carries runtime-facing policy flags and
  derived hold semantics in `handled_key.h`
- `key_runtime_slot_press_interaction(...)` mutates that same type before
  storing it into slot state in `key_runtime_slot_press_reduce.c`
- feedback and scan reducers later consume the stored value as if it were the
  stable slot interaction contract

This means one struct currently represents:

- authored lookup output
- per-press resolved interaction state
- feedback/release-policy metadata

That is a leaky abstraction. A future behavior family will tend to add another
field or another flag to `handled_key_resolution_t`, then teach lookup, slot press,
scan, release, feedback, and tests how to interpret it.

Why this matters:

- it raises the cost of adding new key behavior families
- it still makes the meaning of a handled-key resolution/context value partly
  phase-dependent
- it encourages feature policy to spread through flags instead of through
  narrower contracts

Recommended direction:

- split authored resolution from slot-owned interaction state
- keep `handled_key_lookup_*()` as the immutable authored-resolution seam
- create a narrower slot contract that owns the chosen tap-count branch and the
  normalized hold/release policy for this physical press

Implementation update:

- the first slice of this recommendation is now in place:
  `active_key_state_t` stores `key_runtime_slot_interaction_t`, and reducers
  read it through `key_runtime_slot_cached_interaction(...)`
- authored handled-key lookup is now explicitly named
  `handled_key_resolution_t`
- cached hold policy now lives with the slot interaction contract instead of
  being recomputed at every feedback/scan consumer
- `key_runtime_slot_interaction_t` now carries an explicit `.resolution`
  object, and the public `handled_key_view_t` alias has now been removed
- runtime/process/host seams now name authored lookup output as
  `handled_key_resolution_t` directly
- the remaining gap is that `handled_key_resolution_t` still carries a large
  amount of derived runtime-facing semantics, so the slot-owned contract is
  clearer than before but the authored branch record is still doing multiple
  jobs

Example shape:

```c
typedef struct {
    uint16_t            keycode;
    uint8_t             tap_count;
    key_behavior_step_t step;
    uint16_t            default_tap_action;
    bool                has_more_taps;
    bool                is_layer_surface;
    pd_mode_mask_t      pd_mode;
} handled_key_resolution_t;

typedef struct {
    handled_key_resolution_t          resolution;
    handled_key_interaction_policy_t  policy;
    key_runtime_slot_hold_strategy_t  hold_strategy;
    uint16_t                          tap_action;
    uint8_t                           tap_repeat_count;
    uint16_t                          tap_hold_term;
    uint16_t                          longer_hold_term;
    uint16_t                          multi_tap_term;
} key_runtime_slot_interaction_t;
```

That would make the slot model clearer and keep feedback/release code from
depending on "whatever the cached handled-key resolution happens to mean at this phase".

### 2. Release behavior is still a monolithic feature matrix

`key_runtime_slot_release_active.c` is still the main place where cross-feature
release semantics are reconstructed from:

- slot phase
- fallback hold state
- immediate-hold state
- interrupted layer-tap state
- pd-mode quick-lock behavior
- long-hold release takeover
- multi-tap buffering

That file is readable, but it is still a hotspot. Adding one more release-time
semantic will likely require touching:

- `key_runtime_slot_release_active.c`
- `key_runtime_slot_scan_reduce.c`
- `handled_key.h`
- at least one integration harness

The earlier hold-policy cleanup helped the threshold path more than the release
path. Release resolution still largely depends on local condition choreography.

Why this matters:

- release-time bugs are the hardest to reason about in this repo
- future hold modes will keep inflating one reducer instead of extending a
  typed contract
- tests will keep needing detailed slot setup because the release rules are not
  represented as a compact object

Recommended direction:

- compute a release contract when the press begins or when the tap-count branch
  changes
- let the release reducer execute a small contract rather than re-derive the
  matrix from scratch

Example shape:

```c
typedef enum {
    KEY_RELEASE_STRATEGY_NONE = 0,
    KEY_RELEASE_STRATEGY_TAP,
    KEY_RELEASE_STRATEGY_RELEASE_HOLD,
    KEY_RELEASE_STRATEGY_PD_MODE_LOCK_TAP,
    KEY_RELEASE_STRATEGY_BUFFER_MULTI_TAP,
} key_release_strategy_t;

typedef struct {
    key_release_strategy_t strategy;
    bool                   release_owned_state;
    bool                   cancel_on_layer_interrupt;
    uint16_t               primary_action;
    uint16_t               alternate_long_hold_action;
    pd_mode_mask_t         pd_mode_lock;
} key_release_contract_t;
```

Then the release reducer becomes "pick branch from elapsed time and execute",
not "reconstruct policy from many local booleans".

Implementation update:

- the active-release reducer now executes a typed
  `key_runtime_slot_release_contract_t`
- quick tap, immediate-hold quick release, fallback suppression, pd-mode quick
  lock, release-hold selection, and multi-tap buffering are now expressed
  through that contract instead of each branch reinterpreting raw interaction
  fields independently
- the remaining gap is timing: the contract is still derived on demand from the
  cached slot interaction at release time rather than being cached when the
  press or tap-count branch resolves

### 3. PD-mode state still advertises composition while the runtime enforces exclusivity

The pd-mode subsystem has a clean command controller, but its state model still
suggests "many active modes can coexist" even though the runtime mostly
enforces "one effective active mode, one effective lock":

- the public read model uses `pd_mode_mask_t active_flags` and
  `locked_flags`
- snapshots compute `first_active_index` and `first_locked_index`
- `pd_mode_apply_activate_mode(...)` and `pd_mode_apply_lock_mode(...)` clear
  other active/locked modes first
- pointer handling, DPI selection, key interception, and RGB rendering all
  prefer the first active mode

This mismatch shows up in several places:

- the representation is a flag set
- the controller is exclusive
- many consumers depend on manifest order as the effective priority rule

Why this matters:

- adding a stacked or overlay mode later would require a subsystem-wide rethink
- even without stacked modes, the current API makes the exclusivity invariant
  less obvious than it should be
- "first active index" becomes de facto control flow in several modules

Recommended direction:

- make exclusivity explicit in the public state model unless true composition
  is a planned feature
- prefer a typed active/locked mode identity over generic active bitmasks
- if multiple simultaneous modes are intentionally desirable later, encode that
  as a manifest-level arbitration model rather than as accidental bitmask
  freedom

For the current design, a simpler state shape would fit the real invariant
better:

```c
typedef struct {
    pd_mode_id_t local_active;
    pd_mode_id_t local_locked;
    pd_mode_id_t display_active;
    pd_mode_id_t display_locked;
} pd_mode_state_t;
```

Derived traits can still be computed from the selected mode definition row.
That would remove the need for several "first active" readers and make the
control-plane contract more honest.

### 4. `compat/` is valuable, but its boundaries are still too broad

The repo already moved fork-specific assumptions into `users/noah/lib/compat/`,
which is good. The remaining issue is that one compatibility surface still
mixes unrelated domains:

- `qmk_contract.h` exposes VIA playback, Charybdis pointer DPI/sniping, and
  auto-mouse APIs
- `qmk_via_contract.h` separately owns VIA EEPROM/command plumbing
- `qmk_mod_contract.c` is another distinct compatibility seam again

This means "QMK contract" currently names several different kinds of coupling:

- forked dynamic-macro playback
- pointing-device fork helpers
- auto-mouse internals
- modifier symbol override behavior

Why this matters:

- future QMK-fork updates will be harder to localize
- feature owners have to know too much about a generic compat bucket
- compatibility review becomes less discoverable than the runtime itself

Recommended direction:

- split the compatibility layer by owning subsystem
- keep a small umbrella only if you still want a single include path

Suggested split:

- `compat/qmk_via_playback_contract.h`
- `compat/qmk_via_storage_contract.h`
- `compat/qmk_pointing_contract.h`
- `compat/qmk_auto_mouse_contract.h`
- `compat/qmk_mod_contract.h`

That would make upstream/fork drift easier to audit and keep feature code from
depending on unrelated compat helpers by accident.

### 5. Macro semantics still cross two partially separate execution models

The earlier hardcoded-macro IR work improved the repo materially, but the macro
system still has two meaningfully different pipelines:

- hardcoded macros: payload DSL -> repo IR -> repo playback
- VIA defaults: payload DSL -> QMK/VIA byte encoding
- live VIA playback: QMK/VIA byte encoding -> forked compatibility playback

That is better than before, but the repo still does not have one canonical
macro command model for both hardcoded and VIA-backed surfaces.

Why this matters:

- new macro features will need changes in more than one semantic pipeline
- debugging "why did the hardcoded macro work but the VIA macro not work"
  remains harder than it should be
- compatibility with upstream VIA encoding is still partly protected by copied
  playback logic rather than by a first-class repo-level codec contract

Recommended direction:

- treat the repo's macro command sequence as the canonical model
- make VIA encoding and decoding a codec around that model
- keep QMK/VIA byte compatibility as a boundary, not as a second semantic core

A practical next step would be:

1. define a shared macro command sequence object
2. compile payload DSL into that object
3. encode that object into VIA bytes for default seeding
4. add a decode/probe path in tests so VIA playback compatibility is checked
   against the same command sequence

The current `qmk_contract_probe` coverage is the right place to extend this.

### 6. The host suite is strong, but too much higher-level coverage still depends on storage layout

The repo's host test surface is a major strength. The remaining issue is not
"there are not enough tests". It is "too many higher-level tests still depend
on internal layout and raw mutable globals":

- direct writes into `noah_runtime_shared_state`
- direct slot access through `key_runtime_slot_for_position(...)`
- direct construction of `handled_key_resolution_t`
- scenario harness exposure of `runtime_shared_state_t`

Some of that is absolutely appropriate for low-level storage or reducer tests.
The problem is where those patterns bleed upward into broader behavior tests.

Why this matters:

- it hardens internal storage layout into de facto public API
- it makes refactors noisier than necessary
- it raises the cost of changing slot or runtime-state shapes even when
  semantics stay the same

Recommended direction:

- keep raw-storage tests for truly low-level modules
- move more integration tests onto scenario/builders plus
  `noah_runtime_debug_snapshot(...)`
- avoid exposing whole shared-state pointers from scenario helpers unless the
  test is explicitly about storage

## Testing And Debuggability

Debuggability is already above average for firmware, but there is one clear
gap: the trace surface records plans and executed effects, not enough of the
decision path that produced them.

Right now the trace surfaces tell you:

- a plan existed
- how many effects it contained
- which effects executed
- pd-mode/layer/split-sync events

They do not tell you enough about:

- which release strategy won
- whether a hold contract promoted or downgraded
- why a pd-mode quick-lock tap was or was not taken
- why a pending multi-tap chain flushed vs reused

Recommended direction:

- add a small typed decision trace for key-runtime branch points
- keep the existing ring buffer compact, but spend a few event ids on
  high-value state-machine decisions

Useful additions would be:

- `NOAH_TRACE_KEY_RUNTIME_EVENT_RELEASE_RESOLUTION`
- `NOAH_TRACE_KEY_RUNTIME_EVENT_HOLD_POLICY_DECISION`
- `NOAH_TRACE_KEY_RUNTIME_EVENT_MULTI_TAP_DECISION`

That would make future refactors much easier to verify without widening the
console-only trace text surface.

## Recommended Refactor Order

1. Split authored handled-key resolution from slot-owned interaction state.
   This has the best payoff because it simplifies release logic, feedback, and
   future behavior growth at the same time.
2. Extract a typed release contract from the current release matrix.
   That will make new hold/release behaviors cheaper to add and easier to test.
3. Make pd-mode exclusivity explicit in the public state model.
   That removes hidden "first active" priority assumptions from the rest of the
   pointing stack.
4. Break `compat/` into feature-owned compatibility surfaces.
   This is lower risk than runtime refactors and improves discoverability fast.
5. Move macro semantics toward one canonical command model and treat VIA as a
   codec boundary.
6. Keep shifting higher-level tests toward semantic builders and richer traces.

## Bottom Line

This userspace is architecturally healthy. The remaining work is not to invent
new layers of abstraction. It is to make a few existing abstractions more
honest:

- one type should mean one thing
- one state model should reflect the real invariant
- one compatibility file should correspond to one dependency boundary
- one test surface should assert behavior without requiring storage knowledge

If those seams are tightened, this codebase should continue to scale well for
new behaviors, new modes, and new authored profiles without needing another
large structural rewrite.
