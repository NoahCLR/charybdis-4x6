# Userspace Architecture Review

Date: 2026-04-11

Status: active review created after [review-01](../2026-04-11-review-01/userspace-architecture-review.md). Follow-up work has already landed for the shared source manifest, the first structured key-runtime scenario harness, a dedicated key-runtime admission boundary, explicit handled-key slot lifecycle phase plus hold-strategy state, a shared slot-result surface between slot reduction and `key_runtime_transition.c`, a reducer-style `key_runtime_slot_step(...)` seam, a consolidated reducer implementation in `key_runtime_slot_step.c`, phase-local active scan and active release handling, explicit local pending-multi-tap release/scan handling, explicit handled-press context/outcome handling, reducer-owned lifecycle effect transitions that removed `key_runtime_slot_effect.c` from the runtime build surface, a position-indexed handled-key slot table that removed the fixed two-slot overlap ceiling, grouped handled-key slot owner/lifecycle/binding/timing subrecords in shared state, explicit event/phase handler tables inside `key_runtime_slot_step.c`, dedicated internal reducer-policy homes for lifecycle effects and pending multi-tap handling, removal of the flat slot-state compatibility overlay, internal release/scan reducer modules that shrink the main slot-step file, and position-owned handled-key release routing so layer changes during a held key do not strand slot state behind a remapped release keycode; the remaining recommendations below focus on what is still architecturally open after those changes.

Scope: the `noah` userspace in this repo only. This review ignores hardware changes and evaluates software structure, boundaries, state flow, extension cost, and verification surfaces.

## Executive Summary

This is a strong firmware userspace by QMK standards. The repo already has the
right big pieces:

- a real authored-data/runtime split between [`noah_keymap.h`](../../users/noah/noah_keymap.h), [`noah_runtime.h`](../../users/noah/noah_runtime.h), and [`noah_keymap_ids.h`](../../users/noah/noah_keymap_ids.h)
- explicit ownership modules for layers and modifiers
- manifest-driven pointing-device modes
- stage-based RGB rendering
- broad host coverage plus compile-gated header boundaries

The main architectural risk is now concentrated in one area: the handled-key
runtime. The old single-active-key bottleneck is gone, and the follow-up work
has already improved the surrounding structure by splitting admission policy
into its own module, adding a scenario-level host harness, moving to explicit
slot lifecycle phase and hold-strategy state, consolidating handled press,
release, scan, interrupt, and flush reduction behind one slot-step seam,
extracting lifecycle-effect plus pending-multi-tap policy into dedicated
internal helper modules, removing the flat slot-state compatibility overlay,
and splitting release/scan reduction into private event-family reducers. That
is a much better architecture than the earlier helper pile, but it is still
not yet a smaller explicit phase/event FSM core, and the handled-key engine is
still the place most likely to fight the next real feature.

The current extension cost looks like this:

- easy: new authored key behaviors, new layers, new RGB data, normal new pd
  modes that fit the existing manifest traits
- medium: new mode policy that needs another shared trait or another central
  policy consumer
- hard: new handled-key semantics or any feature that changes the handled-key
  lifecycle itself

## What Is Working Well

### 1. The authored/runtime boundary is now explicit and enforceable

The split between authoring, shared ids, and hook entry points is clear:

- [`users/noah/noah_keymap.h`](../../users/noah/noah_keymap.h)
- [`users/noah/noah_runtime.h`](../../users/noah/noah_runtime.h)
- [`users/noah/noah_keymap_ids.h`](../../users/noah/noah_keymap_ids.h)

That boundary is not just documented; it is compile-gated in
[`tests/host/run_feature_gate_compile_tests.sh`](../../tests/host/run_feature_gate_compile_tests.sh).
This is one of the best long-term maintainability decisions in the repo.

Why it matters:

- authored profile data stays in the keymap path
- runtime modules consume a narrower shared interface
- hook overrides are treated as integration work instead of normal data authoring

### 2. Ownership-heavy concerns have named homes

Layer and modifier ownership are both modeled explicitly instead of being left
to ad hoc QMK side effects:

