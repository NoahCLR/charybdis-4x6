# Userspace Architecture Review

Date: 2026-04-11

Status: active review created after [review-01](../2026-04-11-review-01/userspace-architecture-review.md). Follow-up work has already landed for the shared source manifest, the first structured key-runtime scenario harness, a dedicated key-runtime admission boundary, dedicated release/scan slot-transition modules, dedicated press/effect slot-transition modules, the first handled press/release slot-event wrappers, scan-specific slot-event wrappers that replaced the old public scan resolution/apply structs, an explicit handled-key slot lifecycle phase plus hold-strategy model, and a shared slot-result surface that now sits between slot event producers and `key_runtime_transition.c`; the remaining recommendations below focus on what is still architecturally open after those changes.

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
runtime. The old single-active-key bottleneck is gone, and some follow-up work
has already improved the surrounding structure by splitting admission policy
into its own module, splitting release/scan slot transitions into their own
modules, splitting press/effect helpers into their own modules, and adding a
scenario-level host harness. The runtime now also has the start of a more
event-shaped surface for handled press/release/scan planning, plus an explicit
slot lifecycle phase and hold-strategy model instead of the earlier
`hold_fired` / `hold_one_shot_fired` / `implicit_hold` /
`fallback_hold_pending` flag mix. It is still not a single explicit reducer,
and the handled-key engine is still the place most likely to fight the next
real feature.

The current extension cost looks like this:

- easy: new authored key behaviors, new layers, new RGB data, normal new pd
  modes that fit the existing manifest traits
- medium: new mode policy that needs another shared trait or another central
  policy consumer
- hard: new handled-key semantics, more overlap than the current slot ceiling,
  or any feature that changes the handled-key lifecycle itself

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
- [`users/noah/lib/key/key_runtime_slot_effect.c`](../../users/noah/lib/key/key_runtime_slot_effect.c)
- [`users/noah/lib/key/key_runtime_slot_press.c`](../../users/noah/lib/key/key_runtime_slot_press.c)
- [`users/noah/lib/key/key_runtime_slot_release.c`](../../users/noah/lib/key/key_runtime_slot_release.c)
- [`users/noah/lib/key/key_runtime_slot_scan.c`](../../users/noah/lib/key/key_runtime_slot_scan.c)
- [`users/noah/lib/key/key_runtime_state.h`](../../users/noah/lib/key/key_runtime_state.h)
- [`users/noah/lib/key/key_runtime_slot.c`](../../users/noah/lib/key/key_runtime_slot.c)
- [`users/noah/lib/key/key_runtime_transition.c`](../../users/noah/lib/key/key_runtime_transition.c)

What is good now:

- slot ownership is explicit
- slot admission and reclaim lookup now have a named home in
  [`key_runtime_admission.c`](../../users/noah/lib/key/key_runtime_admission.c)
- press planning and effect-request helpers now have named homes in
  [`key_runtime_slot_press.c`](../../users/noah/lib/key/key_runtime_slot_press.c)
  and
  [`key_runtime_slot_effect.c`](../../users/noah/lib/key/key_runtime_slot_effect.c)
- release and scan slot-transition contracts now have named homes in
  [`key_runtime_slot_release.c`](../../users/noah/lib/key/key_runtime_slot_release.c)
  and
  [`key_runtime_slot_scan.c`](../../users/noah/lib/key/key_runtime_slot_scan.c)
- handled press, release, and scan now start from named slot-event wrappers
  instead of raw helper orchestration inside `key_runtime_transition.c`
- press, release, scan, interrupt, and pending multi-tap flush paths now all
  adapt into a shared slot-result surface in
  [`key_runtime_slot_result.c`](../../users/noah/lib/key/key_runtime_slot_result.c)
  before the transition layer plans system effects
- scan-specific resolution/apply details now stay private to
  [`key_runtime_slot_scan.c`](../../users/noah/lib/key/key_runtime_slot_scan.c)
  instead of leaking through the public header
- active slot lifecycle state now has explicit `phase` and `hold_strategy`
  fields instead of the older hold/strategy boolean combination
- press/release/scan are separated
- effect execution is separated from state mutation
- [`key_runtime_transition.c`](../../users/noah/lib/key/key_runtime_transition.c)
  now consumes one shared slot-result shape instead of translating
  press/release/scan-specific structs separately

What is still expensive:

- `active_key_state_t` now has explicit lifecycle phase and hold-strategy state,
  but it still mixes lifecycle, timing, authored behavior, runtime ownership,
  and interruption policy in one mutable storage struct
- [`key_runtime_state.h`](../../users/noah/lib/key/key_runtime_state.h) exposes a
  narrower storage-oriented surface plus shared slot helpers
- lifecycle mutation and transition planning are still spread across
  `key_runtime_slot_effect.c`, `key_runtime_slot_press.c`,
  `key_runtime_slot_release.c`, `key_runtime_slot_scan.c`, and
  `key_runtime_slot_result.c` instead of one explicit reducer with one slot
  event/result model backed by one reducer implementation
- the transition layer no longer knows the press/release/scan-specific local
  structs, but those local structs still exist behind the shared slot-result
  adapter instead of being collapsed into one reducer contract directly

