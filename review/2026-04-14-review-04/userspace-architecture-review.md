# Userspace Architecture Review

Date: 2026-04-14

Status: architecture-focused review of the current userspace design after the
audit and cleanup work recorded through `review/2026-04-14-review-03/`.

Scope:

- userspace runtime architecture and subsystem boundaries
- extensibility of key behavior, action, macro, layer, and pointing features
- state flow, testing seams, and long-term maintainability

Out of scope:

- hardware changes
- upstream QMK redesign outside this repo
- correctness-only bug hunting unrelated to architecture

## Executive Summary

The userspace is in a materially better place than the earlier refactor notes:
authored keymap data is kept mostly data-only, the handled-key runtime has a
clearer materialization seam, compat assumptions are centralized under
`users/noah/lib/compat/`, and runtime debug/trace surfaces make the state model
observable.

The main remaining architecture risks are not in the basic directory layout.
They are in the places where the code still centralizes decisions procedurally:

1. the key-runtime "index" is still a rebuild-on-read cache instead of a
   maintained registry
2. the input pipeline remains a hard-coded ordered chain in
   `noah_process_record_user(...)`
3. the action model is split across multiple files, so new action classes are
   not added in one place
4. several host runtime suites still validate a test-only reconstruction of
   handled-key materialization instead of the production materializer

None of those are immediate correctness failures, but they are the main places
where future features will force edits to core logic instead of extending a
stable seam.

Implementation note (2026-04-14):

- the follow-up implementation for this review landed the maintained
  key-runtime index, the ordered process-record handler pipeline, the runtime
  test cutover to the real `handled_key_materialize(...)` seam, and the move
  of handled-key policy builders out of public `handled_key.h`
- the action-model cleanup landed as shared action-kind
  metadata/classification in `users/noah/lib/action/action_kind.c` plus
  internal handled-key policy hints; lifecycle tap/press/release ops still
  remain local to `action_lifecycle.c`

## Findings

### No Must-Fix Architecture Break Found

I did not find a single architectural fault that makes the current userspace
untenable. The system is coherent enough to maintain. The issues below are
should-fix design bottlenecks that will accumulate debt as the runtime grows.

### Should-Fix

#### 1. The new key-runtime "index" is still a full-table rebuild cache, not a maintained registry

References:

- `users/noah/lib/key/runtime/key_runtime_index.h:21-55`
- `users/noah/lib/key/runtime/key_runtime_transition.c:145-176`
- `users/noah/lib/key/runtime/key_runtime_transition.c:211-228`
- `users/noah/lib/key/runtime/key_runtime_preflight.c:20-26`
- `users/noah/lib/key/runtime/key_runtime_feedback.c:77-79`
- `users/noah/lib/key/runtime/key_runtime_feedback.c:164-190`

Why this matters:

- The header describes the index as an explicit registry cached in shared
  state, but the implementation still rebuilds it by scanning
  `slots_by_position[]` whenever consumers need it.
- Preflight, transition scan/interrupt/flush, and feedback all call
  `key_runtime_index_rebuild()` directly before reading the index snapshot.
- That means the architectural contract is still "remember to rebuild before
  you ask cross-slot questions", not "slot mutation updates shared index state".

Why this is a long-term extensibility risk:

- Adding a new cross-slot concept means touching both the slot storage model and
  the rebuild routine, then relying on every caller to keep using the same
  refresh pattern.
- The abstraction is leaky: callers think they are reading a registry, but they
  are really consuming a lazily reconstructed derivative of the slot table.
- This keeps cross-slot coordination coupled to storage layout instead of to a
  narrow mutation surface.

Recommended direction:

- Move index maintenance behind the slot mutation API so writes/reset/pending
  multi-tap changes update the registry directly.
- If immediate maintenance is too invasive, add a single dirty-bit plus
  `key_runtime_index_refresh_if_needed()` barrier and stop calling
  `key_runtime_index_rebuild()` from scattered consumers.

