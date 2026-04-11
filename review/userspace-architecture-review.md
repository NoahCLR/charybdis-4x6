# Userspace Architecture Review

Date: 2026-04-11

Scope: `noah` userspace for the Charybdis 4x6. This review intentionally ignores hardware changes and focuses on software architecture, module boundaries, extensibility, state flow, and long-term maintainability.

## Summary

This codebase is substantially better structured than a typical single-file QMK keymap. The strongest parts are:

- explicit ownership layers for modifiers, held actions, and momentary layers
- a real transition planner in the key runtime instead of ad hoc side effects
- a manifest-driven pointing-mode registry
- meaningful host-side test coverage across the critical runtime pieces

The main architectural constraint is that the key-behavior engine still centers on one global unresolved active key. That design keeps the current implementation coherent, but it is the primary limit on future overlap, richer concurrency, and composability.

## Key Findings

### 1. The key-behavior engine is still single-concurrency

The runtime keeps one global `active_key` and one global `multi_tap` state. Any new press interrupts or flushes that global state before proceeding.

Why this matters:

- it works for the current profile, but it is the main scalability ceiling
- overlapping custom dual-role behavior will keep forcing edits into central transition code
- future features like multiple simultaneous custom holds or richer chord semantics will be harder to extend cleanly

Key references:

- `users/noah/lib/state/runtime_shared_state.h:17`
- `users/noah/lib/key/key_runtime_preflight.c:33`
- `users/noah/lib/key/key_runtime_transition.c:249`
- `users/noah/lib/key/key_runtime_transition.c:773`

### 2. The pointing-mode manifest is good, but incomplete as an abstraction

Mode identity is centralized in `pd_mode_manifest.h`, but policy still leaks into registry and layer-policy code through special cases for `ARROW`, `PINCH`, and scroll modes.

Why this matters:

- adding a simple new mode is clean
- adding a mode with different anchoring, dragscroll, DPI, or typing-surface behavior still requires core runtime edits
- the current model is closer to a structured registry than a true plugin boundary

Key references:

- `users/noah/lib/pointing/pd_mode_manifest.h:45`
- `users/noah/lib/pointing/pd_mode_registry.c:21`
- `users/noah/lib/pointing/pd_mode_registry.c:81`
- `users/noah/lib/pointing/pointer_layer_policy.c:21`

### 3. The authoring surface is broader than it should be

`keymap.c` gets a convenient authoring API, but `users/noah/noah_keymap.h` also re-exports runtime helpers and hook-related integration surfaces.

Why this matters:

- the keymap authoring boundary is not sharply separated from runtime integration
- future alternate keymaps or profiles will have a larger accidental surface area
- the weak-hook pattern is pragmatic, but a keymap can silently bypass shared behavior if it overrides a hook and forgets to call `noah_*`

Key references:

- `users/noah/noah_keymap.h:118`
- `users/noah/hooks.c:7`
- `users/noah/runtime_init.c:28`

### 4. Core behavior depends on fork-specific QMK contracts

Some important behavior relies on copied or overridden QMK internals:

- the VIA macro sender is forked into userspace
- modifier ownership relies on overriding `register_mods()` and `unregister_mods()`
- several runtime modules depend on `pointing_device_auto_mouse.h` from the firmware fork

Why this matters:

- the choices are reasonable, but they create hidden compatibility boundaries
- upgrades depend on scattered "re-validate this copy/override" assumptions
- portability is reduced because the compatibility layer is not centralized

Key references:

- `users/noah/lib/action/action_lifecycle.c:50`
- `users/noah/lib/state/keyboard_mod_ownership.c:192`
- `users/noah/lib/pointing/pd_runtime.c:11`

### 5. Validation is good for mechanics, weaker for authored configuration integrity

The host suite is strong on subsystem behavior, but some authored-data risks are still unchecked:

- duplicate `key_behaviors[]` rows silently shadow later definitions
- RGB mode color coverage is opportunistic rather than validated
- profile-level authored data is not checked as aggressively as runtime semantics

Why this matters:

- the runtime is reasonably robust
- configuration drift will become a bigger risk as the authored surface grows
- this is exactly the kind of debt that compounds slowly and quietly

Key references:

- `users/noah/lib/key/key_behavior_lookup.c:16`
- `users/noah/lib/key/key_behavior_lookup.c:136`
- `users/noah/lib/key/keymap_validation.c:49`
- `users/noah/lib/rgb/rgb_runtime.c:95`

## Architecture Assessment By Area

### Architecture and separation of concerns

The strongest separation exists in these modules:

