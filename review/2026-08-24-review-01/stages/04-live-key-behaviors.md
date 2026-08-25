# Stage 04 — Live Key-Behavior Vertical Slice

Status: blocked on Stage 02

Implementation note: the isolated cross-language payload package has landed
early. It defines exact canonical row and sparse-step bytes, semantic actions,
stable wire hold-mode ids, timing overrides, auto-mouse anchoring, capacity
checks, and malformed-input rejection in Studio and firmware. Firmware
validation is reader-backed and retains no payload-sized buffer or max-sized
native row/step arrays. It does not yet translate source expressions into the
wire model, materialize semantic wire actions for the key runtime, publish an
effective provider, validate whole-profile references/action-ABI identity, or
perform any device write.

## Objective

Make supported key_behaviors rows live-editable, persistent, split-safe, and
source-aware without changing the established key-runtime semantics.

## Entry Criteria

- Stage 02 safe activation and rollback are proven.
- Behavior domain and capacity are frozen for Profile Wire v1.
- The effective profile exposes a compact behavior lookup contract.
- Profile Studio fake device supports behavior candidates and wait-for-safe
  status.
- Key behavior semantics and validation in the current tree have been
  re-read; this stage does not rely only on the opening review.

## Supported Milestone A Fields

For each row:

- keycode identity;
- tap-hold, longer-hold, and multi-tap timing overrides;
- auto-mouse anchoring flag;
- up to the compiled tap-count ceiling;
- tap action presence and action;
- hold and long-hold presence, action, mode, and repeat rate.

The live schema supports only action kinds already accepted by the runtime
action contract. Adding a new action kind or executable handler remains a
firmware change.

## Runtime Design

Replace direct key_behaviors[] scanning with an effective behavior provider.
The provider should:

- preserve compiled-default behavior when no live override exists;
- use a compact encoded store and a bounded index rather than copying native
  key_behavior_t rows wholesale;
- return an immutable row/view for the active generation;
- keep lookup and materialization cost bounded;
- validate duplicates, action support, timing, repeat rate, tap depth, layer
  references, macro slots, pointer modes, and reachability;
- expose generation identity for diagnostics.

Do not hold pointers into a candidate or old generation across activation.
Press tokens that already exist must complete under the documented activation
rule. The default rule is to wait for full runtime quiescence before publishing
a behavior generation.

## Quiescence Evidence

At minimum, activation tests must cover:

- ordinary physical key down;
- managed held keycode or modifier;
- pending tap-hold threshold;
- pending multi-tap chain;
- repeating action;
- delayed or queued release;
- momentary and locked layer ownership;
- active pointer mode and pointer-mode lock;
- active macro and macro-owned keys;
- active combo and delayed combo output;
- auto-mouse anchoring and mouse-button ownership;
- split peer pending.

The activation owner reports why it is waiting. It must not force-release user
state merely to make a profile apply appear fast.

## Profile Studio Contracts

- Existing behavior editor remains the source editor.
- Add and edit become live candidates when connected and compatible.
- Removing a row is supported.
- Field validation occurs locally for fast feedback and again in firmware.
- Apply status can be waiting for keys to be released.
- The UI shows the prior generation remains active while waiting.
- Pull recreates supported initializer syntax without damaging unrelated rows,
  comments, macros, combos, or layout.
- Semantic diff identifies row and field changes.

## Likely Files

- users/noah/lib/profile/runtime/ behavior provider and index
- users/noah/lib/key/behavior/key_behavior.h
- users/noah/lib/key/behavior/key_behavior_lookup.c and headers
- users/noah/lib/key/behavior/keymap_validation.c
- key runtime integration only where generation or quiescence requires it
- ownership and diagnostic modules for safe-boundary evidence
- keyboards/.../keymap.c only for default materialization contract changes
- tools/charybdis-profile-studio/ behavior operations and diff
- behavior, key runtime, profile, lifecycle, split, memory, and stack tests

## Deliverables

- [x] Desktop canonical key-behavior payload codec and golden vector
- [x] Matching reader-backed firmware codec and cross-language fixture
- [ ] Effective behavior provider
- [ ] Compact bounded index
- [ ] Complete Milestone A row validation
- [ ] Direct compiled-array consumers removed from production lookup
- [ ] Safe waiting activation with reason diagnostics
- [ ] Add, edit, and remove live flows
- [ ] Persistent apply and peer convergence
- [ ] Semantic diff, push, pull, and reset
- [ ] Regression gate against direct array reads
- [ ] Updated docs, Studio UI, stage, risks, and progress

Early codec evidence:

- `users/noah/lib/profile/schema/key_behavior_domain_v1.c` encodes canonical
  row/step order and validates from a bounded reader at arbitrary base offsets;
- the validated domain handle is statically bounded to 128 bytes and individual
  reads to 12 bytes, with accessors that re-resolve row identity instead of
  trusting caller-supplied offsets;
- `tests/fixtures/key_behavior_domain_v1.fixture` is consumed directly by C
  and JavaScript;
- `tests/host/run_key_behavior_domain_v1_tests.sh` runs normal and ASan/UBSan
  coverage for the exact vector, every frozen capacity, malformed records, and
  reader failure at every read boundary.

The remaining materialization seam is deliberate: wire actions are validated
as stable `{kind, operand}` values, but no code yet converts them into a
generation-owned runtime action/handled-key view. That conversion must first
validate the connected action-ABI digest and whole-profile layer, PD-mode, and
macro references, then publish only at the Stage 02 safe activation boundary.

## Verification

Run all matching targeted runners from AGENTS.md, including:

- key behavior lookup and validation
- keymap and real-profile validation
- key-runtime release matrix
- modifier-hold, pd-mode, and layer-lock integrations
- key-runtime scenario and integration harness
- action lifecycle, held action, owned keycode, layer ownership
- macro lifecycle where behavior actions target macros
- combo-origin and split sync where behavior keys originate from combos
- safe activation tests for every quiescence case above
- malformed, duplicate, capacity, unsupported action, and cross-reference
  profile candidates
- Profile Studio checks and behavior UI harness
- feature-gate compile
- full host suite
- firmware compile
- fresh memory and stack gates
- real-board behavior apply, wait, persist, reboot, reconnect, both USB
  orientations, role swap, and stuck-output matrix
- git diff --check

## Exit Criteria

- Every behavior field currently editable by Studio can be applied live.
- Add, edit, and remove operations survive reboot and converge to the peer.
- An active or pending key lifecycle delays activation without changing the old
  behavior or leaving outputs stuck.
- Invalid candidates leave the prior generation untouched.
- Default behavior is unchanged when no live profile is valid.
- Provider lookup meets measured memory, stack, and runtime bounds.
- Source/device diff, push, pull, reset, tests, firmware compile, and hardware
  evidence are recorded.

## Handoff

Stage 05 treats Stages 03 and 04 as two domains of one profile transaction. It
must test simultaneous RGB and behavior edits and resolve any conflicting
preview, invalidation, or source operation semantics.
