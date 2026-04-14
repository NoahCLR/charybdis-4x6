# Userspace Refactor Follow-Up Review

Date: 2026-04-14

Status: follow-up audit of the code after the audit-driven cleanup recorded in
`review/2026-04-14-review-02/`. This is a quality review of the implementation
that now exists, not a fresh architecture pass.

Scope:

- handled-key materialization and runtime interaction cleanup
- macro provider/default-seeding cleanup
- host tests and review notes where they define the real seam

Out of scope:

- hardware changes
- upstream QMK redesign
- new architecture proposals unrelated to the landed refactor

## Executive Summary

The prior must-fix handled-key regression is fixed. Transparent multi-tap
continuation metadata now comes from the effective lower tap source, the old
`*_at_position(...)` helpers are gone, and the duplicate
`handled_key_interaction_policy_t` surface was removed. I did not find a new
must-fix correctness regression in this pass.

Two should-fix issues remain:

1. the new macro provider encode helper is still payload-specific and does not
   actually serialize provider-owned cached IR
2. the runtime header still exports authored-to-interaction constructors that
   bypass contextual materialization and now serve only host tests

## Findings

### No Must-Fix Issues Found

The previous correctness risk around transparent multi-tap materialization is
resolved in the current tree.

### Should-Fix

#### 1. `macro_slot_provider_encode_write(...)` is still payload-specific, so the new provider boundary can diverge from the IR it just validated

References:

- `users/noah/lib/macro/macro_slot_provider.h:12-20`
- `users/noah/lib/macro/macro_slot_provider.h:33-39`
- `users/noah/lib/macro/macro_slot_provider.c:53-72`
- `users/noah/lib/macro/via_macro_defaults.c:46-65`
- `users/noah/lib/macro/via_macro_defaults.c:134-145`
- `tests/host/via_macro_defaults_test.c:225-251`

Why this is a problem:

- The provider abstraction says the semantic source seam is `load_ir(...)`, but
  `macro_slot_provider_encode_write(...)` does not serialize the cached IR it
  just loaded.
- Instead, it first marks the slot valid through `macro_slot_provider_load(...)`
  and then re-reads raw payload text through `lookup_payload(...)`, finally
  calling `macro_payload_encode_write(...)` on that text.
- The VIA defaults provider shows the leak clearly: `via_macro_defaults_load_ir`
  only validates the payload and leaves `ir->length = 0`, while the actual
  seeded bytes still come from a second raw-payload compile in
  `macro_slot_provider_encode_write(...)`.

Why this matters:

- The cache no longer guarantees that the validated provider output and the
  bytes written to storage are the same artifact.
- A provider that normalizes, synthesizes, or otherwise decouples IR from raw
  payload text would silently diverge on the encode path.
- The new helper also implicitly requires both `load_ir` and `lookup_payload`,
  but that requirement is not represented in the type shape.

Test gap:

- The new host test only checks that the helper uses cached load state and still
  calls `lookup_payload(...)` / `macro_payload_encode_write(...)`.
- It does not assert IR-to-bytes equivalence or exercise failure cases where the
  callbacks disagree.

Recommended fix:

- Either make the encode path serialize cached IR with
  `macro_payload_encode_ir_write(...)` and require providers to return real IR,
  or explicitly narrow the helper to payload-backed providers and rename it so
  the abstraction matches the implementation.

#### 2. `key_runtime_slot_interaction_from_resolution(...)` and `key_runtime_slot_binding_from_resolution(...)` are now test-only authored bypass paths, but they still live in the public runtime header

References:

- `users/noah/lib/key/runtime/key_runtime_interaction.h:84-109`
- `users/noah/lib/key/runtime/key_runtime_interaction.h:199-212`
- `tests/host/key_runtime_slot_test.c:61`
- `tests/host/key_runtime_slot_test.c:112`
- `tests/host/key_runtime_transition_test.c:125`
- `tests/host/key_runtime_feedback_test.c:48`
- `tests/host/runtime_debug_test.c:342`

Why this is a problem:

- The handled-key cleanup intentionally made explicit contextual
  `handled_key_materialize(...)` the real seam for runtime behavior.
- These helpers still let callers build slot interaction state directly from
  authored `handled_key_resolution_t`, including authored `has_more_taps` and
  authored `tap_resolves_on_press`, without any position context.
- In the current tree, production userspace no longer uses these constructors;
  they are only referenced by host tests.

Why this matters:

- The runtime public surface is larger than it needs to be and still advertises
  a bypass path around contextual materialization.
- Future production code could accidentally reach for these helpers and
  reintroduce the same authored-vs-materialized seam the refactor just removed.
- This also weakens the accuracy of the review record: the latest progress note
  says runtime and host tests were cut over to the materialization seam, but the
  authored-to-interaction constructors still keep an alternate path alive.

Recommended fix:

- Move these constructors into test support or an internal-only header, or make
  them explicitly test-only if they truly need to remain.
- Keep the production runtime header centered on materialized interaction state,
  not on rebuilding it from authored resolution.

### Optional Cleanup

#### 3. The newest progress note slightly overstates completion

References:

- `review/2026-04-14-review-02/progress.md:77-90`
- `review/2026-04-14-review-02/progress.md:137-139`

Why this is a problem:

- The progress entry says the runtime/tests were cut over to the explicit
  materialization seam, that VIA default seeding moved onto the provider/cache
  layer, and that there are no next steps.
- The two should-fix issues above mean that statement is directionally right,
  but stronger than the code justifies today.

Recommended fix:

- Update the active review notes once the remaining test-only authored runtime
  constructors are internalized and the macro provider encode path is either
  truly IR-backed or explicitly documented as payload-backed.

## Areas Assessed As Solid

- The previous transparent multi-tap regression is fixed. The runtime now uses
  `materialized.tap_has_more_taps` and `materialized.tap_resolves_on_press`, and
  the new lookup test covers both press-resolve and release-resolve inheritance.
- Removing the old handled-key `*_at_position(...)` helpers materially cleaned
  the public handled-key surface.
- Removing `handled_key_interaction_policy_t` made the runtime interaction
  contract easier to follow.
- The pd-mode identity transport and runtime debug registry exposure still look
  solid after the cleanup.

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
- this review pass only adds `review/2026-04-14-review-03/`
