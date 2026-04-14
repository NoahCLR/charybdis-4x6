# Userspace Refactor Follow-Up Review

Date: 2026-04-14

Status: follow-up audit of the refactor work reviewed and implemented under
`review/2026-04-14-review-01/`. This is a quality review of the code that now
exists, not a fresh architecture ideation pass.

Scope:

- handled-key contract/materialization refactor
- key-runtime registry/index refactor
- pd-mode identity sync refactor
- macro provider/cache refactor
- host-test and review-note quality where they define the real contract

Out of scope:

- hardware changes
- upstream QMK redesign
- new architecture proposals unrelated to the landed refactor

## Executive Summary

The refactor direction is mostly correct. The handled-key file split is easier
to navigate than the old monolith, pd-mode identity sync is cleaner than the
old flag transport, and the macro decode split makes the live-VIA path easier
to follow.

The audit did find one real semantic regression risk and several places where
the implementation stopped at a transitional midpoint:

1. transparent multi-tap materialization does not preserve the lower source's
   continuation metadata, so transparent wrappers can change multi-tap runtime
   behavior
2. handled-key still exposes the old contextual accessors publicly, and the
   host tests still lean on those helpers instead of treating
   `handled_key_materialize(...)` as the contract seam
3. the macro provider layer did not actually absorb VIA default seeding, even
   though the prior review notes say it did

## Findings

### Must-Fix

#### 1. Transparent multi-tap wrappers still use the top row's continuation metadata, so the refactor can change lower-layer multi-tap behavior

References:

- `users/noah/lib/key/interaction/handled_key_materialize.c:40-48`
- `users/noah/lib/key/runtime/slot/key_runtime_slot.c:214-217`
- `users/noah/lib/key/interaction/multi_tap_engine.c:85-105`
- `users/noah/lib/key/interaction/multi_tap_engine.c:108-140`
- `tests/host/key_behavior_lookup_test.c:278-289`

Why this is a problem:

- `handled_key_materialize(...)` now correctly pulls the effective tap action
  and repeat count from the lower transparent source, but it still derives
  `tap_resolves_on_press` from the original authored `resolution`.
- `key_runtime_slot_advance_pending_multi_tap(...)` then passes
  `resolution.has_more_taps` from that same original top-row lookup into
  `multi_tap_advance(...)`.
- The multi-tap engine uses `has_more_taps` and `tap_resolves_on_press` to
  decide whether a chain should keep waiting for another tap or resolve
  immediately.

Concrete failure mode:

- If a transparent wrapper key has fewer authored tap-count rows than the lower
  handled key at the same position, the runtime can reuse the lower tap action
  while still believing the chain has no more taps or should resolve on press.
- That means the action payload is taken from the lower source, but the timing
  and continuation policy are taken from the upper wrapper, which is exactly
  the kind of mixed-source semantics this refactor was supposed to eliminate.

Why the current tests missed it:

- The transparent multi-tap lookup test only checks the resolved tap action and
  repeat count.
- It does not assert the inherited `has_more_taps` / `tap_resolves_on_press`
  semantics or exercise the runtime end-to-end through the pending multi-tap
  engine.

Recommended fix:

- Materialize the effective multi-tap continuation metadata from the same
  transparent tap source that provides `tap_action` and `tap_repeat_count`.
- Add an end-to-end host test where a transparent wrapper sits over a lower key
  with at least one additional tap step beyond the wrapper's authored rows.

### Should-Fix

#### 2. The handled-key migration is still transitional: legacy `*_at_position(...)` APIs remain public, and tests still treat them as the real seam

References:

- `users/noah/lib/key/interaction/handled_key.h:220-248`
- `users/noah/lib/key/interaction/handled_key_materialize.c:64-93`
- `tests/host/key_runtime_feedback_test.c:241-253`
- `tests/host/key_behavior_lookup_test.c:219-338`

Why this is a problem:

- The intended phase-2 boundary was explicit contextual materialization through
  `handled_key_materialize(...)`.
- The header still exports the old position-aware accessors for tap, hold,
  flags, layer, and pd-mode, and the implementation simply re-routes them back
  through the materializer.
- Several host suites then stub `handled_key_materialize(...)` by rebuilding a
  materialized value from those old accessors instead of using the real
  production logic.

Why this matters:

- The public API is still larger and more transitional than the review claimed.
- The tests do not strongly protect the new seam; they can keep passing while a
  materialization bug exists, as long as the old helpers and the test stubs
  agree with each other.

Recommended fix:

- Remove or internalize the legacy `handled_key_resolution_*_at_position(...)`
  accessors once the remaining call sites are migrated.
- Update the host tests to assert `handled_key_materialize(...)` directly,
  especially for transparency and same-position fallback behavior.

#### 3. The macro provider abstraction did not actually absorb VIA default seeding, so the subsystem still has two source-loading pipelines

References:

- `users/noah/lib/macro/via_macro_defaults.c:134-149`
- `users/noah/lib/macro/macro_slot_provider.h:12-37`
- `review/2026-04-14-review-01/progress.md:117-120`

Why this is a problem:

- The new provider/cache layer now covers hardcoded validation/playback and
  live VIA decode/playback.
- VIA default validation does use the provider cache, but VIA default seeding
  still walks `via_macro_payloads[]` directly and calls
  `macro_payload_encode_write(...)` itself.
- That means the refactor did not actually produce one shared source/provider
  layer for all macro sources. It produced a shared validation/playback layer
  plus a separate authored-default seeding pipeline.

Why this matters:

- Future changes to provider behavior, diagnostics, normalization, or
  invalidation rules will still need to be implemented in two places.
- The prior review notes overstate what landed, which weakens review integrity.

Recommended fix:

- Either move VIA default seeding behind provider-owned load/encode helpers, or
  narrow the compatibility/provider claims in the review notes so they match
  the real implementation.

### Optional Cleanup

#### 4. `handled_key_interaction_policy_t` and `interaction.policy` are dead duplicates of `contract.hold` / `contract.long_hold`

References:

- `users/noah/lib/key/interaction/handled_key.h:59-62`
- `users/noah/lib/key/interaction/handled_key.h:200-204`
- `users/noah/lib/key/runtime/key_runtime_interaction.h:72-75`
- `users/noah/lib/key/runtime/key_runtime_interaction.h:195-199`
- `tests/host/key_runtime_slot_test.c:132-135`
- `tests/host/key_runtime_transition_test.c:144-147`

Why this is a problem:

- The refactor introduced a new behavior contract, but the runtime struct still
  stores a separate `policy` object containing the same hold/long-hold
  semantics.
- In the audited code, production logic reads `interaction.contract` and
  `interaction.release`; the duplicate `policy` field is only being kept alive
  by construction-time assignment and test assertions.

Why this matters:

- It makes the runtime surface look more complex than it is.
- It suggests there are two distinct semantic layers when there is really only
  one.

Recommended fix:

- Delete `handled_key_interaction_policy_t` and `interaction.policy` unless a
  real independent use case appears.

## Areas Assessed As Solid

- The handled-key file split is materially easier to navigate than the old
  single-file implementation.
- The pd-mode identity transport is cleaner and less fragile than the old
  mask-based split sync path. The empty-packet initialization for
  `PD_MODE_ID_NONE` is also handled correctly now.
- The runtime debug snapshot exposes the new registry state clearly enough to
  support higher-level assertions.
- Splitting `macro_payload_decode_qmk_stream(...)` out of
  `macro_payload_run.c` was a real improvement; the live-VIA decode path is now
  easier to locate and reason about.

## Verification

Commands run for this audit:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- full host suite passed
- firmware build passed

Workspace scope:

- no sibling workspace folders were edited
- this review pass only adds `review/2026-04-14-review-02/`
