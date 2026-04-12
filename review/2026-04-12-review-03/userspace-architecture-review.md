# Userspace Architecture Review

Date: 2026-04-12

Status: new review created after
[2026-04-12-review-02](../2026-04-12-review-02/userspace-architecture-review.md).
This pass re-reviews the current repo after the handled-key effect cleanup,
pd-mode lifecycle ownership work, and scenario-harness rebuild already landed
today.

Scope: software architecture, structure, and long-term extensibility inside
this repo only. Hardware changes are intentionally out of scope. Because the
board is fixed, this review prioritizes clarity, ownership, and extension
cost over generalized hardware abstraction.

## Executive Summary

The userspace is in a strong place. The important architectural decisions are
correct and should be preserved:

- the authored/runtime split between
  [`keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c),
  [`rgb_config.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c),
  [`noah_keymap.h`](../../users/noah/noah_keymap.h),
  [`noah_keymap_ids.h`](../../users/noah/noah_keymap_ids.h), and
  [`noah_runtime.h`](../../users/noah/noah_runtime.h)
- narrow QMK hook ownership through
  [`hooks.c`](../../users/noah/hooks.c) and
  [`runtime_init.c`](../../users/noah/runtime_init.c)
- explicit compatibility surfaces in
  [`lib/compat/`](../../users/noah/lib/compat/)
- manifest-driven build ownership through
  [`source_manifest.mk`](../../users/noah/source_manifest.mk) and the compile
  gate in
  [`run_feature_gate_compile_tests.sh`](../../tests/host/run_feature_gate_compile_tests.sh)
- explicit ownership registries for layers, held actions, repeats, and
  keyboard modifiers
- unusually strong host verification and runtime-debug tooling

This codebase does not need a rewrite and does not need a plugin framework.
Its strength is that most extension work is still declarative:

- new key behaviors are authored in `key_behaviors[]`
- new layers are authored in `keymaps[][]` plus keymap config
- new pd modes enter through `NOAH_PD_MODE_LIST(...)`
- RGB stays table-driven in `rgb_config.c`

The main remaining architecture risks are now local and specific:

1. cross-subsystem trace coverage is still thinner than the
   rest of the runtime's host verification story
2. the handled-key reducers are coherent but dense enough that the next major
   lifecycle addition should split them by ownership seam instead of adding
   more local helper layers
3. `pd_mode_registry.c` now carries the next most likely pointing-side split,
   because manifest materialization, lifecycle hooks, and state transitions
   still live together there

Those are maintainability issues, not signs that the overall design is wrong.

## 1. Architecture And Separation Of Concerns

### What is working well

Responsibility division is mostly clear:

- `keyboards/.../keymaps/noah/` owns authored profile data
- `users/noah/lib/key/` owns handled-key resolution and runtime flow
- `users/noah/lib/pointing/` owns pd-mode policy and handlers
- `users/noah/lib/rgb/` owns staged rendering
- `users/noah/lib/state/` owns long-lived ownership state and split sync
- `users/noah/lib/compat/` owns QMK/fork assumptions

That split is real in code, not just in folder names. The compile gate also
enforces the header boundary that keymap-owned files do not include
`noah_runtime.h` and runtime modules do not include `noah_keymap.h`.

The top-level QMK hook surface is also disciplined. `hooks.c` is only a weak
hook bridge, while `runtime_init.c` owns shared init and scan orchestration.
That keeps QMK integration narrow and makes it easier to reason about which
subsystems actually participate in startup, scan, and rendering.

### Where the separation still blurs

The handled-key path is conceptually one pipeline, and handled-key resolution
now lives in its own implementation file. The remaining blur is that the
effect-producing path is still spread across several adjacent layers:

- resolved handled-key lookup in
  [`handled_key.c`](../../users/noah/lib/key/handled_key.c)
- event entry and preflight in
  [`key_runtime_process.c`](../../users/noah/lib/key/key_runtime_process.c)
  and
  [`key_runtime_preflight.c`](../../users/noah/lib/key/key_runtime_preflight.c)