- [`users/noah/lib/state/layer_ownership.c`](../../users/noah/lib/state/layer_ownership.c)
- [`users/noah/lib/state/keyboard_mod_ownership.c`](../../users/noah/lib/state/keyboard_mod_ownership.c)

These modules do a good job of turning hard-to-debug firmware behavior into
bounded local policy with tests.

### 3. The pd-mode system is genuinely data-driven for ordinary growth

The manifest model in
[`users/noah/lib/pointing/pd_mode_manifest.h`](../../users/noah/lib/pointing/pd_mode_manifest.h)
is the right abstraction for mode identity. A normal new mode can stay mostly
manifest/handler/keymap/RGB/doc work, and that is exactly the extension shape a
firmware userspace should aim for.

The fact that the repo has a dedicated maintainer workflow doc in
[`docs/ADDING_PD_MODE.md`](../../docs/ADDING_PD_MODE.md) is also a positive
architectural signal: the intended extension path is explicit.

### 4. The RGB runtime is decomposed into meaningful render stages

[`users/noah/lib/rgb/rgb_runtime.c`](../../users/noah/lib/rgb/rgb_runtime.c)
keeps composition order obvious: base layer frame, preview, pd-mode overlay,
then interaction feedback. This is easier to reason about than the usual
single-hook RGB pileup and should scale cleanly for more overlays.

### 5. Testability is much better than typical firmware code

The host suite covers ownership, validation, pd modes, split sync, the handled
key runtime, macro/VIA paths, and RGB rendering. The compile gates also check
header boundaries and traced-build variants.

This matters because architecture only stays good if the invariants are cheap to
recheck.

## Main Findings

### 1. The handled-key runtime is still an implicit FSM, not an explicit one

The strongest remaining architectural smell is in the key runtime:

