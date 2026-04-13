# Userspace Architecture Review

Date: 2026-04-13

Status: active follow-up quality audit after
[2026-04-13-review-04](../2026-04-13-review-04/userspace-architecture-review.md).
This pass audits the code that now exists after the earlier refactor work. It
is not a fresh design pass. The hardware is treated as fixed; this review is
about software quality, semantic fidelity, maintainability, and whether the
landed abstractions actually improved the codebase.

Implementation update later the same day: the follow-up fixes from this audit
have now landed. Pending multi-tap release now preserves the long-hold-only
release path, the invariant release phase-policy table is reducer-local again
instead of cached per slot, active slot storage now carries
`key_runtime_slot_interaction_t` directly with no dead `valid` / `view`
wrapper, and the RGB layer-render host harness now models exclusive pd-mode
selection state instead of synthetic local flag composition. The findings
below are retained as the rationale for those changes.

Scope:

- `users/noah/` runtime and compatibility surfaces
- host tests and maintainer docs that define the refactor's real contract
- current authored/runtime seams as shipped in this repo

Out of scope:

- hardware changes
- upstream QMK redesign
- speculative architecture replacement without evidence from the live code

## Executive Summary

The refactor materially improved the codebase. The handled-key authored/runtime
split is more explicit, pd-mode selection state is more honest, compat seams
are cleaner, and the macro pipeline now has one real semantic core. The host
suite and firmware build are also green.

That said, this pass found one real semantic hole plus a few places where the
new abstractions still overexpose implementation detail:

1. pending multi-tap release does not fully preserve the documented
   longer-hold-on-release contract
2. the cached release "phase contract" still stores global policy as per-slot
   mutable state
3. cached slot interaction storage still exposes dead wrapper fields that host
   code treats like public API
4. one higher-level RGB suite still models pd-mode with obsolete composable
   flag semantics instead of the exclusive runtime state that now exists

## Findings

### Must-fix

#### 1. Pending multi-tap release drops the long-hold-only release path

References:

- `users/noah/lib/key/interaction/multi_tap_engine.c:108-140`
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c:85-99`
- `docs/INTERACTION_MODEL.md:141-167`
- `tests/host/key_runtime_slot_test.c:1010-1044`

Why this is a problem:

- `multi_tap_resolve_hold(...)` only reasons about `mt->hold`; it never reads
  `mt->long_hold`.
- The follow-up upgrade in
  `key_runtime_slot_pending_multi_tap_release_context(...)` only selects the
  cached release-hold contract when either:
  - there is no primary hold and the resolved action is already `KC_NO`, or
  - the resolved action already equals the primary release-hold action.
- A tap-count branch with `.long_hold = TAP_ON_RELEASE_AFTER_HOLD(...)` and no
  `.hold` therefore falls back to tap resolution on release, even after
  `longer_hold_term` has elapsed.

Why this matters:

- The documented interaction model says each tap-count branch can define its
  own tap, hold, and longer-hold behavior, and release-resolved hold modes are
  part of that model.
- The active-slot release path already has the cached release-hold selector to
  choose between primary and long release actions. Pending multi-tap is now the
  inconsistent path.
- Current coverage only proves the case where both primary and long
  release-on-release actions exist. The test at
  `tests/host/key_runtime_slot_test.c:1010-1044` does not cover the
  long-hold-only branch, so this regression can stay green.

Recommended direction:

- Make pending multi-tap release select from the cached release-hold contract
  whenever a release-time long-hold action is available, not only when a
  primary hold action is present or the resolved action is `KC_NO`.
- Add a host regression that covers:
  - multi-tap pending hold
  - no primary hold
  - `long_hold = TAP_ON_RELEASE_AFTER_HOLD(...)`
  - elapsed time beyond `longer_hold_term`

### Should-fix

#### 2. The release phase-contract table is still global policy duplicated into every slot

References:

- `users/noah/lib/key/runtime/key_runtime_interaction.h:36-65`
- `users/noah/lib/key/runtime/key_runtime_interaction.h:145-189`
- `tests/host/key_runtime_slot_test.c:570-575`
- `tests/host/key_runtime_slot_test.c:681-683`

Why this is a problem:

- `key_runtime_slot_release_phase_contracts_build()` takes no interaction data
  and returns the same table for every slot.
- That invariant table is still copied into every
  `key_runtime_slot_release_contract_t`.
- Low-level tests now assert the cached phase-table booleans directly, which
  means the refactor widened the stored/public shape of the contract instead of
  actually reducing the surface area that matters.

Why this matters:

- This is complexity moved, not complexity removed.
- The slot-owned release contract should carry slot-owned facts: tap outcome,
  release-hold actions, pd-mode lock behavior, fallback suppression, and other
  per-press semantics.
- The phase table itself is currently reducer policy. Caching it per slot
  increases storage, expands debug/test surfaces, and hardens a static policy
  matrix into mutable-looking state without any real semantic benefit.

Recommended direction:

- Keep slot-specific release facts cached.
- Move the invariant phase table back to reducer-local `static const` policy or
  a shared immutable helper, and keep tests focused on outcomes rather than the
  exact cached boolean layout.

#### 3. Cached slot interaction storage still exposes dead wrapper state and keeps raw slot layout sticky

References:

- `users/noah/lib/key/runtime/key_runtime_shared_state.h:39-50`
- `users/noah/lib/key/runtime/key_runtime_shared_state.h:64-70`
- `users/noah/lib/key/runtime/slot/key_runtime_slot.c:284-287`
- `tests/host/key_runtime_transition_test.c:138-145`
- `tests/host/runtime_debug_test.c:366-368`
- `tests/host/runtime_debug_test.c:435-436`

Why this is a problem:

- `active_key_state_t` still stores cached interaction behind
  `slot->interaction.valid` and `slot->interaction.view`.
- The runtime never uses `valid` to guard behavior; it simply reads
  `slot->interaction.view`.
- Host tests and the debug snapshot still treat both wrapper fields as real
  contract surface and mutate/assert them directly.

Why this matters:

- The refactor's stated goal was to make `key_runtime_slot_interaction_t` the
  real slot-owned semantic cache.
- Keeping a dead wrapper around it means the raw storage layout is still acting
  like public API, which makes later refactors noisier and encourages more
  test setup that writes slot internals directly.

Recommended direction:

- Either make the wrapper semantically meaningful and enforce it in runtime
  code, or remove it.
- Prefer semantic slot-fixture helpers for tests over direct writes to
  `.interaction.view`.

### Optional Cleanup

#### 4. The RGB layer-render suite still uses obsolete pd-mode flag semantics

References:

- `tests/host/rgb_layer_render_test.c:139-145`
- `tests/host/rgb_layer_render_test.c:200-233`
- `tests/host/rgb_layer_render_test.c:279-312`
- `users/noah/lib/pointing/runtime/pd_mode_snapshot.c:5-8`

Why this is worth cleaning up:

- The live runtime now stores one selected active mode and one selected locked
  mode.
- `rgb_layer_render_test.c` still fabricates local and display pd-mode state as
  raw flag sets and reconstructs "the first active mode" from those flags.
- That lets a higher-level rendering suite pass under states the real runtime
  does not produce.
- The file header in `pd_mode_snapshot.c` still mentions "first active mode
  selection", which preserves the old mental model in maintainer-facing code.

Recommended direction:

- Update the RGB render stubs to use exclusive `active_mode` / `locked_mode`
  state or the real pd-mode controller.
- Refresh the stale snapshot comment so docs match the implementation.

## Areas That Are Solid

These parts of the refactor look genuinely successful in the live tree:

- The handled-key seam is materially cleaner:
  `users/noah/lib/key/interaction/handled_key.h`,
  `users/noah/lib/key/runtime/key_runtime_interaction.h`, and the slot
  reducers now separate authored lookup from slot-owned cached semantics in a
  way that is easier to reason about than the old view-style contract.
- The pd-mode state model is better:
  `users/noah/lib/state/runtime/runtime_shared_state.h`,
  `users/noah/lib/pointing/runtime/pd_mode_state.c`, and
  `users/noah/lib/pointing/runtime/pd_mode_snapshot.c` now expose the
  exclusivity invariant directly instead of hiding it behind "first active"
  readers.
- The compat split is real, not cosmetic:
  `users/noah/lib/compat/qmk_pointing_contract.h`,
  `qmk_auto_mouse_contract.h`,
  `qmk_via_playback_contract.h`, and
  `qmk_via_storage_contract.h` are narrower and easier to audit than the old
  broad umbrella surfaces.
- The macro refactor achieved its design goal:
  `users/noah/lib/macro/macro_payload.c`,
  `macro_payload_encode.c`,
  `macro_payload_run.c`,
  `via_macro_defaults.c`, and
  `users/noah/lib/compat/qmk_contract.c` now revolve around one IR model with
  VIA treated as a codec boundary, not as a separate semantic engine.

## Verification

Verification run during this audit:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- full host suite passed
- firmware build passed