Why this matters:

- adding one new hold mode or one new interrupt rule is likely to touch storage,
  resolution structs, slot mutation helpers, transition planning, and tests in
  several files
- invalid or surprising state combinations are prevented mostly by convention,
  not by type shape
- the code is modular by file, but the mental model is still spread across too
  many internal protocols

This is the subsystem most likely to accumulate technical debt.

### 2. The two-slot ceiling is still a hard scalability boundary

The current runtime makes the handled-key overlap limit explicit:

- `KEY_RUNTIME_ACTIVE_SLOT_CAPACITY` is `2` in
  [`users/noah/lib/state/runtime_shared_state.h`](../../users/noah/lib/state/runtime_shared_state.h)
- slot selection and reclaim fallback now live in
  [`users/noah/lib/key/key_runtime_admission.c`](../../users/noah/lib/key/key_runtime_admission.c)

That is much better than the old hidden single-key ceiling, but it is still a
policy boundary built into the runtime architecture.

Why this matters:

- any future feature that expects more than two independent handled-key
  lifecycles will have to fight the storage model first
- the reclaim/flush policy is part of correctness now, not just performance
- other ownership modules already show a better scaling pattern; for example,
  [`layer_ownership.c`](../../users/noah/lib/state/layer_ownership.c) keys its
  bindings by physical switch capacity instead of a tiny fixed slot count

If the intended long-term design really is “at most two overlapping handled
keys,” that needs to stay a named contract. If not, this is the first place to
refactor.

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

### 4. Structured scenario coverage exists now, but only at the first layer

The handled-key runtime now has a real scenario harness:

- [`tests/host/key_runtime_scenario_harness.c`](../../tests/host/key_runtime_scenario_harness.c)
- [`tests/host/key_runtime_scenario_test.c`](../../tests/host/key_runtime_scenario_test.c)

This closes an earlier testing gap, but the coverage is still intentionally
small.

Why this matters:

- the current harness proves the shape works, but not yet across the full range
  of interrupt, reclaim, and long-hold transitions
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

- the key runtime’s internal helper protocol is still larger than the concept it
  is trying to model
- transitional aliases in
  [`key_runtime_state.h`](../../users/noah/lib/key/key_runtime_state.h) show the
  runtime boundary is still mid-migration

### Code organization and structure

Good:

- top-level foldering under `users/noah/lib/`
- human-facing docs in `README.md` and `docs/`
- authored profile discoverability in `keymap.c`

Needs attention:

- `key_runtime_slot.c` is too large to remain the long-term center of key
  behavior policy
- `key_runtime_state.h` is carrying too many internal protocol types

### State management and flow

Positive:

- central shared state ownership is clearer than file-local globals
- press/release/scan flow is understandable at the module level

Risk:

- slot state still depends on several booleans whose legal combinations are
  implicit
- concurrency policy is partly a storage limitation, not purely business logic

### Scalability of the design

The repo will scale well for:

- more authored data
- more tests
- a modest number of additional pd modes
- more RGB overlays built in the current staged model

The repo will scale poorly for:

- significantly richer handled-key lifecycles
- features that need more than two concurrent handled-key states
- repeated source-list drift between build/test wiring

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

### Recommendation 1: make the handled-key runtime an explicit reducer

Refactor toward one slot-level update entry point that consumes events and emits
effects.

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

### Recommendation 2: continue separating slot capacity policy from lifecycle policy

The first step is now done:

- slot lookup/admission/reclaim policy lives in
  [`users/noah/lib/key/key_runtime_admission.c`](../../users/noah/lib/key/key_runtime_admission.c)
- admission and reclaim behavior has dedicated coverage in
  [`tests/host/key_runtime_admission_test.c`](../../tests/host/key_runtime_admission_test.c)

Next step:

- introduce a smaller named policy surface for:
  - slot capacity
  - reclaim preference
  - overflow behavior
- keep that policy explicit enough that a future storage-model change does not
  need another whole-runtime sweep

Longer-term option:

- move from a fixed two-slot pool to a key-position indexed ownership table, the
  same way `layer_ownership` already keys bindings by physical location

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

### Recommendation 4: expand the scenario harness into a reducer regression matrix

The initial harness now exists. The next step is to grow it around the
transitions most likely to regress during reducer/FSM work.

Best next traces:

- reclaim after pending multi-tap ownership
- interrupt-driven fallback hold activation
- long-hold promotion after immediate-hold registration
- two-slot contention where the primary slot is reused as overflow

## Bottom Line

This codebase is not suffering from broad architectural failure. The top-level
design is already disciplined and significantly ahead of typical keyboard
firmware repos.

The main thing to protect now is not the outer shape of the repo; it is the
internal shape of the handled-key engine. The manifest deduplication, scenario
harness, admission split, release/scan boundary extraction, press/effect
boundary extraction, press/release/scan slot-event wrappers, and explicit slot
lifecycle phase extraction were the right setup moves. If the remaining
handled-key core becomes an explicit reducer/FSM with a cleaner long-term
capacity policy, the rest of the architecture is in a good position to keep
scaling.
