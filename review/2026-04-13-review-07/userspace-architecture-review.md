# Userspace Architecture Review

Date: 2026-04-13

Status: architecture review plus the first action-contract follow-up logged in
[progress.md](./progress.md). This file now reflects the current
post-follow-up architecture rather than only the review-open state.

Scope:

- `users/noah/` runtime architecture, interfaces, and extension seams
- the authored-data boundary in
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/`
- host-test and debug surfaces where they define the real contract

Out of scope:

- hardware changes
- upstream QMK redesign
- replacing this userspace with a different firmware stack

## Executive Summary

The codebase is in a materially better place than most keyboard userspaces.
The authored keymap/runtime split is real, the handled-key path is a real state
machine rather than ad hoc hook branching, QMK compatibility seams are mostly
centralized, and the host test surface is broad enough to support architectural
change without flying blind.

The main long-term cost is no longer "the runtime is mysterious." The main
cost is "adding a new capability still means editing core dispatch and state
modules." The remaining extensibility bottlenecks are:

1. action semantics are substantially more centralized now; the remaining
   coupling is in handled-key policy inference rather than lifecycle dispatch
2. pd-mode precedence and remote-display semantics still depend partly on
   manifest order and shared policy helpers instead of explicit mode-owned
   descriptors
3. key-runtime cross-key coordination still works by repeated whole-table
   sweeps
4. the macro subsystem shares one IR but not one source/cache abstraction

The right direction is evolutionary, not a rewrite. Keep the current
state-machine approach for the key runtime. Refactor the extension seams around
descriptor tables, smaller ownership registries, and shared source adapters.

Implementation status after follow-ups in this review folder:

- reduced further: `noah_action_desc_t` now owns authored-surface support and
  direct press-consumption queries, and `action_lifecycle.c` now routes
  tap/press/release through a kind-owned ops table instead of one large branch
  tree
- still open: handled-key fallback and implicit-hold policy still inspect
  action kinds directly instead of consuming narrower action-owned policy
  helpers
- open: pd-mode identity and remote-display semantics still rely on implicit
  registry-order precedence
- open: key-runtime cross-key coordination still uses whole-table sweeps
- open: the macro subsystem still lacks a shared source/cache abstraction

## Areas That Are Solid

- The keymap/runtime split is real. `keymap.c` remains largely authored data,
  while shared behavior lives under `users/noah/`.
- The handled-key runtime is now understandable as slot state plus ordered
  effects. `users/noah/lib/key/runtime/` is much easier to reason about than a
  direct-hook architecture.
- QMK compatibility work is mostly centralized under `users/noah/lib/compat/`,
  which is the right boundary for fork-specific behavior.
- `runtime_debug` and the host suite make architectural work testable instead
  of aspirational. This repo has better feedback loops than most firmware
  userspaces.
- The docs are aligned with the architecture often enough that they are useful
  as contracts, especially `README.md`, `docs/KEY_RUNTIME.md`, and
  `docs/ADDING_PD_MODE.md`.

## Findings

### High Priority

#### 1. Action semantics are mostly centralized now, but handled-key policy still inspects action kinds directly

Status:

- Reduced further in the action-contract follow-ups. The descriptor now caches
  authored-surface and direct-press capability queries, and lifecycle dispatch
  now routes through a kind-owned ops table. The main remaining coupling is in
  handled-key fallback/implicit-hold policy.

References:

- `users/noah/lib/action/action_dispatch.h`
- `users/noah/lib/action/action_lifecycle.c`
- `users/noah/lib/key/interaction/key_behavior_lookup.c:47-61`
- `users/noah/lib/key/interaction/keymap_validation.c:16-18`
- `users/noah/lib/key/runtime/key_runtime_preflight.c:83-95`
- `users/noah/lib/key/interaction/handled_key.c:55-76`

Why this matters:

- `noah_action_describe(...)` is the canonical action classifier, but it is
  not yet the canonical action behavior interface.
- The first follow-up removed the easiest duplication: authored-surface
  support and direct-action consumption now come from the descriptor contract
  instead of being re-derived in validation and preflight callers.
- The second follow-up moved tap/press/release behavior onto a kind-owned ops
  table inside `action_lifecycle.c`, so lifecycle dispatch is no longer one
  large action-kind branch tree.
- The main remaining semantic coupling is handled-key fallback and
  implicit-hold inference.
- Adding a genuinely new action kind or capability still means editing several
  core modules instead of introducing one new implementation unit.

Where the coupling shows up:

- `action_dispatch.h` decides what an action *is*.
- `action_lifecycle.c` now owns a smaller action-ops table keyed by action
  kind instead of one large tap/press/release branch tree.
- `key_behavior_lookup.c` and `keymap_validation.c` now consume the shared
  authored-surface capability instead of reconstructing their own raw-layer
  exceptions.
- `key_runtime_preflight.c` now consumes the shared
  `consumes_direct_press` capability instead of hand-maintaining the
  layer-lock/pd-lock special case.
- `handled_key.c` separately decides fallback-hold and implicit-hold behavior
  based on the same action vocabulary.

Why this is the main extensibility bottleneck:

- Adding a new feature such as a userspace-owned sticky action, a different
  one-shot surface, or a new per-key system action will require coordinated
  edits across the classifier, authored validation, lifecycle dispatch, and
  preflight routing.
- That is manageable today because the action vocabulary is still small. It is
  exactly what will become expensive once the repo grows more runtime-owned
  features.

Recommended direction:

- Keep the descriptor-owned capability contract and the new action-ops table.
- Treat the remaining action work as policy cleanup, not dispatch cleanup:
  decide whether handled-key fallback and implicit-hold inference should use
  narrower action-owned helpers instead of direct kind checks.
- Avoid expanding the capability bitset further unless a new caller truly
  needs shared metadata; the current shape is now good enough for the existing
  validation, preflight, and lifecycle consumers.

Example shape:

```c
typedef struct {
    bool (*authored_use_supported)(uint16_t action, hold_behavior_mode_t hold_mode);
    bool (*consumes_direct_press)(uint16_t action);
    void (*tap)(uint16_t action);
    void (*press)(keypos_t key_pos, uint16_t action);
    void (*release)(keypos_t key_pos, uint16_t action);
} noah_action_ops_t;