- reducer events in
  [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
- builder expansion in
  [`key_runtime_slot_result.c`](../../users/noah/lib/key/key_runtime_slot_result.c)
- execution in
  [`key_runtime_transition.c`](../../users/noah/lib/key/key_runtime_transition.c)

That is not inherently bad, but it means the architectural unit maintainers
care about, "a handled key becomes ordered effects", is still implemented as a
chain of related but separately named micro-surfaces.

The pd-mode authority split is now in a better place. Local authoritative
state and mirrored display state are separate, and public query names are
explicit about which one they expose. Preserve that seam: policy and
lifecycle code should keep using local queries, while RGB/UI-like consumers
should stay on display queries.

## 2. Modularity And Extensibility

### Extension work that scales well today

Adding new authored behaviors is already in good shape:

- schema in
  [`key_behavior.h`](../../users/noah/lib/key/key_behavior.h)
- lookup in
  [`key_behavior_lookup.c`](../../users/noah/lib/key/key_behavior_lookup.c)
- validation in
  [`keymap_validation.c`](../../users/noah/lib/key/keymap_validation.c)
- concrete profile data in
  [`keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)

Adding layers is also reasonable because the layer model is explicit and
shared:

- authored layer enum in keymap `config.h`
- runtime ownership in
  [`layer_ownership.c`](../../users/noah/lib/state/layer_ownership.c)
- RGB layer rendering in
  [`rgb_layer_stage.c`](../../users/noah/lib/rgb/rgb_layer_stage.c)
- validation through real-profile and keymap validation tests

Pd-mode extensibility is materially better than in earlier reviews. The mode
manifest now owns identity, generated keycodes, traits, and optional lifecycle
hooks. That means a normal new mode can still be added with one manifest row,
one handler implementation, one RGB color entry, and authored key exposure in
`keymap.c`.

### Extension work that is still too expensive

The handled-key runtime is still the most expensive subsystem to extend.
Adding a genuinely new effect or lifecycle stage still tends to require
coordinated edits in:

- `key_runtime_effect.h`
- `key_runtime_slot_effect.h`
- `key_runtime_slot_result.c`
- `key_runtime_transition.c`
- scenario harness and host test stubs

That edit fanout is the main extensibility bottleneck in the repo.

Pd modes are in a better place now that mode-local handler state lives in
separate implementation files. The remaining pressure point is
[`pd_mode_registry.c`](../../users/noah/lib/pointing/pd_mode_registry.c),
because manifest materialization, lifecycle hook routing, and state transition
policy still live together there.

### Architectural pattern recommendation

Do not add a generic plugin system. The current design is better served by
stronger board-specific seams:

- declarative authored tables for profile data
- manifest rows for pd modes
- one explicit handled-key effect vocabulary
- named ownership registries for stateful side effects

That is already the right pattern for a fixed board.

## 3. Abstractions And Interfaces

### Strong abstractions

These surfaces are meaningful and worth keeping:

- `noah_keymap.h` vs `noah_runtime.h`
- `noah_keymap_ids.h`
- `source_manifest.mk`
- `qmk_contract.h`
- `runtime_debug.h`
- `pd_mode_manifest.h`
- RGB stage interfaces and staged renderer order in
  [`rgb_runtime.c`](../../users/noah/lib/rgb/rgb_runtime.c)

They each represent a real ownership boundary.

### Improved abstraction: `handled_key_view_t` is now resolved-only

[`handled_key.h`](../../users/noah/lib/key/handled_key.h) presents
`handled_key_view_t` as the resolved handled-key contract, and follow-up work
on 2026-04-12 now makes that true in code:

- handled-key resolution lives in
  [`handled_key.c`](../../users/noah/lib/key/handled_key.c)
- the public view no longer exposes raw `key_behavior_view_t`
- the public view no longer carries a `resolved` flag
- downstream runtime callers now use handled-key flags/accessors instead of
  reading authored behavior through the handled-key surface

That is the right abstraction for this repo. The remaining handled-key issue is
not data leakage anymore; it is simply keeping future reducer growth
disciplined now that the public contract is clean.

### Improved abstraction: effect-bearing surfaces now share one queue vocabulary

Follow-up work on 2026-04-12 finished the public vocabulary cleanup for the
effect-bearing handled-key surfaces:

- slot results and transition plans now both expose `items/count/overflowed`
- the shared queue field vocabulary lives in
  [`key_runtime_effect_queue.h`](../../users/noah/lib/key/key_runtime_effect_queue.h)
- host tests intentionally fill both queue capacities and assert overflow
  logging plus saturation behavior

The reducer-local builder layer still exists, but it no longer leaks into the
public queue naming. That is a reasonable balance for this repo.

### Improved abstraction: pd-mode local and display queries are now explicit

The local/display query split resolves the earlier ambiguity around whether a
pd-mode query meant authoritative runtime state or mirrored split-sync UI
state. That is the right abstraction for this repo. Future work should keep
new policy code on local queries and reserve display queries for rendering and
other mirrored consumers.

### Improved abstraction: pd-mode implementations now follow per-mode ownership

Follow-up work on 2026-04-12 split the former
`pd_mode_handlers.c` monolith into per-mode translation units:

- [`pd_mode_volume.c`](../../users/noah/lib/pointing/pd_mode_volume.c)
- [`pd_mode_brightness.c`](../../users/noah/lib/pointing/pd_mode_brightness.c)
- [`pd_mode_zoom.c`](../../users/noah/lib/pointing/pd_mode_zoom.c)
- [`pd_mode_arrow.c`](../../users/noah/lib/pointing/pd_mode_arrow.c)

Shared vertical-axis behavior now lives in
[`pd_mode_handler_common.h`](../../users/noah/lib/pointing/pd_mode_handler_common.h),
while arrow-mode-specific state and key interception stay isolated in
`pd_mode_arrow.c`.

## 4. Code Organization And Structure

### Overall structure is good

The repo is easy to explain:

- authored profile in `keyboards/.../keymaps/noah/`
- reusable runtime in `users/noah/lib/`
- maintainer docs in `README.md` and `docs/`
- architecture history in `review/`
- host verification in `tests/host/`

The build surface is also unusually well organized. `source_manifest.mk` is a
real architectural asset because the firmware build and the feature-gate
compile tests both derive their source inventory from the same manifest.

### Improved discoverability: handled-key resolution now matches file ownership

Follow-up work on 2026-04-12 resolved the biggest naming mismatch here:
`handled_key.h` is now implemented in
[`handled_key.c`](../../users/noah/lib/key/handled_key.c), while
[`key_runtime.c`](../../users/noah/lib/key/key_runtime.c) has gone back to
generic runtime helpers. Preserve that split so handled-key resolution and
generic runtime utilities do not blur together again.

### Modules that are approaching "split when touched again"

These files are not architectural failures, but they are the ones most likely
to benefit from a focused split on the next substantial change:

- [`pd_mode_registry.c`](../../users/noah/lib/pointing/pd_mode_registry.c):
  lifecycle hook definitions, manifest materialization, DPI policy, and state
  transitions all live together
- [`held_action.c`](../../users/noah/lib/key/held_action.c): pure-mod binding
  logic, general held-action ownership, and repeat cleanup live together
- [`key_runtime_slot_step.c`](../../users/noah/lib/key/key_runtime_slot_step.c)
  and
  [`key_runtime_slot_release_reduce.c`](../../users/noah/lib/key/key_runtime_slot_release_reduce.c):
  these are coherent, but already dense enough that further growth should be
  deliberate

The right response is not "split them now because they are large". The right
response is "when the next real change lands here, split by ownership seam
instead of adding another layer of local helpers".

## 5. State Management And Flow

### What is working well

State ownership is much better than typical QMK userspace:

- key slot storage and pd flags live in
  [`runtime_shared_state.h`](../../users/noah/lib/state/runtime_shared_state.h)
- layer state ownership is explicit in
  [`layer_ownership.c`](../../users/noah/lib/state/layer_ownership.c)
- modifier ownership is explicit in
  [`keyboard_mod_ownership.c`](../../users/noah/lib/state/keyboard_mod_ownership.c)
- held actions and repeats are explicit in
  [`held_action.c`](../../users/noah/lib/key/held_action.c) and
  [`held_repeat.c`](../../users/noah/lib/key/held_repeat.c)

The release-by-physical-position invariant in the handled-key runtime is also
an important long-term strength. It prevents layer remaps from corrupting the
meaning of a key that is already in flight.

### Remaining state-management risk

The repo now has a good aggregate snapshot/reset seam in
[`runtime_debug.c`](../../users/noah/lib/state/runtime_debug.c), but that file
also reveals a truth about the current architecture: runtime state is still
distributed across several global modules, and tests need one explicit reset
surface to put them back into sync.

That is acceptable for firmware. It becomes a problem only if new state starts
appearing as more file-local globals without also joining that snapshot/reset
surface. The repo should keep a strict policy from here:

- new cross-scan state belongs either in `runtime_shared_state.h` or in an
  explicit ownership module
- if a new ownership module is added, `runtime_debug.h` and its tests should
  grow in the same pass

### Specific flow risk

[`key_runtime_process.c`](../../users/noah/lib/key/key_runtime_process.c)
contains a sensible and documented top-level event order, but that order is
architecturally significant:

1. synthetic bypass
2. preflight
3. active pd-mode key interception
4. handled-key runtime
5. direct action keys
6. macro dispatch

That is fine while the set of phases stays small. If another cross-cutting
subsystem is added here later, the cost will not just be one new `if` branch;
it will be deciding exactly where in the pipeline that subsystem belongs.

## 6. Scalability Of The Design

For this fixed board, CPU or storage scale is not the real issue. Board-sized
arrays and linear scans are acceptable and arguably preferable because they
keep ownership rules simple.

The real scaling risks are conceptual:

- another handled-key feature increases reducer/effect surface area
- another bespoke pd mode with nontrivial lifecycle work increases pressure on
  `pd_mode_registry.c`
- another stateful subsystem increases the number of places that must stay in
  reset/debug sync

There are also a few hard-coded limits that are acceptable now but should stay
visible:

- `pd_mode_mask_t` is still a `uint16_t`, so the current design caps out at 16
  shared modes
- `KEY_RUNTIME_SLOT_RESULT_CAPACITY` is `8`
- transition plans in `key_runtime_transition.c` cap at `16` effects

Those are not urgent problems, but they are the places where future feature
growth can fail abruptly instead of degrading gracefully.

## 7. Testing And Debuggability

This repo is above average here.

Strengths:

- subsystem-specific host runners under `tests/host/`
- real-profile validation tests for authored data
- source-manifest-driven feature-gate compile tests
- header-boundary enforcement in
  [`run_feature_gate_compile_tests.sh`](../../tests/host/run_feature_gate_compile_tests.sh)
- `runtime_debug` snapshot/reset surface
- scenario harness rebuilt on real `key_runtime_effect_t`

The main remaining gap is live observability across subsystem boundaries.
`key_runtime_trace` is useful, but it is still runtime-specific. Pd-mode
lifecycle, split sync, and ownership modules do not all share one structured
debug vocabulary.

An optional unified event sink would help future debugging more than more
ad hoc printf traces would.

Example direction:

```c
typedef enum {
    NOAH_TRACE_KEY_EFFECT = 0,
    NOAH_TRACE_PD_MODE,
    NOAH_TRACE_LAYER_OWNERSHIP,
    NOAH_TRACE_SPLIT_SYNC,
} noah_trace_kind_t;

void noah_trace_emit(noah_trace_kind_t kind, uint16_t a, uint16_t b);
```

That does not need to be a full logging framework. A small ring buffer behind
`CONSOLE_ENABLE` or a dedicated debug flag would already make multi-subsystem
bugs easier to replay.

Saturation coverage is also better now:

- the slot host suite intentionally fills slot-result capacity
- the transition host suite intentionally fills transition-plan capacity
- both suites assert the overflow flag and first-overflow logging path

Keep those tests in place whenever queue capacities or overflow policy change.

## 8. Concrete Recommendations

### Recommendation 1: harden the handled-key contract

Priority: high

Status: implemented in follow-up work on 2026-04-12

All three cleanups landed:

1. handled-key resolution moved out of `key_runtime.c` into a dedicated
   `handled_key.c`
2. `handled_key_view_t` is now fully resolved on its public surface
3. slot results and transition plans now share one explicit effect-queue
   vocabulary with matching overflow coverage

That keeps the extension cost for new handled-key behavior closer to one public
pipeline instead of several adjacent vocabularies.

### Recommendation 2: preserve explicit pd-mode local and display queries

Priority: high

Status: implemented in follow-up work on 2026-04-12

Keep local authoritative state and split-mirrored state distinct.

Example direction:

```c
typedef struct {
    pd_mode_mask_t local_active;
    pd_mode_mask_t local_locked;
    pd_mode_mask_t remote_display_active;
    pd_mode_mask_t remote_display_locked;
} pd_mode_runtime_state_t;
```

Then make query names explicit:

- `pd_mode_local_active(...)`
- `pd_mode_display_active(...)`

That prevents future non-UI subsystems from accidentally acting on mirrored
state on the slave half.

### Recommendation 3: split pd-mode implementations by mode when the next bespoke mode lands

Priority: medium

Status: implemented in follow-up work on 2026-04-12

The manifest stayed exactly as it was, while mode-local implementations moved
into per-mode translation units plus a small shared axis-helper header. That
keeps mode-owned state discoverable without introducing a plugin system or
changing the public pd-mode registry contract.

### Recommendation 4: keep the build/test contract manifest-driven

Priority: medium

The `source_manifest.mk` plus feature-gate compile test combination is one of
the best extensibility decisions in the repo. Preserve it.

When new runtime files appear:

- add them to `source_manifest.mk` in the same pass
- update compile/test runners only when a new feature slice truly needs a new
  runner
- keep header-boundary checks centralized instead of scattering include
  policies into comments

### Recommendation 5: add overflow and cross-subsystem trace coverage

Priority: medium

Add:

- saturation tests for slot-result and transition-plan capacities
- one shared optional trace sink for key-runtime, pd-mode, and split-sync
  events

That will pay off faster than a larger refactor because the remaining
architecture risks are mostly about understanding cross-subsystem flow when
something unusual happens.

## Final Assessment

This userspace already has the right long-term shape for a fixed keyboard:

- authored profile data stays declarative
- runtime policy is centralized
- QMK-specific contracts are isolated
- state ownership is explicit
- tests are strong enough to support continued refactoring

The next improvements should be targeted, not sweeping:

- tighten the handled-key public contract
- preserve the explicit pd-mode authority/display split
- split large implementation files only when the next real feature touches
  them

That path improves extensibility without throwing away the codebase's current
strengths.
