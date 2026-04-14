# Userspace Architecture Review

Date: 2026-04-14

Status: follow-up quality audit of the refactor work landed through
`review/2026-04-14-review-05/`.

Scope:

- correctness and regression risk in the review-05 follow-up cleanup
- abstraction quality after the handled-key, index, and action-kind changes
- test quality, interface cleanliness, and review integrity for the current
  landed tree

Out of scope:

- fresh architecture brainstorming
- hardware changes
- upstream QMK redesign outside this repo

## Findings

### No Must-Fix Issues Found

I did not find a new production correctness failure that is clearly shipping
broken firmware behavior today. The full host suite and firmware build are
green, and the review-05 follow-up did materially improve the handled-key and
runtime seams.

### Should-Fix

#### 1. The action-kind seam is cleaner internally, but it is now over-coupled to runtime side effects and the host test surface is compensating with a broad weak-stub shim

References:

- `users/noah/lib/action/action_kind.c:17-20`
- `users/noah/lib/action/action_kind.c:47-153`
- `tests/host/action_kind_host_stubs.c:13-78`
- `users/noah/lib/state/ownership/layer_ownership.h:27-32`
- `tests/host/run_key_behavior_lookup_tests.sh:15-30`

Why this matters:

- `action_kind.c` now owns both classification metadata and executable
  tap/press/release hooks. That means even metadata-only consumers, such as
  key-behavior lookup and validation runners, now have to link the full action
  side-effect dependency surface.
- The host runners are compensating by pulling in one shared weak shim file,
  `tests/host/action_kind_host_stubs.c`, across many suites.
- That shim is already drifting from the real interfaces:
  `layer_ownership_toggle_lock_state(...)` and
  `layer_ownership_momentary_release(...)` are declared as `bool` in the real
  header, but the shared shim defines them as `void`.

Why this is still a quality and regression risk:

- The action-kind cleanup did remove `caps` from the public descriptor, but it
  also turned the action-kind translation unit into a link-time magnet for
  unrelated runtime hooks.
- The shared weak shim can now hide exactly the sort of accidental coupling the
  host runners should be surfacing.
- Because the shim is not type-checked against the real headers, interface
  drift can slip in silently, as it already has.

Recommended direction:

- Keep the shared action-kind table, but separate pure classification metadata
  from side-effectful dispatch hooks again, or move the hook table behind a
  narrower adapter translation unit.
- At minimum, make the shared stub file include the real headers it is faking
  so signature drift fails at compile time instead of being masked by weak
  linkage.

#### 2. The new key-runtime index is mutation-maintained, but the public API still exposes raw storage layout and core consumers still depend on that layout directly

References:

- `users/noah/lib/key/runtime/key_runtime_index.h:11-17`
- `users/noah/lib/key/runtime/key_runtime_shared_state.h:53-60`
- `users/noah/lib/key/runtime/key_runtime_transition.c:145-153`
- `users/noah/lib/key/runtime/key_runtime_transition.c:157-164`
- `tests/host/key_runtime_index_test.c:63-82`

Why this matters:

- Review-05 successfully internalized index mutation, but the public read side
  still exposes `key_runtime_index_state_t` and
  `key_runtime_index_state_snapshot()`.
- That structure includes raw slot-index arrays and sentinel-encoded ownership
  fields, so callers can couple themselves to storage layout instead of using
  semantic accessors.
- The problem is not theoretical: `key_runtime_transition.c` copies
  `pending_multi_tap_slots[]` and `active_slots[]` out of the public snapshot,
  and the dedicated index test also reaches into the raw snapshot for counts.

Why this is still a maintainability problem:

- The new registry is supposed to be a semantic boundary, but the current API
  still makes the internal representation part of the contract.
- That will make future changes harder, such as changing iteration strategy,
  compacting the representation, or hiding raw slot indices from non-runtime
  consumers.

Recommended direction:

- Keep the mutation-maintained registry, but narrow the public surface to
  accessors and purpose-built iteration helpers.
- Reserve `key_runtime_index_state_t` and raw slot-index arrays for runtime
  internals and debug snapshots, not general consumers.

#### 3. The host handled-key fixture still standardizes an internal override path, so higher-level runtime suites are not fully constrained to the public authored-to-materialized seam

References:

- `tests/host/include/host_handled_key_fixture.h:8-21`
- `tests/host/key_runtime_transition_test.c:265-274`
- `tests/host/key_runtime_slot_test.c:160-169`

Why this matters:

- The review-05 follow-up did remove the old resolution-based runtime
  constructors, and the main suites now start from production
  `handled_key_materialize(...)`.
- But the shared test fixture still includes `handled_key_internal.h` and
  exposes `host_key_runtime_slot_interaction_from_overridden_materialized(...)`,
  which refreshes contracts through the internal
  `handled_key_materialized_refresh_contract(...)` helper.
- Higher-level runtime suites use that helper for behavior tests, not just
  narrow constructor tests.

Why this is still a test-quality concern:

- The common fixture is now encoding an internal “mutate arbitrary materialized
  state and refresh it” protocol as part of the routine host-testing path.
- That keeps the main runtime suites closer to production than before, but they
- are still not fully pinned to the intended public seam of authored lookup +
  explicit materialization + runtime consumption.

Recommended direction:

- Keep direct materialized override helpers local to constructor-level tests.
- For broader runtime behavior suites, prefer authored input plus explicit
  context, or add a narrower public/test-only builder that does not require
  importing handled-key internals.

### Optional Cleanup

#### 4. The review-05 progress note still reads like the final verification had not happened

References:

- `review/2026-04-14-review-05/progress.md:139-155`

Why this matters:

- The implementation note says the next steps are to run the final full host
  suite and firmware build.
- In the current tree, those commands have already been run successfully, so
  the progress note understates the landed state.

Recommended direction:

- Update the review-05 progress note so its verification record and next-step
  section match the actual completed work.

## Areas Assessed As Solid

- The handled-key contract is no longer duplicated across the public runtime
  interaction seam; `handled_key_materialized_t.contract` is now the clear
  runtime-facing source of truth.
- The key-runtime index is genuinely mutation-maintained in production now, and
  the dedicated index regression unit materially improves confidence over the
  earlier review-04 state.
- The action descriptor public surface is smaller after removing `caps`, and
  public capability checks now route through helper APIs instead of exposing raw
  descriptor storage.

## Verification

Commands run for this audit:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- full host suite passed
- firmware build passed and produced `.build/bastardkb_charybdis_4x6_noah.uf2`

Workspace scope:

- no sibling workspace folders were edited
