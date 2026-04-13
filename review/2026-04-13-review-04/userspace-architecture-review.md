# Userspace Architecture Review

Date: 2026-04-13

Status: active review created after
[2026-04-13-review-03](../2026-04-13-review-03/userspace-architecture-review.md).
This pass reviews the current live userspace after the earlier contract cleanup
work. Hardware is treated as fixed; this review is only about software
architecture, structure, and long-term extensibility.

Implementation update later the same day: the first slice of Finding 1 and the
first slice of Finding 2 have landed, followed by the authored/runtime seam
migration, the removal of the last public handled-key compatibility alias, the
removal of the slot interaction's anonymous direct-field mirror, a narrower
slot-owned branch contract, and a thinner authored handled-key resolution.
Active slot storage now uses `key_runtime_slot_interaction_t` as the slot-owned
cached interaction contract, while authored lookup stays on the outer seam as
`handled_key_resolution_t` with `handled_key_resolution_*` accessors. The
authored resolution now carries keycode, tap-count branch selection, authored
step data, timing, layer/pd metadata, and structural flags; the slot
interaction owns the live binding, cached hold policy, and cached release
contract. The active-release reducer executes that typed release contract
instead of reconstructing release semantics from raw hold flags, and runtime
consumers no longer treat cached slot interaction as a stored authored
resolution object. A later Finding 2 slice also narrowed the cached release
contract itself so tap materialization is now carried as a typed tap outcome
contract instead of a loose `tap_action` plus `buffers_multi_tap` pairing. The
next Finding 2 slice then replaced the bespoke phase-resolver matrix in
`key_runtime_slot_release_active.c` with a compact phase-contract table that
drives one generic release-phase interpreter. The next slice after that moved
primary release-hold selection and long-hold takeover onto a shared cached
release-hold contract that both active release and pending multi-tap now use.
The final Finding 2 slice then moved the phase-contract table itself onto the
cached release contract, so the release reducer now selects a cached
phase-policy view instead of owning a local phase-policy table. The next
debuggability slice then added typed key-runtime decision trace events for
release resolution, hold-policy decisions, and pending multi-tap chain
reuse/flush paths, so the shared runtime trace now records the state-machine
choices that produce release effects instead of only the resulting plans.
Later the same day, the pd-mode exclusivity follow-up also landed: runtime
storage and snapshots now expose explicit selected active/locked mode identity
for local and display state. Compatibility bitmasks now survive only as
derived snapshot and split-sync transport fields instead of shared control
state. Pointer handling, DPI
selection, RGB pd-mode rendering, and pointer-layer policy now read explicit
active mode identity instead of routing through `first_active_*` readers.

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

- the handled-key resolution/interaction boundary is now much healthier, but
  the remaining public bridge cleanup is now landed, so the main follow-up is
  just to keep future tests from drifting back toward synthetic
  resolution-first slot setup
- release behavior is more structured now, but one reducer is still the main
  hotspot for cross-feature release choreography
- pd modes now store explicit selection state and their public snapshot view
  matches that invariant; only command/transport seams still use flag-shaped
  compatibility payloads
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

### 1. The handled-key resolution/interaction seam was the main architectural leak, and the core split is now landed

The handled-key contract is materially better than it was at the start of this
review:

- `handled_key_lookup_tap_count(...)` builds `handled_key_resolution_t` from
  authored config in `handled_key.c`
- `handled_key_resolution_t` now carries the authored branch record: keycode,
  tap-count selection, chosen authored step, timing, layer/pd metadata, and
  structural flags
- `key_runtime_slot_interaction_t` now owns the slot-time contract:
  branch selection snapshot, normalized binding, cached hold policy, and
  cached release semantics
- `key_runtime_slot_press_interaction(...)` mutates the slot-owned interaction
  contract instead of widening the authored resolution object again
- feedback, scan, and release consumers now read slot semantics from cached
  slot interaction instead of treating cached slot state as a stored authored
  resolution object