- `layer_ownership`
- `keyboard_mod_ownership`
- `held_action`
- `action_lifecycle`

Those modules encode real policy and are separately testable. The key runtime also has a useful split between decision logic and effect execution through the transition plan.

The weakest separation exists in:

- the single-active-key runtime model
- pointing-mode policy, which still mixes identity, registry, and special-case behavior
- RGB rendering, which has become the largest multi-responsibility runtime file

### Modularity and extensibility

Easy today:

- add a new `key_behaviors[]` row
- add a new layer and update layer-owned tables
- add a simple pointing mode that matches the existing behavioral model

Hard today:

- add overlapping or concurrent handled-key semantics
- add a pointing mode with nonstandard policy behavior
- add new runtime-visible feedback without touching central orchestrators

The current design is extensible when a feature fits existing abstractions. It is much less extensible when a feature crosses ownership or arbitration boundaries.

### Abstractions and interfaces

Good abstractions:

- layer ownership
- modifier ownership
- held action registration
- key transition planning

Leaky abstractions:

- pointing modes
- weak hook chaining
- QMK fork compatibility

### Code organization

The repo is organized substantially better than average for firmware userspace:

- `users/noah/lib/key`
- `users/noah/lib/action`
- `users/noah/lib/pointing`
- `users/noah/lib/state`
- `users/noah/lib/rgb`

The key concern is size concentration in a few files:

- `users/noah/lib/key/key_runtime_transition.c`
- `users/noah/lib/rgb/rgb_runtime.c`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`

Those are not inherently bad, but they are the main hotspots where future complexity will accumulate.

### State management and flow

The state model is deliberate, not accidental. That is a strength.

The tradeoff is that the state machine remains centralized:

- active key state
- multi-tap state
- feedback state
- pd active/locked flags
- split-sync projection

This makes reasoning possible today, but it also means concurrency improvements will require structural changes rather than local extensions.

### Scalability risk

The main technical-debt vectors are:

- global handled-key state instead of per-key FSM state
- pointer-mode special cases that grow in registry/policy code
- RGB runtime continuing to absorb more rendering rules
- authored config validation lagging behind authored feature growth

## Recommended Plan

### Phase 0: Add guardrails first

- validate duplicate `key_behaviors[]` keycodes
- validate pd-mode RGB coverage and other authored table integrity
- add a profile-level host test that compiles and validates the real authored data
- add optional key-runtime transition tracing for debugging

### Phase 1: Centralize QMK/fork compatibility

- create a small compatibility layer for copied VIA macro behavior
- isolate assumptions around `register_mods()` overrides
- isolate fork-specific `auto_mouse` integration points

Goal: make firmware-contract risk explicit and localized.

### Phase 2: Narrow the authoring interface

- split `users/noah/noah_keymap.h` into:
  - an authoring header
  - a runtime integration header
- keep `keymap.c` on the authoring surface only

Goal: reduce accidental coupling between authored profile data and runtime internals.

### Phase 3: Make pd modes trait-driven

- extend the manifest/registry with traits instead of hard-coded mode identity checks
- move policy such as auto-mouse anchoring, typing-layer preference, dragscroll ownership, and owned-mod behavior into data

Goal: new modes should usually require only manifest, handler, keymap, RGB, and doc changes.

### Phase 4: Replace the single active-key model

- keep the transition-plan pattern
- replace the one-global-active-key model with a small per-`keypos_t` FSM table
- migrate multi-tap and hold promotion to per-key state

Goal: preserve the current behavioral model while removing the main extensibility ceiling.

### Phase 5: Break RGB into render stages

- split `rgb_runtime.c` into composable stages:
  - layer composition
  - automouse blend
  - preview overlay
  - pd-mode overlay
  - key feedback overlay

Goal: keep the current render order while making RGB logic easier to extend and test.

### Phase 6: Expand integration coverage

- add tests for authored-profile integrity
- add tests for pd-mode trait behavior
- add tests for overlapping handled-key state once the FSM refactor lands
- add tests for hook override chaining expectations

## Suggested Execution Order

Recommended order for lowest regression risk:

1. validation and debug instrumentation
2. QMK/fork compatibility isolation
3. authoring/runtime header split
4. pd-mode trait refactor
5. multi-key runtime foundation
6. per-key FSM migration
7. RGB runtime decomposition
8. final cleanup and docs refresh

## Verification Status

At review time, the host suite completed successfully:

- `sh tests/host/run_all_host_tests.sh`

That result is important context: the current architecture is not broken. The recommendation is to improve extension boundaries before future feature growth turns the current hotspots into long-term maintenance bottlenecks.