typedef struct {
    noah_action_kind_t         kind;
    uint16_t                   action;
    uint16_t                   caps;
    uint8_t                    layer;
    pd_mode_mask_t             pd_mode;
    const noah_action_ops_t   *ops;
} noah_action_desc_t;
```

That keeps the current action taxonomy, but makes new action kinds additive
instead of cross-cutting.

#### 2. Pd-mode identity, precedence, and split-display semantics are still partially implicit

References:

- `users/noah/lib/pointing/defs/pd_mode_manifest.h:68-74`
- `users/noah/lib/pointing/runtime/pd_mode_registry.c:20-55`
- `users/noah/lib/pointing/runtime/pd_mode_registry.c:74-80`
- `users/noah/lib/pointing/policy/pd_mode_policy.h:12-33`
- `users/noah/lib/pointing/runtime/pd_mode_state.c:98-110`
- `users/noah/lib/pointing/runtime/pd_mode_snapshot.c:15-39`
- `users/noah/lib/state/runtime/split_runtime_sync.h:16-22`
- `users/noah/lib/pointing/defs/pd_mode_flags.h:21-36`

Why this matters:

- Local pd-mode state is intentionally exclusive, which is good.
- Remote display state is reconstructed from flag masks by asking for the
  "first" matching mode in registry order.
- That means mode precedence on the slave half is still partly a property of
  manifest row order, not an explicit part of the mode contract.

Where the abstraction leaks:

- `split_runtime_sync_packet_t` still transports `pd_mode_flags` and
  `pd_mode_locked_flags`, even though the runtime only wants one effective
  active mode and one effective locked mode for UI purposes.
- `pd_mode_policy_first_snapshot_mode(...)` picks the first flag whose bit
  matches `pd_modes[index].mode_flag`, so display precedence is coupled to the
  generated registry ordering.
- `pd_mode_registry.c` still owns the auto-mouse lock lifecycle object for one
  mode family, which means registry materialization is not yet purely data.

Why this is a design bottleneck:

- Adding a mode with unusual precedence or mixed policy is harder than it
  should be because precedence is implicit and shared policy still lives outside
  the mode definition.
- The flag-mask transport also keeps a misleading multi-mode surface alive in a
  system that now behaves as exclusive state.
- Future work such as per-mode display priority, mirrored overlay modes, or
  half-local UI state will have to fight the current packet shape.

Recommended direction:

- Make effective active/locked mode identity explicit in the sync contract.
- Keep trait flags if they are still useful locally, but do not derive display
  identity from "first flag wins."
- Move mode-specific lifecycle objects and default policy metadata fully into
  mode-owned descriptors; keep `pd_mode_registry.c` as a table materializer and
  lookup layer.

Example shape:

```c
typedef struct {
    pd_mode_mask_t                   mode_flag;
    uint8_t                          sync_id;
    uint8_t                          display_priority;
    uint16_t                         keycode;
    uint16_t                         lock_action;
    pd_mode_traits_t                 traits;
    const pd_mode_lifecycle_hooks_t *lifecycle;
    const pd_mode_policy_vtable_t   *policy;
} pd_mode_descriptor_t;