That removes the main leaky abstraction. A future behavior family no longer has
to widen one "resolution means everything" struct just to participate in slot
policy.

Why this matters:

- it raises the cost of adding new key behavior families
- it made the meaning of a handled-key resolution/context value partly
  phase-dependent
- it encouraged feature policy to spread through flags instead of through
  narrower contracts

Recommended direction:

- keep `handled_key_lookup_*()` as the immutable authored-resolution seam
- keep `key_runtime_slot_interaction_t` as the only slot-owned semantic cache
- continue shrinking compatibility bridges that reconstruct authored
  resolution-like values from slot interaction for tests/debug use

Implementation update:

- `active_key_state_t` stores `key_runtime_slot_interaction_t`, and reducers
  read it through `key_runtime_slot_cached_interaction(...)`
- authored handled-key lookup is explicitly named `handled_key_resolution_t`
- cached hold policy now lives with the slot interaction contract instead of
  being recomputed at every feedback/scan consumer
- `key_runtime_slot_interaction_t` no longer stores the full authored
  resolution object; it now caches branch selection, slot binding, policy, and
  release contract
- `handled_key_resolution_t` now carries authored branch data instead of stored
  runtime hold strategy, tap action, and release-policy decisions
- `key_runtime_slot_interaction(...)` now returns the slot-owned interaction
  contract directly, with no public runtime-header helper that reconstructs an
  authored resolution from slot state
- the remaining cleanup here is mostly hygiene: keep new host fixtures from
  synthesizing authored resolutions just to mutate slot-owned interaction state

Example shape:

```c
typedef struct {
    uint16_t            keycode;
    uint8_t             tap_count;
    key_behavior_step_t step;
    uint16_t            tap_hold_term;
    uint16_t            longer_hold_term;
    uint16_t            multi_tap_term;
    uint8_t             layer;
    pd_mode_mask_t      pd_mode;
    bool                has_more_taps;
    uint16_t            flags;
} handled_key_resolution_t;

typedef struct {
    uint16_t             keycode;
    uint8_t              tap_count;
    key_behavior_step_t  step;
} key_runtime_slot_selection_t;

typedef struct {
    key_runtime_slot_selection_t       selection;
    key_runtime_slot_binding_t         binding;
    key_runtime_slot_hold_strategy_t   hold_strategy;
    uint8_t                            layer;
    pd_mode_mask_t                     pd_mode;
    uint16_t                           flags;
    handled_key_interaction_policy_t   policy;
    key_runtime_slot_release_contract_t release;
} key_runtime_slot_interaction_t;
```

That is now the actual runtime shape. The main follow-up is simply to keep
debug/test seams from drifting back toward "synthetic resolution as the default
slot view".

### 2. Release behavior is narrower, but still concentrated in one hotspot

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
- the cached release contract now carries a typed tap outcome contract, so the
  reducer executes one of `DISPATCH_ACTION` / `BUFFER_MULTI_TAP` instead of
  re-deriving tap materialization from `tap_action` plus buffer flags
- the active-release reducer now routes slot phases through a compact
  phase-contract table instead of one bespoke resolver per phase
- primary release-hold selection and long-hold takeover now live behind a
  shared cached release-hold contract, and pending multi-tap reuses that same
  helper instead of a separate slot-policy selector
- the phase-contract table itself now lives on the cached release contract, so
  tap/hold/phase policy is consistently slot-owned instead of split between
  cached interaction and reducer-local tables
- the remaining work here is mostly observability and polish: if future
  changes make release debugging noisy again, add typed decision trace events
  rather than widening the contract again

### 3. PD-mode state used to advertise composition while the runtime enforced exclusivity

The pd-mode subsystem had a clean command controller, but its state model used
to suggest "many active modes can coexist" even though the runtime mostly
enforced "one effective active mode, one effective lock":

- the public read model exposed `pd_mode_mask_t active_flags` and
  `locked_flags` as if they were the primary invariant