#### 2. The process-record pipeline is still a hard-coded ordered chain, so new input features must edit the core router

References:

- `users/noah/lib/key/runtime/key_runtime_process.c:23-80`
- `users/noah/lib/key/runtime/key_runtime_preflight.c:20-103`
- `users/noah/lib/action/action_lifecycle.c:40-59`
- `users/noah/lib/action/action_lifecycle.c:226-260`

Why this matters:

- `noah_process_record_user(...)` is still the one place that decides the full
  event order: synthetic bypass, preflight, pd-mode interception, handled-key
  runtime, direct-action dispatch, then macro dispatch.
- Direct-action handling also lives half in preflight and half in the action
  lifecycle path, so "what consumes this key event first?" is still controlled
  procedurally rather than by a declared pipeline.

Why this is a long-term extensibility risk:

- Adding a new cross-cutting input feature such as tap-dance-style authored
  actions, a new mode interceptor, or another ownership-aware direct action
  means editing this central router and reasoning about ordering against every
  existing branch.
- The separation of concerns is better at the module level than at the event
  routing level. The core router is still closed for extension and open for
  modification.

Recommended direction:

- Replace the hand-written chain with a small ordered handler pipeline, for
  example:

  ```c
  typedef bool (*noah_record_handler_fn)(noah_record_event_t *event);

  static const noah_record_handler_fn noah_record_handlers[] = {
      noah_record_handler_synthetic,
      noah_record_handler_preflight,
      noah_record_handler_pd_mode,
      noah_record_handler_handled_key,
      noah_record_handler_direct_action,
      noah_record_handler_macro,
  };
  ```

- Keep the ordering explicit, but move each phase behind a consistent handler
  contract so new features extend the table instead of rewriting the router.

#### 3. The action abstraction is still defined in pieces, so adding a new action kind is a cross-cutting change

References:

- `users/noah/lib/action/action_dispatch.h:59-152`
- `users/noah/lib/action/action_dispatch.h:214-233`
- `users/noah/lib/action/action_lifecycle.c:40-59`
- `users/noah/lib/action/action_lifecycle.c:155-260`
- `users/noah/lib/key/interaction/key_behavior_lookup.c:46-58`
- `users/noah/lib/key/interaction/handled_key.h:132-206`

Why this matters:

- Action classification lives in `noah_action_describe(...)` and its capability
  bits.
- Tap/press/release execution lives in `noah_action_ops_by_kind`.
- One-shot and direct-press behavior lives in a separate
  `noah_action_handle_one_shot_press(...)` branch.
- Authored behavior validation depends on the capability matrix in
  `key_behavior_lookup.c`.
- Held-key semantics such as preview layers, held lifecycle use, and feedback
  behavior are re-derived again in `handled_key_behavior_contract(...)`.

Why this is a long-term extensibility risk:

- A new action class is not added in one place. It currently requires parallel
  edits to action description, lifecycle dispatch, authored validation, and
  handled-key semantics.
- That is manageable today, but it means the "action abstraction" is not yet a
  single extensibility seam. It is a contract spread across multiple modules.

Recommended direction:

- Collapse the action-kind definition into one registry-like description table
  that owns:
  - classification
  - authored-use support
  - tap/press/release ops
  - direct-press behavior
  - hold/feedback semantics hints
- Keep the runtime-specific hold contract builder separate if needed, but feed
  it from one per-action-kind definition instead of scattered conditionals.

#### 4. The shared host handled-key fixture still bypasses the production materializer, which weakens the main runtime seam

References:

- `tests/host/include/host_handled_key_fixture.h:11-31`
- `tests/host/key_runtime_slot_test.c:499-500`
- `tests/host/key_runtime_preflight_test.c:298-299`
- `tests/host/key_runtime_feedback_test.c:202-203`
- `tests/host/runtime_debug_test.c:198-199`

