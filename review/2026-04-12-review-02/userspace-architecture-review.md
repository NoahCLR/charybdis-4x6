# Userspace Architecture Review

Date: 2026-04-12

Status: new review created after
[2026-04-12-review-01](../2026-04-12-review-01/userspace-architecture-review.md).
This pass reassesses the repo after the handled-key, pd-mode lifecycle,
runtime-debug, and held-repeat follow-up work already recorded there.

Scope: software architecture, structure, and long-term extensibility inside
this repo only. Hardware concerns are intentionally out of scope.

## Executive Summary

The codebase is in a good architectural place for a fixed board. The major
foundations are correct and worth preserving:

- the authored/runtime split between
  [`keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c),
  [`noah_keymap.h`](../../users/noah/noah_keymap.h),
  [`noah_keymap_ids.h`](../../users/noah/noah_keymap_ids.h), and
  [`noah_runtime.h`](../../users/noah/noah_runtime.h)
- the source-manifest build surface in
  [`source_manifest.mk`](../../users/noah/source_manifest.mk)
- manifest-driven pd-mode identity in
  [`pd_mode_manifest.h`](../../users/noah/lib/pointing/pd_mode_manifest.h)
- the staged RGB renderer in
  [`rgb_runtime.c`](../../users/noah/lib/rgb/rgb_runtime.c)
- unusually strong host coverage, compile gates, and authored-profile
  validation

The follow-up items identified at the start of this review have now landed.
What remains are localized maintenance concerns, not urgent architecture
debt. The recommendation is still not a rewrite and not a plugin framework:
preserve the current data-driven design and only make further local cleanups
when a concrete extension actually needs them.

## Status Update After Initial Implementation

The first follow-up item from this review has now landed in the repo:

- explicit emission helpers now live in
  `users/noah/lib/action/action_dispatch.*`
- handled-key transitions, direct lock taps, held-repeat dispatch,
  delayed-action replay, and pd-mode tap helpers now use that explicit surface
- pd-mode helpers no longer call fallback-hold activation directly

`action_dispatch()` still exists as a compatibility wrapper with the
historical runtime-default policy, but it is no longer the only intended
output seam. The remaining implementation priority now starts with pd-mode
lifecycle ownership and the scenario harness rebuild.

## Status Update After Pd-Mode Lifecycle Refactor

The second follow-up item from this review has now landed in the repo:

- `pd_mode_def_t` rows now own an optional lifecycle pointer
- the manifest row shape now carries lifecycle ownership for unusual modes
- the registry no longer selects per-mode lifecycle hooks through a central
  switch

The remaining implementation priority is now the scenario-harness rebuild and
the smaller handled-key interface cleanup.

## Status Update After Scenario Harness Refactor

The third follow-up item from this review has now landed in the repo:

- the scenario harness now stores shared `key_runtime_effect_t` payloads
  directly
- scenario resets now flow through `noah_runtime_reset_for_test()`
- scenario assertions now consume the shared effect union instead of a
  harness-only flattened payload

The remaining implementation priority is now the smaller handled-key
interface cleanup.

## Status Update After Effect Vocabulary Cleanup

The fourth follow-up item from this review has now landed in the repo:

- the reducer-local request layer now reads as a builder surface
- slot results now carry `key_runtime_effect_t` directly
- transition plans now carry `key_runtime_effect_t` directly
- transition tracing and host tests now assert the shared effect vocabulary
  without slot-result or transition-specific re-export macros

There are no remaining required implementation items from this review.

## Architecture And Separation Of Concerns

### What is working well

- `keymap.c` is still a real authored-data unit, not a runtime spillover file.
- `hooks.c` and `runtime_init.c` keep QMK entry points narrow and explicit.
- `qmk_contract.*` and `qmk_via_contract.*` still isolate fork-specific QMK
  assumptions correctly.
- Ownership concerns have real homes:
  - `layer_ownership.c`
  - `keyboard_mod_ownership.c`
  - `held_action.c`
  - `held_repeat.c`
- The key runtime now has a shared effect vocabulary and a shared runtime
  snapshot surface. That is materially better than the architecture captured
  in older reviews.

### Handled-key effects now read as one pipeline

The runtime now has one obvious executable effect vocabulary in
[`key_runtime_effect.h`](../../users/noah/lib/key/key_runtime_effect.h).

The remaining reducer-local helper in
[`key_runtime_slot_effect.h`](../../users/noah/lib/key/key_runtime_slot_effect.h)
is intentionally narrower: it is a builder surface for slot-policy code, not
another executable effect dialect. Slot results and transition plans now carry
`key_runtime_effect_t` directly.

## Modularity And Extensibility

### Easy today

- adding new authored `key_behaviors[]` rows
- adding layers
- adding combos that stay within userspace ownership rules
- adding ordinary pd modes through the manifest/handler path
- adding RGB overlays that fit the current stage compositor

### Expensive today

- extending the handled-key reducer with another effect type or another stage
  without updating several interface layers
- renaming or reorganizing the reducer/executor surface without touching
  multiple effect-facing headers
- adding another bespoke pd mode with substantial local state without putting
  more pressure on `pd_mode_handlers.c`
- expanding high-level scenario testing still requires explicit shared-effect
  assertions, even though it no longer needs a harness-only effect/reset
  dialect

### Why new key behaviors still scale well

The authored path remains coherent:

- schema in
  [`key_behavior.h`](../../users/noah/lib/key/key_behavior.h)
- lookup in
  [`key_behavior_lookup.c`](../../users/noah/lib/key/key_behavior_lookup.c)
- validation in
  [`keymap_validation.c`](../../users/noah/lib/key/keymap_validation.c)
  and the dedicated host validation runners

For this board, that is the right extensibility shape. Complexity stays on the
runtime side while the authoring surface remains declarative.

### Why pd modes are in better shape now

The lifecycle refactor removed the main extensibility bottleneck: unusual
mode-owned side effects now live on the `pd_mode_def_t` row instead of behind
a registry-owned switch.

The remaining friction is more local than architectural:

- mode-local state and helper logic still accumulate in
  [`pd_mode_handlers.c`](../../users/noah/lib/pointing/pd_mode_handlers.c)
- another bespoke mode would probably justify splitting one or more handlers
  into per-mode files

That is a manageable maintenance concern, not a structural design problem.

## Abstractions And Interfaces

### Strong abstractions

- `noah_keymap.h` vs `noah_runtime.h`
- `noah_keymap_ids.h`
- `pd_mode_manifest.h`
- `runtime_debug.h`
- `source_manifest.mk`
- the RGB stage interfaces

These abstractions are meaningful. They describe real ownership boundaries.

### Remaining leaky abstraction: the handled-key effect surface still has three names

The runtime now has one shared executable effect type in
[`key_runtime_effect.h`](../../users/noah/lib/key/key_runtime_effect.h),
which is good. But the public surfaces around it still require a maintainer to
understand:

- effect requests in
  [`key_runtime_slot_effect.h`](../../users/noah/lib/key/key_runtime_slot_effect.h)
- slot results in
  [`key_runtime_slot_result.h`](../../users/noah/lib/key/key_runtime_slot_result.h)
- transition-plan aliases in
  [`key_runtime_transition.h`](../../users/noah/lib/key/key_runtime_transition.h)

Those are lighter-weight than the earlier architecture, but they still make
the reducer/executor seam feel more layered than it really is.

### Remaining abstraction leak: handled keys are partly resolved and partly raw

`handled_key_view_t` now carries resolved semantics, which is an improvement.
But it also still exposes the underlying `key_behavior_view_t`, and some
callers still inspect `key.behavior.handled` directly instead of consuming a
fully stable resolved surface.

That is not a functional bug. It is an interface-stability issue: maintainers
can still reach through the abstraction instead of staying on the resolved
meaning layer.

## Code Organization And Structure

### Repo layout is good

The top-level split remains easy to explain:

- keymap-authored data in `keyboards/.../keymaps/noah/`
- reusable runtime under `users/noah/lib/`
- targeted host verification in `tests/host/`
- human-facing docs under `README.md` and `docs/`

That is a strong base.

### The main organization issue is still discoverability inside the key runtime

The key runtime is smaller and cleaner than it used to be, but it is still
hard to learn as one flow. A maintainer has to jump across:

- `key_runtime_process.c`
- `key_runtime_preflight.c`
- `key_runtime_transition.c`
- `key_runtime_slot_step.c`
- `key_runtime_slot_policy.c`
- `key_runtime_slot_release_reduce.c`
- `key_runtime_slot_scan_reduce.c`
- `key_runtime_slot_pending_multi_tap.c`

Those files are not individually too large. The problem is that the conceptual
story is still hidden behind implementation-stage names.

### PD-mode handler organization is the next likely friction point

[`pd_mode_handlers.c`](../../users/noah/lib/pointing/pd_mode_handlers.c)
holds all mode-local state, thresholds, and helper behavior in one file. That
works with the current mode count, but it means a new mode still adds:

- more file-local static state
- more mode-specific helper functions
- more merge pressure in one translation unit

For a repo that is otherwise disciplined about ownership, that is one of the
few places that still feels centralized by convenience rather than by concept.

## State Management And Flow

### What is strong

- board-sized runtime slots keyed by physical position are correct for this
  fixed board
- `runtime_shared_state_t` is a better central state home than the older
  scattered globals
- `runtime_debug.h` now gives tests one aggregate snapshot/reset seam

### Remaining state issue: debug state is aggregated, but execution policy is still distributed

The runtime can now snapshot its state. That is a real improvement. But the
policy that decides when state changes still lives in multiple output and mode
paths:

- `action_dispatch.c`
- `pd_mode_handlers.c`
- `key_runtime_transition.c`
- `held_repeat.c`

So the codebase has improved state observability more than it has improved
state transition locality.

## Scalability Of The Design

### What will scale well

- more authored data
- more validation rules
- more RGB stages
- modestly more pd modes
- more host test binaries

### What will create debt fastest

- more special-case action emitters
- more unusual pd modes that require central registry edits
- more effect types that force request/result/transition alias updates
- more scenario harness logic that re-stubs production interfaces

### Important non-issue: linear scans

Linear scans in behavior lookup, pd-mode lookup, and board-sized slot tables
are acceptable here. The board is fixed. The main scalability risk is
conceptual drift, not lookup cost.

## Testing And Debuggability

### Current posture is strong

This repo is already better tested than most keyboard firmware projects:

- the host suite covers ownership, pd modes, split sync, handled-key runtime,
  macros, validation, RGB, and compile gates
- `runtime_debug.h` gives tests a shared reset/snapshot seam
- real-profile validation protects the authored configuration surface

### Scenario testability posture is now aligned

[`key_runtime_scenario_harness.h`](../../tests/host/key_runtime_scenario_harness.h)
and
[`key_runtime_scenario_harness.c`](../../tests/host/key_runtime_scenario_harness.c)
now consume the same effect/reset surfaces that production runtime code
already exposes:

1. `key_runtime_effect_t`
2. `noah_runtime_reset_for_test()`

That removes the main test-drift risk that remained after the earlier
runtime-debug work.

## Findings

### 1. High at review time: action emission was a hidden state-transition surface

Evidence:

- `action_dispatch()` activates fallback holds before tapping
- pd-mode tap helpers do the same thing directly before synthetic taps or
  shortcuts
- repeat scheduling also reuses `action_dispatch()`

Impact:

- a new action emitter can accidentally change key-runtime semantics
- output code is harder to reason about because it is not only output code
- tests and future simulators need to know about runtime mutation policy that
  is not visible in the function name

Recommendation:

- make the emission policy explicit with one output-intent surface instead of
  burying it inside `action_dispatch()` or re-implementing it in pd handlers

Suggested direction:

```c
typedef struct {
    bool settle_pending_fallback_holds;
    bool preserve_keyboard_mod_state;
} noah_emit_policy_t;

void noah_emit_tap(uint16_t action, noah_emit_policy_t policy);
```

That lets callers state whether they are normal key-runtime effects,
pd-handler synthetic taps, or special shortcut paths without relying on hidden
coupling.

Follow-up status:

- resolved in the first implementation slice recorded in
  [progress.md](./progress.md)

### 2. Medium-High: pd-mode lifecycle extensibility is still partly central

Evidence:

- manifest rows own the normal mode definition path
- lifecycle hooks are selected in `pd_mode_registry.c`
- unusual mode wiring still depends on the central registry switch
- all mode-local handlers and state still accumulate in `pd_mode_handlers.c`

Impact:

- ordinary modes remain easy to add
- unusual modes still require core-runtime edits
- the registry becomes the place where special behavior knowledge accumulates

Recommendation:

- move lifecycle hooks into the mode definition row itself so the registry only
  executes data it already owns
- split mode-local handlers/state into per-mode files once another bespoke mode
  lands

Suggested direction:

```c
typedef struct {
    pd_mode_mask_t                   mode_flag;
    uint16_t                         keycode;
    uint16_t                         lock_action;
    pd_mode_handler_t                handler;
    pd_mode_key_handler_t            key_handler;
    pd_mode_reset_fn_t               reset;
    uint16_t                         dpi;
    pd_mode_traits_t                 traits;
    const pd_mode_lifecycle_hooks_t *lifecycle;
} pd_mode_def_t;
```

That keeps the current static model, but removes the registry switch as the
next extensibility choke point.

Follow-up status:

- resolved in the second implementation slice recorded in
  [progress.md](./progress.md)

### 3. Medium at review time: the handled-key effect model exposed overlapping protocol layers

Evidence:

- `key_runtime_slot_effect.h` defines request kinds
- `key_runtime_slot_result.h` aliases executable effects again
- `key_runtime_transition.h` aliases the same effect type a second time

Impact:

- adding one new effect still requires touching several effect-facing headers
- the reducer/executor flow is harder to explain than the real runtime model
- names suggest more translation layers than actually exist

Recommendation:

- keep one executable effect type
- rename the request layer to make it clear it is only a reducer-local builder
- stop re-exporting transition-specific aliases for the same effect enum

The architecture is close here; the remaining work is mostly interface cleanup,
not a redesign.

Follow-up status:

- resolved in the fourth implementation slice recorded in
  [progress.md](./progress.md)

### 4. Medium: the scenario harness still mirrors runtime contracts instead of consuming them

Evidence:

- custom scenario effect enum in `key_runtime_scenario_harness.h`
- manual `noah_runtime_shared_state` reset in `key_runtime_scenario_harness.c`
- custom stubs for action dispatch, held actions, layer ownership, and pd-mode
  lock effects

Impact:

- tests can drift away from production effect contracts
- integration scenarios become more expensive to expand across subsystems
- the strongest debug surface (`runtime_debug.h`) is not the default scenario
  harness surface

Recommendation:

- rebuild the scenario harness around shared runtime contracts:
  - `key_runtime_effect_t`
  - `noah_runtime_reset_for_test()`
  - optional typed effect observers in the transition executor

That would let higher-level tests use the same state/effect vocabulary as the
runtime instead of maintaining a test-only dialect.

Follow-up status:

- resolved in the third implementation slice recorded in
  [progress.md](./progress.md)

## Concrete Next Steps

1. No further mandatory follow-up items remain from this review.
2. If another bespoke pd mode lands, consider splitting
   `users/noah/lib/pointing/pd_mode_handlers.c` into per-mode files.

## Overall Judgment

This userspace is already structurally strong for a fixed Charybdis board.
The next wins are not broad framework changes. They are targeted seam
cleanups that make the current design easier to extend without central files
quietly becoming policy magnets.
