# Userspace Architecture Review

Date: 2026-04-14

Status: follow-up quality audit of the refactor work landed through
`review/2026-04-14-review-04/`.

Scope:

- correctness and regression risk in the newly landed runtime seams
- quality of the new abstractions and interface cleanup
- test quality and review integrity for the refactor follow-up

Out of scope:

- fresh architecture brainstorming
- hardware changes
- upstream QMK redesign outside this repo

## Findings

### No Must-Fix Issues Found

I did not find a new correctness failure that is clearly shipping broken
behavior today. The refactor is substantially cleaner than the pre-review-04
state, and the full host suite plus firmware build are green.

### Should-Fix

#### 1. The handled-key “materialized contract” is still duplicated, and the internal policy seam leaks back out through runtime interaction

References:

- `users/noah/lib/key/interaction/handled_key_materialize.c:7-35`
- `users/noah/lib/key/runtime/key_runtime_interaction.h:11-12`
- `users/noah/lib/key/runtime/key_runtime_interaction.h:165-197`
- `tests/host/key_runtime_slot_test.c:82-85`
- `tests/host/key_runtime_transition_test.c:190-193`
- `tests/host/key_runtime_transition_test.c:252-254`

Why this matters:

- `handled_key_materialize(...)` now computes and stores
  `handled_key_materialized_t.contract`, which suggests the materialized
  handled key is the authoritative behavior bundle.
- But `key_runtime_slot_materialize(...)` ignores that cached contract and
  rebuilds `interaction.contract` from lower-level fields with a second call to
  `handled_key_behavior_contract(...)`.
- `key_runtime_interaction.h` also includes `handled_key_policy.h` directly, so
  the internal policy builders are still reachable from any consumer of the
  runtime interaction header.
- The host runtime suites still rely on that leak: they “refresh” interaction
  views by calling `handled_key_behavior_contract(...)` themselves.

Why this is still a regression risk:

- The new explicit materialization seam was supposed to collapse behavior into
  one resolved contract. Right now there are still two authoritative ways to
  obtain that contract.
- If contract construction ever gains logic that is not perfectly reducible
  from `binding + flags + pd_mode + hold_strategy`, the runtime path and the
  materializer path can silently drift again.
- Tests are currently coupled to the recompute path, so they would not force a
  clean cutover.

Recommended direction:

- Make the materialized contract the single source of truth for runtime
  interaction construction.
- Stop including `handled_key_policy.h` from `key_runtime_interaction.h`.
- Update the remaining tests to compare or inject fully materialized
  interactions without calling the internal contract builder directly.

#### 2. The action-kind consolidation is only partial, and the public action descriptor still exposes internal capability storage

References:

- `users/noah/lib/action/action_kind_internal.h:5-18`
- `users/noah/lib/action/action_dispatch.h:73-100`
- `users/noah/lib/action/action_lifecycle.c:22-30`
- `users/noah/lib/action/action_lifecycle.c:171-240`

Why this matters:

- `action_kind_internal.h` says the shared action-kind metadata covers
  “authored validation, lifecycle dispatch, and handled-key hold policy”.
- In practice, `noah_action_kind_def_t` only contains capability bits and two
  handled-key policy hints.
- Tap/press/release dispatch still lives in a separate per-kind table inside
  `action_lifecycle.c`.
- `noah_action_desc_t` still exports a raw `caps` field publicly, and all of
  the public predicates read that stored bitfield directly.

Why this is still a maintainability problem:

- The action model is cleaner than before, but it is not actually one internal
  contract yet.
- Adding or changing an action kind still spans the shared definition table,
  the public descriptor/capability surface, and the lifecycle ops table.
- Because the descriptor stores `caps`, internal action-kind representation is
  still bleeding into the public API rather than being kept behind predicates
  and classifier helpers.

Recommended direction:

- Either finish the consolidation by moving lifecycle ops into the shared
  action-kind definition, or narrow the internal header/comment so it does not
  imply that lifecycle already uses the shared table.
- Consider removing `caps` from the public descriptor and keeping capability
  decisions behind helpers if the goal is a smaller, more stable public action
  surface.

#### 3. The new mutation-maintained index is only weakly protected by tests because several runtime suites still resync the whole table manually

References:

- `tests/host/key_runtime_feedback_test.c:49-62`
- `tests/host/key_runtime_admission_test.c:42-45`
- `tests/host/key_runtime_preflight_test.c:77-85`
- `tests/host/key_runtime_transition_test.c:126-135`
- `tests/host/runtime_debug_test.c:400-405`
- `tests/host/runtime_debug_test.c:474-477`

Why this matters:

- The review-04 follow-up changed the runtime contract from “rebuild before
  read” to “mutations keep the registry current”.
- But several of the low-level runtime suites still call
  `key_runtime_index_sync_slot(...)` across the full slot table before making
  assertions.
- That means those suites are not actually verifying the new guarantee that the
  production mutation sites keep the index up to date.
- The new runtime-debug assertions do check that index fields are present in
  snapshots, but they only cover a generic populated state and a reset state,
  not positive transitions for `preview_owner_slot` or
  `pending_fallback_slot`.

Why this is a testing risk:

- A missed `key_runtime_index_sync_slot(...)` call in a production mutation path
  can still slip through if the surrounding tests manually resynchronize state
  before reading it.
- That is exactly the regression class this refactor was trying to remove.

Recommended direction:

- Keep white-box slot-state tests if they are useful, but add at least one
  mutation-focused suite that only mutates state through the production slot
  APIs and then asserts index membership/order/preview/fallback without any
  manual resync step.
- Add explicit positive assertions for `preview_owner_slot` and
  `pending_fallback_slot` transitions, not just the `UINT8_MAX` reset case.

### Optional Cleanup

#### 4. The review-04 progress note now overstates finality

References:

- `review/2026-04-14-review-04/progress.md:157-161`

Why this matters:

- The implementation record says “otherwise no immediate follow-up is required
  from this review pass”.
- That is stronger than the current code supports. The handled-key contract is
  still duplicated/leaky, the action-kind consolidation is still partial, and
  the new registry contract still has a meaningful test gap.

Recommended direction:

- Trim that note to reflect that the pass materially improved the design, but
  did not completely close the remaining abstraction and test-seam debt.

## Areas Assessed As Solid

- The key-runtime index is now genuinely mutation-maintained in the production
  runtime; the earlier rebuild-on-read architecture issue is resolved.
- `noah_process_record_user(...)` is now structurally clearer as an ordered
  stage pipeline, and the routing order is easier to inspect than the older
  monolithic chain.
- The main runtime suites do go through the production
  `handled_key_materialize(...)` seam now; the old position-helper compatibility
  layer is gone.
- The repo still preserves a strong authored-data vs reusable-runtime split,
  and compat contracts remain centralized under `users/noah/lib/compat/`.

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