- snapshots computed `first_active_index` and `first_locked_index`
- `pd_mode_apply_activate_mode(...)` and `pd_mode_apply_lock_mode(...)` clear
  other active/locked modes first
- pointer handling, DPI selection, key interception, and RGB rendering all
  preferred the first active mode

That mismatch showed up in several places:

- the representation is a flag set
- the controller is exclusive
- many consumers depend on manifest order as the effective priority rule

Why this matters:

- adding a stacked or overlay mode later would require a subsystem-wide rethink
- even without stacked modes, the current API makes the exclusivity invariant
  less obvious than it should be
- "first active index" becomes de facto control flow in several modules

Implementation update:

- runtime shared state now carries explicit selected mode identity for
  `local_active_mode`, `local_locked_mode`, `remote_display_active_mode`, and
  `remote_display_locked_mode`
- `pd_mode_snapshot_view_t` now exposes `active_mode`, `locked_mode`,
  `active_index`, and `locked_index` instead of `first_active_*`
- `pd_mode_state.c` now orchestrates exclusivity around one selected active
  mode and one selected lock instead of walking bitmask-shaped local state
- `pd_runtime.c`, `pd_mode_lifecycle.c`, `rgb_pd_mode_stage.c`, and
  `pointer_layer_policy.c` now use explicit selected mode identity instead of
  "first active" routing
- split sync still sends compatibility flags, but the remote apply path now
  collapses them immediately into one selected display mode and one selected
  display lock
- local split-sync snapshot helpers now derive compatibility flags from the
  selected local mode identity instead of reading stored mirror fields

The current design now matches the simpler state shape this review called for:

```c
typedef struct {
    pd_mode_mask_t active_mode;
    pd_mode_mask_t locked_mode;
    pd_mode_traits_t active_traits;
    uint8_t active_index;
    uint8_t locked_index;
} pd_mode_snapshot_view_t;
```

Derived traits now come from the selected active mode definition row. The only
flag-shaped compatibility surface left here is the command/split-sync
transport seam, which is the right place for it.

### 4. `compat/` now has feature-owned seams, which is the right long-term shape

The repo already had the right instinct here: keep fork-specific assumptions in
`users/noah/lib/compat/` instead of scattering them through runtime modules.
That structure is now materially tighter than it was at the start of this
review:

- `qmk_via_playback_contract.h` owns the forked VIA dynamic-macro playback seam
- `qmk_via_storage_contract.h` owns VIA EEPROM/command classification
- `qmk_pointing_contract.h` owns Charybdis pointer DPI/sniping helpers
- `qmk_auto_mouse_contract.h` owns fork-specific auto-mouse helpers
- `qmk_mod_contract.h` names the modifier symbol-override seam explicitly
- `qmk_contract.h` and `qmk_via_contract.h` are now compatibility umbrellas
  instead of the normal include path for feature code

That means "QMK contract" no longer hides several different kinds of coupling
behind one broad header. Feature code now includes the boundary it actually
depends on.

Why this matters:

- future QMK-fork updates are easier to localize and review
- feature owners no longer need to know about unrelated compat helpers
- compatibility drift is more discoverable than it was when the broad umbrella
  header was the default surface

The important design constraint now is to keep this split honest:

- new VIA playback work should stay on `qmk_via_playback_contract.h`
- new VIA storage/reset work should stay on `qmk_via_storage_contract.h`
- new pointing or auto-mouse coupling should not be re-added to
  `qmk_contract.h`

That is the right shape for future fork auditing without forcing a larger
runtime rewrite.

### 5. Macro semantics now have one canonical repo model, with VIA reduced to a codec boundary

The macro system now has the cleaner shape this review was asking for:

- payload DSL compiles into `macro_payload_ir_t`
- hardcoded macros cache and play that IR
- VIA defaults now compile payload DSL into that same IR and then encode the IR
  into QMK/VIA bytes for seeding
- live VIA playback now decodes the current dynamic macro buffer back into that
  same IR before executing it