Why this matters:

- The shared host helper ignores `handled_key_resolution_ctx_t` entirely and
  reconstructs `handled_key_materialized_t` from authored resolution helpers.
- Multiple runtime suites then stub `handled_key_materialize(...)` with that
  host helper.

Why this is a long-term testing risk:

- The production materializer is now the intended seam for transparency and
  context-sensitive behavior, but a substantial part of the runtime test suite
  still validates a test-owned approximation of that seam.
- That makes the tests easier to write, but it also means future changes to
  transparency or context handling can require updating the fixture and the
  runtime together, which hides integration drift.

Recommended direction:

- Keep the authored-resolution helper for narrow unit tests, but add a second
  tier of runtime tests that link the real `handled_key_materialize(...)`
  implementation and only stub the authored lookup side.
- Treat the current host fixture as a local test convenience, not as the main
  semantic seam for higher-level runtime suites.

### Optional Cleanup

#### 5. `handled_key.h` is still doing too much as a public surface

References:

- `users/noah/lib/key/interaction/handled_key.h:15-97`
- `users/noah/lib/key/interaction/handled_key.h:132-229`

Why this matters:

- The file is simultaneously the public type surface for authored resolution,
  the materialized contract surface, and the inline home for hold-policy and
  feedback-related behavior semantics.
- The split into defaults/transparency/materialize `.c` files helped
  implementation navigation, but the public header still carries a lot of
  runtime policy.

Why this is worth cleaning up:

- It increases compile-time coupling and makes the public handled-key seam
  harder to discover.
- New hold semantics or behavior-contract fields still push more logic into an
  already overloaded header.

Recommended direction:

- Keep `handled_key.h` for the public data types and lookup/materialize API.
- Move inline behavior-contract construction into a smaller internal policy
  header or a `.c` implementation with exported helpers.

## Areas Assessed As Solid

- The repo-level split between authored keymap data under
  `keyboards/.../keymaps/noah/` and reusable runtime logic under `users/noah/`
  is real, not cosmetic.
- The handled-key pipeline is much clearer after the move to explicit
  `handled_key_materialize(...)`.
- The slot reducer structure in `key_runtime_slot_*` is easier to reason about
  than the older monolithic flow; press, release, scan, and pending multi-tap
  logic now have discoverable homes.
- The compat boundary under `users/noah/lib/compat/` is doing useful work:
  split/VIA/QMK assumptions are more centralized than they were in earlier
  reviews.
- Runtime observability is strong for firmware code: the combination of
  `runtime_debug` snapshots, `runtime_trace`, and the host test runner surface
  makes the system much easier to inspect than a typical QMK keymap repo.

## Concrete Recommendations

1. Make the key-runtime index a true mutation-owned registry.
   Start by centralizing all slot writes/resets behind helpers that update
   `key_runtime_index_state_t`, then delete free-standing rebuild calls from
   transition/feedback/preflight consumers.

2. Introduce a small ordered record-handler pipeline for `process_record_user`.
   Keep today’s order, but express it as a handler table so new features extend
   the pipeline instead of modifying the router body.

3. Unify action-kind definition in one place.
   A single per-kind descriptor table should answer:
   - can this be used in authored tap/hold tiers?
   - does it consume direct press?
   - what are its tap/press/release ops?
   - what hold-semantic hints does it expose?

4. Split runtime tests into two layers.
   - fast authored-resolution tests may keep the host fixture
   - higher-level runtime tests should link the real materializer and verify the
     production handled-key seam

5. Keep leaning into the current strengths.
   The repo already has a good foundation for a declarative userspace. The next
   improvements should be about making the existing seams truly authoritative,
   not replacing the whole architecture again.

## Verification

Commands run for this review:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- full host suite passed
- firmware build passed

Workspace scope:

- no sibling workspace folders were edited
- this review pass adds `review/2026-04-14-review-04/`