- [`users/noah/lib/state/runtime_shared_state.h`](../../users/noah/lib/state/runtime_shared_state.h)
- [`users/noah/lib/key/key_runtime_admission.c`](../../users/noah/lib/key/key_runtime_admission.c)
- [`users/noah/lib/key/key_runtime_slot_effect.h`](../../users/noah/lib/key/key_runtime_slot_effect.h)
- [`users/noah/lib/key/key_runtime.c`](../../users/noah/lib/key/key_runtime.c)
- [`users/noah/lib/key/key_runtime_slot_policy.c`](../../users/noah/lib/key/key_runtime_slot_policy.c)
- [`users/noah/lib/key/key_runtime_slot_pending_multi_tap.c`](../../users/noah/lib/key/key_runtime_slot_pending_multi_tap.c)
- [`users/noah/lib/key/key_runtime_slot_release_reduce.c`](../../users/noah/lib/key/key_runtime_slot_release_reduce.c)
- [`users/noah/lib/key/key_runtime_slot_scan_reduce.c`](../../users/noah/lib/key/key_runtime_slot_scan_reduce.c)
- [`users/noah/lib/key/key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
- [`users/noah/lib/key/key_runtime_state.h`](../../users/noah/lib/key/key_runtime_state.h)
- [`users/noah/lib/key/key_runtime_slot.c`](../../users/noah/lib/key/key_runtime_slot.c)
- [`users/noah/lib/key/key_runtime_transition.c`](../../users/noah/lib/key/key_runtime_transition.c)

What is good now:

- slot ownership is explicit
- slot admission and reclaim lookup now have a named home in
  [`key_runtime_admission.c`](../../users/noah/lib/key/key_runtime_admission.c)
- the shared effect-request contract now has a narrow named home in
  [`key_runtime_slot_effect.h`](../../users/noah/lib/key/key_runtime_slot_effect.h)
- handled press, release, scan, interrupt, and pending multi-tap flush
  reduction now all live in
  [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
- handled press, release, scan, interrupt, and pending multi-tap flush now
  route through the reducer-style slot-step contract in
  [`key_runtime_slot_step.h`](../../users/noah/lib/key/key_runtime_slot_step.h)
  instead of `key_runtime_transition.c` calling fragmented per-event entry
  points directly
- press, release, scan, interrupt, and pending multi-tap flush paths now all
  adapt into a shared slot-result surface in
  [`key_runtime_slot_result.c`](../../users/noah/lib/key/key_runtime_slot_result.c)
  before the transition layer plans system effects
- the reducer switch plus handled press/release/scan/interrupt/flush result
  production now live in
  [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
  instead of being split across separate event entry points
- scan-specific resolution/apply details now stay private to
  [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
  instead of leaking through the public header boundary
- active scan now runs through explicit phase-local reducer branches inside
  [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
  instead of a separate resolution/apply mini-protocol
- active release now runs through explicit phase-local reducer branches inside
  [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
  instead of one larger release-resolution condition pile
- slot state now has named `owner`, `lifecycle`, `binding`, and `timing`
  subrecords in
  [`runtime_shared_state.h`](../../users/noah/lib/state/runtime_shared_state.h),
  so runtime code no longer has to treat the handled-key slot as one
  undifferentiated flat record
- host fixtures and runtime code now both use that grouped slot-state model;
  the temporary flat compatibility overlay is gone
- pending multi-tap release and pending multi-tap scan now also reduce through
  a named internal helper module in
  [`key_runtime_slot_pending_multi_tap.c`](../../users/noah/lib/key/key_runtime_slot_pending_multi_tap.c)
  instead of ad hoc resolve-then-patch logic inside the main reducer file
- handled press now reduces through explicit local press contexts and named
  outcomes inside
  [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
- active-scan phase dispatch, release phase resolution, and top-level slot
  event dispatch now flow through explicit handler tables in
  [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
  instead of only through large local switch statements
- press-begin, release-resolution, and most lifecycle effect planning no
  longer sit behind separate runtime modules; they now live directly inside the
  slot reducer
- release-hold selection, fallback-hold activation, interrupt policy, flush
  behavior, threshold-hold dispatch, and long-hold promotion now have a named
  internal home in
  [`key_runtime_slot_policy.c`](../../users/noah/lib/key/key_runtime_slot_policy.c)
  instead of remaining as one larger helper pile inside the reducer
- handled release and active scan now also have dedicated private reducer homes
  in
  [`key_runtime_slot_release_reduce.c`](../../users/noah/lib/key/key_runtime_slot_release_reduce.c)
  and
  [`key_runtime_slot_scan_reduce.c`](../../users/noah/lib/key/key_runtime_slot_scan_reduce.c),
  so `key_runtime_slot_step.c` reads more like an event router plus handled
  press reducer instead of a monolithic reducer file
- active slot lifecycle state now has explicit `phase` and `hold_strategy`
  fields instead of the older hold/strategy boolean combination
- fallback hold activation outside the reducer now has a small dedicated
  runtime helper in [`key_runtime.c`](../../users/noah/lib/key/key_runtime.c)
- effect execution is still separated from state mutation through the shared
  `key_runtime_slot_effect_request_t` contract
- [`key_runtime_transition.c`](../../users/noah/lib/key/key_runtime_transition.c)
  now consumes one shared slot-result shape instead of translating
  press/release/scan-specific structs separately

What is still expensive:

- `active_key_state_t` is now grouped into owner/lifecycle/binding/timing
  subrecords everywhere, but it still carries a fairly broad mutable lifecycle
  and timing surface for one slot record
- [`key_runtime_state.h`](../../users/noah/lib/key/key_runtime_state.h) exposes a
  narrower storage-oriented surface plus shared slot helpers
- one public slot event/result model now exists, and one reducer
  entry point now exists in `key_runtime_slot_step.c`, but the reducer family
  still spans several files and still does not collapse into one smaller typed
  phase/event FSM core
- the transition layer no longer knows the press/release/scan-specific local
  structs, and those extra structs are gone; the remaining fragmentation is now
  in implementation logic rather than in cross-module result protocols
- the reducer now coordinates smaller internal policy surfaces, but the
  underlying phase transitions still do not share one tighter phase/event
  state-machine surface

Why this matters:

- adding one new hold mode or one new interrupt rule is still likely to touch
  storage, reducer-local helper paths, transition planning, and tests in
  several files, even if some of those policy families now have better named
  homes
- invalid or surprising state combinations are constrained more clearly than
  before, but still rely partly on reducer conventions instead of one smaller
  typed transition table
- the code is modular by file, but the mental model is still spread across one
  large reducer plus several storage and transition helpers

This is the subsystem most likely to accumulate technical debt.

### 2. Handled-key storage is now keyed by physical position

The old fixed overlap ceiling is gone:

- handled-key storage now uses a board-sized table in
  [`users/noah/lib/state/runtime_shared_state.h`](../../users/noah/lib/state/runtime_shared_state.h)
- press admission now maps directly by physical key position in
  [`users/noah/lib/key/key_runtime_admission.c`](../../users/noah/lib/key/key_runtime_admission.c)
- pending multi-tap state stays co-located with the same physical-key slot in
  [`users/noah/lib/key/key_runtime_slot.c`](../../users/noah/lib/key/key_runtime_slot.c)

Why this matters:

- distinct handled keys no longer evict each other just because two unrelated
  positions are already busy
- the storage model now matches the scaling shape already used by
  [`layer_ownership.c`](../../users/noah/lib/state/layer_ownership.c)
- the remaining handled-key complexity is now mostly reducer/state-shape
  complexity rather than slot-capacity policy

The remaining tradeoff is simpler: storage now scales with matrix size, and the
runtime scans a larger table for feedback and transition passes. That is a much
better trade than the old correctness boundary for this board.

### 3. Pd-mode traits are effective, but the next novel policy will still land in central code

The pd-mode system is in a good state for normal growth, but not yet at a true
plugin boundary.

Evidence:

- trait definitions live in
  [`users/noah/lib/pointing/pd_mode_manifest.h`](../../users/noah/lib/pointing/pd_mode_manifest.h)
- registry-side behavior still consumes those traits centrally in
  [`users/noah/lib/pointing/pd_mode_registry.c`](../../users/noah/lib/pointing/pd_mode_registry.c)
- pointer-layer behavior also consumes them centrally in
  [`users/noah/lib/pointing/pointer_layer_policy.c`](../../users/noah/lib/pointing/pointer_layer_policy.c)

What this means in practice:

- ordinary new modes are easy
- modes with a genuinely new cross-cutting policy still require:
  - a new trait bit
  - at least one new central consumer branch
  - test and doc updates across multiple modules

That is acceptable today. It just means the trait system should be treated as a
midpoint, not as the final extensibility model.

### 4. Structured scenario coverage exists now, and the first reducer matrix is landing

The handled-key runtime now has a real scenario harness:

- [`tests/host/key_runtime_scenario_harness.c`](../../tests/host/key_runtime_scenario_harness.c)
- [`tests/host/key_runtime_scenario_test.c`](../../tests/host/key_runtime_scenario_test.c)

This closes an earlier testing gap, and the first high-risk reducer traces now
exist there too.

Why this matters:

- the current harness now covers:
  - single-tap timeout resolution
  - pending multi-tap reclaim before a new handled press
  - interrupt-driven fallback hold activation
  - long-hold promotion after immediate-hold registration
  - third distinct handled press preserving the earlier active positions
- that is enough coverage to protect the next reducer cleanups better than
  before
- the remaining reducer/FSM refactor will be safer once more of the current
  behavior is captured as reusable event traces
- the scenario surface is now the right place to pin down semantics that are too
  cross-cutting for slot-level unit tests alone

## Review By Priority

### Architecture and separation of concerns

Strong:

- authored profile data is kept data-driven in
  [`keyboards/.../keymaps/noah/keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- runtime entry points are centralized in
  [`users/noah/runtime_init.c`](../../users/noah/runtime_init.c) and
  [`users/noah/hooks.c`](../../users/noah/hooks.c)
- layer/mod ownership, pd modes, RGB, and split sync each have a named module

Weakest remaining boundary:

- handled-key behavior is split into many helpers, but the lifecycle semantics
  are not represented as one explicit state model

### Modularity and extensibility

Easy today:

- add new `key_behaviors[]` rows
- add new layers
- add new combo outputs that respect userspace ownership rules
- add normal new pd modes
- add RGB authored data

Hard today:

- add a new handled-key lifecycle concept
- increase overlapping handled-key concurrency
- add a pd-mode policy that does not fit current traits cleanly

### Abstractions and interfaces

Good abstractions:

- `noah_keymap.h` / `noah_runtime.h` / `noah_keymap_ids.h`
- `layer_ownership`
- `keyboard_mod_ownership`
- the pd-mode manifest
- staged RGB rendering

Leaky abstractions:

- the key runtime reducer is still larger than the lifecycle concept it is
  trying to model
- `active_key_state_t` is grouped clearly now, but it still exposes a fairly
  wide mutable slot record for the long-term reducer model

### Code organization and structure

Good:

- top-level foldering under `users/noah/lib/`
- human-facing docs in `README.md` and `docs/`
- authored profile discoverability in `keymap.c`

Needs attention:

- `key_runtime_slot_step.c` is still too large to be the final long-term home
  of handled-key lifecycle policy
- `active_key_state_t` now reads more clearly, but the slot record itself is
  still broader than an eventual tighter FSM state surface

### State management and flow

Positive:

- central shared state ownership is clearer than file-local globals
- press/release/scan flow is understandable at the module level

Risk:

- slot phase and event dispatch are explicit now, and effect planning,
  pending-multi-tap handling, release, and active scan all have named internal
  homes, but those reducers still have to stay in sync by convention
- interrupt and flush policy no longer live as anonymous local reducer helpers,
  but they are still policy conventions rather than one smaller typed FSM core

### Scalability of the design

The repo will scale well for:

- more authored data
- more tests
- a modest number of additional pd modes
- more RGB overlays built in the current staged model

The repo will scale poorly for:

- significantly richer handled-key lifecycles
- more lifecycle variants landing in `key_runtime_slot_step.c` without another
  reducer/FSM cleanup

### Testing and debuggability

Current testing/debugging posture is strong:

- full host suite in
  [`tests/host/run_all_host_tests.sh`](../../tests/host/run_all_host_tests.sh)
- compile-boundary checks in
  [`tests/host/run_feature_gate_compile_tests.sh`](../../tests/host/run_feature_gate_compile_tests.sh)
- optional console tracing in
  [`users/noah/lib/key/key_runtime_trace.c`](../../users/noah/lib/key/key_runtime_trace.c)

Remaining limitation:

- the repo now has a structured scenario oracle, but only a small number of
  runtime traces are captured there so far

## Concrete Recommendations

### Recommendation 1: keep tightening the handled-key reducer into a smaller explicit FSM core

The first major reducer step is now done:

- one slot-level update entry point now exists in
  [`key_runtime_slot_step.h`](../../users/noah/lib/key/key_runtime_slot_step.h)
- handled press, release, scan, interrupt, and flush all reduce through
  [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
- the shared effect output contract is now narrow and explicit in
  [`key_runtime_slot_effect.h`](../../users/noah/lib/key/key_runtime_slot_effect.h)
- active-scan phase dispatch, release phase resolution, and top-level slot
  event dispatch now run through explicit handler tables inside
  [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
- slot state is grouped into named owner/lifecycle/binding/timing subrecords in
  [`runtime_shared_state.h`](../../users/noah/lib/state/runtime_shared_state.h)
- the flat compatibility overlay is gone; tests and production now share the
  same grouped slot-state model
- lifecycle-effect policy now has a small named internal home in
  [`key_runtime_slot_policy.c`](../../users/noah/lib/key/key_runtime_slot_policy.c)
- pending multi-tap release/scan policy now has a named internal home in
  [`key_runtime_slot_pending_multi_tap.c`](../../users/noah/lib/key/key_runtime_slot_pending_multi_tap.c)
- active release and active scan now also have named internal reducer homes in
  [`key_runtime_slot_release_reduce.c`](../../users/noah/lib/key/key_runtime_slot_release_reduce.c)
  and
  [`key_runtime_slot_scan_reduce.c`](../../users/noah/lib/key/key_runtime_slot_scan_reduce.c)

The remaining goal is to make the reducer implementation itself look more like
an explicit phase/event FSM core.

Target shape:

```c
typedef enum {
    KEY_SLOT_IDLE,
    KEY_SLOT_PRESSED,
    KEY_SLOT_PENDING_MULTI_TAP,
    KEY_SLOT_HELD,
    KEY_SLOT_REPEAT,
    KEY_SLOT_PENDING_HOLD_RELEASE,
} key_slot_phase_t;

typedef enum {
    KEY_EVENT_PRESS,
    KEY_EVENT_RELEASE,
    KEY_EVENT_SCAN_TAP_TERM,
    KEY_EVENT_SCAN_LONG_HOLD_TERM,
    KEY_EVENT_OTHER_KEY_PRESS,
    KEY_EVENT_FORCE_FLUSH,
} key_event_kind_t;

typedef struct {
    key_event_kind_t kind;
    uint16_t keycode;
    keypos_t key_pos;
    uint16_t elapsed;
} key_runtime_event_t;

key_runtime_effects_t key_runtime_slot_step(key_runtime_slot_t *slot,
                                            key_runtime_event_t event,
                                            key_behavior_view_t behavior);
```

Benefits:

- fewer internal protocol types in headers
- legal states become type-shaped instead of comment-shaped
- new behavior work lands mostly in one reducer plus effect tests

### Recommendation 2: keep the position-indexed storage model explicit

This storage refactor is now done:

- handled-key slots are keyed by physical position
- slot selection no longer depends on reclaiming a tiny overlap pool
- admission coverage now protects the direct per-position selection contract

The remaining guidance is to keep that boundary explicit:

- avoid reintroducing tiny-capacity slot assumptions in new runtime helpers or
  tests
- keep whole-table scans centralized to the places that actually need them
- keep new runtime code on the named owner/lifecycle/binding/timing groups
  instead of drifting back to one flat slot record everywhere
- treat the remaining reducer shape and the breadth of the slot record, not
  slot capacity, as the next handled-key simplification targets

### Recommendation 3: promote pd-mode traits into a small policy object before the next unusual mode

Do not over-engineer this yet, but prepare for the first mode that wants a new
cross-cutting behavior.

Suggested direction:

```c
typedef struct {
    pd_mode_traits_t traits;
    void (*on_activate)(pd_mode_mask_t mode);
    void (*on_deactivate)(pd_mode_mask_t mode);
    void (*on_lock)(pd_mode_mask_t mode);
    void (*on_unlock)(pd_mode_mask_t mode);
} pd_mode_policy_t;
```

The point is not dynamic plugins. The point is to keep novel mode policy from
turning `pd_mode_registry.c` into another central accumulator.

### Recommendation 4: keep growing the scenario harness into a reducer regression matrix

The initial matrix now exists, and the first high-risk traces have landed:

- reclaim after pending multi-tap ownership
- interrupt-driven fallback hold activation
- long-hold promotion after immediate-hold registration
- third distinct handled press preserving the earlier active positions

Next traces worth adding:

- pd-mode lock-tap release behavior after a locked press
- release-hold-pending long-hold promotion across scan then release
- reclaim behavior when a held action survives flush
- multi-tap pending-hold release paths that intentionally preserve the chain

## Bottom Line

This codebase is not suffering from broad architectural failure. The top-level
design is already disciplined and significantly ahead of typical keyboard
firmware repos.

The main thing to protect now is not the outer shape of the repo; it is the
internal shape of the handled-key engine. The manifest deduplication, scenario
harness, admission split, reducer seam, phase/hold-strategy extraction,
phase-local release and scan handling, explicit pending multi-tap handling,
explicit handled-press outcomes, reducer-owned lifecycle effect transitions,
grouped slot subrecords, dedicated internal slot-policy and pending-multi-tap
helper modules, private release/scan reducers, and explicit phase/event
dispatch tables were the right moves. The position-indexed slot table was the
other major structural fix. The remaining handled-key work is now narrower:
either stop here with a much better reducer and storage surface, or later keep
shrinking the reducer family and slot-state surface toward a smaller explicit
FSM core.
