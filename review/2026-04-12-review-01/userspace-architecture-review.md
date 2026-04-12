# Userspace Architecture Review

Date: 2026-04-12

Status: new review created after
[2026-04-11-review-02](../2026-04-11-review-02/userspace-architecture-review.md).
This pass reviews the current `noah` userspace as it exists after the recent
handled-key reducer and multi-scan regression work already captured there.

Scope: software architecture, structure, and long-term extensibility inside
this repo only. Hardware changes are out of scope.

## Executive Summary

This is a disciplined firmware userspace. The strongest architectural choices
are still the right ones:

- the authored/runtime split between
  [`noah_keymap.h`](../../users/noah/noah_keymap.h),
  [`noah_runtime.h`](../../users/noah/noah_runtime.h), and
  [`noah_keymap_ids.h`](../../users/noah/noah_keymap_ids.h)
- manifest-driven pd modes through
  [`pd_mode_manifest.h`](../../users/noah/lib/pointing/pd_mode_manifest.h)
- explicit ownership modules for layers, modifiers, and held actions
- stage-based RGB rendering in
  [`rgb_runtime.c`](../../users/noah/lib/rgb/rgb_runtime.c)
- unusually strong host coverage, compile gates, and real-profile validation

The main remaining architectural risk is not broad repo structure. It is the
handled-key execution path and the number of cross-module protocols required to
move one key gesture from authored behavior to runtime side effects.

The core judgment for this codebase is:

- do not rewrite it into a generic plugin system
- do keep pushing the handled-key path toward one explicit event/effect model
- do keep pd modes static and manifest-driven, but give unusual modes a better
  lifecycle policy seam before the trait model grows another layer of central
  branches

## Architecture And Separation Of Concerns

### What is working well