That means the repo no longer has two separate semantic cores for "what a
macro means". The canonical model is the repo IR, and VIA bytecode is now an
encoding/decoding boundary around it.

Why this matters:

- new macro features now have one obvious implementation target
- debugging hardcoded vs VIA macro mismatches is much cheaper because both
  routes now share the same IR executor
- the old forked VIA playback copy has been replaced by an explicit codec path
  instead of remaining an independent semantic pipeline

Implementation update:

- `macro_payload_validate(...)` and `macro_payload_play(...)` now route through
  IR compilation instead of relying on separate visitor-only execution paths
- `macro_payload_encode_write(...)` now compiles payload DSL into IR first, and
  IR-to-VIA encoding is exposed explicitly as `macro_payload_encode_ir_write(...)`
- live VIA macro playback now decodes QMK dynamic-macro bytes through
  `macro_payload_decode_qmk_stream(...)` and then executes the resulting IR
- the codec now recognizes the down/tap/up pattern emitted for tap chords and
  reconstructs it back into the canonical tap-list IR form
- the remaining intentional distinction is transport policy, not macro meaning:
  VIA playback still keeps delayed plain-text emission so live VIA-edited
  macros preserve current QMK-compatible pacing while sharing the same IR for
  delays, holds, taps, and text content

The current shape is now the right one:

```c
payload DSL -> macro_payload_ir_t -> repo playback
payload DSL -> macro_payload_ir_t -> VIA bytes
VIA bytes -> macro_payload_ir_t -> repo playback
```

That is enough architectural cleanup here. Future work should add features to
the IR and codec, not reintroduce separate hardcoded and VIA macro semantics.

### 6. The host suite is strong, but too much higher-level coverage still depends on storage layout

The repo's host test surface is a major strength. The remaining issue is not
"there are not enough tests". It is "too many higher-level tests still depend
on internal layout and raw mutable globals":

- direct writes into `noah_runtime_shared_state`
- direct slot access through `key_runtime_slot_for_position(...)`
- direct construction of `handled_key_resolution_t`
- some higher-level assertions still reach through raw slot/lifecycle fields
  instead of semantic helpers or debug snapshots

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

Implementation update:

- the key-runtime scenario harness no longer exposes
  `runtime_shared_state_t *` as a public test seam
- scenario-level slot assertions now have semantic helpers for owner keycode,
  held-action keycode, pending multi-tap state, and hold completion instead of
  reaching through live slot storage directly
- the shared key-runtime integration harness now routes snapshot reads through
  semantic slot helpers for owner keycode, held-action keycode, pending
  multi-tap count/state, and hold completion instead of exporting raw
  `active_key_state_t *` to higher-level tests
- integration coverage in the pd-mode, layer-lock, and modifier-hold suites
  now consumes those semantic helpers instead of reading slot owner/lifecycle
  fields directly
- the remaining work here is now mostly write-side staging: `runtime_debug`
  fixtures still write raw shared state directly when they could stage state
  through builders or subsystem entry points before taking a snapshot

## Testing And Debuggability

Debuggability is already above average for firmware, and the biggest trace gap
from the start of this review is now closed.

The shared trace surface now records:

- transition plans
- executed effects
- release-resolution decisions
- hold-policy decisions
- pending multi-tap reuse/flush decisions
- pd-mode/layer/split-sync events

That is the right level for this codebase: the ring buffer stays compact, but
future key-runtime refactors no longer need to infer the release path only
from downstream effects or from console-only strings.

The remaining testing/debuggability work is less about trace vocabulary and
more about surface choice:

- keep moving higher-level tests toward scenario/builders plus
  `noah_runtime_debug_snapshot(...)`
- avoid treating raw shared-state layout as public API unless the test is
  explicitly about storage

## Recommended Refactor Order

Finding 6 is the main remaining structural follow-up:

1. Keep shifting higher-level tests toward semantic builders and richer traces.

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
