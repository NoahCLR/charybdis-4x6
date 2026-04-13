# Userspace Architecture Review

Date: 2026-04-13

Status: new architecture review pass after the implementation follow-ups logged in
[2026-04-13-review-05](../2026-04-13-review-05/userspace-architecture-review.md).
This pass focuses on long-term software architecture and extensibility for the
current userspace as it exists today. The hardware is treated as fixed.

Scope:

- `users/noah/` runtime structure, module boundaries, and extension seams
- the keymap-owned authored/runtime boundary in
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/`
- host-test structure and maintainer-facing docs where they reveal the real
  architectural contract

Out of scope:

- hardware changes
- upstream QMK redesign
- speculative replacement of the current userspace with a different firmware
  stack

## Executive Summary

The codebase is materially stronger than it was before the recent refactor
passes. The authored keymap/runtime split is real, the key runtime now has an
explicit slot/effect model, pd-mode state is no longer hidden behind ad hoc
flag composition, macro semantics are centralized around one IR, and the host
suite is broad enough to make architectural claims defensible.

The remaining risks are mostly not "this is broken today" risks. They are
"future features will keep paying a coordination tax" risks. The main pattern
behind them is policy duplication: release semantics are implemented in more
than one reducer, slot interaction materialization still has override-heavy
callers, pd-mode definition mixes identity with policy, effect capacities are
hard-coded and lossy, and only the key runtime currently has a genuinely shared
high-level host harness.

## Findings

### High Priority

#### 1. Release semantics still live in two engines

References:

- `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c:28-93`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c:257-329`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c:45-147`
- `tests/host/key_runtime_release_matrix_test.c:264-369`
- `tests/host/key_runtime_release_matrix_test.c:371-380`

Why this matters:

- Active-slot release and pending-multi-tap release both resolve tap vs
  release-hold vs long-hold semantics, but they do not share one decision
  engine.
- `key_runtime_slot_release_active.c` owns a phase-contract matrix and a rich
  `key_runtime_slot_release_context_t`; pending multi-tap reconstructs a
  smaller parallel context and re-derives enough of the same rules to stay
  compatible.
- The existence of `key_runtime_release_matrix_test.c` is evidence that the
  architecture currently relies on tests to keep two semantic implementations
  equivalent.

Why this is a design bottleneck:

- Any future release-semantic change has to be audited in both reducers.
- The code is already correct enough to justify keeping the matrix test, but
  the test is compensating for duplication instead of protecting one shared
  resolver.
- This is exactly the kind of drift that stays green until a third path or new
  hold style appears.

Recommended direction:

- Extract one release-decision helper that consumes cached slot interaction plus
  path-specific context and returns a semantic decision object.
- Keep the active-slot and pending-multi-tap files as adapters that translate
  storage/layout differences into that shared query and then map the result
  into effects.

Example shape:

```c
typedef struct {
    key_runtime_slot_phase_t phase;
    key_runtime_slot_release_contract_t contract;
    key_runtime_slot_interaction_t interaction;
    uint16_t elapsed;
    bool held_action_active;
    bool repeat_active;
    bool layer_interrupted;
    bool pd_mode_was_locked_on_press;
    bool preserve_pending_chain;
} key_runtime_release_query_t;

typedef struct {
    enum { RELEASE_NONE, RELEASE_TAP, RELEASE_ACTION, RELEASE_PD_LOCK } outcome;
    uint16_t action;
    uint8_t repeat_count;
    bool release_owned_state;
} key_runtime_release_decision_t;
```

#### 2. `key_runtime_slot_interaction_t` is not yet a stable boundary

References:

- `users/noah/lib/key/runtime/key_runtime_interaction.h:59-175`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.c:19-40`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.c:43-61`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.c:143-158`

Why this matters:

- `key_runtime_slot_interaction_from_resolution(...)` looks like the canonical
  authored-resolution to cached-slot contract boundary.
- The press reducer immediately rebuilds and mutates that contract in
  `key_runtime_slot_press_interaction(...)` by rewriting tap action, repeat
  count, hold tiers, timing, flags, policy, and release contract.
- The pending-multi-tap reuse path then feeds alternate resolutions and
  override values into that custom builder.

Why this is a design bottleneck:

- Adding a new cached interaction field will require auditing every place that
  reconstructs interaction state, not just the canonical builder.
- The abstraction leaks which fields are "really derived from resolution" and
  which ones are safe to patch after the fact.
- The more special press-entry paths the runtime grows, the more likely it is
  that one of them forgets to keep a new field or invariant in sync.

Recommended direction:

- Introduce one explicit materialization API for "slot binding as pressed now",
  separate from "authored lookup as written in the keymap".
- Make the press reducer pass override inputs into that materializer instead of
  mutating a partly-built interaction struct.

Example shape:

```c
typedef struct {
    handled_key_resolution_t resolution;
    uint16_t tap_action;
    hold_behavior_t hold;
    hold_behavior_t long_hold;
    uint16_t tap_hold_term;
    uint16_t longer_hold_term;
    uint16_t multi_tap_term;
    key_runtime_slot_hold_strategy_t hold_strategy;
} key_runtime_slot_materialize_args_t;

key_runtime_slot_interaction_t
key_runtime_slot_materialize(key_runtime_slot_materialize_args_t args);
```

#### 3. Pd-mode extensibility is still distributed policy, not truly mode-owned behavior

References:

- `users/noah/lib/pointing/defs/pd_mode_manifest.h:12-74`
- `users/noah/lib/pointing/runtime/pd_mode_state.c:31-68`
- `users/noah/lib/pointing/runtime/pd_mode_state.c:207-253`
- `users/noah/lib/pointing/runtime/pd_mode_lifecycle.c:27-143`
- `users/noah/lib/pointing/policy/pointer_layer_policy.c:20-102`