- [`keyboards/.../keymaps/noah/keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
  is still a real authored-data surface, not a runtime dumping ground.
- [`users/noah/runtime_init.c`](../../users/noah/runtime_init.c) and
  [`users/noah/hooks.c`](../../users/noah/hooks.c) keep QMK hook integration
  narrow and explicit.
- [`users/noah/lib/compat/qmk_contract.h`](../../users/noah/lib/compat/qmk_contract.h)
  and
  [`users/noah/lib/compat/qmk_via_contract.h`](../../users/noah/lib/compat/qmk_via_contract.h)
  correctly isolate fork-specific contracts.
- Layer and modifier semantics have named homes in
  [`layer_ownership.c`](../../users/noah/lib/state/layer_ownership.c),
  [`keyboard_mod_ownership.c`](../../users/noah/lib/state/keyboard_mod_ownership.c),
  and [`held_action.c`](../../users/noah/lib/key/held_action.c).

### Main architecture issue: the handled-key path is still split across too many protocol layers

One handled key currently crosses all of these surfaces:

- [`key_runtime_process.c`](../../users/noah/lib/key/key_runtime_process.c)
- [`key_runtime_preflight.c`](../../users/noah/lib/key/key_runtime_preflight.c)
- [`key_runtime_press.c`](../../users/noah/lib/key/key_runtime_press.c)
- [`key_runtime_release.c`](../../users/noah/lib/key/key_runtime_release.c)
- [`key_runtime_transition.c`](../../users/noah/lib/key/key_runtime_transition.c)
- [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
- [`key_runtime_slot_release_reduce.c`](../../users/noah/lib/key/key_runtime_slot_release_reduce.c)
- [`key_runtime_slot_scan_reduce.c`](../../users/noah/lib/key/key_runtime_slot_scan_reduce.c)
- [`key_runtime_slot_policy.c`](../../users/noah/lib/key/key_runtime_slot_policy.c)
- [`action_lifecycle.c`](../../users/noah/lib/action/action_lifecycle.c)
- [`held_action.c`](../../users/noah/lib/key/held_action.c)

That is not automatically wrong. Firmware often needs orchestration,
state mutation, and side-effect execution to stay separate. The problem here is
that the separation is split by two axes at once:

- by physical event phase: process, preflight, press, release, scan
- by effect protocol: slot effect request, slot result effect, transition plan

The repo has already made this much better than it was on 2026-04-11, but the
current handled-key engine is still an implicit state machine spread across
storage helpers, reducer helpers, transition planning, and effect execution.

### Hidden dependency worth calling out explicitly

[`action_dispatch.c`](../../users/noah/lib/action/action_dispatch.c) looks like
a generic leaf, but `action_dispatch()` calls
`key_runtime_activate_pending_fallback_hold()` before it taps the action.
That means "dispatching an action" is not actually a pure output step; it can
retroactively mutate handled-key runtime state.

That coupling is defensible because fallback holds are part of the interaction
model, but it is easy for a maintainer to miss. Any new dispatch call site can
change key-runtime semantics, not just output behavior.

## Modularity And Extensibility

### Easy today

- adding new authored `key_behaviors[]` rows
- adding new layers
- adding new combos that respect userspace ownership rules
- adding normal new pd modes through the manifest and handler path
- adding RGB overlays that fit the current staged compositor

### Expensive today

- adding a new handled-key lifecycle concept
- changing the meaning of fallback hold, interrupt, or multi-tap hold
- introducing a pd mode with cross-cutting side effects that do not fit the
  current trait bits cleanly

### Why new key behaviors are easy

The authored path is coherent:

- schema in [`key_behavior.h`](../../users/noah/lib/key/key_behavior.h)
- lookup and validation in
  [`key_behavior_lookup.c`](../../users/noah/lib/key/key_behavior_lookup.c) and
  [`keymap_validation.c`](../../users/noah/lib/key/keymap_validation.c)
- materialization in
  [`keymap_materialize.h`](../../users/noah/lib/keymap_materialize.h)

That is a good extensibility shape for a fixed board. It keeps complexity on
the runtime side while the authoring surface remains declarative.

### Why new handled-key semantics are still hard

Adding one new lifecycle branch still tends to touch:

- resolved behavior rules in [`key_runtime.c`](../../users/noah/lib/key/key_runtime.c)
- slot state shape in
  [`runtime_shared_state.h`](../../users/noah/lib/state/runtime_shared_state.h)
- reducer policy in
  [`key_runtime_slot_policy.c`](../../users/noah/lib/key/key_runtime_slot_policy.c)
- release or scan resolution in the dedicated reducers
- transition planning in
  [`key_runtime_transition.c`](../../users/noah/lib/key/key_runtime_transition.c)
- feedback rules in
  [`key_runtime_feedback.c`](../../users/noah/lib/key/key_runtime_feedback.c)
- scenario and integration tests

That is the clearest sign that the lifecycle concept is still larger than the
current abstraction boundary around it.

## Abstractions And Interfaces

### Strong abstractions

- `noah_keymap.h` / `noah_runtime.h` / `noah_keymap_ids.h`
- `pd_mode_manifest.h` as a generated identity surface
- `qmk_contract.h` and `qmk_via_contract.h`
- the RGB stage pipeline in [`rgb_runtime.c`](../../users/noah/lib/rgb/rgb_runtime.c)
- the shared userspace source manifest in
  [`users/noah/source_manifest.mk`](../../users/noah/source_manifest.mk)

These are meaningful abstractions. They are narrow, stable, and already backed
by tests or compile gates.

### Leaky abstraction 1: `handled_key_view_t` is too thin

[`handled_key.h`](../../users/noah/lib/key/handled_key.h) exposes a
`handled_key_view_t`, but today that type is mostly a wrapper around
`key_behavior_view_t`.

The actual handled-key semantics are still recomputed in several places:

- tap action selection in [`key_runtime.c`](../../users/noah/lib/key/key_runtime.c)
- hold strategy selection in
  [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
- pd-mode lock behavior in
  [`key_runtime_slot_release_reduce.c`](../../users/noah/lib/key/key_runtime_slot_release_reduce.c)
- feedback interpretation in
  [`key_runtime_feedback.c`](../../users/noah/lib/key/key_runtime_feedback.c)

That means the "resolved handled key" abstraction does not yet own the full
meaning of a handled key.

This pass closed most of that gap for the live runtime path. The remaining
design choice for feedback and debugging should be: resolve once when the
handled press begins, cache the slot semantics needed by downstream readers,
and keep later consumers off mutable binding re-interpretation.

### Leaky abstraction 2: effect protocols are duplicated

The handled-key path currently has three related effect vocabularies:

- [`key_runtime_slot_effect.h`](../../users/noah/lib/key/key_runtime_slot_effect.h)
- [`key_runtime_slot_result.h`](../../users/noah/lib/key/key_runtime_slot_result.h)
- transition effects in
  [`key_runtime_transition.c`](../../users/noah/lib/key/key_runtime_transition.c)

This is cleaner than direct reducer side effects, but it still makes the
transition layer partly a translator instead of only an executor.

### Leaky abstraction 3: key-behavior lookup also owns policy

[`key_behavior_lookup.c`](../../users/noah/lib/key/key_behavior_lookup.c)
does more than translate authored rows. It also knows:

- raw QMK layer-action policy
- pd-mode keycode identity
- which held behaviors are supported

That is not a bug, but it means "behavior lookup" is also part of runtime
policy enforcement. The name suggests a narrower role than the file actually
owns.

## Code Organization And Structure

### Overall structure is good

The repo-level split is coherent:

- `keyboards/.../keymaps/noah/` for authored profile data
- `users/noah/lib/key/` for key engine logic
- `users/noah/lib/pointing/` for pd modes
- `users/noah/lib/rgb/` for RGB runtime and stages
- `users/noah/lib/state/` for ownership and sync
- `tests/host/` for host coverage

That is a maintainable top-level shape.

### The key runtime is still hard to discover as a flow

The key-runtime code is now smaller in individual chunks than it used to be,
but it is still spread across many similarly named files:

- `key_runtime.c`
- `key_runtime_process.c`
- `key_runtime_preflight.c`
- `key_runtime_press.c`
- `key_runtime_release.c`
- `key_runtime_scan.c`
- `key_runtime_transition.c`
- `key_runtime_slot_step.c`
- `key_runtime_slot_policy.c`
- `key_runtime_slot_release_reduce.c`
- `key_runtime_slot_scan_reduce.c`
- `key_runtime_slot_pending_multi_tap.c`

This is a naming problem as much as a size problem. The files are split by
implementation stage, but the conceptual model is "resolve a handled key,
reduce an event, produce effects, execute effects". The current filenames do
not make that model obvious to a new maintainer.

### `held_action.c` is carrying multiple responsibilities

[`held_action.c`](../../users/noah/lib/key/held_action.c) currently owns:

- per-key held modifier bindings
- per-key held non-modifier bindings
- repeat scheduling
- pointer-layer anchor maintenance for repeated mouse actions

That file is still internally coherent, but it is now a mixed ownership and
scheduler module rather than one single concept.

### Documentation is strong for capabilities, weaker for runtime internals

The docs set is good for users and for profile authoring:

- [`README.md`](../../README.md)
- [`docs/INTERACTION_MODEL.md`](../../docs/INTERACTION_MODEL.md)
- [`docs/ADDING_PD_MODE.md`](../../docs/ADDING_PD_MODE.md)

What is still missing is one stable maintainer-facing runtime map for the
handled-key engine. The review docs are useful, but they are time-scoped and
not the main discoverability surface.

## State Management And Flow

### Good state decisions

- handled-key state is keyed by physical position in
  [`runtime_shared_state.h`](../../users/noah/lib/state/runtime_shared_state.h)
- pd-mode active and locked flags also live in shared state
- release routing by physical position means layer changes during a hold do not
  strand the release behind a remapped keycode

For this fixed board, board-sized position tables are the right choice. This
repo should not optimize for a hypothetical generic keyboard framework.

### The remaining state problem is distributed observability

Not all runtime state lives in `runtime_shared_state_t`.

Additional mutable state still sits in file-local statics inside:

- [`layer_ownership.c`](../../users/noah/lib/state/layer_ownership.c)
- [`keyboard_mod_ownership.c`](../../users/noah/lib/state/keyboard_mod_ownership.c)
- [`held_action.c`](../../users/noah/lib/key/held_action.c)

That is fine for encapsulation, but it makes cross-subsystem debugging harder.
When a real interaction goes wrong, the relevant state is distributed across:

- key runtime slots
- pending multi-tap state
- held action ownership
- layer ownership
- managed modifier ownership
- pd-mode flags

There is no single read-only runtime snapshot surface for that full picture.

### Test reset patterns show the same gap

The host suite is broad, but many tests still carry custom reset plumbing such
as `test_reset_state()` or `test_reset_stubs()` in individual test files.
That is a practical sign that the runtime does not yet expose one clean reset
and snapshot surface for higher-level test harnesses.

## Scalability Of The Design

### What will scale well

- more authored data in `keymap.c`
- more validation rules
- more RGB stages or overlays in the current compositor model
- a moderate number of additional pd modes
- more host tests and compile-gated variants

### What will create debt fastest

- more handled-key lifecycle variants without first simplifying the reducer
  protocol
- more central pd-mode trait consumers for unusual modes
- more cross-module side effects that are only implied by naming, not encoded
  in a single event/effect model

### Important non-issue: raw lookup complexity

Some lookups are linear, for example `key_behavior_count` scans in
[`key_behavior_lookup.c`](../../users/noah/lib/key/key_behavior_lookup.c) and
mode scans across `PD_MODE_COUNT`. On this fixed board, that is acceptable.
The maintainability bottleneck is conceptual coupling, not raw lookup cost.

## Testing And Debuggability

### Current posture is strong

This repo is already far ahead of normal keyboard firmware code:

- 29 host test binaries currently live under `tests/host/`
- the full suite in
  [`run_all_host_tests.sh`](../../tests/host/run_all_host_tests.sh) covers
  ownership, pd modes, split sync, handled-key runtime, RGB, macros, and
  real-profile validation
- [`run_feature_gate_compile_tests.sh`](../../tests/host/run_feature_gate_compile_tests.sh)
  protects header boundaries and build-surface drift
- [`key_runtime_scenario_harness.c`](../../tests/host/key_runtime_scenario_harness.c)
  gives the handled-key runtime a more realistic event-trace surface
- [`key_runtime_trace.c`](../../users/noah/lib/key/key_runtime_trace.c)
  provides targeted console observability when enabled

### Remaining debug gap

The trace surface is textual and transition-centric. It does not expose one
typed snapshot of all relevant runtime ownership state before and after a
gesture. That makes regression debugging more manual than it needs to be.

The current scenario harness is also focused on the key runtime. There is not
yet an equivalent higher-level scripted harness for:

- pd-mode lifecycle plus layer policy
- held-action ownership plus mod ownership
- split-sync state mirroring as one traceable scenario

## Concrete Recommendations

### 1. Introduce a real `resolved_handled_key_t`

Create one resolved handled-key object that owns:

- tap action
- hold behavior
- long-hold behavior
- hold strategy
- layer identity if applicable
- pd-mode identity if applicable
- timing values
- capability flags such as fallback hold, implicit hold, multi-tap

That would replace the current pattern where those facts are re-derived across
`key_runtime.c`, `key_runtime_slot_step.c`,
`key_runtime_slot_release_reduce.c`, and `key_runtime_feedback.c`.

Suggested shape:

```c
typedef struct {
    uint16_t keycode;
    uint16_t tap_action;
    hold_behavior_t hold;
    hold_behavior_t long_hold;
    key_runtime_slot_hold_strategy_t hold_strategy;
    uint16_t tap_hold_term;
    uint16_t longer_hold_term;
    uint16_t multi_tap_term;
    uint8_t layer;
    pd_mode_mask_t pd_mode;
    uint16_t flags;
} resolved_handled_key_t;
```

Status after this review pass:

- implemented via the expanded `handled_key_view_t`
- press, release, pending multi-tap, and transition planning now consume the
  resolved handled-key object directly
- feedback preview-layer now reads cached slot semantic metadata seeded from
  that resolved object instead of re-resolving against slot bindings
- debug/test work still needs one shared runtime snapshot surface

Priority: high.

### 2. Collapse slot-result and transition effects into one effect vocabulary

Once the handled-key resolver above exists, reduce the remaining protocol
layers by giving the reducer one stable effect type that the executor can run
directly.

Suggested direction:

```c
typedef enum {
    NOAH_RUNTIME_EFFECT_DISPATCH_ACTION,
    NOAH_RUNTIME_EFFECT_HELD_REGISTER,
    NOAH_RUNTIME_EFFECT_HELD_UNREGISTER,
    NOAH_RUNTIME_EFFECT_RELEASE_OWNED_STATE,
    NOAH_RUNTIME_EFFECT_REPEAT_START,
    NOAH_RUNTIME_EFFECT_LAYER_PRESS,
    NOAH_RUNTIME_EFFECT_LAYER_RELEASE,
    NOAH_RUNTIME_EFFECT_FEEDBACK_PULSE,
    NOAH_RUNTIME_EFFECT_PD_MODE_LOCK_TAP,
    NOAH_RUNTIME_EFFECT_DELAYED_ACTION,
} noah_runtime_effect_kind_t;

typedef struct {
    noah_runtime_effect_kind_t kind;
    keypos_t                   key_pos;
    union { /* action/layer/pd payloads */ } data;
} noah_runtime_effect_t;
```

Status after this review pass: implemented in
[`key_runtime_effect.h`](../../users/noah/lib/key/key_runtime_effect.h),
[`key_runtime_slot_result.c`](../../users/noah/lib/key/key_runtime_slot_result.c),
and
[`key_runtime_transition.c`](../../users/noah/lib/key/key_runtime_transition.c).

That would let [`key_runtime_transition.c`](../../users/noah/lib/key/key_runtime_transition.c)
be mostly an executor instead of a translator.

Priority: high.

### 3. Keep pd modes static, but add lifecycle policy hooks before adding the next unusual mode

Do not introduce a full plugin system. The current manifest model is the right
fit for this board. The next step should be smaller:

- keep `pd_mode_def_t`
- keep manifest-generated identity
- add optional activation/deactivation policy hooks for cross-cutting side
  effects that do not belong in central trait branches

Suggested shape:

```c
typedef struct {
    void (*on_activate)(pd_mode_mask_t mode);
    void (*on_deactivate)(pd_mode_mask_t mode);
    void (*on_lock)(pd_mode_mask_t mode);
    void (*on_unlock)(pd_mode_mask_t mode);
} pd_mode_lifecycle_hooks_t;
```

That would keep future mode-specific auto-mouse, modifier, or side-effect
rules from pushing more logic into
[`pd_mode_registry.c`](../../users/noah/lib/pointing/pd_mode_registry.c) and
[`pointer_layer_policy.c`](../../users/noah/lib/pointing/pointer_layer_policy.c).

Status after this review pass:

- implemented as an internal optional lifecycle-hook seam in
  [`pd_mode_registry.c`](../../users/noah/lib/pointing/pd_mode_registry.c)
- normal pd-mode additions still stay on the manifest-first path; standard
  modes do not need extra runtime wiring
- pinch mode's left-GUI ownership moved off a dedicated manifest trait and onto
  lifecycle hooks
- shared lock/unlock auto-mouse ownership now runs through the same lifecycle
  surface while remaining trait-driven for common lockable modes

Priority: medium.

### 4. Separate held-action ownership from repeat scheduling

Split [`held_action.c`](../../users/noah/lib/key/held_action.c) into:

- held modifier/action ownership
- repeat scheduling and pointer-anchor behavior

Keep the public API stable if desired. The point is not file-count purity; it
is making "ownership registry" and "time-based repeat engine" distinct
maintenance units.

Priority: medium.

### 5. Add one read-only runtime snapshot API and one shared test reset API

Expose a debug and test surface that can reset and inspect the whole runtime
without each test file rebuilding that logic.

Suggested direction:

```c
typedef struct {
    runtime_shared_state_t core;
    /* read-only snapshots from layer ownership, held actions, and mod ownership */
} noah_runtime_debug_snapshot_t;

void noah_runtime_debug_snapshot(noah_runtime_debug_snapshot_t *out);
void noah_runtime_reset_for_test(void);
```

Status after this review pass:

- implemented in
  [`runtime_debug.h`](../../users/noah/lib/state/runtime_debug.h) and
  [`runtime_debug.c`](../../users/noah/lib/state/runtime_debug.c)
- layer ownership, held-action ownership, and keyboard modifier ownership now
  expose read-only debug snapshots for higher-level tests
- host tests can reset the shared runtime through one helper instead of mixing
  ad hoc `noah_runtime_shared_state` zeroing with subsystem-specific cleanup
  calls
- `runtime_shared_state_reset()` now restores valid slot defaults for the key
  runtime core state instead of leaving test resets to raw zeroed storage

This would make scenario-level assertions and failure diagnosis faster,
especially for cross-subsystem bugs.

Priority: medium.

### 6. Write one permanent maintainer doc for the key runtime

Add something like `docs/KEY_RUNTIME_ARCHITECTURE.md` that explains:

- event entry points
- reducer stages
- slot phases
- effect execution
- ownership interactions

The repo already documents user-facing behavior well. It now needs one stable
internal map for the most complex subsystem.

Priority: medium.

## Final Assessment

This userspace is in good architectural shape overall. The repo has already
solved the hard foundational problems that many QMK userspaces never solve:
explicit ownership, real authored/runtime separation, strong test coverage, and
fork contracts kept behind compat layers.

The remaining work is now narrower and more technical: make the handled-key
runtime easier to extend without touching half a dozen protocol layers, and
give pd modes one better seam for unusual policy before central branching grows
again. If those two areas are addressed, the current structure should age well
without a large rewrite.