typedef struct __attribute__((packed)) {
    uint16_t automouse_progress;
    uint8_t  active_mode_id;
    uint8_t  locked_mode_id;
    uint8_t  key_feedback_flags;
    uint8_t  key_preview_layer;
} split_runtime_sync_packet_t;
```

That would remove manifest-order precedence from the architecture and make
mode-owned policy truly mode-owned.

### Medium Priority

#### 3. Key-runtime cross-key coordination is still table-scan driven rather than registry-driven

References:

- `users/noah/lib/key/runtime/key_runtime_shared_state.h:49-71`
- `users/noah/lib/key/runtime/key_runtime_preflight.c:40-78`
- `users/noah/lib/key/runtime/key_runtime_transition.c:144-173`
- `users/noah/lib/key/runtime/key_runtime_transition.c:207-214`
- `users/noah/lib/key/runtime/slot/key_runtime_slot.c:173-192`
- `users/noah/lib/key/runtime/key_runtime_feedback.c:73-88`
- `users/noah/lib/key/runtime/key_runtime_feedback.c:178-188`

Why this matters:

- The fixed hardware means these sweeps are not a performance emergency.
- They are still an architectural pressure point because the runtime discovers
  many cross-key facts by re-walking the entire slot table each time.

Where the coordination tax shows up:

- Preflight scans every slot to decide whether another active key exists and
  whether unrelated pending multi-taps need flushing.
- Transition flow scans every slot again to interrupt keys, flush pending
  multi-taps, and run active-scan and pending-scan passes.
- Feedback and preview-layer aggregation also derive their answer by scanning
  the entire slot array.

Why this becomes a scalability problem:

- Every new cross-key rule tends to become "add another sweep."
- That keeps state ownership implicit: the real active set, pending multi-tap
  set, and preview-layer owners are not first-class runtime objects.
- It also makes future scheduling work harder. If the runtime ever grows more
  deadline-driven behavior, the architecture will have to keep layering more
  scan passes on top of slot storage.

Recommended direction:

- Keep the direct `slots_by_position[]` storage.
- Add small runtime registries for "active slots", "pending multi-tap slots",
  and "preview contributors".
- Let scan and preflight iterate those registries instead of the full table.

Example shape:

```c
typedef struct {
    active_key_state_t slots_by_position[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t            active_slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t            active_slot_count;
    uint8_t            pending_multi_tap_slots[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
    uint8_t            pending_multi_tap_count;
} key_runtime_shared_state_t;
```

That would keep the current reducers intact while making global coordination
more explicit and easier to extend.

#### 4. The macro subsystem shares one IR, but not one source/cache abstraction

References:

- `users/noah/lib/macro/macro_dispatch.c:11-58`
- `users/noah/lib/macro/macro_dispatch.c:61-82`
- `users/noah/lib/macro/via_macro_defaults.c:18-72`
- `users/noah/lib/macro/via_macro_defaults.c:104-166`
- `users/noah/lib/compat/qmk_contract.c:17-69`
- `users/noah/lib/macro/macro_payload_run.c:64-155`
- `users/noah/lib/macro/macro_payload_run.c:184-260`

Why this matters:

- Hardcoded macros, authored VIA defaults, and dynamic VIA playback all
  eventually use the same payload language and IR.
- But each source still owns separate validation state, separate slot traversal,
  and separate loading logic.

Where the duplication is:

- `macro_dispatch.c` maintains a per-slot validation/IR cache for hardcoded
  macros.
- `via_macro_defaults.c` maintains a separate per-slot validity cache for VIA
  default payloads and its own seeding flow.
- `qmk_contract.c` separately walks QMK's VIA storage and decodes it into IR.
- `macro_payload_run.c` still mixes IR construction helpers, QMK-stream decode,
  and execution behavior in one large implementation unit.

Why this is an extensibility problem:

- Adding another macro source, profile bank, or import/export path will likely
  repeat the same "is payload valid, how do I read slot N, when do I compile
  IR" questions again.
- The runtime already has the right common denominator: `macro_payload_ir_t`.
  The missing piece is a shared source adapter/caching layer.

Recommended direction:

- Add a source abstraction for "read payload slot", "compile/cache slot", and
  "iterate all slots".
- Split the remaining mixed responsibilities in `macro_payload_run.c` into an
  IR builder/decoder side and an execution side.

Example shape:

```c
typedef struct {
    uint8_t slot_count;
    bool (*payload_for_slot)(uint8_t slot, const char **payload);
    bool (*ir_for_slot)(uint8_t slot, macro_payload_ir_t *out);
} macro_source_t;

bool macro_source_validate_all(const macro_source_t *source);
bool macro_source_play_slot(const macro_source_t *source, uint8_t slot);
```

That would let hardcoded macros, VIA defaults, and live VIA storage behave like
different adapters over the same runtime model instead of adjacent systems.

## Review By Requested Axis

### Architecture & Separation Of Concerns

- Strong: key runtime reducers, ownership modules, and QMK compat seams are far
  cleaner than typical firmware hook code.
- Weak point: action semantics still cut across classification, validation,
  lifecycle, and preflight instead of having one behavior contract.
- Weak point: pd-mode registry, shared policy, and split-display selection are
  still not completely separated from mode-owned behavior.

### Modularity & Extensibility

- Adding new authored key rows and layer data is easy.
- Adding a genuinely new action kind, pd-mode policy variant, or macro source
  still requires modifying core modules.
- The best leverage is not a generic plugin system; it is action descriptors,
  pd-mode descriptors, and macro source adapters.

### Abstractions & Interfaces

- `key_runtime_slot_interaction_t` and the slot/effect model are now meaningful
  abstractions.
- `pd_mode_snapshot_t` is useful, but the remote-display contract underneath it
  still leaks registry order.
- `noah_action_desc_t` is a useful descriptor, but it is not yet a complete
  interface because callers still branch on its internals.

### Code Organization & Structure

- The directory split by ownership is good and discoverable.
- Remaining file-size hotspots such as `macro_payload_run.c`,
  `pd_mode_state.c`, `key_runtime_slot_pending_multi_tap.c`, and
  `key_runtime_slot_release_active.c` are not inherently wrong, but they do
  show where multiple responsibilities still converge.
- The current structure is strong enough that refactors can stay local if they
  target the right seams.

### State Management & Flow

- The handled-key runtime uses explicit phases, cached interaction, and ordered
  effects. That is a strong state-management story.
- Pd-mode local state is explicit, but effective display state is still derived
  through implicit precedence rules.
- Key-runtime global flow still relies on whole-table discovery instead of
  explicit registries for active or pending entities.

### Scalability Of The Design

- Because the hardware is fixed, runtime cost is acceptable.
- The real scaling risk is architectural: more features will keep increasing the
  number of core modules that must change together unless the extension seams
  become more explicit.
- The current design scales well for more authored data, but not yet as well
  for more runtime-owned feature classes.

### Testing & Debuggability

- This is a strong area. The host suite, `runtime_debug`, and `runtime_trace`
  make the architecture auditable.
- The next debug improvement with the best payoff would be to expose more of
  the new proposed registries or descriptor identities in `runtime_debug` so
  tests can assert architecture-level state directly instead of inferring it
  from behavior.

## Concrete Refactoring Sequence

1. Completed first pass: `noah_action_desc_t` now owns authored-surface and
   direct-press capability queries, and lifecycle dispatch now routes through a
   kind-owned action-ops table. Next, decide whether handled-key policy should
   consume narrower action-owned helpers instead of direct kind checks.
2. Make pd-mode display precedence explicit in the sync/state contract and move
   remaining mode-specific lifecycle/policy ownership out of registry core.
3. Add explicit key-runtime registries for active and pending slots, while
   keeping the current slot reducers intact.
4. Introduce a shared macro source/cache abstraction over hardcoded macros,
   VIA defaults, and live VIA storage.
5. After each step, extend `runtime_debug` and targeted host tests so the new
   seam is directly observable.

## Verification

Verification run during this review:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- worktree was clean before the review was opened
- verification results are recorded in [progress.md](./progress.md)

Workspace scope:

- no sibling workspace folders were edited
- this review adds files only under `review/2026-04-13-review-07/`