Why this matters:

- A simple mode fits the current manifest-driven system well.
- A non-trivial mode still spreads its meaning across at least four places:
  manifest row, state transitions, lifecycle side effects, and pointer-layer
  policy.
- The manifest row itself already mixes identity, handler selection, reset
  callback, DPI, traits, and lifecycle hook selection.

Why this is a design bottleneck:

- `PDM(...)` rows are compact, but they are also dense. The DRAGSCROLL and
  PINCH rows encode several cross-cutting behaviors in one macro line.
- Traits are consumed in different subsystems for different reasons:
  exclusivity/state in `pd_mode_state.c`, auto-mouse/DPI in
  `pd_mode_lifecycle.c`, and layer visibility in `pointer_layer_policy.c`.
- Adding a mode with a new shared behavior usually means editing generic
  runtime policy, not just adding a mode-owned file.

Recommended direction:

- Keep the manifest as the identity registry, but move growing behavior policy
  out of trait-bit interpretation and into explicit mode policy objects.
- Let shared defaults remain shared, but let a mode definition supply a
  narrow policy surface for the places that currently interpret traits.

Practical target:

- identity: keycode, lock keycode, display name/index
- pointer behavior: handler/key handler/reset
- transition policy: auto-mouse anchoring, typing-layer preference, DPI owner,
  lock-side effects

That keeps "add a new mode" mode-owned even when behavior goes beyond a plain
threshold handler.

### Medium Priority

#### 4. Effect queue caps are a hidden scalability ceiling

References:

- `users/noah/lib/key/runtime/slot/key_runtime_slot_result.h:13-17`
- `users/noah/lib/key/runtime/key_runtime_transition.h:20-24`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_result.c:23-36`
- `users/noah/lib/key/runtime/key_runtime_transition.c:37-56`

Why this matters:

- Slot reducers can emit at most `8` effects, and transition plans can hold at
  most `16`.
- Overflow does not fail fast. It drops effects and only logs if console output
  is available.
- Current feature combinations fit, but the design has no structural proof that
  future combinations will.

Why this is a design bottleneck:

- Missing side effects will present as "sometimes no layer release", "missing
  delayed action", or "missing lock toggle", not as an obvious compile error.
- As more release-time, interrupt-time, and split-sync side effects are added,
  these hard caps become architectural limits rather than implementation
  details.

Recommended direction:

- Add host assertions that fail tests if any result/plan ever sets
  `overflowed`.
- Document or static-assert the current worst-case effect envelope.
- If the envelope is getting close to the cap, move from one monolithic plan to
  incremental execution between reduction passes.

#### 5. Test architecture is strongest where there is a shared harness; other suites still rebuild their own runtime world

References:

- `tests/host/include/qmk_stub.h:210-254`
- `tests/host/key_runtime_scenario_harness.c:15-115`
- `tests/host/pd_mode_handlers_test.c:30-140`
- `tests/host/rgb_layer_render_test.c:35-107`
- `tests/host/pd_mode_key_runtime_integration_test.c:45-140`

Why this matters:

- The key runtime now has a genuine reusable scenario harness and shared reset
  path via `noah_runtime_reset_for_test()`.
- Other suites still define local timer, mod, layer, pd-mode, and output stubs
  from scratch.
- `qmk_stub.h` gives a common API surface, but not a common runtime fixture.

Why this is a design bottleneck:

- Stub drift becomes a maintenance problem as the userspace grows.
- Integration bugs are more likely when each suite reinvents its own partial
  model of keyboard state.
- Adding new tests outside the key runtime still costs more setup code than it
  should.

Recommended direction:

- Create a shared host fixture layer for timers, modifiers, layer state,
  pointing-mode state, and action logs.
- Keep the key-runtime scenario harness on top of that substrate.
- Move pd-mode, RGB, and split-sync suites toward the same reset/snapshot
  helpers so "what is a realistic runtime state?" only has one answer.

## Areas That Are Solid

- The authored keymap/runtime split is real. `keymap.c` remains mostly data,
  while shared runtime policy stays under `users/noah/`.
- The key runtime is now explicit about ownership and flow: slot state,
  reducers, effect queues, and transition execution are easier to reason about
  than direct side-effect branching.
- Pd-mode state is significantly improved by the exclusive active/locked model
  and the read-only snapshot view.
- Macro behavior is centralized around one IR pipeline instead of separate
  semantic implementations for hardcoded and VIA-backed macros.
- Documentation quality is unusually good for firmware userspace work:
  `README.md`, `docs/KEY_RUNTIME.md`, and `docs/ADDING_PD_MODE.md` make the
  intended structure auditable.
- The host suite is broad and valuable. The architectural concerns above are
  visible largely because the codebase already exposes good test seams.

## Concrete Recommendations

1. Extract a shared key-release resolver and make active release and pending
   multi-tap release thin adapters around it.
2. Replace post-construction interaction mutation with one slot-materialization
   API so `key_runtime_slot_interaction_t` becomes a trustworthy cached
   contract.
3. Split pd-mode identity from pd-mode policy. Keep the manifest-generated
   registry, but move growing behavior out of shared trait interpretation and
   into mode-owned policy objects or callbacks.
4. Make effect-queue overflow a host-test failure, not just a console-side
   warning.
5. Build a shared host runtime fixture under `tests/host/` and reuse it across
   pd-mode, RGB, split-sync, and action-lifecycle suites.

## Verification

Verification run during this review:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- worktree was clean before the review was opened
- full host suite passed
- firmware build passed

Workspace scope:

- no sibling workspace folders were edited
- this review adds files only under `review/2026-04-13-review-06/`
